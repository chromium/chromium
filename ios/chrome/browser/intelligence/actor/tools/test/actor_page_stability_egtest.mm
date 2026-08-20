// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_tools_base_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

@interface ActorPageStabilityTestCase : ActorToolsBaseTestCase
@end

@implementation ActorPageStabilityTestCase

- (void)tearDownHelper {
  [ActorAppInterface resolveInFlightAutofillPredictions];
  [super tearDownHelper];
}

#pragma mark - Helpers

// Navigates to a test page (A) which has a button that links to another test
// page (B). Page B has a delay before the LCP, configured by `lcpDelay`.
//
// This returns a ClickAction which targets the button in (A), which is used to
// ensure that the page stability code waits for the LCP.
- (optimization_guide::proto::Action)
    setupActionForTwoPageNavigationWithLcpDelay:(base::TimeDelta)lcpDelay {
  std::string page_b_html = base::StringPrintf(
      R"(
    <html>
    <body>
      <div id='status'></div>
      <script>
        setTimeout(() => {
          const el = document.createElement('h1');
          el.style.fontSize = '100px';
          el.innerText = 'This is the LCP header!';
          document.body.appendChild(el);
          document.getElementById('status').innerText = 'LCP Triggered';
        }, %d);
      </script>
    </body>
    </html>
  )",
      static_cast<int>(lcpDelay.InMilliseconds()));
  GURL page_b_url = [self URLForHTML:page_b_html];

  std::string page_a_html = base::StringPrintf(
      R"(
    <html>
    <body>
      <button onclick="window.location.href='%s'">Go to B</button>
    </body>
    </html>
  )",
      page_b_url.spec().c_str());
  [ChromeEarlGrey loadURL:[self URLForHTML:page_a_html]];
  [ChromeEarlGrey waitForWebStateContainingText:"Go to B"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* click_action = action.mutable_click();
  click_action->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  click_action->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  click_action->set_click_count(optimization_guide::proto::ClickAction::SINGLE);
  [self setCoordinatesOnTarget:click_action->mutable_target()
                  withSelector:"button"];

  return action;
}

#pragma mark - Page Stability Tests

// Tests that the stability check succeeds when the page is stable.
- (void)testWaitForStability_onStablePage_Succeeds {
  // This page makes no mutations until the button is clicked.
  GURL url = self.testServer->GetURL("/actor/page_stability.html");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Initial Content"];
  __block NSError* stabilityError = nil;
  __block BOOL completed = NO;
  [ActorAppInterface waitForPageStabilityWithCompletion:^(NSError* error) {
    stabilityError = error;
    completed = YES;
  }];

  BOOL success = [[GREYCondition conditionWithName:@"Wait for stability"
                                             block:^BOOL {
                                               return completed;
                                             }] waitWithTimeout:10.0];

  GREYAssertTrue(success, @"WaitForStability timed out.");
  GREYAssertNil(stabilityError, @"WaitForStability failed: %@", stabilityError);
}

// Tests that the stability check times out when the page doesn't stabilize.
- (void)testWaitForStability_onUnstablePage_timesOut {
  // Disable synchronization since the test page is intentionally unstable.
  ScopedSynchronizationDisabler disabler;
  GURL url = self.testServer->GetURL("/actor/page_stability.html");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Initial Content"];

  // Make the test page mutate the DOM to be unstable beyond the timeout.
  int durationMs = kPageStabilityTimeoutMs * 2;
  // Since the page applies these mutations evenly across the duration, set the
  // count to be larger than the mutation cap for each window that fits in the
  // duration. We also multiply a factor of 3 to ensure that JS task scheduling
  // doesn't cause us to accidentally report stable on an unstable page.
  int count = (kPageStabilityMutationCap + 1) * 3 *
              (durationMs / kPageStabilityWindowDurationMs);
  NSString* configScript =
      [NSString stringWithFormat:
                    @"window.sustainedMutations = {count: %d, duration: %d};",
                    count, durationMs];
  (void)[ChromeEarlGrey evaluateJavaScript:configScript];
  NSString* triggerScript = @"document.getElementById('mutate').click();";
  (void)[ChromeEarlGrey evaluateJavaScript:triggerScript];

  __block NSError* stabilityError = nil;
  __block BOOL completed = NO;
  [ActorAppInterface waitForPageStabilityWithCompletion:^(NSError* error) {
    stabilityError = error;
    completed = YES;
  }];

  BOOL success = [[GREYCondition conditionWithName:@"Wait for stability"
                                             block:^BOOL {
                                               return completed;
                                             }] waitWithTimeout:10.0];

  GREYAssertTrue(success,
                 @"WaitForStability timed out in EGTest condition wait.");
  GREYAssertNotNil(stabilityError, @"WaitForStability should have failed.");
}

