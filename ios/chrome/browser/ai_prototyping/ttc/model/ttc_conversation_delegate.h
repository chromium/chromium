// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_DELEGATE_H_

#import <Foundation/Foundation.h>

@class TTCConversation;

// Delegate protocol for observing initialization, energy updates, and errors
// from a `TTCConversation`.
@protocol TTCConversationDelegate

// Invoked when the conversation backend has initialized and the session is
// ready for user interaction.
- (void)conversationDidInitialize:(TTCConversation*)conversation;

// Invoked when the conversation backend session has closed cleanly.
- (void)conversationDidClose:(TTCConversation*)conversation;

// Invoked on the UI thread when the input perceptual RMS energy level
// in [0.0, 1.0] has been updated.
- (void)conversation:(TTCConversation*)conversation
    didUpdateAudioEnergy:(float)energy;

// Invoked on the UI thread when an audio or backend error occurs.
- (void)conversation:(TTCConversation*)conversation
    didEncounterError:(NSError*)error;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_DELEGATE_H_
