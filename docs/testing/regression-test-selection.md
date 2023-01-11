# Regression test selection (RTS)

[TOC]

## What is RTS?
RTS is a selection strategy to run a minimal amount of tests to validate a CL.
Since small code changes don’t always need every test to run, RTS decides which
tests are the most important to run based on which files changed. In our
application of RTS we attempt to catch at least 95% of CL rejections when RTS
is applied (the remaining 5% are still caught in CQ+2).

Our model is being generated daily on the rts-model-packager builder to keep
adjusting the model for new data and tests.


## RTS in CQ+1

When enabled, RTS in a CQ+1 build will choose the tests most likely to fail
based on how how often the test files and source files in the current CL have
changed together as well as the distance in the file graph of those files.


## RTS in CQ+2

When enabled, RTS does not run the same strategy. Instead, to ensure we still
have 100% test coverage, the tests that were skipped by RTS are run. This will
only work if a build with RTS enabled is found in the last 24 hours that can
be reused, otherwise the build will compile and run all tests as if RTS doesn't
exist.


## What if RTS is causing problems with my CL?
If RTS needs to be disabled, add this footer to the commit message:

    Disable-Rts:True

This will disable RTS for both Dry Run and submit so that all tests will run in
both. If you believe the build failed because of RTS infrastructure please file
a bug http://crbug.com/new under the ‘Infra>Client>Chrome’ component.


## Is RTS running in my build?

RTS is only enabled on a subset of long pole builders. Within these builders
RTS is also only enabled on a subset of test suites that are compatible with
the rest of the pipeline. Some file changes, such as DEPS change will also
disable RTS on a CL by CL basis. To know for certain that RTS is actually being
used in the CQ+1 build page, "Ran tests selected by RTS" can be seen in the test
suite's results step and an output property `"rts_was_used": true`. In a CQ+2
build "Ran tests previously skipped by RTS" will instead be seen in test suites'
results step.


## Useful links

- [Design Doc](https://bit.ly/chromium-rts)
- [Brews and Bites](https://docs.google.com/presentation/d/1MUqTf14yznxg7zzT00VxRxw1FHc2a3_PrWoFJZqu0GI)
- [rts-model-packager](https://ci.chromium.org/p/chromium/builders/ci/rts-model-packager)
