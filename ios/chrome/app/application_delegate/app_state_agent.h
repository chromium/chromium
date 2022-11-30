// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_AGENT_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_AGENT_H_

#import <UIKit/UIKit.h>

@class AppState;

// AppState agents are objects owned by the app state and providing some
// app-scoped function. They can be driven by AppStateObserver events.
@protocol AppStateAgent <NSObject>

@required
// Sets the associated app state. Called once and only once. Consider starting
// the app state observation in your implementation of this method.
// Do not call this method directly. Calling [AppState addAgent]: will call it.
- (void)setAppState:(AppState*)appState;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_AGENT_H_
