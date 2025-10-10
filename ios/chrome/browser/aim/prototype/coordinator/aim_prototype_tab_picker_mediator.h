// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_MEDIATOR_H_

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_mutator.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/coordinator/base_grid_mediator.h"

@class AimPrototypeTabPickerMediator;
@protocol AimPrototypeTabPickerConsumer;

// The tabs attachment delegate.
@protocol AimPrototypeTabsAttachmentDelegate

/// Sends the selected tabs identifiers to the tabs attachment delegate.
- (void)attachSelectedTabs:(AimPrototypeTabPickerMediator*)tabPickerMediator
       selectedIdentifiers:(NSSet<GridItemIdentifier*>*)selectedIdentifiers;

@end

// The tab picker mediator for AIM.
@interface AimPrototypeTabPickerMediator
    : BaseGridMediator <AimPrototypeTabPickerMutator>

- (instancetype)
      initWithGridConsumer:(id<TabCollectionConsumer>)gridConsumer
         tabPickerConsumer:(id<AimPrototypeTabPickerConsumer>)tabPickerConsumer
    tabsAttachmentDelegate:
        (id<AimPrototypeTabsAttachmentDelegate>)tabsAttachmentDelegate;

/// The mediator's delegate for attaching selected tabs.
@property(nonatomic, weak) id<AimPrototypeTabsAttachmentDelegate>
    tabsAttachmentDelegate;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_MEDIATOR_H_
