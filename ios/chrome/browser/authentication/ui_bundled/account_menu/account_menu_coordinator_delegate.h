// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_DELEGATE_H_
@class AccountMenuCoordinator;

@protocol AccountMenuCoordinatorDelegate <NSObject>

- (void)accountMenuCoordinatorWantsToBeStopped:
    (AccountMenuCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_DELEGATE_H_
