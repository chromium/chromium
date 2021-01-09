# Regression test selection (RTS)

Regression Test Selection (RTS) is a technique to intellegently select tests to
run, without spending too many resources on testing, but still detecting bad
code changes.

[TOC]

## Current strategy

The current strategy is to skip files which we deem unaffected by the CL's
changelist. Affectedness is estimated using the heuristic that dependent files
are committed together. We look at git history and measure how often each file
A appears together each file B in the same commit. If A affects B,
and B affects C, we assume A also affects C. We represent these transitive
relationships in a graph where files are the nodes and the probability that the
two files affect each other are the weighted edges. The distance between two
nodes represents the (inverse) probability that these two files affect
each other. Finally, we skip tests in files that are farther than a
certain distance from the files in the CL.

## Skipping mechanism

Test skipping happens at the GN level in
[source_set](/build/config/BUILDCONFIG.gn) and [test](/testing/test.gni)
GN targets.

## Known failure modes

There are not known to be many instances of these failure modes in the codebase.
Those that are known are never excluded by our model.

- **Shared state in test files**: Consider a test file A that contains unit tests, as well as some variables
used in another file B. When our RTS strategy excludes A, but not B, a
compilation error will occur.
- **main() defined in test files**: A test file contains tests and the `main()` function for the entire suite.
When it is excluded, the whole suite fails to compile.

## Design Docs

- [File-level RTS](http://doc/1KWG82gNpkaRAchlp3jtENFdlefGvJxMHkAszlu9fo1c)
- [Chrome RTS](http://doc/10RP1XRw8ZSrvgVky1flH7ykAaIaG15RymM4gW7bFURQ)

