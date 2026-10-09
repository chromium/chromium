// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_AUDIO_TTC_AUDIO_ENGINE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_AUDIO_TTC_AUDIO_ENGINE_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/intelligence/ttc_audio_engine_protocol.h"

@class AVAudioEngine;
@class TTCAudioPlayer;
@class TTCAudioRecorder;

// Domain for errors originated by TTCAudioEngine.
extern NSString* const kTTCAudioEngineErrorDomain;

// Error codes for TTCAudioEngine.
enum class TTCAudioEngineErrorCode : NSInteger {
  kInputNodeUnavailable = -1,
  kStartupCancelled = -2,
  kPermissionDenied = -3,
  kEngineStartFailed = -4,
};

// Default open-source `TTCAudioEngineProtocol` audio graph implementation
// backed by Apple's `AVAudioEngine`, `TTCAudioRecorder`, and `TTCAudioPlayer`.
//
// Responsibilities:
// - Owns the `AVAudioEngine` instance, enables hardware voice processing
//   (`VoiceProcessingIO`) on `inputNode` when `aecMode` is `kHardware`, and
//   starts/stops the processing graph.
// - Installs `TTCAudioRecorder` taps on `inputNode`, converts captured 16kHz
//   Float32 buffers into clamped signed 16-bit PCM bytes, and forwards them
//   with normalized RMS input energy to `TTCAudioEngineDelegate`.
// - Attaches `TTCAudioPlayer` (`AVAudioPlayerNode`) to `mainMixerNode` to
//   schedule and drain 24kHz signed 16-bit PCM playback buffers.
// - Observes `AVAudioEngineConfigurationChangeNotification` scoped to its
//   `AVAudioEngine` to reinstall taps and resume playback after internal graph
//   resets.
//
// Assumes `AVAudioSession` is already active. Does not manage `AVAudioSession`
// category/activation, microphone permissions, or routing policy; those are
// owned by `TTCAudioSessionController` and `TTCAudioSessionManager`.
@interface TTCAudioEngine : NSObject <TTCAudioEngineProtocol>

// Designated initializer. Initializes with the specified `AVAudioEngine`,
// `TTCAudioRecorder`, and `TTCAudioPlayer` components. Passing nil for any
// component instantiates a default instance.
- (instancetype)initWithAudioEngine:(AVAudioEngine*)audioEngine
                           recorder:(TTCAudioRecorder*)recorder
                             player:(TTCAudioPlayer*)player
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_AUDIO_TTC_AUDIO_ENGINE_H_
