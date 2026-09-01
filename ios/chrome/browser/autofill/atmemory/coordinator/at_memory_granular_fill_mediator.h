// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_GRANULAR_FILL_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_GRANULAR_FILL_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_mutator.h"

@protocol AtMemoryCommands;
@protocol AtMemoryFillCommands;
@protocol AtMemoryGranularFillConsumer;

namespace autofill {
struct Suggestion;
}

// Mediator for AtMemory granular fill.
@interface AtMemoryGranularFillMediator : NSObject <AtMemoryGranularFillMutator>

// The consumer for this mediator.
@property(nonatomic, weak) id<AtMemoryGranularFillConsumer> consumer;

// Handler for filling commands.
@property(nonatomic, weak) id<AtMemoryFillCommands> fillHandler;

// Handler for AtMemory commands.
@property(nonatomic, weak) id<AtMemoryCommands> atMemoryHandler;

// Initializes the mediator by moving `suggestion`.
- (instancetype)initWithSuggestion:(autofill::Suggestion&&)suggestion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_GRANULAR_FILL_MEDIATOR_H_
