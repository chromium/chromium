// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation_state.h"

@protocol TTCAudioController;
@protocol TTCBackend;
@protocol TTCConversationDelegate;

// Error domain for `TTCConversation` errors.
extern NSString* const kTTCConversationErrorDomain;

// Error codes associated with `kTTCConversationErrorDomain`.
enum class TTCConversationErrorCode {
  kAudioCaptureFailure = -1,
};

// Manages the conversation state and coordinates between audio input/output
// and the transport session for TalkToChrome on iOS.
@interface TTCConversation : NSObject

// Delegate receiving state changes, audio chunks, energy, and error events.
@property(nonatomic, weak) id<TTCConversationDelegate> delegate;

// Current state of the conversation (kStopped, kListening, kTalking).
@property(nonatomic, readonly, assign) TTCConversationState state;

// Underlying audio controller.
@property(nonatomic, readonly) id<TTCAudioController> audioController;

// Underlying model execution backend.
@property(nonatomic, readonly) id<TTCBackend> backend;

// Last error encountered by the conversation, or nil.
@property(nonatomic, readonly) NSError* lastError;

// Designated initializer injecting a custom audio controller and backend.
- (instancetype)initWithAudioController:(id<TTCAudioController>)audioController
                                backend:(id<TTCBackend>)backend
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer using a default `TTCAudioEngine` and default backend.
- (instancetype)init;

#pragma mark - Session Controls

// Starts the conversation session: begins audio capture and transitions to
// `kListening`.
- (void)start;

// Stops the conversation session: halts active audio capture and playback,
// and transitions to `kStopped`.
- (void)stop;

// Stops capture and playback, clears the audio controller and conversation
// delegates, and invalidates in-flight operations. Safe to call multiple times.
- (void)disconnect;

#pragma mark - Model Responses

// Plays 24kHz response audio PCM data received from the assistant model,
// transitioning the conversation state to `kTalking`. Called by
// `TTCSessionController` (or the model backend router) as streaming audio
// chunks arrive.
- (void)playResponseAudio:(NSData*)pcm24kData;

// Marks the completion of an assistant turn, transitioning state back to
// `kListening`. Called by `TTCSessionController` (or the model backend router)
// when the server signals turn completion, or internally when audio playback
// finishes.
- (void)finishTurn;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_H_
