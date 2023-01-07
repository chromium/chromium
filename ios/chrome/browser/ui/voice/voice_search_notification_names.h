// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_VOICE_VOICE_SEARCH_NOTIFICATION_NAMES_H_
#define IOS_CHROME_BROWSER_UI_VOICE_VOICE_SEARCH_NOTIFICATION_NAMES_H_

#import <Foundation/Foundation.h>

// Voice Search UI notifications.

// Notification when the Voice Search UI is displayed.
extern NSString* const kVoiceSearchWillShowNotification;
// Notification when the Voice Search UI is dismissed.
extern NSString* const kVoiceSearchWillHideNotification;

// Text-To-Speech notifications.  The sending objects for these notifications
// are the TextToSpeechPlayers handling playback.

// Notification when TTS audio data is prepared for playback.
extern NSString* const kTTSAudioReadyForPlaybackNotification;
// Notification when TTS starts playing.
extern NSString* const kTTSWillStartPlayingNotification;
// Notification when TTS stops playing.
extern NSString* const kTTSDidStopPlayingNotification;

#endif  // IOS_CHROME_BROWSER_UI_VOICE_VOICE_SEARCH_NOTIFICATION_NAMES_H_
