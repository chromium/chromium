// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ENTRY_FLOW_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ENTRY_FLOW_COORDINATOR_H_

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_entry_flow_result.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class GeminiStartupState;

// Coordinator that manages the full Gemini entry flow: sign-in,
// eligibility checks, and session start. Reports the outcome
// to the caller via a completion block.
@interface GeminiEntryFlowCoordinator
    : ChromeCoordinator <AccountMenuCoordinatorDelegate>

// Initializes the coordinator with all required parameters.
// - `baseViewController`: The view controller to present UI from.
// - `browser`: The browser instance.
// - `startupState`: Entry point and configuration for the Gemini session.
// - `accessPoint`: The sign-in access point for metrics. Only used if
//   sign-in is triggered.
// - `showSnackbarOnCompletion`: Whether to show a snackbar when the flow
//   completes with an ineligible state.
// - `completion`: Called with the final result of the flow.
- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
                       browser:(Browser*)browser
                  startupState:(GeminiStartupState*)startupState
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
      showSnackbarOnCompletion:(BOOL)showSnackbarOnCompletion
                    completion:(GeminiEntryFlowCompletion)completion;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ENTRY_FLOW_COORDINATOR_H_
