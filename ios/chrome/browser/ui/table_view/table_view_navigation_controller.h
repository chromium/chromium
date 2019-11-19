// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class ChromeTableViewController;

// TableViewNavigationController encapsulates a ChromeTableViewController inside
// a UINavigationController.
@interface TableViewNavigationController : UINavigationController

- (instancetype)initWithTable:(ChromeTableViewController*)table
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithRootViewController:(UIViewController*)rootViewController
    NS_UNAVAILABLE;
- (instancetype)initWithNavigationBarClass:(Class)navigationBarClass
                              toolbarClass:(Class)toolbarClass NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// The ChromeTableViewController owned by this ViewController.
@property(nonatomic, readonly, weak)
    ChromeTableViewController* tableViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_NAVIGATION_CONTROLLER_H_
