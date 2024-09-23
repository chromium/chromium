// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSION_INFO_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSION_INFO_H_

#import <Foundation/Foundation.h>

namespace web {
enum Permission : NSUInteger;
enum PermissionState : NSUInteger;
}  // namespace web

// Object that stores permission information.
@interface PermissionInfo : NSObject

@property(nonatomic, assign) web::Permission permission;
@property(nonatomic, assign) web::PermissionState state;

@end

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSION_INFO_H_
