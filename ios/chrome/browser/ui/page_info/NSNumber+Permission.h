// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_NSNUMBER_PERMISSION_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_NSNUMBER_PERMISSION_H_

#import <Foundation/Foundation.h>

namespace web {
enum class Permission;
}  // namespace web

// TODO(crbug.com/1286782): remove this category and use literal expressions in
// page_info_permissions_mediator.mm.
@interface NSNumber (Permission)

// NSNumber object from web::Permission.
+ (instancetype)cr_numberWithPermission:(web::Permission)permission;

// Underlying web::Permission value.
@property(readonly) web::Permission cr_permissionValue;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_NSNUMBER_PERMISSION_H_
