// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYER_H_
#define IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYER_H_

#import <Foundation/Foundation.h>

// Class responsible for playing TextToSpeech audio data and emitting audio
// levels.
@interface TextToSpeechPlayer : NSObject

// Whether there is audio data prepared to be played.
@property(nonatomic, readonly, getter=isReadyForPlayback) BOOL readyForPlayback;

// Whether the TextToSpeechPlayer is currently playing audio.
@property(nonatomic, readonly, getter=isPlayingAudio) BOOL playingAudio;

// Stores a reference to `audioData` and clears up prior TTS state.  Calling
// this function while `playingAudio` is YES will cancel prior playback.  This
// function will post a kTTSAudioReadyForPlaybackNotification notification with
// the TexToSpeechPlayer used as the sending object.
- (void)prepareToPlayAudioData:(NSData*)audioData;

// Creates an AVAudioPlayer and begins playing audio data passed to
// `-prepareToPlayAudioData:`.  No-op if called when `readyForPlayback` is NO
// or `playingAudio` is YES.  This function will post a
// kTTSWillStartPlayingNotification with the TextToSpeechPlayer used as the
// sending object.
- (void)beginPlayback;

// Stops any active playback and notifies observers that TTS has stopped.  This
// function will post a kTTSDidStopPlayingNotification notification with the
// TextToSpeechPlayer used as the sending object.
- (void)cancelPlayback;

@end

#endif  // IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYER_H_
