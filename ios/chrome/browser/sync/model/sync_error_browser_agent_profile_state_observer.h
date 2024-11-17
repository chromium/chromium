// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_PROFILE_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_PROFILE_STATE_OBSERVER_H_

#import <Foundation/Foundation.h>

class SyncErrorBrowserAgent;
@class ProfileState;

// ProfileState observer for `SyncErrorBrowserAgent`.
@interface SyncErrorBrowserAgentProfileStateObserver : NSObject

- (instancetype)initWithProfileState:(ProfileState*)profileState
               syncErrorBrowserAgent:(SyncErrorBrowserAgent*)browserAgent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)start;

- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_PROFILE_STATE_OBSERVER_H_
