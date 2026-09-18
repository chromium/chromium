// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_PLAYER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_PLAYER_H_

#import <AVFAudio/AVFAudio.h>
#import <Foundation/Foundation.h>

#import "base/containers/span.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_player_delegate.h"

@class TTCAudioPlayer;

namespace ttc {

// Converts signed 16-bit linear PCM `source` samples to normalized Float32
// values in the range [-1.0, 1.0] into `destination`. If either span is empty,
// this is a no-op.
void ConvertInt16ToFloat32(base::span<const int16_t> source,
                           base::span<float> destination);

}  // namespace ttc

// Audio player managing audio node attachment to the shared AVAudioEngine,
// 24kHz synthesized voice buffer scheduling, and immediate buffer drain.
@interface TTCAudioPlayer : NSObject

// Delegate receiving playback lifecycle events.
@property(nonatomic, weak) id<TTCAudioPlayerDelegate> delegate;

// Whether synthesized response audio is currently actively playing through
// the player node.
@property(nonatomic, readonly, assign) BOOL isPlaying;

// Designated initializer. Initializes with the specified player node and
// playback format.
- (instancetype)initWithPlayerNode:(AVAudioPlayerNode*)playerNode
                    playbackFormat:(AVAudioFormat*)playbackFormat
    NS_DESIGNATED_INITIALIZER;

// Default initializer configuring a default `AVAudioPlayerNode` and 24kHz mono
// Float32 format.
- (instancetype)init;

// Attaches the internal player node to `audioEngine` and connects it to the
// engine's `mainMixerNode` using the player's playback format. Returns `YES`
// on success, or `NO` and sets `error` if connection fails.
- (BOOL)attachToAudioEngine:(AVAudioEngine*)audioEngine error:(NSError**)error;

// Detaches the player node from `audioEngine` and stops playback.
- (void)detachFromAudioEngine:(AVAudioEngine*)audioEngine;

// Schedules a chunk of 24kHz 16-bit linear PCM audio for streaming playback.
- (void)playStreamingAudioChunk:(NSData*)pcm24kData;

// Schedules an `AVAudioPCMBuffer` for playback, resampling to 24kHz Float32
// mono if its format differs from the player's format.
- (void)playPCMBuffer:(AVAudioPCMBuffer*)buffer;

// Immediately stops playback and clears all pending scheduled buffers.
- (void)stopPlaybackImmediately;

// Resumes node playback if `isPlaying` is YES but the player node was stopped
// due to an engine restart or hardware route change.
- (void)resumePlaybackAfterEngineRestart;

// Resets internal state and pending buffer tracking.
- (void)reset;

// Sets `isPlaying` state for testing purposes without starting hardware.
- (void)setIsPlayingForTesting:(BOOL)isPlaying;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_PLAYER_H_
