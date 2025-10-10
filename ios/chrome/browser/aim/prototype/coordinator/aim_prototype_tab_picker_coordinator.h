// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_COORDINATOR_H_

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_mediator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// The tab picker coordinator for AIM.
@interface AimPrototypeTabPickerCoordinator
    : ChromeCoordinator <AimPrototypeTabsAttachmentDelegate>

// Returns `YES` if the coordinator is started.
@property(nonatomic, readonly) BOOL started;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_COORDINATOR_H_
