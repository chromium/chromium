// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// ViewController for the feed preview. It displays a loaded webState UIView.
@interface DiscoverFeedPreviewViewController : UIViewController

// Inits the view controller with |webStateView|.
- (instancetype)initWithView:(UIView*)webStateView NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNAme
                         bundle:(NSBundle*)nibBundle NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_VIEW_CONTROLLER_H_
