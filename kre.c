/* kre.c - a simple regex engine and search tool
 *
 * --- SETUP ---
 * 
 * I've written this file to be able to:
 * 
 *   * be included as a single file in an existing C/C++ project
 *   * be compiled as a 'kre' executable 
 *
 * To compile as a 'kre' executable (that works basically like grep), run:
 * 
 * $ cc -DEXE -o kre kre.c
 * $ ./kre --help
 * ...
 * 
 * Otherwise, it should work like any other C/C++ files in your project,
 *   but you'll need just the definitions. In this file, you should copy
 *   all the code between '//// HEADER START ////' and '//// HEADER END ////'
 *   to your project's header file.
 * 
 * 
 * --- BASIC USAGE ---
 * 
 * First, create a regex pattern:
 *   kre_pat pat;
 *   kre_pat_init(&pat, "[a-zA-Z_][a-zA-Z_0-9]*");
 * Then, create an simulator:
 *   kre_sim sim;
 *   kre_sim_init(&sim, &pat);
 * Then, you can feed a string through the iterator:
 *   for (i = 0; i < len; ++i) {
 *     // attempt to start matching here
 *     kre_sim_feedz(&sim, KRE_START);
 *     if (kre_sim_feedc(&sim, src[i])) {
 *       // match found!
 *     }
 *   }
 * You can give it any source characters, and 'kre_sim_feedc'
 *   will return true if the source string matches at a given position
 * This usage will only tell whether a match is found, not what
 *   the matching substring is. As a result, this method can be used
 *   on streams with low overhead and low memory usage
 * 
 * 
 * --- ADVANCED USAGE ---
 * 
 * For many use cases, the actual substring that matches (along with
 *     capture groups) is desired. To do this, you can use the iterator 
 *     structure (kre_iter):
 *   kre_iter iter;
 *   kre_iter_init(&iter, &pat);
 *   for (i = 0; i < len; ++i) {
 *     kre_iter_feedc(&iter, src[i]);
 *     for (j = 0; j < iter->matches_len; ++j) {
 *       struct kre_iter_match* m = iter->matches[j];
 *       char* ms = kre_iter_match_gets(&iter, m);
 *       printf("got match: %s\n", ms);
 *       free(ms);
 *     }
 *   }
 * 
 * LICENSE: https://kata.tools/kpl
 * 
 * @author: Cade Brown <me@cade.site>
 */

//// HEADER START ////

/// INCLUDES ///

// C stdlib
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>


/// TYPEDEFS ///

// describes what kinds of nodes there are
enum kre_kind {
    // matches epsilon (i.e. empty string/anything)
    KRE_EPS,

    // matches any character that is part of a set
    // this kind is also used for single characters, which are trivially a set of 1 characters
    // NOTE: Unicode is not supported... this is left as an exercise for the reader
    KRE_SET,

};

// regular expression NFA node structure
struct kre_node {

    // what kind of node is this?
    // see 'KRE_*' above
    enum kre_kind kind;

    // outward edges of the NFA node (as indices into an array of nodes), OR:
    //   -1: empty/no edge
    //   -2: open edge that is unlinked, which means a match state
    int u, v;

    // if kind==KRE_SET, this is the set of characters that this node matches
    // NOTE: as mentioned, Unicode is note supported, and so only 256 characters are supported, corresponding to ASCII (first 128), and byte values
    bool *set;

};

// regular expression pattern, which can be used to search or validate text
typedef struct {

    // number of NFA nodes
    int nfa_len;

    // the NFA nodes
    struct kre_node* nfa;

    // which node in 'nfa' to start matching with
    int nfa_start;

    // the source the pattern was compiled from
    // NOTE: this is just for debugging purposes, and is not used during the actual search
    char* src;

} kre_pat;


// regular expression state simulator, used to simulate the NFA state machine
// NOTE: this only tells whether something has matched, not what the
//         matching substring/groups is/are. for that, use 'kre_iter'
typedef struct {

    // the pattern being searched for (should not change!)
    kre_pat* pat;

    // bitset representing the current state(s) of the NFA, i.e. 'in[s]' tells whether
    //   the NFA is currently in state 's'
    // possible optimization: http://c-faq.com/misc/bitsets.html
    bool* in;
    
    // another array of inputs, used as ping-pong buffers to efficiently feed the iterator
    bool* lastin;

} kre_sim;


// internal structure that represents a single
struct kre_iter_path {

    // simulator used for this path
    kre_sim sim;

    // match start (inclusive) and end (exclusive) as positions
    int Ms, Me;

    // group start (inclusive) and end (exclusive) as positions
    int *Gs, *Ge;

};

// regular expression search iterator, used to iterate over matches found
typedef struct {

    // length and capacity of the array
    int paths_len, paths_cap;

    // array of paths, which are valid paths that have been initialized
    struct kre_iter_path* paths;

    // length and capacity of the input buffer
    int buf_len, buf_cap;

    // input buffer
    char* buf;

} kre_iter;


