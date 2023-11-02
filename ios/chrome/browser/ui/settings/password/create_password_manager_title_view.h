// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_CREATE_PASSWORD_MANAGER_TITLE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_CREATE_PASSWORD_MANAGER_TITLE_VIEW_H_

@class BrandedNavigationItemTitleView;
@class NSString;

namespace password_manager {

// Creates a branded title view for the navigation bar of the Password Manager.
BrandedNavigationItemTitleView* CreatePasswordManagerTitleView(NSString* title);

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_CREATE_PASSWORD_MANAGER_TITLE_VIEW_H_
