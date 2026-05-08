// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_COORDINATOR_TAB_RESUMPTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_COORDINATOR_TAB_RESUMPTION_MEDIATOR_H_

#import <UIKit/UIKit.h>

class Browser;
@class ContentSuggestionsMetricsRecorder;

@protocol NewTabPageActionsDelegate;
class OptimizationGuideService;
class PrefService;
@class TabResumptionConfig;
@protocol TabResumptionMediatorDelegate;

namespace commerce {
class ShoppingService;
}  // namespace commerce

namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator for managing the state of the TabResumption Magic Stack module.
@interface TabResumptionMediator : NSObject

// The latest state of the item config for the Tab Resumption module.
@property(nonatomic, strong, readonly) TabResumptionConfig* itemConfig;

// The delegate for this mediator.
@property(nonatomic, weak) id<TabResumptionMediatorDelegate> delegate;

// Delegate for reporting content suggestions actions to the NTP.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

// Recorder for content suggestions metrics.
@property(nonatomic, weak)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

// Default initializer.
- (instancetype)
          initWithLocalState:(PrefService*)localState
                 prefService:(PrefService*)prefService
             identityManager:(signin::IdentityManager*)identityManager
                     browser:(Browser*)browser
    optimizationGuideService:(OptimizationGuideService*)optimizationGuideService
             shoppingService:(commerce::ShoppingService*)shoppingService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects this mediator.
- (void)disconnect;

// Disables the tab resumption module.
- (void)disableModule;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_COORDINATOR_TAB_RESUMPTION_MEDIATOR_H_
