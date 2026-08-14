// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol AtMemoryCommands;
@protocol AtMemoryGranularFillMutator;

// View controller that displays the details page with tap-to-fill chips for
// AtMemory granular fill.
@interface AtMemoryGranularFillViewController
    : ChromeTableViewController <AtMemoryGranularFillConsumer>

// Mutator for user actions on this view controller.
@property(nonatomic, weak) id<AtMemoryGranularFillMutator> mutator;

// Handler for AtMemory commands.
@property(nonatomic, weak) id<AtMemoryCommands> atMemoryHandler;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_VIEW_CONTROLLER_H_
