// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_CONTROLLER_H_

#import <Foundation/Foundation.h>

@protocol TTCAudioController;

// Delegate protocol receiving captured audio chunks, energy levels, route
// changes, and lifecycle events from `TTCAudioController`.
@protocol TTCAudioControllerDelegate <NSObject>
@optional

// Called on the main thread when a chunk of 16kHz mono linear PCM audio has
// been captured by the microphone.
- (void)audioController:(id<TTCAudioController>)controller
    didCaptureAudioChunk:(NSData*)pcmData;

// Called on the main thread when the input perceptual RMS energy level in
// [0.0, 1.0] has been updated.
- (void)audioController:(id<TTCAudioController>)controller
    didUpdateInputEnergy:(float)energy;

// Called on the main thread when audio response playback starts.
- (void)audioControllerDidStartPlayback:(id<TTCAudioController>)controller;

// Called on the main thread when audio response playback stops or all queued
// audio buffers finish rendering.
- (void)audioControllerDidStopPlayback:(id<TTCAudioController>)controller;

// Called on the main thread when the active physical audio route changes (e.g.
// headphones or AirPods connected or disconnected).
- (void)audioControllerDidChangeRoute:(id<TTCAudioController>)controller;

// Called on the main thread when an unrecoverable audio error occurs.
- (void)audioController:(id<TTCAudioController>)controller
      didEncounterError:(NSError*)error;

@end

// Defines the unified interface for the TalkToChrome audio subsystem,
// abstracting microphone capture, speaker playback, barge-in clearing, and
// acoustic route inspection.
@protocol TTCAudioController <NSObject>

// Delegate receiving audio buffers, energy updates, and lifecycle events.
@property(nonatomic, weak) id<TTCAudioControllerDelegate> delegate;

// Whether the controller is actively capturing microphone audio.
@property(nonatomic, readonly, assign, getter=isCapturing) BOOL capturing;

// Whether response audio is actively playing through the speaker or headphones.
@property(nonatomic, readonly, assign, getter=isPlaying) BOOL playing;

// Whether microphone input is routed directly to the speaker for local testing
// without network roundtrips.
@property(nonatomic, assign, getter=isLoopbackEnabled) BOOL loopbackEnabled;

// Whether audio output is currently physically routed through the device's
// built-in loudspeaker. Returns NO when audio is routed to headphones,
// AirPods, or external audio accessories.
@property(nonatomic, readonly, assign, getter=isOutputRoutedToSpeaker)
    BOOL outputRoutedToSpeaker;

// Starts capturing audio from the microphone asynchronously. Invokes
// `completion` on the main thread once the capture graph and session are ready.
- (void)startCaptureWithCompletion:(void (^)(BOOL success,
                                             NSError* error))completion;

// Stops capturing microphone audio and detaches the input tap.
- (void)stopCapture;

// Enqueues a chunk of 24kHz 16-bit linear PCM response audio for streaming
// playback. Starts the playback graph if not already running.
- (void)playStreamingAudioChunk:(NSData*)pcm24kData;

// Instantly clears all queued, unrendered playback audio (used on barge-in /
// user interruption).
- (void)clearPlaybackQueue;

// Immediately stops response playback and releases playback buffers.
- (void)stopPlayback;

// Generates and plays a 440Hz synthesized test tone for developer diagnostics.
- (void)playTestTone;

// Immediately stops test tone playback.
- (void)stopTestTone;

// Deterministically releases audio resources and deactivates the audio session.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_CONTROLLER_H_
