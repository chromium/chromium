// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

@class PermissionInfo;

namespace web {
enum Permission : NSUInteger;
enum PermissionState : NSUInteger;
}  // namespace web

// Consumer for model to push configurations to the permissions UI.
@protocol InfobarPermissionsModalConsumer <NSObject>

// The permissions description being displayed in the InfobarModal.
- (void)setPermissionsDescription:(NSString*)permissionsDescription;

// The list of permission being displayed in the InfobarModal.
- (void)setPermissionsInfo:(NSArray<PermissionInfo*>*)permissionsInfo;

// Called when the state of given permission changed.
- (void)permissionStateChanged:(PermissionInfo*)info;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_MODAL_CONSUMER_H_