/// API ///

// make a new regular expression pattern from 'src', returns NULL (if successful),
//   or a string describing the error. you should pass the error string to 'free()'
//   when you are done with it
// NOTE: call 'kre_pat_free(pat)' when you're done with it
char*
kre_pat_init(kre_pat* pat, const char* src);

// free a regular expression pattern's resources/memory
void
kre_pat_free(kre_pat* pat);


// initialize a simulator with a given pattern
// NOTE: call 'kre_sim_free(sim)' when you're done with it
void
kre_sim_init(kre_sim* sim, kre_pat* pat);

// free an simulator's resources/memory
void
kre_sim_free(kre_sim* sim);

// reset the simulator's state, as if it were just created
void
kre_sim_reset(kre_sim* sim);

// feed a single character to the simulator, returning whether it is in
//   a matching state
bool
kre_sim_feedc(kre_sim* sim, char c);


// initialize an iterator with a given pattern
// NOTE: call 'kre_iter_free(iter)' when you're done with it
void
kre_iter_init(kre_iter* iter, kre_pat* pat);

// initialize an iterator's resources/memory
void
kre_iter_free(kre_iter* iter);

// reset the iterator's state, as if it were just created
void
kre_iter_reset(kre_iter* iter);

// feed a single character to the iterator, returning whether there were any
//   finalized matches produced
bool
kre_iter_feedc(kre_iter* iter, char c);

//// HEADER END ////




/// IMPL ///

//// IMPL: kre_pat ////

static int
kre_parse_(kre_pat* pat, const char* src);

char*
kre_pat_init(kre_pat* pat, const char* src) {
    // first, copy over the source for debugging purposes
    int sl = strlen(src);
    pat->src = malloc(sl + 1);
    memcpy(pat->src, src, sl);
    pat->src[sl] = '\0';

    // start out with no NFA nodes
    pat->nfa_len = 0;
    pat->nfa = NULL;

    // now, actually parse and return the start state
    pat->nfa_start = kre_parse_(pat, src);

    // no error
    return NULL;
}

void
kre_pat_free(kre_pat* pat) {
    free(pat->src);
    free(pat->nfa);
}


//// IMPL: kre_sim ////

void
kre_sim_init(kre_sim* sim, kre_pat* pat) {
    sim->pat = pat;
    sim->in = malloc(sizeof(*sim->in) * pat->nfa_len);
    sim->lastin = malloc(sizeof(*sim->lastin) * pat->nfa_len);

    // start off by resetting it
    kre_sim_reset(iter);
}

void
kre_sim_free(kre_sim* sim) {
    free(sim->in);
    free(sim->lastin);
}

void
kre_sim_reset(kre_sim* sim) {
    // initialize everything to false
    int i;
    for (i = 0; i < pat->nfa_len; i++) {
        sim->in[i] = sim->lastin[i] = false;
    }
    // then, set the start state to true
    sim->in[sim->pat->nfa_start] = true;
}

// add a state
static bool
kre_sim_add_(kre_sim* sim, int i) {
    if (i == -1) {
        // empty/nothing further
        return false;
    } else if (i == -2) {
        // match/accept
        return true;
    }

    // should have handled special signals/values
    assert(i >= 0);
    // should be in range
    assert(i < sim->pat->nfa_len);

    bool res = false;

    struct kre_node* n = &sim->pat->nfa[i];
    if (sim->pat->nfa[i].kind == KRE_EPS) {
        // on epsilon nodes, simulate an instant transition to those states
        // NOTE: an simulator should never be "in" a state that is an epsilon node, since
        //         by definition it should instantly transition out as well
        if (kre_sim_add_(sim, n->u)) res = true;
        if (kre_sim_add_(sim, n->v)) res = true;
    } else {
        // not epsilon, so transition directly and record it
        sim->in[i] = true;
    }

    return res;
}

bool
kre_sim_feedc(kre_sim* sim, char c) {
    // swap buffers, since we're about to overwrite them
    bool* tmp = sim->in;
    sim->in = sim->lastin;
    sim->lastin = tmp;

    // clear what states we are in
    int i;
    for (i = 0; i < sim->pat->nfa_len; i++) {
        sim->in[i] = false;
    }

    bool res = false;

    // now, traverse where we were in (lastin), and see if we can transition to any new states,
    //   and add those to the current states we're in
    for (i = 0; i < sim->pat->nfa_len; i++) {
        if (sim->lastin[i]) {
            struct kre_node* n = &sim->pat->nfa[i];

            // whether the current character ('c') matches the current node
            bool valid = false;
            if (n->kind == KRE_SET) {
                valid = n->set[c];
            }

            if (valid) {
                // since this state is valid, try to transition to other states as well (recursively)
                if (kre_sim_add_(sim, n->u)) res = true;
                if (kre_sim_add_(sim, n->v)) res = true;
            }
        }
    }

    return res;
}

