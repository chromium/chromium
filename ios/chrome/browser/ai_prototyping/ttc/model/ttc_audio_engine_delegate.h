// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_DELEGATE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_DELEGATE_H_

#import <Foundation/Foundation.h>

@class TTCAudioEngine;

// Delegate protocol to receive microphone energy updates, lifecycle events,
// and audio state changes from `TTCAudioEngine`.
@protocol TTCAudioEngineDelegate <NSObject>
@optional

// Called on the main thread when a new buffer of microphone energy is computed.
- (void)audioEngine:(TTCAudioEngine*)engine didUpdateInputEnergy:(float)energy;

// Called on the main thread when an error occurs in the audio engine or
// session.
- (void)audioEngine:(TTCAudioEngine*)engine didEncounterError:(NSError*)error;

// Called on the main thread when audio capture starts.
- (void)audioEngineDidStartRecording:(TTCAudioEngine*)engine;

// Called on the main thread when audio capture stops.
- (void)audioEngineDidStopRecording:(TTCAudioEngine*)engine;

// Called on the main thread when response audio playback starts.
- (void)audioEngineDidStartPlayback:(TTCAudioEngine*)engine;

// Called on the main thread when response audio playback stops.
- (void)audioEngineDidStopPlayback:(TTCAudioEngine*)engine;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_DELEGATE_H_
