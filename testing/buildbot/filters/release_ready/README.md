# About release ready filters

This directory is meant to house test filters for release ready builders， by
which the release builders could determine the best commit for release.


## Test Types

We have two types of tests running in rel-ready builders:

  * Release blocker suites: the suite enforces all listed tests to run. These
    suites only accept exact positive filter. All test case names are
    explicitly listed. If any test case is not found, suite will fail.
  * CI test suites: Suites like others run in our CI builders. Any filters are
    accepted. Usually these suites have configured failure percentage, thus a
    single test failure does not impact the entire release.


## Guilde line to update rel-ready filters

Tests running on rel-ready are critical to release quality. If you have to
change them, please provide detailed justification in your CL.

For release_blocker filter, test them locally with
`--enforce-exact-positive-filter`, to make sure the test is built on your
target platform. We will add rel-ready trybots to verify filter changes in
23Q4(https://crbug.com/1479548).
