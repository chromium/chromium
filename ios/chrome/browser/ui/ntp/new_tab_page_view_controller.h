// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class ContentSuggestionsViewController;
@class DiscoverFeedViewController;

// View controller containing all the content presented on a standard,
// non-incognito new tab page.
@interface NewTabPageViewController : UIViewController <UIScrollViewDelegate>

// Initializes view controller with NTP content view controllers.
// |discoverFeedViewController| represents the Discover feed for suggesting
// articles. |contentSuggestionsViewController| represents other content
// suggestions, such as the most visited site tiles.
- (instancetype)initWithDiscoverFeedViewController:
                    (DiscoverFeedViewController*)discoverFeedViewController
                  contentSuggestionsViewController:
                      (UICollectionViewController*)
                          contentSuggestionsViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
