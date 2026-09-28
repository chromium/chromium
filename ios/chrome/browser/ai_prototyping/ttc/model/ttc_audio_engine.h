// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_controller.h"

@class TTCAudioEngine;
@class TTCAudioPlayer;
@class TTCAudioRecorder;

// Audio engine managing audio hardware graph orchestration, session
// configuration, and microphone capture and speaker playback delegation for
// TalkToChrome.
@interface TTCAudioEngine : NSObject <TTCAudioController>

// Whether the audio engine is actively capturing audio from the microphone.
@property(nonatomic, readonly, assign, getter=isCapturing) BOOL capturing;

// Whether synthesized response audio is actively playing through the speaker.
@property(nonatomic, readonly, assign, getter=isPlaying) BOOL playing;

// Whether microphone input is routed directly to the speaker for local
// testing without network roundtrips.
@property(nonatomic, assign, getter=isLoopbackEnabled) BOOL loopbackEnabled;

// Whether audio output is currently physically routed through the device's
// built-in loudspeaker. Returns NO when audio is routed to headphones,
// AirPods, or external audio accessories.
@property(nonatomic, readonly, assign, getter=isOutputRoutedToSpeaker)
    BOOL outputRoutedToSpeaker;

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

// Configures the audio session for capture and playback on a background queue
// and starts capturing 16kHz mono audio via an input node tap.
- (void)startCaptureWithCompletion:(void (^)(BOOL success,
                                             NSError* error))completion;

// Stops capturing microphone audio and detaches the input tap.
- (void)stopCapture;

// Schedules a chunk of 24kHz 16-bit linear PCM response audio for streaming
// playback. Starts the audio engine if it is not currently running.
- (void)playStreamingAudioChunk:(NSData*)pcm24kData;

// Immediately stops response playback and purges all scheduled, unplayed
// buffers for barge-in interruption.
- (void)stopPlaybackImmediately;

// Immediately clears all queued, unrendered playback audio for barge-in.
// Conforms to `TTCAudioController`.
- (void)clearPlaybackQueue;

// Immediately stops response playback. Conforms to `TTCAudioController`.
- (void)stopPlayback;

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
