// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_H_

#import <Foundation/Foundation.h>

@protocol TTCAudioController;
@protocol TTCBackend;
@protocol TTCConversationDelegate;

// Coordinates between audio input/output and the transport session for TTC on
// iOS.
@interface TTCConversation : NSObject

// Delegate receiving initialization, energy, and error events.
@property(nonatomic, weak) id<TTCConversationDelegate> delegate;

// Underlying audio controller.
@property(nonatomic, readonly) id<TTCAudioController> audioController;

// Underlying model execution backend.
@property(nonatomic, readonly) id<TTCBackend> backend;

// Designated initializer injecting a custom audio controller and backend.
- (instancetype)initWithAudioController:(id<TTCAudioController>)audioController
                                backend:(id<TTCBackend>)backend
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer using a default `TTCAudioEngine` and default backend.
- (instancetype)init;

#pragma mark - Session Controls

// Starts the conversation session: connects to the backend and begins audio
// capture.
- (void)start;

// Stops the conversation session: halts active audio capture and playback, and
// disconnects the backend.
- (void)stop;

// Stops capture and playback, clears the audio controller and conversation
// delegates, and invalidates in-flight operations. Safe to call multiple times.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_H_
