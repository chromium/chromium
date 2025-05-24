// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_RECORDING_COMMAND_DISPATCHER_H_
#define IOS_CHROME_TEST_EARL_GREY_RECORDING_COMMAND_DISPATCHER_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

// A `CommandDispatcher` subclass that records all dispatches that are not
// otherwise handled.
@interface RecordingCommandDispatcher : CommandDispatcher

// A record of selectors which have been dispatched through this dispatcher.
@property(nonatomic, readonly) NSArray<NSString*>* dispatches;

// Executes the given `block` if a `selector` is dispatched.
- (void)setAction:(ProceduralBlock)block forSelector:(SEL)selector;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_RECORDING_COMMAND_DISPATCHER_H_
