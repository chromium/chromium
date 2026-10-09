// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"

namespace ttc {
class TtcBackend;
}  // namespace ttc

class TtcConversation;
@protocol TTCSessionControllerObserver;

// Coordinates the lifecycle, observer notifications, and backgrounding teardown
// for an active TTC session on iOS.
@interface TTCSessionController : NSObject

// Underlying conversation coordinator.
@property(nonatomic, readonly) TtcConversation* conversation;

// Current lifecycle state of this session.
@property(nonatomic, readonly, assign) TTCSessionLifecycle lifecycle;

// Designated initializer injecting a custom conversation coordinator.
- (instancetype)initWithConversation:
    (std::unique_ptr<TtcConversation>)conversation NS_DESIGNATED_INITIALIZER;

// Convenience initializer creating a `TtcConversation` with `backend`.
- (instancetype)initWithBackend:(std::unique_ptr<ttc::TtcBackend>)backend;

// Convenience initializer using a default `TtcConversation`.
- (instancetype)init;

#pragma mark - Session Lifecycle

// Starts the session.
- (void)startSession;

// Marks the session as initialized and transitions lifecycle to `kLive`.
- (void)onSessionInitialized;

// Transitions the session lifecycle to `kFinished`.
- (void)stopSession;

// Stops the session, unregisters background observers, and removes all
// registered observers.
- (void)disconnect;

#pragma mark - Audio & Errors

// Notifies observers that the user's audio input level has updated.
// `audioLevel` is normalized in the range [0.0, 1.0].
- (void)userAudioLevelDidUpdate:(float)audioLevel;

// Notifies observers that a session error occurred.
- (void)failWithError:(NSError*)error;

#pragma mark - Observers

// Adds an observer to receive session events.
- (void)addObserver:(id<TTCSessionControllerObserver>)observer;

// Removes an observer from receiving session events.
- (void)removeObserver:(id<TTCSessionControllerObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_H_
