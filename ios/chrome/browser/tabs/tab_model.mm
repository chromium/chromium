// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif


@interface TabModel () {
  // Weak reference to the underlying shared model implementation.
  WebStateList* _webStateList;

  // Enabler for |_webStateList|
  WebUsageEnablerBrowserAgent* _webEnabler;

  // Weak reference to the session restoration agent.
  SessionRestorationBrowserAgent* _sessionRestorationBrowserAgent;

  ChromeBrowserState* _browserState;
}

@end

@implementation TabModel {
  BOOL _savedSessionDuringBackgrounding;
}

#pragma mark - Overriden

- (void)dealloc {
  // -disconnect should always have been called before destruction.
  DCHECK(!_browserState);
}

#pragma mark - Public methods

- (instancetype)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    _webStateList = browser->GetWebStateList();
    _browserState = browser->GetBrowserState();
    DCHECK(_browserState);

    _sessionRestorationBrowserAgent =
        SessionRestorationBrowserAgent::FromBrowser(browser);
    _webEnabler = WebUsageEnablerBrowserAgent::FromBrowser(browser);

    _savedSessionDuringBackgrounding = NO;

    // Register for resign active notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillResignActive:)
               name:UIApplicationWillResignActiveNotification
             object:nil];
    // Register for background notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidEnterBackground:)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];
    // Register for foregrounding notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillEnterForeground:)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];
  }
  return self;
}

// NOTE: This can be called multiple times, so must be robust against that.
- (void)disconnect {
  if (!_browserState)
    return;

  if (!_savedSessionDuringBackgrounding) {
    [self saveSessionOnBackgroundingOrTermination];
    _savedSessionDuringBackgrounding = YES;
  }

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  _sessionRestorationBrowserAgent = nullptr;
  _browserState = nullptr;

  // Close all tabs. Do this in an @autoreleasepool as WebStateList observers
  // will be notified (they are unregistered later). As some of them may be
  // implemented in Objective-C and unregister themselves in their -dealloc
  // method, ensure they -autorelease introduced by ARC are processed before
  // the WebStateList destructor is called.
  @autoreleasepool {
    _webStateList->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);
  }

  _webStateList = nullptr;
}

#pragma mark - Notification Handlers

// Called when UIApplicationWillResignActiveNotification is received.
// TODO(crbug.com/1115611): Move to SceneController.
- (void)applicationWillResignActive:(NSNotification*)notify {
  if (_webEnabler->IsWebUsageEnabled() && _webStateList->GetActiveWebState()) {
    SnapshotTabHelper::FromWebState(_webStateList->GetActiveWebState())
        ->WillBeSavedGreyWhenBackgrounding();
  }
}

// Called when UIApplicationDidEnterBackgroundNotification is received.
// TODO(crbug.com/1115611): Move to SceneController.
- (void)applicationDidEnterBackground:(NSNotification*)notify {
  // When using the Scene API (which requires iOS 13.0 or later and a recent
  // enough device), UIApplicationDidEnterBackgroundNotification is not sent
  // to the application if it is terminated by swipe gesture while it is in
  // the foreground. The notification is send if Scene API is not used. In
  // order to avoid saving twice the session on app termination, use a flag
  // to record that the session was saved.
  [self saveSessionOnBackgroundingOrTermination];
  _savedSessionDuringBackgrounding = YES;
}

// Called when UIApplicationWillEnterForegroundNotification is received.
// TODO(crbug.com/1115611): Move to SceneController.
- (void)applicationWillEnterForeground:(NSNotification*)notify {
  // Reset the boolean to allow saving the session state the next time the
  // application is backgrounded or terminated.
  _savedSessionDuringBackgrounding = NO;
}

#pragma mark - Saving session on backgrounding or termination

- (void)saveSessionOnBackgroundingOrTermination {
  if (!_browserState)
    return;

  // Normally, the session is saved after some timer expires but since the app
  // is about to be backgrounded or terminated send true to save the session
  // immediately.
  _sessionRestorationBrowserAgent->SaveSession(/*immediately=*/true);

  // Write out a grey version of the current website to disk.
  if (_webEnabler->IsWebUsageEnabled() && _webStateList->GetActiveWebState()) {
    SnapshotTabHelper::FromWebState(_webStateList->GetActiveWebState())
        ->SaveGreyInBackground();
  }
}

@end
