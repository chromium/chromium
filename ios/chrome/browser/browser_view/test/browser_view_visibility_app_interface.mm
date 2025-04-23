// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/test/browser_view_visibility_app_interface.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_observer_bridge.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@interface BrowserViewVisibilityAppInterface () <BrowserViewVisibilityObserving>
@property(nonatomic, assign) BrowserViewVisibilityState currentState;
@end

@implementation BrowserViewVisibilityAppInterface {
  // Observes changes of the browser view visibility state.
  raw_ptr<BrowserViewVisibilityNotifierBrowserAgent>
      _browserViewVisibilityNotifierBrowserAgent;
  std::unique_ptr<BrowserViewVisibilityObserverBridge>
      _browserViewVisibilityObserverBridge;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _currentState = BrowserViewVisibilityState::kNotInViewHierarchy;
    _browserViewVisibilityNotifierBrowserAgent =
        BrowserViewVisibilityNotifierBrowserAgent::FromBrowser(
            chrome_test_util::GetMainBrowser());
    _browserViewVisibilityObserverBridge =
        std::make_unique<BrowserViewVisibilityObserverBridge>(self);
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

#pragma mark - BrowserViewVisibilityObserving

- (void)browserViewDidChangeToVisibilityState:
            (BrowserViewVisibilityState)currentState
                                    fromState:(BrowserViewVisibilityState)
                                                  previousState {
  self.currentState = currentState;
}

#pragma mark - Helper

- (void)observeBrowserViewVisibilityState {
  _browserViewVisibilityNotifierBrowserAgent->AddObserver(
      _browserViewVisibilityObserverBridge.get());
}

- (void)unobserveBrowserViewVisibilityState {
  _browserViewVisibilityNotifierBrowserAgent->RemoveObserver(
      _browserViewVisibilityObserverBridge.get());
}

@end
