// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_MEDIATOR_H_

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"

@protocol AtMemoryConsumer;

@protocol ManualFillContentInjector;

@class AtMemorySearchItem;

// Mediator for AtMemory.
@interface AtMemoryMediator : NSObject <AtMemoryViewControllerDelegate>

// The consumer for this mediator.
@property(nonatomic, weak) id<AtMemoryConsumer> consumer;

// The content injector.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

// Sets the recent fills list for testing.
+ (void)setRecentFills:(NSArray<AtMemorySearchItem*>*)recentFills;

// The recent fills list.
@property(nonatomic, readonly, copy) NSArray<AtMemorySearchItem*>* recentFills;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_MEDIATOR_H_
