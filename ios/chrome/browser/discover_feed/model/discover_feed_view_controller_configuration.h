// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VIEW_CONTROLLER_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VIEW_CONTROLLER_CONFIGURATION_H_

#import <Foundation/Foundation.h>

class Browser;
@protocol DiscoverFeedManageDelegate;
@protocol DiscoverFeedPreviewDelegate;
@protocol FeedSignInPromoDelegate;
@protocol UIScrollViewDelegate;

// Configuration object used to create and configure a
// Discover Feed ViewController.
@interface DiscoverFeedViewControllerConfiguration : NSObject

// Browser used by Discover Feed ViewController.
@property(nonatomic, assign) Browser* browser;

// UIScrollViewDelegate used by Discover Feed ViewController.
@property(nonatomic, weak) id<UIScrollViewDelegate> scrollDelegate;

// DiscoverFeedPreviewDelegate used by Discover Feed ViewController.
@property(nonatomic, weak) id<DiscoverFeedPreviewDelegate> previewDelegate;

// DiscoverFeedManageDelegate used by Discover Feed ViewController.
@property(nonatomic, weak) id<DiscoverFeedManageDelegate> manageDelegate;

// FeedSignInPromoDelegate used by Discover Feed ViewController.
@property(nonatomic, weak) id<FeedSignInPromoDelegate> signInPromoDelegate;

@end

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VIEW_CONTROLLER_CONFIGURATION_H_
