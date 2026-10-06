// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_RECORDER_TESTING_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_RECORDER_TESTING_H_

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder.h"

@interface TTCAudioRecorder (Testing)

// Processes incoming audio buffers from unit tests on the UI thread,
// forwarding with current instance formats.
- (void)handleInputBuffer:(AVAudioPCMBuffer*)buffer;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_RECORDER_TESTING_H_
