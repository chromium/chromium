// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VOICE_PLATE_CONSUMER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VOICE_PLATE_CONSUMER_H_

#import <Foundation/Foundation.h>

// High-level session UI states for the TTC Voice Plate.
enum class TTCSessionUIState {
  // Idle state; audio and network sessions are inactive.
  kIdle = 0,
  // Establishing connection with model endpoint.
  kConnecting,
  // Handshake complete; microphone is capturing and streaming upstream.
  kListening,
  // A fatal network or hardware error occurred.
  kError,
};

// Consumer protocol receiving session state, audio telemetry, and errors
// to render in the TTC Voice Plate.
@protocol TTCVoicePlateConsumer <NSObject>

// Updates the current high-level voice session state.
- (void)setSessionState:(TTCSessionUIState)state;

// Updates whether microphone capture is muted.
- (void)setMicMuted:(BOOL)micMuted;

// Updates the microphone input energy level in RMS (normalized 0.0 - 1.0).
- (void)setMicEnergyLevel:(float)rms;

// Displays `errorMessage`, or falls back to a default session error message if
// `errorMessage` is nil.
- (void)showErrorMessage:(NSString*)errorMessage;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VOICE_PLATE_CONSUMER_H_
