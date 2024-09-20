// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_CENTRAL_ACCOUNT_VIEW_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_CENTRAL_ACCOUNT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

struct ManagementState;

// View for the signed-in account, used in account settings page. Contains the
// following subviews:
// 1. Rounded avatarImage used for the account user picture. The value cannot be
// nil.
// 2. Name displayed in main label. The value can be nil.
// In case the value is nil, the main label will show the email and there will
// be no secondary label.
// 3. Email subtitle displayed in secondary label. The value cannot be nil.
@interface CentralAccountView : UIView

- (instancetype)initWithFrame:(CGRect)frame
                  avatarImage:(UIImage*)avatarImage
                         name:(NSString*)name
                        email:(NSString*)email
              managementState:(ManagementState)managementState
              useLargeMargins:(BOOL)useLargeMargins;

// Returns the view parameters.
- (UIImage*)avatarImage;
- (NSString*)name;
- (NSString*)email;
- (BOOL)managed;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_CENTRAL_ACCOUNT_VIEW_H_
