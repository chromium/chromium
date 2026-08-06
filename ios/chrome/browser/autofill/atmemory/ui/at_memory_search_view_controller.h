// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol AtMemoryCommands;
@protocol AtMemorySearchResultCommands;

// View controller for AtMemory search.
@interface AtMemorySearchViewController
    : ChromeTableViewController <AtMemorySearchConsumer>

// Handler for actions related to the AtMemory search results.
@property(nonatomic, weak) id<AtMemorySearchResultCommands> searchResultHandler;

// Handler for AtMemory commands.
@property(nonatomic, weak) id<AtMemoryCommands> atMemoryHandler;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_VIEW_CONTROLLER_H_
