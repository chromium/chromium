// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCASTER_H_
#define IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCASTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer.h"

// An interface for lightweight synchronization of object properties; it is
// generally intended to allow properties of UI-layer objects (typically view
// controllers) to be observed without the observer needing to know the identity
// of the observed object.
//
// ChromeBroadcaster is not intended to be able to be used for arbitrary
// property observation; rather there is a defined protocol (BroadcastObserver)
// of observer methods which are associated with broadcasting objects.
//
// (The class is named 'ChromeBroadcaster' to avoid various symbol conflicts
// that the terser name 'Broadcaster' creates, but associated classes and
// properties will refer to instances of this class as just a 'Broadcaster' for
// simplicity.)
@interface ChromeBroadcaster : NSObject

// Makes the value (property) of `object` identified by `valueKey` observable
// via `selector`. It is an error if `selector` is not defined in the
// BroadcastObserver protocol, or if a value is already being broadcast
// for `selector`.
// If there are already observers for `selector`, they will have their observer
// methods called immediately with the current broadcast value, before this
// method returns.
- (void)broadcastValue:(NSString*)valueKey
              ofObject:(NSObject*)object
              selector:(SEL)selector;

// Stop broadcasting for `selector`. This doesn't remove or change any
// observers for that selector. If `selector` is not being broadcast, this
// method does nothing.
- (void)stopBroadcastingForSelector:(SEL)selector;

// Adds `observer` as an observer for `selector`. If `selector` is already being
// broadcast, `selector` will be called on `observer` with the current value of
// the broadcast property before this method returns.
// It is an error if `selector` is not one of the methods in the
// BroadcastObserver protocol, or if `observer` does not respond to `selector`.
- (void)addObserver:(id<ChromeBroadcastObserver>)observer
        forSelector:(SEL)selector;

// Removes `observer` from the observers for `selector`. If `observer` is also
// an observer for another selector, this method will not change that.
// It is an error if `selector` is not one of the methods in the
// BroadcastObserver protocol.
- (void)removeObserver:(id<ChromeBroadcastObserver>)observer
           forSelector:(SEL)selector;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCASTER_H_
