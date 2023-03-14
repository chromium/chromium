// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COMMAND_DISPATCHER_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COMMAND_DISPATCHER_H_

#import <Foundation/Foundation.h>

// CommandDispatcher allows coordinators to register as command handlers for
// specific selectors.  Other objects can call these methods on the dispatcher,
// which in turn will forward the call to the registered handler.

// While CommandDispatcher conforms functionally to any protocols it is
// dispatching for, the compiler doesn't (and can't) know that. To call
// dispatched methods on a dispatcher, it's best to use a typecast to an
// id pointer conforming to the relevant protocols. Such a pointer should also
// be passed into objects that need to call, but not configure, the dispatcher
// (anything other than a coordinator). To create such a pointer in a way that
// both compiles and is checked for correctness at runtime, use the provided
// function-like macro HandlerForProtocol, defined below. Usage is as follows:
//
//   id<SomeProtocol> handler = HandlerForProtocol(dispatcher, SomeProtocol);
//
//     `dispatcher` should be a CommandDispatcher object, and SomeProtocol is
//     the *name* of a protocol (not a string, not a Protocol* pointer, and not
//     an @protocol() expression to generate one).
//
// This will typecast `dispatcher` to an id<SomeProtocol> (for compile-time
// type checking), and verify that `dispatcher` is currently dispatching
// for `protocol` (for run-time verification). If `dispatcher` isn't dispatching
// for `protocol`, HandlerForProtocol() returns nil and DCHECKs.
//
#define HandlerForProtocol(Dispatcher, ProtocolName) \
  static_cast<id<ProtocolName>>(                     \
      [Dispatcher strictCallableForProtocol:@protocol(ProtocolName)])

@interface CommandDispatcher : NSObject

// Registers the given `target` to receive forwarded messages for the given
// `selector`.
- (void)startDispatchingToTarget:(id)target forSelector:(SEL)selector;

// Removes forwarding registration for the given `selector`.
- (void)stopDispatchingForSelector:(SEL)selector;

// Registers the given `target` to receive forwarded messages for the methods of
// the given `protocol`. Only required instance methods are registered. The
// other definitions in the protocol are ignored.
- (void)startDispatchingToTarget:(id)target forProtocol:(Protocol*)protocol;

// Removes forwarding registration for the given `selector`. Only dispatching to
// required instance methods is removed. The other definitions in the protocol
// are ignored.
- (void)stopDispatchingForProtocol:(Protocol*)protocol;

// Removes all forwarding registrations for the given `target`.
- (void)stopDispatchingToTarget:(id)target;

// YES if the dispatcher is currently dispatching for `protocol`, including
// (recursively) any protocols that `protocol` conforms to.
- (BOOL)dispatchingForProtocol:(Protocol*)protocol;

// Returns the receiver if it is dispatching for `protocol`, and CHECK()s
// otherwise.
- (CommandDispatcher*)strictCallableForProtocol:(Protocol*)protocol;

// After this method is called, -stopDispatching methods will stop dispatching,
// but this object will continue to respond to registered selectors by silently
// failing. This method should be called on -applicationWillTerminate. It helps
// avoid untangling the dispatcher chains in the correct order, which sometimes
// can be very hard.
- (void)prepareForShutdown;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COMMAND_DISPATCHER_H_
