---
name: ios/remove-obsolete-todos-when-reenabling-tests
description: When fixing bugs or deflaking code covered by suppressed tests, re-enable the tests and remove obsolete test_expectations.txt entries, DISABLED_ prefixes, EARL_GREY_TEST_SKIPPED guards, and tracking TODOs
rubric: testing
severity: should_fix
citations:
  - path: agents/skills/disable-test/SKILL.md
    heading: 'Step 2: Obtain Bug Reference'
    quote: Every disabled test MUST be tracked by a bug to ensure it eventually gets fixed and re-enabled.
  - path: agents/skills/disable-test/SKILL.md
    heading: 'Step 4: Documentation Requirement'
    quote: '**ALWAYS** include a `TODO` comment with a link to the Buganizer issue obtained in Step 2.'
---

**Rule.** When fixing bugs, modifying features, or deflaking code covered by
suppressed tests, verify whether the test now passes and re-enable it. A
suppression is an `//ios/testing/test_expectations.txt` entry for an EarlGrey
test (`*_egtest.mm`), a `DISABLED_` prefix with a `TODO(crbug.com/<id>)` on a
GTest unit test (`*_unittest.mm`), or a legacy in-test guard (a `MAYBE_` or
`FLAKY_` prefix, a runtime `EARL_GREY_TEST_DISABLED`, or an OS version check
such as `@available`). Re-enabling means deleting the `test_expectations.txt`
entry, prefix, macro, or runtime guard and completely removing the associated
tracking `// TODO(crbug.com/<id>): ...` comment. Do not leave obsolete tracking
TODOs on active tests or commented-out guards in place. Conversely, if an
EarlGrey test still fails on certain devices or OS versions, keep (or narrow)
its `test_expectations.txt` entry with the matching tags, for example
`crbug.com/234567 [ iOS26 ] WindowTestCase/testOpenNewWindow [ Failure ]`,
rather than adding an in-test guard; a still-failing GTest unit test keeps its
`DISABLED_` prefix and tracking TODO.

**Why.** Leaving a suppression in place when the underlying issue has been fixed
keeps the test skipped, or its failures tolerated, on continuous integration
bots, preventing automated regression detection. Conversely, leaving
commented-out disablement macros or obsolete tracking TODOs introduces dead
code, clutters tests with misleading debt, and risks prematurely re-enabling
failing tests that break tryjobs. Keeping EarlGrey suppressions in
`test_expectations.txt` scopes them to the failing configuration and lets the CQ
flag when an expected-failing test starts passing, which an in-test guard never
does.

## Examples

Bad:

```cpp
// Bad: Deflaked an EarlGrey test but left its suppression in place.
// ios/testing/test_expectations.txt
crbug.com/123456 SaveCardTestCase/testSaveCardBottomSheetShowsLoading [ Pass Failure ]

// Bad: Fixed the underlying issue but left a commented-out guard and TODO:
- (void)testOpenNewWindow {
  // TODO(crbug.com/234567): Re-enable test on iOS 26.
  // EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  OpenNewWindow();
}

// Bad: Re-enabled GTest unit test but forgot to remove the tracking TODO:
// TODO(crbug.com/345678): Flaky network response.
TEST_F(NetworkTest, FetchDataSuccess) {
  ExecuteFetch();
}
```

Good:

```cpp
// Good: Re-enabled EarlGrey test: the test_expectations.txt entry is deleted
// and the test method carries no TODO.
- (void)testSaveCardBottomSheetShowsLoading {
  [self waitForLoadingConfirmation];
}

// Good: Still failing only on iOS 26: keep (or narrow) the expectations entry
// instead of an in-test guard; the test body stays clean.
// ios/testing/test_expectations.txt
crbug.com/234567 [ iOS26 ] WindowTestCase/testOpenNewWindow [ Failure ]

// Good: Re-enabled GTest unit test with the tracking TODO cleanly removed:
TEST_F(NetworkTest, FetchDataSuccess) {
  ExecuteFetch();
}
```
