// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_SESSION_MANAGER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_SESSION_MANAGER_H_

#import <Foundation/Foundation.h>

// Domain for errors originated by TTCAudioSessionManager.
extern NSString* const kTTCAudioSessionManagerErrorDomain;

// Error codes for TTCAudioSessionManager.
enum class TTCAudioSessionManagerErrorCode : NSInteger {
  kCancelled = -1,
};

// Manages the AVAudioSession lifecycle for TalkToChrome, encapsulating
// category activation, option configuration, category restoration upon
// teardown, and route inspection.
@interface TTCAudioSessionManager : NSObject

// Human-readable name of the current active audio input port (e.g. "iPhone
// Microphone", "AirPods"). Returns nil if no input port is available.
@property(nonatomic, readonly) NSString* activeInputRouteName;

// Human-readable name of the current active audio output port (e.g. "Speaker",
// "Headphones"). Returns nil if no output port is available.
@property(nonatomic, readonly) NSString* activeOutputRouteName;

// Configures and activates the AVAudioSession synchronously on the caller's
// thread with PlayAndRecord category, defaulting to the speaker and enabling
// Bluetooth and AirPlay routes. Returns nil on success or the NSError
// encountered during category or activation configuration.
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
// active prior to TalkToChrome configuration, and deactivates the session
// notifying other audio apps.
- (void)restoreAudioSessionCategory;

// Tears down the audio session manager, cancelling any in-flight startup tasks
// and restoring the prior audio session configuration.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_SESSION_MANAGER_H_
