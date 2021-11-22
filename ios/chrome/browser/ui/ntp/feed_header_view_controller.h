// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@interface FeedHeaderViewController : UIViewController

// Button for opening top-level feed menu.
@property(nonatomic, readonly, strong) UIButton* menuButton;

// The base title string of the feed header, excluding modifiers.
@property(nonatomic, copy) NSString* titleText;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_HEADER_VIEW_CONTROLLER_H_
