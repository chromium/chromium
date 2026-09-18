// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_PLAYER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_PLAYER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class TTCAudioPlayer;

// Delegate protocol receiving playback lifecycle notifications from
// `TTCAudioPlayer`.
@protocol TTCAudioPlayerDelegate <NSObject>
@optional

// Called on the main thread when response audio playback begins.
- (void)audioPlayerDidStartPlayback:(TTCAudioPlayer*)player;

// Called on the main thread when all queued audio buffers have completed
// playback or when playback was stopped.
- (void)audioPlayerDidStopPlayback:(TTCAudioPlayer*)player;

// Called on the main thread when an error occurs during buffer conversion or
// playback.
- (void)audioPlayer:(TTCAudioPlayer*)player didEncounterError:(NSError*)error;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_PLAYER_DELEGATE_H_
