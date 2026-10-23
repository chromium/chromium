---
name: ios/evaluate-earl-grey-skip-guards-before-setup
description: In EarlGrey integration tests requiring a runtime capability skip that test_expectations.txt cannot express, evaluate EARL_GREY_TEST_SKIPPED (never XCTSkip or EARL_GREY_TEST_DISABLED) at the start of the test using device idiom helpers
rubric: testing
severity: should_fix
citations:
  - path: ios/testing/earl_grey/disabled_test_macros.h
    quote: '#define EARL_GREY_TEST_SKIPPED(message)'
  - path: ios/best_practices.md
    heading: Testing Best Practices
    quote: Do not use `DISABLED_` or `FLAKY_` method prefixes or hard-coded runtime/compile-time skips in test code to suppress failing or flaky tests. Instead, add an entry to `//ios/testing/test_expectations.txt` linking the `crbug.com/123456` issue.
---

**Rule.** In EarlGrey integration tests (`*_egtest.mm`), an in-test runtime skip
is only legitimate for a condition that `//ios/testing/test_expectations.txt`
cannot express (for example `[ChromeEarlGrey areMultipleWindowsSupported]`);
failing or flaky tests are suppressed in that file instead (see
`ios/suppress-failing-tests-in-test-expectations`). When such a skip is needed:

1. Implement it with `EARL_GREY_TEST_SKIPPED` (from
   `ios/testing/earl_grey/disabled_test_macros.h`) and a message naming the
   specific limitation, never Apple XCTest's `XCTSkip`, `XCTSkipIf`, or
   `XCTSkipUnless`.
2. Place and evaluate the guard on the first lines of the test method, before
   expensive setup such as app relaunches (e.g., `RelaunchWithIPHFeature`) or
   test server initialization.
3. Plain iPhone/iPad gating is expressible with the `iPhone`/`iPad` tags; when
   an in-test form-factor check is still needed, query
   `[ChromeEarlGrey isIPadIdiom]` or `[ChromeEarlGrey isIPhoneIdiom]`, not
   `[ChromeEarlGrey isCompactWidth]` (multitasking iPads present compact width),
   keep the check polarity aligned with the targeted form factor
   (`isIPhoneIdiom` when skipping on iPhone), and omit redundant `isIPadIdiom`
   checks once multi-window support is verified.

**Why.** `EARL_GREY_TEST_SKIPPED` is the sanctioned in-test skip: it logs a
recognizable `Earl Grey Test Skipped` marker and returns, keeping skips uniform
and searchable across the suite, whereas direct `XCTSkip*` calls bypass that
convention (the harness reserves `XCTSkip` for applying `[ Skip ]`
expectations). Delaying skip guards until after app relaunches or fixture
initialization wastes continuous integration execution time, while checking UI
elements without skipping on unsupported form factors causes test failures.
Inverting skip conditions or checking `isCompactWidth` (which matches
multitasking iPads) incorrectly skips valid runs or fails on target devices.
Finally, `areMultipleWindowsSupported` queries system multi-scene support, which
is exclusively available on iPad, making additional `isIPadIdiom` checks
redundant.

## Examples

Bad:

```cpp
// Bad: XCTest's skip instead of the EarlGrey macro (and a failing test hidden
// in-test instead of a test_expectations.txt entry).
- (void)testPressEscapeHidesLensOverlayContainer {
  XCTSkip(@"crbug.com/123456");
  [ChromeEarlGrey loadURL:GURL("about:blank")];
}

// Bad: Skip guard placed after expensive setup, using isCompactWidth instead
// of the device idiom (and expressible as a [ iPhone ] tag anyway).
- (void)testPullToRefresh {
  RelaunchWithIPHFeature(@"IPH_iOSPullToRefreshFeature", YES);
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPhone.");
  }
}

// Bad: Redundant iPad idiom check after multi-window capability check.
if (![ChromeEarlGrey areMultipleWindowsSupported]) {
  EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");
}
if (![ChromeEarlGrey isIPadIdiom]) {
  EARL_GREY_TEST_SKIPPED(@"Split screen not supported.");
}
```

Good:

```cpp
// Good: Runtime skip only for a condition test_expectations.txt cannot
// express, evaluated first, before any relaunch or setup.
- (void)testOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_SKIPPED(@"Multiple windows can't be opened.");
  }
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [ChromeEarlGrey openNewWindow];
}

// Good: When an in-test form-factor check is needed, use the idiom helper
// with the right polarity, at test startup.
- (void)testPullToRefresh {
  if ([ChromeEarlGrey isIPhoneIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Pull to refresh is only tested on iPad.");
  }
  RelaunchWithIPHFeature(@"IPH_iOSPullToRefreshFeature", YES);
}
```
