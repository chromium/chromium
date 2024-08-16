// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSIONS_CONSUMER_H_

#import <Foundation/Foundation.h>

@class PermissionInfo;

namespace web {
enum Permission : NSUInteger;
enum PermissionState : NSUInteger;
}  // namespace web

// Consumer for model to push configurations to the permissions UI.
@protocol PermissionsConsumer <NSObject>

// The list of permissions being displayed. The first NSNumber represents the
// `web::Permission` int value and the second its associated
// `web::PermissionState`.
- (void)setPermissionsInfo:(NSDictionary<NSNumber*, NSNumber*>*)permissionsInfo;

// Called when the state of given permission changed.
- (void)permissionStateChanged:(PermissionInfo*)info;

@optional

// The permissions description being displayed.
- (void)setPermissionsDescription:(NSString*)permissionsDescription;

@end

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSIONS_CONSUMER_H_
