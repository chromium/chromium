// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_WEBSOCKET_BACKEND_TESTING_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_WEBSOCKET_BACKEND_TESTING_H_

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"

@class NSURLSessionTask;

// Testing category exposing internal payload construction and message parsing.
@interface TTCWebSocketBackend (Testing)

// Count of currently in-flight audio message sends.
@property(nonatomic, assign) NSUInteger inFlightSends;

// Simulates a fatal transport or session error for testing.
// @param error Underlying error to process.
- (void)handleFatalErrorWithError:(NSError*)error;

// Transitions the backend to handshaking state for testing server responses.
- (void)simulateHandshakingStateForTesting;

// Simulates URL session task completion with an error.
// @param task The task associated with the completion.
// @param error The error with which the task completed.
- (void)webSocketTask:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error;

// Parses a JSON payload received from the streaming service and dispatches
// delegate events.
// @param payload Raw JSON data received from the WebSocket transport.
- (void)parseServerMessage:(NSData*)payload;

// Simulates clean remote connection closure for testing.
- (void)handleConnectionClosed;

// Builds the UTF-8 JSON payload for the initial streaming service setup
// handshake.
// @param model Identifier of the model to configure.
// @param voiceName Name of the prebuilt voice configuration.
// @param systemInstruction System persona instruction string.
+ (NSData*)createSetupPayloadWithModel:(NSString*)model
                             voiceName:(NSString*)voiceName
                     systemInstruction:(NSString*)systemInstruction;

// Builds the UTF-8 JSON payload wrapping 16kHz linear PCM audio into
// realtimeInput.
// @param pcmData Raw 16kHz mono linear PCM audio data.
+ (NSData*)createAudioChunkPayloadWithPCMData:(NSData*)pcmData;

// Builds the UTF-8 JSON payload for submitting a user text query.
// @param text Query text to transmit as a user turn.
+ (NSData*)createTextInputPayloadWithText:(NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_WEBSOCKET_BACKEND_TESTING_H_
