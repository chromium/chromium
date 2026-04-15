// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_MEDIATOR_H_

#import <set>

#import "ios/chrome/browser/tab_picker/ui/tab_picker_mutator.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/coordinator/base_grid_mediator.h"
#import "ios/web/public/web_state.h"

@protocol TabPickerLogger;
@class TabPickerMediator;
@protocol TabPickerConsumer;

// The tabs attachment delegate.
@protocol TabsAttachmentDelegate

// Returns the max number of tab attachments.
- (NSUInteger)maxTabAttachmentCount;

/// Sends the selected tabs identifiers to the tabs attachment delegate.
/// `cachedWebStateIDs` contains the IDs of the tabs that have their content
/// cached.
- (void)attachSelectedTabs:(TabPickerMediator*)tabPickerMediator
       selectedWebStateIDs:(std::set<web::WebStateID>)selectedWebStateIDs
         cachedWebStateIDs:(std::set<web::WebStateID>)cachedWebStateIDs;

/// Returns the web state IDs that are preselected.
- (std::set<web::WebStateID>)preselectedWebStateIDs;

@end

// The tab picker mediator.
@interface TabPickerMediator : BaseGridMediator <TabPickerMutator>

- (instancetype)initWithGridConsumer:(id<TabCollectionConsumer>)gridConsumer
                   tabPickerConsumer:(id<TabPickerConsumer>)tabPickerConsumer
              tabsAttachmentDelegate:
                  (id<TabsAttachmentDelegate>)tabsAttachmentDelegate;

// Delegate for logging events
@property(nonatomic, weak) id<TabPickerLogger> logger;

/// The mediator's delegate for attaching selected tabs.
@property(nonatomic, weak) id<TabsAttachmentDelegate> tabsAttachmentDelegate;

@end

#endif  // IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_MEDIATOR_H_
