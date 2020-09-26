# On disabling tests

Sometimes you don't want to run a test that you've written (or that
you've imported, like conformance tests).

There are a number of different ways to "disable" a test.

*   If the test is an entire binary or test suite, the first (and
    simplest) first way is to simply not build (or build, but not run)
    the test binary, of course.

*   The second way (for tests in C++) is to not compile a test in a
    given configuration, e.g., #ifndef WIN. In this situation, the only
    way you would know the test existed and was disabled would be to
    examine the source code.  In most cases today, we use this path for
    tests that will never be enabled, but sometimes we do this to
    temporarily skip tests as well.

*   The third way, for GTest-based tests, is a variant of the second
    way: instead of compiling it out completely, you change the name, so
    that you simply don't run the test by default. But, at least in this
    case, you can potentially determine at runtime the list of disabled
    tests, because the code is still in the binary. And, potentially you
    can still force the test to be run via a command line flag.

*   A fourth way is for a test harness to skip over a test at runtime
    for some reason, e.g., the harness determines that you're running on
    a machine w/ no GPU and so the GPU tests are never invoked. Here you
    can also ask the harness which tests are being skipped.

*   A fifth way is for a test harness to run the test, but then have the
    test detect at runtime that it should skip or exit early (e.g., the
    test itself could detect there was no GPU). Depending on how the
    test does this, it may be impossible for you to really detect that
    this happened, and you'd just view the test as 'passing'.

*   A sixth way is to use [expectations files and filter
    files](https://bit.ly/chromium-test-list-format), and have the test
    harness use that file to decide what to run and what to skip.

In theory, we should eventually consistently have either or both of
expectations files and filter files for all test steps. We still don't
have this consistently everywhere in Chrome (as of 2020-09-18), but
folks are working on them expanding the number of kinds of tests that do
have them. Once we do have them, we can expect people to stop using at
least the third path.

As you can see from the above, it's difficult if not impossible to
determine "all of the disabled tests" at any point in time. At best,
you'd have to decide what subsets of disabled tests that you're
targeting, and which you'd like to ignore.

You could also choose to "ban" certain approaches, but those bans might
be hard to enforce, and some approaches may practically be necessary in
some cases.

Ultimately, the more temporary disabling we can do via the sixth path,
the better off we probably are: the sixth path is the easiest for us to
write tooling to support and the most generic of all of the approaches.
