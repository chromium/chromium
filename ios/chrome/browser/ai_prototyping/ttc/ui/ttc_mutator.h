// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_MUTATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_MUTATOR_H_

#import <Foundation/Foundation.h>

// Protocol for mutating model and hardware state based on user interactions
// in the TalkToChrome prototype UI.
@protocol TTCMutator <NSObject>

// Starts an end-to-end voice session.
- (void)startSession;

// Gracefully terminates the active voice session.
- (void)stopSession;

// Enables or disables local microphone loopback to the speaker.
- (void)setLoopbackEnabled:(BOOL)enabled;

// Plays a 440Hz test audio tone at 24kHz through the speaker.
- (void)playTestAudio;

// Stops test audio playback.
- (void)stopTestAudio;

// Notifies the mutator that the UI has appeared, triggering state hydration.
- (void)viewWillAppear;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_MUTATOR_H_
