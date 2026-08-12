// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"

// Delegate protocol for handling view state changes.
@protocol GeminiViewStateDelegate <NSObject>

// Called when the view state changes.
- (void)didSwitchToViewState:(ios::provider::GeminiViewState)viewState;

// Switch to `viewState`.
- (void)switchToViewState:(ios::provider::GeminiViewState)viewState;

// Called when the processing status changes with a dormant reason.
- (void)didUpdateProcessingStatus:(ios::provider::GeminiClientMode)processStatus
                    dormantReason:
                        (ios::provider::GeminiDormantReason)dormantReason
                        sessionID:(NSString*)sessionID
                   conversationID:(NSString*)conversationID;

// Called when the processing status changes.
- (void)didUpdateProcessingStatus:
            (ios::provider::GeminiClientMode)processingStatus
                        sessionID:(NSString*)sessionID
                   conversationID:(NSString*)conversationID;

// Called when the user taps the Live button in Gemini UI.
- (void)geminiLiveUserDidTapLiveButton;

// Called when the user presses the Live stop button.
- (void)geminiLiveUserDidPressStopButton;

// Called when the user barges in during Gemini Live session.
- (void)geminiLiveUserDidBargeIn;

// Called when the Gemini view mode changes.
- (void)didSwitchToMode:(ios::provider::GeminiViewMode)mode;

// Called when the Gemini UI did appear.
- (void)geminiUIDidAppear;

// Called when the user taps the New Chat button in Gemini UI.
- (void)didTapNewChatButton;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_DELEGATE_H_
