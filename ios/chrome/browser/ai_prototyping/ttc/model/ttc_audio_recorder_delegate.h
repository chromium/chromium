// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_RECORDER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_RECORDER_DELEGATE_H_

#import <AVFAudio/AVFAudio.h>
#import <Foundation/Foundation.h>

@class TTCAudioRecorder;

// Delegate protocol receiving audio capture updates and RMS energy levels
// from `TTCAudioRecorder`.
@protocol TTCAudioRecorderDelegate <NSObject>

// Called on the main thread when a new audio buffer has been captured,
// resampled, and its perceptual energy level in [0.0, 1.0] has been calculated.
- (void)audioRecorder:(TTCAudioRecorder*)recorder
    didUpdateInputEnergy:(float)energy;

@optional
// Called on the main thread when a resampled 16kHz mono Float32 audio buffer
// is produced.
- (void)audioRecorder:(TTCAudioRecorder*)recorder
     didCaptureBuffer:(AVAudioPCMBuffer*)buffer;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_RECORDER_DELEGATE_H_
