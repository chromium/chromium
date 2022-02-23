// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_MODAL_DELEGATE_H_

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

@class PermissionInfo;

// Delegate to handle permissions modal actions.
API_AVAILABLE(ios(15.0))
@protocol InfobarPermissionsModalDelegate <InfobarModalDelegate>

// Method invoked when the user taps a switch.
- (void)updateStateForPermission:(PermissionInfo*)permissionDescription;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_MODAL_DELEGATE_H_
