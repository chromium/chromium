// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_POSTED_OBSERVER_LIST_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_POSTED_OBSERVER_LIST_H_

#import <Foundation/Foundation.h>

// Holds weak references to observers of a single protocol and notifies them
// asynchronously, on the sequence where the list was created. Inspired by
// `base::ObserverListThreadSafe`, restricted to a single sequence.
//
// - Notifications are delivered in posting order, never synchronously from
//   `-postNotification:`.
// - An observer receives exactly the notifications posted after
//   `-addObserver:` and not yet delivered when `-removeObserver:` is called.
//   Re-adding starts a fresh registration.
// - Observers may add or remove themselves or others from within a
//   notification.
// - Pending registrations and notifications are still delivered after the
//   owner releases the list.
//
// `ObserverType` (e.g. `id<MyObserver>`) must match the protocol passed to
// `-initWithProtocol:`. Must be created and used on a single sequence.
@interface PostedObserverList<ObserverType> : NSObject

// Initializes an empty list for observers conforming to `protocol`.
- (instancetype)initWithProtocol:(Protocol*)protocol NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Registers `observer` for the notifications posted after this call. No-op if
// `observer` is already registered or pending.
- (void)addObserver:(ObserverType)observer;

// Same as `-addObserver:`, and calls `onRegistered` with `observer` before any
// notification posted after this call, e.g. to send it a snapshot of the state
// at call time. `onRegistered` must not capture the list's owner, which may be
// destroyed before it runs.
- (void)addObserver:(ObserverType)observer
       onRegistered:(void (^)(ObserverType observer))onRegistered;

// Unregisters `observer`, cancelling its pending registration and
// notifications. No-op if `observer` is nil.
- (void)removeObserver:(ObserverType)observer;

// Posts `notification`, which is called with `observers`, a proxy forwarding
// each protocol message to the observers added before this call and not
// removed since. `notification` must not capture the list's owner, which may be
// destroyed before it runs.
- (void)postNotification:(void (^)(ObserverType observers))notification;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_POSTED_OBSERVER_LIST_H_
