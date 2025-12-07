// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/test/browser_view_visibility_app_interface.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@interface BrowserViewVisibilityAppInterface ()
@property(nonatomic, assign) BrowserViewVisibilityState currentState;
@end

@implementation BrowserViewVisibilityAppInterface {
  // Subscription with the BrowserViewVisibilityNotifierBrowserAgent.
  base::CallbackListSubscription _browserViewVisibilityStateChangedSubscription;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _currentState = BrowserViewVisibilityState::kNotInViewHierarchy;
  }
  return self;
}

+ (instancetype)sharedInstance {
  static BrowserViewVisibilityAppInterface* instance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[BrowserViewVisibilityAppInterface alloc] init];
  });
  return instance;
}

+ (void)startObservingBrowserViewVisibilityState {
  [[BrowserViewVisibilityAppInterface sharedInstance]
      observeBrowserViewVisibilityState];
}

+ (void)stopObservingBrowserViewVisibilityState {
  [[BrowserViewVisibilityAppInterface sharedInstance]
      unobserveBrowserViewVisibilityState];
}

+ (BrowserViewVisibilityState)currentState {
  return [BrowserViewVisibilityAppInterface sharedInstance].currentState;
}

#pragma mark - Helper

- (void)observeBrowserViewVisibilityState {
  __weak BrowserViewVisibilityAppInterface* weakSelf = self;
  _browserViewVisibilityStateChangedSubscription =
      BrowserViewVisibilityNotifierBrowserAgent::FromBrowser(
          chrome_test_util::GetMainBrowser())
          ->RegisterBrowserVisibilityStateChangedCallback(
              base::BindRepeating(^(BrowserViewVisibilityState current_state,
                                    BrowserViewVisibilityState previous_state) {
                weakSelf.currentState = current_state;
              }));
}

- (void)unobserveBrowserViewVisibilityState {
  _browserViewVisibilityStateChangedSubscription = {};
  _currentState = BrowserViewVisibilityState::kNotInViewHierarchy;
}

@end
