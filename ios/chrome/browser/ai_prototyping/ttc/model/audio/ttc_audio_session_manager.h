// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_AUDIO_TTC_AUDIO_SESSION_MANAGER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_AUDIO_TTC_AUDIO_SESSION_MANAGER_H_

#import <Foundation/Foundation.h>

@protocol TTCAudioSessionManagerDelegate;

// Domain for errors originated by TTCAudioSessionManager.
extern NSString* const kTTCAudioSessionManagerErrorDomain;

// Error codes for TTCAudioSessionManager.
enum class TTCAudioSessionManagerErrorCode : NSInteger {
  kCancelled = -1,
};

// Wraps iOS's process-wide `[AVAudioSession sharedInstance]` singleton for TTC.
//
// Responsibilities:
// - Snapshots the prior `AVAudioSession` category, mode, and options
//   before activating `AVAudioSessionCategoryPlayAndRecord` (with
//   `DefaultToSpeaker` and Bluetooth/AirPlay options so iOS handles hardware
//   routing automatically), and restores them upon teardown.
// - Executes CoreAudio/`mediaserverd` session activation and deactivation on a
//   background task runner to avoid blocking the main UI thread.
// - Inspects physical audio routes (`hasHardwareAEC`, `outputRoutedToSpeaker`)
//   and translates `AVAudioSession` notifications (interruptions and hardware
//   route changes) into delegate calls.
//
// Does not own or interact with any audio processing graph (`AVAudioEngine`),
// microphone permission prompts, or PCM audio buffers.
@interface TTCAudioSessionManager : NSObject

// Delegate receiving session interruption and route lifecycle events.
@property(nonatomic, weak) id<TTCAudioSessionManagerDelegate> delegate;

// Whether the currently active audio input hardware supports voice call
// processing (hardware Acoustic Echo Cancellation).
@property(nonatomic, readonly) BOOL hasHardwareAEC;

// Whether audio output is currently physically routed through the device's
// built-in loudspeaker (returns NO when routed to headphones, Bluetooth, or
// handset receiver).
@property(nonatomic, readonly, getter=isOutputRoutedToSpeaker)
    BOOL outputRoutedToSpeaker;

// Configures and activates the AVAudioSession synchronously on the caller's
// thread with PlayAndRecord category, VideoChat mode, and DefaultToSpeaker +
// Bluetooth/AirPlay options. Returns nil on success or the NSError encountered.
// NOTE: This call performs synchronous CoreAudio IPC; prefer
// `configureAudioSessionWithCompletion:` on UI threads to prevent frame drops.
- (NSError*)configureAudioSession;

// Configures and activates the AVAudioSession asynchronously on a background
// task runner to avoid blocking the UI thread on CoreAudio server IPC, then
// invokes `completion` on the UI thread with the resulting error (or nil on
// success).
// @param completion Block invoked on the UI thread with the result.
- (void)configureAudioSessionWithCompletion:
    (void (^)(NSError* error))completion;

// Restores the AVAudioSession category, mode, and categoryOptions that were
// active prior to TTC configuration and deactivates the session notifying other
// audio apps.
- (void)restoreAudioSessionCategory;

// Tears down the audio session manager, cancelling any in-flight startup tasks
// and restoring the prior audio session configuration.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_AUDIO_TTC_AUDIO_SESSION_MANAGER_H_
