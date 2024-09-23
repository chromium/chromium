// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYER_SUBCLASSING_H_

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_player.h"

@class AVAudioPlayer;

// Category exposing the AVAudioPlayer to subclasses.
@interface TextToSpeechPlayer (Subclassing)

// The AVAudioPlayer that is used to play TTS audio data.
@property(nonatomic, readonly) AVAudioPlayer* player;

@end

#endif  // IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYER_SUBCLASSING_H_
