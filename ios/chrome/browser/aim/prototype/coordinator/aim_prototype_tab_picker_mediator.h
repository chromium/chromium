// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_MEDIATOR_H_

#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/coordinator/base_grid_mediator.h"

// The tab picker mediator for AIM.
@interface AimPrototypeTabPickerMediator : BaseGridMediator

- (instancetype)initWithGridConsumer:(id<TabCollectionConsumer>)gridConsumer;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_TAB_PICKER_MEDIATOR_H_
