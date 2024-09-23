// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/permissions/ui_bundled/permissions_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_about_this_site_consumer.h"
#import "ios/chrome/browser/ui/page_info/page_info_history_consumer.h"
#import "ios/chrome/browser/ui/page_info/page_info_presentation_commands.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"

@protocol PageInfoCommands;
@protocol PageInfoPresentationCommands;
@protocol PermissionsDelegate;

// View Controller for displaying the page info.
@interface PageInfoViewController
    : ChromeTableViewController <PageInfoAboutThisSiteConsumer,
                                 PageInfoHistoryConsumer,
                                 PermissionsConsumer,
                                 UIAdaptivePresentationControllerDelegate>

// Designated initializer.
- (instancetype)initWithSiteSecurityDescription:
                    (PageInfoSiteSecurityDescription*)siteSecurityDescription
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Handler for actions related to the entire Page Info UI such as showing or
// dismissing the entire UI.
@property(nonatomic, weak) id<PageInfoCommands> pageInfoCommandsHandler;

// Handler for actions within the Page Info UI.
@property(nonatomic, weak) id<PageInfoPresentationCommands>
    pageInfoPresentationHandler;

// Delegate used to handle permission actions.
@property(nonatomic, weak) id<PermissionsDelegate> permissionsDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
