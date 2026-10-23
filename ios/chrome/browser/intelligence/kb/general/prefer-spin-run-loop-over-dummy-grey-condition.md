---
name: ios/prefer-spin-run-loop-over-dummy-grey-condition
description: In iOS tests, poll explicit conditions using WaitUntilConditionOrTimeout or ChromeEarlGrey helpers instead of fixed delays like SpinRunLoopWithMinDelay or dummy GREYConditions
rubric: readability
severity: should_fix
citations:
  - path: docs/testing/web_tests_tips.md
    heading: Fast
    quote: Tests should be as **fast** as possible, without compromising on the principles below. Blink has several thousand web tests that are run in parallel, and avoiding unnecessary delays is crucial to keeping our Commit Queue in good shape.
---

**Rule.** In iOS unit and integration tests (including EarlGrey tests), do not
use fixed-duration delays or spin-delays (such as
`base::test::ios::SpinRunLoopWithMinDelay()`, `SpinRunLoopWithMaxDelay()`, or
sleep calls) to wait for asynchronous operations, UI state transitions, or
background processing (e.g., waiting between simulated keystrokes or waiting for
form rescanning). Instead, wait directly on an explicit condition or observable
event (such as a state flag updating, a histogram metric being recorded, or web
state changes) using `base::test::ios::WaitUntilConditionOrTimeout()` or
standard `ChromeEarlGrey` wait helpers (such as `waitForJavaScriptCondition:`,
`waitForWebStateContainingText:`, or `waitForUIElementToAppearWithMatcher:`).
Never insert fixed spin-delays before polling wait helpers that already poll
asynchronously. Additionally, if pumping run-loop messages for a fixed duration
is strictly required, use `SpinRunLoopWithMinDelay()` rather than constructing
dummy `GREYCondition` instances that unconditionally return `NO`.

**Why.** Fixed-duration delays and sleep calls guarantee unnecessary latency
across test suites even when operations finish immediately, inflating test
execution time, slowing continuous integration CQ bots, and introducing
intermittent flakiness on busy or slow simulators. Polling wait helpers
(`ChromeEarlGrey` condition helpers and `WaitUntilConditionOrTimeout`) proceed
immediately once the condition is satisfied and provide clear failure messages
on timeout, making preceding spin-delays completely redundant. Furthermore,
constructing dummy `GREYCondition` blocks returning `NO` misuses conditional
polling APIs to simulate sleep and discards `[[nodiscard]]` return values.

## Examples

Bad:

```cpp
// Bad: Fixed delay to wait for async processing, or before a polling helper.
[self triggerFormRescan];
base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
[ChromeEarlGrey waitForJavaScriptCondition:checkValueJS];

// Bad: Dummy GREYCondition returning NO to simulate a delay while pumping.
(void)[[GREYCondition conditionWithName:@"Wait"
                                  block:^BOOL { return NO; }]
    waitWithTimeout:kAdditionalWaitTime];

// Bad: Fixed delay in loop instead of polling for expected UI/web state.
SimulateKeyboardEvent(letter);
base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));
```

Good:

```cpp
// Good: Poll directly for observable metric/condition without upfront delay.
[self triggerFormRescan];
bool updated = base::test::ios::WaitUntilConditionOrTimeout(
    base::test::ios::kWaitForActionTimeout, ^bool {
      return [MetricsAppInterface
                 expectTotalCount:1
                     forHistogram:@"Autofill.Timing.ParseFormsAsync.UpdateCache"] == nil;
    });
GREYAssertTrue(updated, @"Autofill did not update its cache.");
[ChromeEarlGrey waitForJavaScriptCondition:checkValueJS];

// Good: Use SpinRunLoopWithMinDelay if a fixed pump is strictly required.
base::test::ios::SpinRunLoopWithMinDelay(kAdditionalWaitTime);

// Good: Poll explicit condition via ChromeEarlGrey helper.
SimulateKeyboardEvent(letter);
[ChromeEarlGrey waitForJavaScriptCondition:
    [NSString stringWithFormat:@"document.getElementById('%@').value.includes('%@');",
                               fieldID, text]];
```
