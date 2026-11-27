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

@protocol GeminiChatMessageHandler;
@protocol GeminiCommands;
@protocol SettingsCommands;
@protocol SnackbarCommands;

// Handler for the Gemini sessions.
@interface GeminiSessionHandler : NSObject <GeminiSessionDelegate>

// Delegate for view state changes.
@property(nonatomic, weak) id<GeminiViewStateDelegate> geminiViewStateDelegate;

// The Gemini commands handler used by this session handler.
@property(nonatomic, weak) id<GeminiCommands> geminiHandler;

// The handler consulted before user chat messages are sent. Nil when no
// feature intercepts chat messages, in which case every message is sent.
@property(nonatomic, weak) id<GeminiChatMessageHandler> chatMessageHandler;

// The settings commands handler used by this session handler.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

// The snackbar commands handler used by this session handler.
@property(nonatomic, weak) id<SnackbarCommands> snackbarHandler;

// Whether the current session is the first session.
@property(nonatomic, assign) BOOL isFirstSession;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                             tracker:(feature_engagement::Tracker*)tracker
                         prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Records end-of-session metrics and resets session state if a session is
// currently active. No-ops if no session appeared or if metrics were already
// recorded for the session.
- (void)recordSessionDismissed;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SESSION_HANDLER_H_
