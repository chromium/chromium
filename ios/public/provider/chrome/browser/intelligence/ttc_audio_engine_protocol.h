// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_TTC_AUDIO_ENGINE_PROTOCOL_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_TTC_AUDIO_ENGINE_PROTOCOL_H_

#import <Foundation/Foundation.h>

// Echo cancellation mode applied to the microphone capture stream.
enum class TTCAudioAECMode : NSInteger {
  // Unspecified echo cancellation mode.
  kUnknown = 0,
  // No echo cancellation applied.
  kNone = 1,
  // Hardware voice processing (VoiceProcessingIO) echo cancellation.
  kHardware = 2,
  // Software echo cancellation.
  kSoftware = 3,
  // Adaptive software echo cancellation.
  kAdaptiveSoftware = 4,
};

@protocol TTCAudioEngineProtocol;

// Delegate receiving captured audio, playback lifecycle events, and engine
// errors on the creating sequence.
@protocol TTCAudioEngineDelegate <NSObject>

@optional

// Invoked when a chunk of captured 16kHz mono signed 16-bit PCM audio data is
// ready, along with the normalized input audio level in `[0.0, 1.0]`.
// @param engine The audio engine instance.
// @param audioData Captured 16kHz 16-bit signed linear PCM audio bytes.
// @param inputLevel Normalized audio input energy in the range `[0.0, 1.0]`.
- (void)audioEngine:(id<TTCAudioEngineProtocol>)engine
    didCaptureAudioData:(NSData*)audioData
             inputLevel:(float)inputLevel;

// Invoked when the audio engine begins playing scheduled audio buffers.
// @param engine The audio engine instance.
- (void)audioEngineDidStartPlayback:(id<TTCAudioEngineProtocol>)engine;

// Invoked when the audio engine finishes playing all scheduled audio buffers
// or when playback is stopped.
// @param engine The audio engine instance.
- (void)audioEngineDidStopPlayback:(id<TTCAudioEngineProtocol>)engine;

// Invoked when the audio engine encounters an unrecoverable runtime error.
// @param engine The audio engine instance.
// @param error The error encountered by the engine.
- (void)audioEngine:(id<TTCAudioEngineProtocol>)engine
    didEncounterError:(NSError*)error;

@end

// Protocol abstracting the pluggable low-level audio DSP graph, microphone
// capture tap, and streaming playback node for TTC (implemented by the
// open-source `TTCAudioEngine` or a provider-supplied engine).
//
// Responsibilities:
// - Prepares, starts, and stops the underlying audio graph
// (`startWithCompletion:`,
//   `stopWithCompletion:`, `disconnect`).
// - Applies the configured echo cancellation mode (`aecMode`, e.g. hardware
//   `VoiceProcessingIO` vs. adaptive software AEC) when building the graph.
// - Installs/removes the microphone capture tap (`startCapture`, `stopCapture`)
//   and delivers 16kHz mono signed 16-bit PCM chunks with normalized input
//   energy to `TTCAudioEngineDelegate`.
// - Schedules, drains, and flushes 24kHz mono signed 16-bit PCM playback
//   buffers (`schedulePlaybackData:`, `notifyEndOfPlaybackData`,
//   `stopPlayback`).
//
// Assumes `AVAudioSession` is already configured and activated by the caller.
// Concrete implementations do not manage `AVAudioSession` activation/category
// restoration, microphone permission prompts, or routing policy; those are
// owned by `TTCAudioSessionController` and `TTCAudioSessionManager`.
@protocol TTCAudioEngineProtocol <NSObject>

// Delegate receiving capture, playback, and error callbacks.
@property(nonatomic, weak) id<TTCAudioEngineDelegate> delegate;

// Whether the underlying audio graph is currently started and running.
@property(nonatomic, readonly, getter=isStarted) BOOL started;

// Echo cancellation mode to apply when starting the audio graph.
@property(nonatomic, assign) TTCAudioAECMode aecMode;

// Whether microphone capture is currently active on the running graph.
@property(nonatomic, readonly, getter=isCapturing) BOOL capturing;

// Most recent normalized microphone input level in `[0.0, 1.0]`.
@property(nonatomic, readonly) float inputAudioLevel;

// Whether playback buffers are currently scheduled and playing.
@property(nonatomic, readonly, getter=isPlaying) BOOL playing;

// Prepares and starts the underlying audio graph asynchronously.
// Assumes the `AVAudioSession` has already been configured and activated by the
// caller.
// @param completion Block invoked on the creating sequence once the graph has
//     started or failed.
- (void)startWithCompletion:(void (^)(BOOL success, NSError* error))completion;

// Stops the underlying audio graph asynchronously, halting any active capture
// or playback.
// @param completion Block invoked on the creating sequence once the graph has
//     stopped.
- (void)stopWithCompletion:(void (^)(BOOL success, NSError* error))completion;

// Synchronously tears down the audio graph, removes observers, and releases
// resources.
- (void)disconnect;

// Starts microphone capture on a started audio graph (`started == YES`).
// @return YES if capture was started or was already active; NO if the graph is
//     not started or installing the capture tap failed.
- (BOOL)startCapture;

// Stops microphone capture without stopping the underlying audio graph.
- (void)stopCapture;

// Schedules a chunk of 24kHz mono signed 16-bit linear PCM audio data for
// playback on a started audio graph (`started == YES`).
// @param pcm24kData 24kHz 16-bit signed linear PCM audio bytes.
- (void)schedulePlaybackData:(NSData*)pcm24kData;

// Signals that all playback data for the current response turn has been
// scheduled so the engine can notify its delegate when the final buffer drains.
- (void)notifyEndOfPlaybackData;

// Immediately stops playback and flushes any queued playback buffers without
// stopping the underlying audio graph.
- (void)stopPlayback;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_TTC_AUDIO_ENGINE_PROTOCOL_H_
