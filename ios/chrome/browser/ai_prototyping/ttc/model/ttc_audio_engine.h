// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine_delegate.h"

@class TTCAudioEngine;
@class TTCAudioPlayer;
@class TTCAudioRecorder;

// Audio engine managing audio hardware graph orchestration, session
// configuration, and microphone capture and speaker playback delegation for
// TalkToChrome.
@interface TTCAudioEngine : NSObject

// Delegate receiving audio energy metrics and lifecycle events.
@property(nonatomic, weak) id<TTCAudioEngineDelegate> delegate;

// Whether the audio engine is actively capturing audio from the microphone.
@property(nonatomic, readonly, assign) BOOL isRecording;

// Whether synthesized response audio is actively playing through the speaker.
@property(nonatomic, readonly, assign) BOOL isPlaying;

// Whether microphone input is routed directly to the speaker for local
// testing without network roundtrips.
@property(nonatomic, assign) BOOL loopbackEnabled;

// Designated initializer. Initializes with the specified audio recorder and
// audio player components.
- (instancetype)initWithRecorder:(TTCAudioRecorder*)recorder
                          player:(TTCAudioPlayer*)player
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer creating a default `TTCAudioPlayer`.
- (instancetype)initWithRecorder:(TTCAudioRecorder*)recorder;

// Default initializer creating default `TTCAudioRecorder` and `TTCAudioPlayer`
// components.
- (instancetype)init;

// Asynchronously requests microphone record permission from the user.
- (void)requestMicrophonePermissionWithCompletion:
    (void (^)(BOOL granted))completion;

// Configures the audio session for recording and playback on a background queue
// and starts capturing 16kHz mono audio via an input node tap.
- (void)startRecordingWithCompletion:(void (^)(BOOL success,
                                               NSError* error))completion;

// Stops capturing audio and detaches the input node tap.
- (void)stopRecording;

// Schedules a chunk of 24kHz 16-bit linear PCM response audio for streaming
// playback. Starts the audio engine if it is not currently running.
- (void)playStreamingAudioChunk:(NSData*)pcm24kData;

// Immediately stops response playback and purges all scheduled, unplayed
// buffers for barge-in interruption.
- (void)stopPlaybackImmediately;

// Generates and plays a 440Hz synthesized sine wave test tone at 24kHz through
// the speaker.
- (void)playTestTone;

// Immediately stops test tone playback.
- (void)stopTestTone;

// Deterministically stops audio capture and playback, and restores the previous
// audio session category.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_