// Tests that Click tool waits for the page to stabilize if the click triggers
// page instability.
//
// Note that the page stability handler is attached to every tools and this test
// uses the Click tool arbitrarily.
- (void)testClickTool_onPageThatStabilizes_waitsForPageStability {
  GURL url = self.testServer->GetURL("/actor/page_stability.html");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Initial Content"];

  // Inject sustained mutations on click that stop before the stability timeout.
  int durationMs = kPageStabilityTimeoutMs / 4;
  int count = (kPageStabilityMutationCap + 1) *
              (durationMs / kPageStabilityWindowDurationMs);
  NSString* configScript =
      [NSString stringWithFormat:
                    @"window.sustainedMutations = {count: %d, duration: %d};",
                    count, durationMs];
  (void)[ChromeEarlGrey evaluateJavaScript:configScript];

  // Set up a click action.
  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* clickAction = action.mutable_click();
  clickAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickAction->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickAction->set_click_count(optimization_guide::proto::ClickAction::SINGLE);
  [self setCoordinatesOnTarget:clickAction->mutable_target()
                  withSelector:"#mutate"];

  // Track the duration of the action execution.
  base::TimeTicks startTime = base::TimeTicks::Now();
  base::TimeDelta duration;
  {
    ScopedSynchronizationDisabler disabler;
    GREYAssertNil([self executeAction:action], @"Action execution failed.");
    duration = base::TimeTicks::Now() - startTime;
  }

  [ChromeEarlGrey waitForWebStateContainingText:"<mutating...>!"];

  // The page triggers mutations for `durationMs` and the sliding window is
  // `kPageStabilityWindowDurationMs`, so the Click action should take at least
  // `durationMs + kPageStabilityWindowDurationMs` ms to complete.
  int expectedWait = durationMs + kPageStabilityWindowDurationMs;
  GREYAssertTrue(duration.InMilliseconds() >= expectedWait,
                 @"Expected duration to be >= %dms, but was %lldms",
                 expectedWait, duration.InMilliseconds());
}

// Tests that Click tool times out if it causes the page to be unstable beyond
// the timeout.
//
// Note that the page stability handler is attached to every tools and this test
// uses the Click tool arbitrarily.
- (void)testClickTool_onUnstablePage_timesOut {
  GURL url = self.testServer->GetURL("/actor/page_stability.html");
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Initial Content"];

  // Make the test page mutate the DOM to be unstable beyond the timeout.
  int durationMs = kPageStabilityTimeoutMs * 2;
  // Since the page applies these mutations evenly across the duration, set the
  // count to be larger than the mutation cap for each window that fits in the
  // duration. We also multiply a factor of 3 to ensure that JS task scheduling
  // doesn't cause us to accidentally report stable on an unstable page.
  int count = (kPageStabilityMutationCap + 1) * 3 *
              (durationMs / kPageStabilityWindowDurationMs);
  NSString* configScript =
      [NSString stringWithFormat:
                    @"window.sustainedMutations = {count: %d, duration: %d};",
                    count, durationMs];
  (void)[ChromeEarlGrey evaluateJavaScript:configScript];

  // Set up a click action.
  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* clickAction = action.mutable_click();
  clickAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickAction->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickAction->set_click_count(optimization_guide::proto::ClickAction::SINGLE);
  [self setCoordinatesOnTarget:clickAction->mutable_target()
                  withSelector:"#mutate"];
  // Track the duration of the action execution.
  base::TimeTicks startTime = base::TimeTicks::Now();
  base::TimeDelta duration;
  {
    ScopedSynchronizationDisabler disabler;
    GREYAssertNil([self executeAction:action], @"Action execution failed.");
    duration = base::TimeTicks::Now() - startTime;
  }

  // The page triggers mutations for `durationMs` and the sliding window is
  // `kPageStabilityWindowDurationMs`, so the Click action should take at least
  // `durationMs + kPageStabilityWindowDurationMs` ms to complete.
  int expectedTimeoutWait = kPageStabilityTimeoutMs;
  GREYAssertTrue(duration.InMilliseconds() >= expectedTimeoutWait,
                 @"Expected duration to be >= %dms, but was %lldms",
                 expectedTimeoutWait, duration.InMilliseconds());
}

// Tests that `PageStabilityMonitor` waits for Largest Contentful Paint (LCP)
// before finishing tool execution.
- (void)testPageStability_waitsForLcp {
  // Note: This duration must be larger than `kPageStabilityMinWaitMs` (1000ms)
  // and smaller than `kPageStabilityLcpDelayMs` (3000ms).
  static constexpr base::TimeDelta kActualLcpDelay = base::Milliseconds(1500);
  optimization_guide::proto::Action action =
      [self setupActionForTwoPageNavigationWithLcpDelay:kActualLcpDelay];

  // Measure execution time of the actuated click and resulting stability check.
  base::TimeTicks start_time = base::TimeTicks::Now();
  GREYAssertNil([self executeAction:action], @"Action execution failed.");
  base::TimeDelta duration = base::TimeTicks::Now() - start_time;

  // Expected delay is at least `kActualLcpDelay` (Page B's LCP trigger).
  GREYAssertTrue(
      duration.InMilliseconds() >= kActualLcpDelay.InMilliseconds(),
      @"Expected execution to be delayed by at least %dms but it took %dms.",
      static_cast<int>(kActualLcpDelay.InMilliseconds()),
      static_cast<int>(duration.InMilliseconds()));

  // Verify that Page B finished loading and shows the LCP element.
  [ChromeEarlGrey waitForWebStateContainingText:"LCP Triggered"];
}

