---
name: ios/suppress-failing-tests-in-test-expectations
description: When suppressing failing or flaky iOS tests for specific devices, OS versions, or simulators, add EarlGrey entries to //ios/testing/test_expectations.txt and use DISABLED_ with a tracking TODO in unit tests instead of in-test runtime checks
rubric: testing
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Testing Best Practices
    quote: Do not use `DISABLED_` or `FLAKY_` method prefixes or hard-coded runtime/compile-time skips in test code to suppress failing or flaky tests. Instead, add an entry to `//ios/testing/test_expectations.txt` linking the `crbug.com/123456` issue.
  - path: docs/testing/on_disabling_tests.md
    heading: On disabling tests
    quote: A third way is to take advantage of features in your testing framework to skip over tests. Examples include involve adding `DISABLED_` to the test method name for GTest-based tests
---

**Rule.** When an iOS test fails or is flaky only on a specific device form
factor (such as iPad vs. iPhone), OS version, or environment (simulator vs.
device), do not hide the failure inside the test source:

1. In EarlGrey tests (`*_egtest.mm`), never add `DISABLED_` or `FLAKY_` method
   prefixes, open-ended `@available(iOS X, *)` checks, unbounded early returns,
   or hard-coded runtime skips (such as `EARL_GREY_TEST_DISABLED` behind
   `[ChromeEarlGrey isIPadIdiom]` or a version check) to suppress the failure.
   Instead, add an entry to `//ios/testing/test_expectations.txt` linking the
   `crbug.com/<id>` issue, the platform or configuration tags (such as
   `[ iPad ]`, `[ iOS26 ]`, `[ Simulator ]`, or `[ Device ]`), the test name,
   and the expected outcome (`[ Failure ]`, `[ Crash ]`, `[ Skip ]`, or
   `[ Pass Failure ]` for flaky tests).
2. Reserve in-test checks (such as `@available` guards or
   `[ChromeEarlGrey areMultipleWindowsSupported]`) for conditions required for
   compilation or that `test_expectations.txt` cannot express, and implement
   them with `EARL_GREY_TEST_SKIPPED` and a message naming the specific
   limitation rather than a generic `@"Test disabled on iPad."`.
3. In GTest unit tests (`*_unittest.mm`), disable a failing or flaky test with
   the `DISABLED_` prefix and a `TODO(crbug.com/<id>)` comment; never use the
   `FLAKY_` prefix or an open-ended `@available`/OS-version check that silently
   skips the test on every later release.
4. When re-enabling a test, remove its `test_expectations.txt` entry or
   `DISABLED_` prefix together with the associated `TODO` (see
   `ios/remove-obsolete-todos-when-reenabling-tests`).

**Why.** Centralizing EarlGrey suppressions in `test_expectations.txt` scopes
them to the affected configurations through tags and lets the CQ flag when an
expected-failing test starts passing or changes its failure mode, whereas
`DISABLED_`/`FLAKY_` prefixes suppress the test on every bot, open-ended
`@available(iOS X, *)` guards (which can only express a lower bound) keep
skipping on all subsequent releases, and hard-coded idiom checks hide the test
until someone remembers it, all creating silent coverage blind spots with no
re-enable signal. `EARL_GREY_TEST_SKIPPED` with a specific message keeps the
rare unavoidable in-test skip uniform and searchable, and pairing `DISABLED_`
with a `TODO(crbug.com/<id>)` on unit tests (where `FLAKY_` is rejected by
PRESUBMIT) keeps each suppression traceable to a bug so maintainers know when to
re-enable it.

## Examples

Bad:

```cpp
// Bad: FLAKY_ prefix suppresses an iPad-only failure on every configuration.
// TODO(crbug.com/1234567): Flaky on iPad on iOS 17.
- (void)FLAKY_testForceMigration {
  // ...
}

// Bad: Open-ended @available skip also hides the test on every later iOS.
- (void)testCameraAccess {
  if (@available(iOS 17.5, *)) {
    EARL_GREY_TEST_DISABLED(@"Camera access broken on iOS 17.5.");
  }
  // ...
}

// Bad: Hard-coded idiom skip with a generic message and no tracking bug.
- (void)testStartSurface {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  // ...
}

// Bad: FLAKY_ prefix on a GTest unit test (rejected by PRESUBMIT).
TEST_F(GeolocationManagerTest, FLAKY_LocationUpdatesOnCreation) {
  // ...
}
```

Good:

```cpp
// Good: EarlGrey suppressions live in //ios/testing/test_expectations.txt,
// scoped to the affected configuration; the test methods stay untouched.
//   crbug.com/1234567 [ iOS17 iPad ] FooTestCase/testForceMigration [ Pass Failure ]
//   crbug.com/2345678 [ iOS17.5 ] FooTestCase/testCameraAccess [ Failure ]
//   crbug.com/3456789 [ iPad ] FooTestCase/testStartSurface [ Skip ]

// Good: In-test skip only for a condition the expectations file cannot express.
- (void)testOpenSecondWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_SKIPPED(@"Multiple windows are not supported on this device.");
  }
  // ...
}

// Good: Unit test disabled with DISABLED_ and a tracking bug.
// TODO(crbug.com/4567890): Re-enable once the location fake is deterministic.
TEST_F(GeolocationManagerTest, DISABLED_LocationUpdatesOnCreation) {
  // ...
}
```
