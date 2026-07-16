// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SESSION_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SESSION_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_delegate.h"

class PrefService;
class WebStateList;

namespace feature_engagement {
class Tracker;
}

@protocol GeminiCommands;
@protocol SettingsCommands;

// Callback type invoked when a request to detach a tab is made.
typedef void (^GeminiTabDetachRequestCallback)(NSString* tabID);

// Handler for the Gemini sessions.
@interface GeminiSessionHandler : NSObject <GeminiSessionDelegate>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                             tracker:(feature_engagement::Tracker*)tracker
                         prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Delegate for view state changes.
@property(nonatomic, weak) id<GeminiViewStateDelegate> geminiViewStateDelegate;

// The Gemini commands handler used by this session handler.
@property(nonatomic, weak) id<GeminiCommands> geminiHandler;

// The settings commands handler used by this session handler.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

// Whether the current session is the first session.
@property(nonatomic, assign) BOOL isFirstSession;

// Callback invoked when the user requests to detach a tab from the floaty.
@property(nonatomic, copy)
    GeminiTabDetachRequestCallback tabDetachRequestCallback;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SESSION_HANDLER_H_
