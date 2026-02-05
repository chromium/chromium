// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_delegate.h"

class WebStateList;

@protocol BWGCommands;
@protocol SettingsCommands;

// Handler for the BWG sessions.
@interface BWGSessionHandler : NSObject <GeminiSessionDelegate>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Delegate for view state changes.
@property(nonatomic, weak) id<GeminiViewStateDelegate> geminiViewStateDelegate;

// The Gemini commands handler used by this session handler.
@property(nonatomic, weak) id<BWGCommands> geminiHandler;

// The settings commands handler used by this session handler.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_HANDLER_H_