//// IMPL: kre_iter ////

void
kre_iter_init(kre_iter* iter, kre_pat* pat) {
    iter->pat = pat;

    iter->paths_cap = iter->paths_len = 0;
    iter->paths = NULL;

    iter->buf_cap = iter->buf_len = 0;
    iter->buf_len = 0;

    // start off by resetting it
    kre_iter_reset(iter);
}

void
kre_iter_free(kre_iter* iter) {
    int i;
    for (i = 0; i < iter->paths_cap; i++) {
        struct kre_path* p = &iter->paths[i];
        // free each path's resources
        kre_sim_free(&p->sim);
        free(p->Gs);
        free(p->Ge);
    }
    free(iter->paths);
    free(iter->buf);
}

void
kre_iter_reset(kre_iter* iter) {
    // reset lengths to 0, but don't free
    iter->paths_len = 0;
    iter->buf_len = 0;
}

// feed a single character to the iterator, returning whether there were any
//   finalized matches produced
bool
kre_iter_feedc(kre_iter* iter, char c) {
    int i, j;

    // whether we have already added a new path
    bool has_newpath = false;
    for (i = 0; i < iter->paths_len; ++i) {
        struct kre_iter_path* p = &iter->paths[i];
        kre_sim* s = &p->sim;
        if (!has_newpath && p->Ms < 0) {
            // we don't have a new path yet, so add one
            p->Ms = iter->buf_len;
            p->Me = -1;
            has_newpath = true;
        } else if (!has_newpath && i == iter->paths_len - 1) {
            // on last iteration and still don't have a new path, so append
            // NOTE: this will cause there to be an extra iteration to handle it
            if (iter->paths_len >= iter->paths_cap) {
                // reallocate to append new state
                iter->paths_cap = iter->paths_len * 2 + 4;
                iter->paths = realloc(iter->paths, sizeof(*iter->paths) * iter->paths_cap);
                for (i = iter->paths_len; i < iter->paths_cap; ++i) {
                    struct kre_iter_path* p = &iter->paths[i];
                    kre_sim_init(&p->sim, iter->pat);
                    p->Gs = malloc(sizeof(*p->Gs) * iter->pat->nfa_len);
                    p->Ge = malloc(sizeof(*p->Gs) * iter->pat->nfa_len);
                }
            }
            // start a new path to match
            iter->paths[iter->paths_len].Ms = iter->buf_len;
            iter->paths[iter->paths_len].Me = -1;
            iter->paths_len++;
            has_newpath = true;
        } 
        if (p->Ms >= 0) {
            // active path, so feed it
            bool ismatch = kre_sim_feedc(s, c);
            if (ismatch) {
                // have a match, so update the end to the longest match
                p->Me = iter->buf_len;
            }   
            // whether it has any state left (i.e. could still match)
            bool has_instate = false;
            for (j = 0; !has_instate && j < s->nfa_len; ++j) {
                has_instate = s->in[j];
            }

            if (!has_instate) {
                // done with this path, so remove it and/or add match
                if (p->Me >= 0) {
                    // we had a valid match, so add it to the queue
                } else {
                    // no match, so just remove it
                    if (i == iter->paths_len - 1) {
                        // last path, so remove it
                        iter->paths_len--;
                    } else {
                        // just set to invalid
                        p->Ms = -1;
                    }
                }
            }
        }
    }
}

/// CLI ///

// NOTE: compile with '-DEXE' to run as an executable
#ifdef EXE

int
main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s [pat] args...", argv[0]);
        exit(1);
    }

    // initialize search pattern
    kre_pat pat;
    kre_pat_init(&pat, argv[1]);

    // initialize simulator as well
    kre_sim sim;
    kre_sim_init(&sim, &pat);

    // buffering size
    int bufsz = 4096;
    char* buf = malloc(bufsz);

    int i, j;
    for (i = 2; i < argc; i++) {
        // assume file
        // TODO: also check if it's a directory, and recursively search it
        char* arg = argv[i];
        FILE* fp = fopen(arg, "r");
        if (!fp) {
            perror(arg);
            exit(1);
        }
        // handle buffers of the file
        while (true) {
            // try to read a buffer, up to 'bufsz'
            size_t sz = fread(buf, 1, bufsz, fp);
            if (sz == 0) break;

            for (j = 0; j < sz; ++j) {
                if (kre_sim_feedc(&it, buf[j])) {
                    // found match
                    printf("MATCH\n");
                }
            }
        }

        fclose(fp);
    }

    // free resources
    free(buf);
    kre_sim_free(&it);
    kre_pat_free(&pat);
}

#endif // EXE
