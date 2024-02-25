// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SECURITY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SECURITY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"

@protocol PageInfoCommands;
@protocol PageInfoPresentationCommands;

// View Controller for displaying the security subpage of page info.
@interface PageInfoSecurityViewController
    : ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// Handler for actions related to the entire Page Info UI such as showing or
// dismissing the entire UI.
@property(nonatomic, weak) id<PageInfoCommands> pageInfoCommandsHandler;

// Handler for actions within the Page Info UI.
@property(nonatomic, weak) id<PageInfoPresentationCommands>
    pageInfoPresentationHandler;

// Designated initializer.
- (instancetype)initWithSiteSecurityDescription:
    (PageInfoSiteSecurityDescription*)siteSecurityDescription
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SECURITY_VIEW_CONTROLLER_H_
