// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_CONSUMER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_CONSUMER_H_

#import <Foundation/Foundation.h>

// High-level session states for the TalkToChrome experience.
enum class TTCSessionState {
  // Idle state; audio and network sessions are inactive.
  kIdle = 0,
  // Establishing connection with model endpoint.
  kConnecting,
  // WebSocket open; exchanging setup handshake parameters.
  kHandshaking,
  // Handshake complete; microphone is capturing and streaming upstream.
  kListening,
  // Model is synthesizing audio and streaming back responses.
  kModelSpeaking,
  // A fatal network or hardware error occurred.
  kError,
};

// Consumer protocol receiving state and microphone input telemetry to render
// in the TalkToChrome prototype UI.
@protocol TTCConsumer <NSObject>

// Updates the current high-level voice session state.
- (void)setSessionState:(TTCSessionState)state;

// Updates the microphone input energy level in RMS (normalized 0.0 - 1.0).
- (void)setMicEnergyLevel:(float)rms;

// Updates whether the test audio tone is currently playing through the speaker.
- (void)setTestAudioPlaying:(BOOL)isPlaying;

// Updates whether microphone loopback mode is enabled.
- (void)setLoopbackEnabled:(BOOL)enabled;

// Displays an error message in the status label.
- (void)didEncounterError:(NSString*)errorMessage;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_CONSUMER_H_
