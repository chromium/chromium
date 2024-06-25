// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_DELEGATE_H_

@class AccountMenuCoordinator;

// Delegate for the AccountMenu Coordinator
@protocol AccountMenuCoordinatorDelegate <NSObject>

// Requests the delegate to stop `coordinator`.
- (void)acountMenuCoordinatorShouldStop:(AccountMenuCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_DELEGATE_H_
