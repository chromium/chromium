// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FOLLOW_FIRST_FOLLOW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FOLLOW_FIRST_FOLLOW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

class FaviconLoader;
@protocol FirstFollowViewDelegate;
@class FollowedWebChannel;

// The UI that informs the user about the feed and following channels the
// first few times the user follows any channel.
@interface FirstFollowViewController : UIViewController

// The web channel that was recently followed.
@property(nonatomic, strong) FollowedWebChannel* followedWebChannel;

// Delegate to execute actions triggered in this UI.
@property(nonatomic, weak) id<FirstFollowViewDelegate> delegate;

// FaviconLoader retrieves favicons for a given page URL.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

@end

#endif  // IOS_CHROME_BROWSER_UI_FOLLOW_FIRST_FOLLOW_VIEW_CONTROLLER_H_
