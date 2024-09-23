// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSIONS_DELEGATE_H_

#import <Foundation/Foundation.h>

@class PermissionInfo;

// Delegate to handle permissions actions.
@protocol PermissionsDelegate <NSObject>

// Method invoked when the user taps a switch.
- (void)updateStateForPermission:(PermissionInfo*)permissionDescription;

@end

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSIONS_DELEGATE_H_
