// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_COORDINATOR_H_

#include <set>

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_mediator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/web/public/web_state.h"

// Responsible for processing the selection of tab picker.
@protocol AimPrototypeTabPickerSelectionDelegate

// Returns the associated IDs for currently attached tabs.
- (std::set<web::WebStateID>)webStateIDsForAttachedTabs;

// Attaches the selected tabs with
- (void)attachSelectedTabsWithWebStateIDs:
    (std::set<web::WebStateID>)selectedWebStateIDs;

@end

// The tab picker coordinator for AIM.
@interface AimPrototypeTabPickerCoordinator
    : ChromeCoordinator <AimPrototypeTabsAttachmentDelegate>

// Returns `YES` if the coordinator is started.
@property(nonatomic, readonly) BOOL started;

@property(nonatomic, weak) id<AimPrototypeTabPickerSelectionDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_COORDINATOR_H_
