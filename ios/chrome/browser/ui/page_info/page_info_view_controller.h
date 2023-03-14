// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/permissions/permissions_consumer.h"

@protocol PageInfoCommands;
@protocol PermissionsDelegate;

// View Controller for displaying the page info.
@interface PageInfoViewController
    : ChromeTableViewController <PermissionsConsumer,
                                 UIAdaptivePresentationControllerDelegate>

// Designated initializer.
- (instancetype)initWithSiteSecurityDescription:
                    (PageInfoSiteSecurityDescription*)siteSecurityDescription
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@property(nonatomic, weak) id<PageInfoCommands> pageInfoCommandsHandler;

// Delegate used to handle permission actions.
@property(nonatomic, weak) id<PermissionsDelegate> permissionsDelegate
    API_AVAILABLE(ios(15.0));

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
