// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_NOTIFICATION_HANDLER_H_
#define IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_NOTIFICATION_HANDLER_H_

#import <Foundation/Foundation.h>

// A helper object that listens to NSNotifications and manages the TTS player.
@interface TextToSpeechNotificationHandler : NSObject

// Whether or not TTS playback is currently enabled.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// Cancels any in-progress playback.
- (void)cancelPlayback;

@end

#endif  // IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_NOTIFICATION_HANDLER_H_
