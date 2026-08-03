// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_GRANULAR_FILL_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_GRANULAR_FILL_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol AtMemoryFillCommands;

// Mediator for AtMemory granular fill.
@interface AtMemoryGranularFillMediator : NSObject

// Handler for filling commands.
@property(nonatomic, weak) id<AtMemoryFillCommands> fillHandler;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_GRANULAR_FILL_MEDIATOR_H_
