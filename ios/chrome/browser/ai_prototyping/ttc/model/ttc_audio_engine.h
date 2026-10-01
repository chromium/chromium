// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_controller.h"

@class TTCAudioPlayer;
@class TTCAudioRecorder;
@class TTCAudioSessionManager;

// Domain for errors originated by TTCAudioEngine.
extern NSString* const kTTCAudioEngineErrorDomain;

// Error codes for TTCAudioEngine.
enum class TTCAudioEngineErrorCode : NSInteger {
  kInputNodeUnavailable = -1,
  kStartupCancelled = -2,
  kPermissionDenied = -3,
};

// Audio engine managing audio hardware graph orchestration, session
// configuration, and microphone capture and speaker playback delegation for
// TalkToChrome.
@interface TTCAudioEngine : NSObject <TTCAudioController>

// Designated initializer. Initializes with the specified audio recorder,
// audio player, and audio session manager components. Passing nil for any
// component instantiates a default instance.
- (instancetype)initWithRecorder:(TTCAudioRecorder*)recorder
                          player:(TTCAudioPlayer*)player
                  sessionManager:(TTCAudioSessionManager*)sessionManager
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_ENGINE_H_
