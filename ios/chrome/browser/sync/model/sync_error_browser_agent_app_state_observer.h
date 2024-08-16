// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_APP_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_APP_STATE_OBSERVER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/app_state_observer.h"

class SyncErrorBrowserAgent;

// App state observer for `SyncErrorBrowserAgent`.
@interface SyncErrorBrowserAgentAppStateObserver : NSObject <AppStateObserver>

- (instancetype)initWithSyncErrorBrowserAgent:
    (SyncErrorBrowserAgent*)syncErrorBrowserAgent NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_APP_STATE_OBSERVER_H_
