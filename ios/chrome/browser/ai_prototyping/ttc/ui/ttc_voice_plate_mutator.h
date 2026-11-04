// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VOICE_PLATE_MUTATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VOICE_PLATE_MUTATOR_H_

#import <Foundation/Foundation.h>

// Protocol for mutating voice session state based on user interactions with the
// TTC Voice Plate.
@protocol TTCVoicePlateMutator <NSObject>

// Starts an end-to-end voice session.
- (void)startSession;

// Gracefully terminates the active voice session.
- (void)stopSession;

// Toggles microphone capture between muted and unmuted while a session is live.
- (void)toggleMicMuted;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VOICE_PLATE_MUTATOR_H_
