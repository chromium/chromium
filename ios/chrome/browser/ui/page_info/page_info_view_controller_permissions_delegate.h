// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_PERMISSIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_PERMISSIONS_DELEGATE_H_

#import <Foundation/Foundation.h>

namespace web {
enum class Permission;
}  // namespace web

// Protocol used to display permissions section of the page info screen.
API_AVAILABLE(ios(15.0))
@protocol PageInfoViewControllerPermissionsDelegate <NSObject>

// Method to check if the permissions section should be shown on the view
// controller.
- (BOOL)shouldShowPermissionsSection;

// Method to check if the web page has access to the |permission|. The view will
// only show toggles for accessible permissions.
- (BOOL)isPermissionAccessible:(web::Permission)permission;

// Method to retrieve the on/off state of an accessible permission. Note that it
// does NOT work for inaccessible permissions.
- (BOOL)stateForAccessiblePermission:(web::Permission)permission;

// Method invoked when the user taps a switch.
- (void)toggleStateForPermission:(web::Permission)permission;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_PERMISSIONS_DELEGATE_H_
