// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_BACKEND_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_BACKEND_H_

#import <Foundation/Foundation.h>
#import <stdint.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"

class GURL;
@protocol TTCBackend;

// Delegate protocol for receiving session lifecycle, audio output, and
// transcription events from a TTC model backend.
@protocol TTCBackendDelegate <NSObject>
@optional

// Called when the backend has completed setup and handshake, and the session is
// ready for user interaction.
- (void)backendDidInitialize:(id<TTCBackend>)backend;

// Called when the backend session has shut down cleanly.
- (void)backendDidClose:(id<TTCBackend>)backend;

// Called when the backend encountered an unrecoverable error.
- (void)backend:(id<TTCBackend>)backend
    didFailWithError:(TTCErrorCode)errorCode;

// Called when synthesized linear PCM audio arrives from the model.
- (void)backend:(id<TTCBackend>)backend
    didReceiveAudioOutput:(NSData*)audioData
           sequenceNumber:(int64_t)sequenceNumber;

// Called when user speech or model response text is transcribed.
- (void)backend:(id<TTCBackend>)backend
    didReceiveInputTranscription:(NSString*)inputTranscription
             outputTranscription:(NSString*)outputTranscription;

// Called when model generation state changes (started, completed, interrupted).
- (void)backend:(id<TTCBackend>)backend
    didChangeGenerationStateStarted:(BOOL)started
                          completed:(BOOL)completed
                        interrupted:(BOOL)interrupted;

@end

// Protocol interface for communicating with a TTC model backend.
@protocol TTCBackend <NSObject>

// Delegate receiving session lifecycle, audio output, and transcription events.
@property(nonatomic, weak) id<TTCBackendDelegate> delegate;

// Returns whether the backend transport is currently connected.
@property(nonatomic, readonly, assign, getter=isConnected) BOOL connected;

#pragma mark - Connection Lifecycle

// Starts the streaming session with the backend.
- (void)connect;

// Gracefully terminates the active session.
- (void)disconnect;

#pragma mark - Upstream Data

// Sends a chunk of signed 16kHz PCM16 microphone audio upstream.
- (void)sendAudioChunk:(NSData*)audioData;

// Submits a user text query to the backend.
- (void)sendTextInput:(NSString*)text;

// Sends active tab context update to the backend.
- (void)sendContextUpdateWithURL:(const GURL&)url
                           title:(NSString*)title
                         content:(NSString*)content;

// Reports the sequence number of audio played out to speakers.
- (void)reportPlaybackStatus:(int64_t)lastPlayedSequenceNumber;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_BACKEND_H_
