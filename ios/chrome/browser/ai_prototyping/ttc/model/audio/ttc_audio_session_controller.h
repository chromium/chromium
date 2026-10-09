// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_AUDIO_TTC_AUDIO_SESSION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_AUDIO_TTC_AUDIO_SESSION_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"

@protocol TTCAudioEngineProtocol;
@class TTCAudioSessionManager;

// Domain for errors originated by TTCAudioSessionController.
extern NSString* const kTTCAudioSessionControllerErrorDomain;

// Error codes for TTCAudioSessionController.
enum class TTCAudioSessionControllerErrorCode : NSInteger {
  kStartupCancelled = -2,
  kPermissionDenied = -3,
  kCaptureStartFailed = -5,
};

// Policy and lifecycle orchestrator for a TTC audio conversation, implementing
// `<TTCAudioController>` for `TtcConversation`.
//
// Owns one `TTCAudioSessionManager` (process-wide `AVAudioSession` wrapper) and
// one `id<TTCAudioEngineProtocol>` (pluggable audio DSP graph, either
// provider-supplied or the open-source `TTCAudioEngine`), coordinating the
// sequencing and policy decisions between them:
// - Capture startup pipeline (`startCaptureWithCompletion:`): requests
//   microphone permission via `AVAudioApplication`, activates `AVAudioSession`
//   via `TTCAudioSessionManager`, starts the audio engine graph, and begins
//   microphone capture, rolling back session state if cancelled or failed.
// - On-demand playback startup (`playStreamingAudioChunk:`): buffers incoming
//   24kHz PCM chunks that arrive before the engine is running, activates the
//   session and starts the engine on demand, and flushes queued chunks.
// - Graph-independent diagnostics: performs 16kHz-to-24kHz linear-interpolated
//   local loopback (`loopbackEnabled`) and 440Hz sine-wave synthesis
//   (`playTestTone`).
// - Deterministic teardown (`disconnect`): stops the engine graph and restores
//   the prior `AVAudioSession` category.
@interface TTCAudioSessionController : NSObject <TTCAudioController>

// Designated initializer. Initializes with the specified audio engine and
// audio session manager. Passing nil for `audioEngine` instantiates a default
// `TTCAudioEngine`. Passing nil for `sessionManager` instantiates a default
// `TTCAudioSessionManager`.
- (instancetype)initWithAudioEngine:(id<TTCAudioEngineProtocol>)audioEngine
                     sessionManager:(TTCAudioSessionManager*)sessionManager
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_AUDIO_TTC_AUDIO_SESSION_CONTROLLER_H_
