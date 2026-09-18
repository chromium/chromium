// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_RECORDER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_RECORDER_H_

#import <AVFAudio/AVFAudio.h>
#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder_delegate.h"

// Manages microphone audio input tap installation, real-time resampling to
// 16kHz mono Float32 format, and Root Mean Square (RMS) energy computation.
@interface TTCAudioRecorder : NSObject

// Delegate receiving audio recorder events.
@property(nonatomic, weak) id<TTCAudioRecorderDelegate> delegate;

// Whether the audio tap is currently installed and actively capturing audio.
@property(nonatomic, readonly, assign, getter=isRecording) BOOL recording;

// Installs an audio tap on `inputNode`'s bus 0, configuring resampling to 16kHz
// mono Float32 and streaming RMS energy levels. Returns `YES` on success, or
// `NO` and sets `error` if tap installation fails.
- (BOOL)installTapOnInputNode:(AVAudioInputNode*)inputNode
                        error:(NSError**)error;

// Safely removes the audio tap from `inputNode` and stops capture.
- (void)removeTapFromInputNode:(AVAudioInputNode*)inputNode;

// Resets any internal converters or cached buffers.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_RECORDER_H_
