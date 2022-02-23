// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol BrowserCommands;

@protocol PageInfoViewControllerPermissionsDelegate;

// View Controller for displaying the page info.
@interface PageInfoViewController
    : ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// Designated initializer.
- (instancetype)initWithSiteSecurityDescription:
                    (PageInfoSiteSecurityDescription*)siteSecurityDescription
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Handler used to navigate outside the page info.
@property(nonatomic, weak) id<BrowserCommands> handler;

// Delegate used to get current states for permissions and to respond to user
// toggling the switch for a permission.
@property(nonatomic, weak) id<PageInfoViewControllerPermissionsDelegate>
    permissionsDelegate API_AVAILABLE(ios(15.0));

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
