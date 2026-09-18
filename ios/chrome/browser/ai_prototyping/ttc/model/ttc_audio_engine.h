// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine_delegate.h"

@class TTCAudioEngine;
@class TTCAudioRecorder;

// Audio engine managing audio hardware graph orchestration, session
// configuration, and microphone capture delegation for TalkToChrome.
@interface TTCAudioEngine : NSObject

// Delegate receiving audio energy metrics and lifecycle events.
@property(nonatomic, weak) id<TTCAudioEngineDelegate> delegate;

// Whether the audio engine is actively capturing audio from the microphone.
@property(nonatomic, readonly, assign) BOOL isRecording;

// Initializes with the specified audio recorder component.
- (instancetype)initWithRecorder:(TTCAudioRecorder*)recorder
    NS_DESIGNATED_INITIALIZER;

// Default initializer creating a default `TTCAudioRecorder`.
- (instancetype)init;

// Asynchronously requests microphone record permission from the user.
- (void)requestMicrophonePermissionWithCompletion:
    (void (^)(BOOL granted))completion;

// Configures the audio session for recording on a background queue and starts
// capturing 16kHz mono audio via an input node tap.
- (void)startRecordingWithCompletion:(void (^)(BOOL success,
                                               NSError* error))completion;

// Stops capturing audio and detaches the input node tap.
- (void)stopRecording;

// Deterministically stops audio capture and restores the previous audio session
// category.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_
