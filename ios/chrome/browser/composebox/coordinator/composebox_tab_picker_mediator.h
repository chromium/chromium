// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_TAB_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_TAB_PICKER_MEDIATOR_H_

#import <set>

#import "ios/chrome/browser/composebox/ui/composebox_tab_picker_mutator.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/coordinator/base_grid_mediator.h"
#import "ios/web/public/web_state.h"

@class ComposeboxTabPickerMediator;
@protocol ComposeboxTabPickerConsumer;

// The tabs attachment delegate.
@protocol ComposeboxTabsAttachmentDelegate

// Returns the number of non-tab attachments.
- (NSUInteger)nonTabAttachmentCount;

/// Sends the selected tabs identifiers to the tabs attachment delegate.
/// `cachedWebStateIDs` contains the IDs of the tabs that have their content
/// cached.
- (void)attachSelectedTabs:(ComposeboxTabPickerMediator*)tabPickerMediator
       selectedWebStateIDs:(std::set<web::WebStateID>)selectedWebStateIDs
         cachedWebStateIDs:(std::set<web::WebStateID>)cachedWebStateIDs;

/// Returns the web state IDs that are preselected.
- (std::set<web::WebStateID>)preselectedWebStateIDs;

@end

// The tab picker mediator for AIM.
@interface ComposeboxTabPickerMediator
    : BaseGridMediator <ComposeboxTabPickerMutator>

- (instancetype)initWithGridConsumer:(id<TabCollectionConsumer>)gridConsumer
                   tabPickerConsumer:
                       (id<ComposeboxTabPickerConsumer>)tabPickerConsumer
              tabsAttachmentDelegate:
                  (id<ComposeboxTabsAttachmentDelegate>)tabsAttachmentDelegate;

/// The mediator's delegate for attaching selected tabs.
@property(nonatomic, weak) id<ComposeboxTabsAttachmentDelegate>
    tabsAttachmentDelegate;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_TAB_PICKER_MEDIATOR_H_
