# On disabling tests

Sometimes you don't want to run a test that you've written (or that
you've imported, like conformance tests). The test might not be possible to
run in a particular configuration, or be temporarily broken by another
change, or be flaky, or simply not work yet. In these cases (and perhaps others),
you should disable the test :).

There are a number of different ways to do so:

*   If the test is an entire binary or test suite, the first (and
    simplest) way is to simply not build (or build, but not run)
    the test binary, of course. This makes sense for binaries that
    are specific to particular build configurations (e.g., Android JUnit
    tests don't need to be built on Windows).

*   A second way (for tests in C++) is to not compile a test in a
    given configuration, e.g., `#ifndef WIN`. In this situation, the only
    way you would know the test existed and was disabled would be to
    parse the source code. We often do this today for tests that will
    never be enabled in particular build configurations, but sometimes we do
    this to temporarily skip tests as well.

*   A third way is to take advantage of features in your testing framework to
    skip over tests. Examples include involve adding `DISABLED_` to the test
    method name for GTest-based tests, `@unittest.skip` for Python-based tests,
    or using the
    [DisabledTest](../../base/test/android/javatests/src/org/chromium/base/test/DisabledTest.java)
    annotation for JUnit-based Java tests (this works in both instrumentation
    and Robolectric tests). In these cases, you don't run the test by default,
    but you can determine the list of disabled tests at runtime because the
    tests are present in the executable, and you may still be able to force the
    test to be run via a command-line flag.

*   Fourth, for test frameworks that support
    [expectations files or filter files](https://bit.ly/chromium-test-list-format),
    you can use them to decide what to run and what to skip. This moves
    the mechanisms out of the source code and into separate files; there are
    advantages and disadvantages to this. The main advantage is that it
    can make it easier to write tooling to disable tests, and the main
    disadvantage is that it moves the mechanism away from the code it affects,
    potentially making it harder to understand what's going on.

*   Finally, the test harness can run the test, but the test itself
    might detect at runtime that it should exit early for some reason
    rather than actually executing the code paths you'd normally want to
    test. For example, if you have a test for some code path that requires
    a GPU, but there's no GPU on the machine, the test might check for a
    GPU and exit early with "success".

If you want to be able to determine a global picture of which tests
were disabled, you can either parse BUILD files, expectations and filter
files, and source code to try and figure that out, or require the tests be
present in test binaries (i.e., not compiled out) and then run the test
binaries in order to collect the lists of disabled tests and report them
to a central system.

Parsing code can be straightforward for some types of tests, but
difficult-to-impractical to do correctly for others.
