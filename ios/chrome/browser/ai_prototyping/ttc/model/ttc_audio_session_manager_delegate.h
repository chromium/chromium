// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_SESSION_MANAGER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_SESSION_MANAGER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class TTCAudioSessionManager;

// Delegate protocol to receive audio session lifecycle notifications, such as
// interruptions, from `TTCAudioSessionManager`.
@protocol TTCAudioSessionManagerDelegate <NSObject>
@optional

// Called on the UI thread when an audio session interruption begins (e.g. phone
// call, FaceTime, alarm).
// @param manager The audio session manager posting the notification.
- (void)audioSessionManagerDidBeginInterruption:
    (TTCAudioSessionManager*)manager;

// Called on the UI thread when an audio session interruption ends.
// @param manager The audio session manager posting the notification.
// @param shouldResume YES if the system suggests audio processing should
// resume.
- (void)audioSessionManager:(TTCAudioSessionManager*)manager
    didEndInterruptionWithShouldResume:(BOOL)shouldResume;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_SESSION_MANAGER_DELEGATE_H_
