// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_SESSION_MANAGER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_SESSION_MANAGER_H_

#import <Foundation/Foundation.h>

@class AVAudioEngine;
@class AVAudioSessionPortDescription;
@protocol TTCAudioSessionManagerDelegate;

// Domain for errors originated by TTCAudioSessionManager.
extern NSString* const kTTCAudioSessionManagerErrorDomain;

// Error codes for TTCAudioSessionManager.
enum class TTCAudioSessionManagerErrorCode : NSInteger {
  kCancelled = -1,
};

// Audio output destination options for TalkToChrome.
enum class TTCAudioOutputDestination : NSInteger {
  // Built-in bottom loudspeaker.
  kSpeaker = 0,
  // Built-in top handset receiver (earpiece).
  kEarpiece,
  // Connected external audio accessory (Bluetooth HFP/A2DP or wired
  // headphones).
  kExternal,
};

// Manages the AVAudioSession lifecycle for TalkToChrome, encapsulating
// category activation, option configuration, category restoration upon
// teardown, route inspection, and port selection.
@interface TTCAudioSessionManager : NSObject

// Delegate receiving session interruption and lifecycle events.
@property(nonatomic, weak) id<TTCAudioSessionManagerDelegate> delegate;

// Whether the currently active audio input hardware supports voice call
// processing (hardware Acoustic Echo Cancellation).
@property(nonatomic, readonly) BOOL hasHardwareAEC;

// The currently selected audio output destination.
@property(nonatomic, readonly) TTCAudioOutputDestination outputDestination;

// Whether an external audio output accessory (Bluetooth, AirPods, wired
// headphones, AirPlay) is currently connected to the device.
@property(nonatomic, readonly) BOOL isExternalOutputConnected;

// Name of the connected external audio output accessory (e.g. @"AirPods Pro"),
// or nil if no external output accessory is connected.
@property(nonatomic, readonly) NSString* externalOutputDeviceName;

// Human-readable name of the current active audio input port (e.g. "iPhone
// Microphone", "AirPods"). Returns nil if no input port is available.
@property(nonatomic, readonly) NSString* activeInputRouteName;

// Human-readable name of the current active audio output port (e.g. "Speaker",
// "Headphones"). Returns nil if no output port is available.
@property(nonatomic, readonly) NSString* activeOutputRouteName;

// List of all audio input ports currently available on the device.
@property(nonatomic, readonly)
    NSArray<AVAudioSessionPortDescription*>* availableInputs;

// The currently selected preferred input port, or nil if using system default
// routing.
@property(nonatomic, readonly) AVAudioSessionPortDescription* preferredInput;

// Configures and activates the AVAudioSession synchronously on the caller's
// thread with PlayAndRecord category, applying mode and options for
// `outputDestination`. Returns nil on success or the NSError encountered
// during category or activation configuration.
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
// active prior to TalkToChrome configuration, clears port overrides, and
// deactivates the session notifying other audio apps.
- (void)restoreAudioSessionCategory;

// Selects a preferred audio input port. Pass nil to restore system default
// routing. Returns YES on success.
// @param port The port description to prioritize for audio input, or nil.
// @param error Populated with any error encountered while updating the session.
- (BOOL)setPreferredInput:(AVAudioSessionPortDescription*)port
                    error:(NSError**)error;

// Asynchronously sets the preferred audio input port on a background queue
// to prevent main thread stalls during mediaserverd IPC. Invokes `completion`
// on the UI thread.
// @param port The port description to prioritize for audio input, or nil.
// @param completion Block invoked on the UI thread with success status and any
// error.
- (void)setPreferredInput:(AVAudioSessionPortDescription*)port
               completion:(void (^)(BOOL success, NSError* error))completion;

// Asynchronously sets the audio output destination on a background queue
// to prevent main thread stalls during CoreAudio IPC. Dispatches `completion`
// on the UI thread once applied.
// @param destination The target audio output destination.
// @param completion Block invoked on the UI thread with success status and any
// error.
- (void)setOutputDestination:(TTCAudioOutputDestination)destination
                  completion:(void (^)(BOOL success, NSError* error))completion;

// Sets the audio output destination synchronously. Returns YES on success.
// Prefer `setOutputDestination:completion:` on the UI thread to prevent stalls.
// @param destination The target audio output destination.
// @param error Populated with any error encountered while updating the session.
- (BOOL)setOutputDestination:(TTCAudioOutputDestination)destination
                       error:(NSError**)error;

// Asynchronously forces audio output to the built-in speaker on a background
// queue.
// @param forceSpeaker YES to route to loudspeaker; NO to route to external
// accessory if connected or handset earpiece.
// @param completion Block invoked on the UI thread with the result.
- (void)setOutputOverriddenToSpeaker:(BOOL)forceSpeaker
                          completion:(void (^)(BOOL success,
                                               NSError* error))completion;

// Explicitly forces audio output to the built-in speaker even when external
// accessories are connected. Pass NO to restore natural routing.
// @param forceSpeaker YES to route to loudspeaker; NO to route to external
// accessory if connected or handset earpiece.
// @param error Populated with any error encountered while updating the session.
- (BOOL)setOutputOverriddenToSpeaker:(BOOL)forceSpeaker error:(NSError**)error;

// Registers notification observers for session route changes and interruptions.
// If `engine` is non-nil, also registers for engine configuration changes.
// Calling this method unregisters any previously registered observers.
// @param engine Audio engine to observe for configuration changes, or nil if
//     only session-level notifications are needed.
- (void)registerNotificationObserversWithAudioEngine:(AVAudioEngine*)engine;

// Tears down the audio session manager, cancelling any in-flight startup tasks
// and restoring the prior audio session configuration.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_AUDIO_SESSION_MANAGER_H_
