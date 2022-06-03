// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ACCESSORY_TOOLBAR_ACCESSORY_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ACCESSORY_TOOLBAR_ACCESSORY_COORDINATOR_DELEGATE_H_

@class ChromeCoordinator;

@protocol ToolbarAccessoryCoordinatorDelegate

- (void)toolbarAccessoryCoordinatorDidDismissUI:
    (ChromeCoordinator*)toolbarAccessoryCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ACCESSORY_TOOLBAR_ACCESSORY_COORDINATOR_DELEGATE_H_