// Tests that `PageStabilityMonitor` times out when Largest Contentful Paint
// (LCP) takes longer than `kPageStabilityLcpDelayMs`.
- (void)testPageStability_lcpTimesOut {
  // Set the actual LCP delay much larger than the `kPageStabilityLcpDelayMs`
  // timeout (3000ms) to avoid flakiness.
  static constexpr base::TimeDelta kActualLcpDelay = base::Milliseconds(8000);
  optimization_guide::proto::Action action =
      [self setupActionForTwoPageNavigationWithLcpDelay:kActualLcpDelay];

  // Measure execution time of the actuated click and resulting stability check.
  base::TimeTicks start_time = base::TimeTicks::Now();
  GREYAssertNil([self executeAction:action], @"Action execution failed.");
  base::TimeDelta duration = base::TimeTicks::Now() - start_time;

  // Expected execution time to be at least `kPageStabilityLcpDelayMs` (3000ms),
  // but finish before `kActualLcpDelay` (8000ms).
  GREYAssertTrue(
      duration.InMilliseconds() >= kPageStabilityLcpDelayMs,
      @"Expected execution to wait for LCP timeout (%dms) but took %dms.",
      kPageStabilityLcpDelayMs, static_cast<int>(duration.InMilliseconds()));
  GREYAssertTrue(duration.InMilliseconds() < kActualLcpDelay.InMilliseconds(),
                 @"Expected execution to time out before LCP trigger (%dms) "
                 @"but took %dms.",
                 static_cast<int>(kActualLcpDelay.InMilliseconds()),
                 static_cast<int>(duration.InMilliseconds()));

  // Verify that LCP has NOT triggered yet when `executeAction` finishes.
  base::Value status = [ChromeEarlGrey
      evaluateJavaScript:@"document.getElementById('status')?.innerText || ''"];
  GREYAssertTrue(
      status.is_string() && status.GetString().empty(),
      @"LCP element should not be present when `executeAction` times out.");
}

// Tests that `ObservationDelayController` waits for Autofill predictions
// on a page containing form fields before completing tool execution.
- (void)testPageStability_waitsForAutofillPredictions {
  std::string formHTML = R"(
    <html>
    <body>
      <h2>Sign In</h2>
      <form id='login_form' onsubmit='return false;'>
        <label for='username'>Username:</label>
        <input type='text' id='username' name='username' autocomplete='username'><br>
        <label for='password'>Password:</label>
        <input type='password' id='password' name='password' autocomplete='current-password'><br>
        <input type='button' id='submit' value='Sign In'>
      </form>
    </body>
    </html>
  )";

  [ChromeEarlGrey loadURL:[self URLForHTML:formHTML]];
  [ChromeEarlGrey waitForWebStateContainingText:"Sign In"];

  optimization_guide::proto::Action action;
  optimization_guide::proto::ClickAction* clickAction = action.mutable_click();
  clickAction->set_tab_id([ChromeEarlGrey currentTabID].intValue);
  clickAction->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  clickAction->set_click_count(optimization_guide::proto::ClickAction::SINGLE);
  [self setCoordinatesOnTarget:clickAction->mutable_target()
                  withSelector:"#submit"];

  std::string serializedAction;
  action.SerializeToString(&serializedAction);
  NSData* actionData = [NSData dataWithBytes:serializedAction.data()
                                      length:serializedAction.length()];

  // Simulate an in-flight server prediction to force FormPredictionsTracker
  // into a waiting state.
  [ActorAppInterface simulateInFlightAutofillPredictions];

  __block BOOL actionCompleted = NO;
  __block NSError* actionError = nil;

  // Trigger actuated action execution asynchronously.
  [ActorAppInterface executeActionWithProto:actionData
                                 completion:^(NSError* error) {
                                   actionError = error;
                                   actionCompleted = YES;
                                 }];

  // Verify that action execution does NOT finish immediately because Autofill
  // server predictions are still pending in ObservationDelayController.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(300));
  GREYAssertFalse(
      actionCompleted,
      @"Action execution should be blocked waiting for Autofill predictions.");

  // Resolve the in-flight server predictions to unblock FormPredictionsTracker.
  [ActorAppInterface resolveInFlightAutofillPredictions];

  // Verify that action execution now completes promptly upon prediction
  // resolution.
  BOOL success =
      [[GREYCondition conditionWithName:@"Wait for action execution completion"
                                  block:^BOOL {
                                    return actionCompleted;
                                  }]
          waitWithTimeout:base::test::ios::kWaitForActionTimeout.InSecondsF()];

  GREYAssertTrue(success,
                 @"Action execution timed out after resolving predictions.");
  GREYAssertNil(actionError, @"Action execution failed with error: %@",
                actionError);
}

@end
