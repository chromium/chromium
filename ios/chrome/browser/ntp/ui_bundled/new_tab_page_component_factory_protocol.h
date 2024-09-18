// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COMPONENT_FACTORY_PROTOCOL_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COMPONENT_FACTORY_PROTOCOL_H_

class Browser;
@class ContentSuggestionsCoordinator;
@class DiscoverFeedViewControllerConfiguration;
@class FeedHeaderViewController;
@class FeedMetricsRecorder;
@class FeedWrapperViewController;
@protocol FeedWrapperViewControllerDelegate;
typedef NS_ENUM(NSInteger, FollowingFeedSortType);
@class NewTabPageHeaderViewController;
@class NewTabPageMediator;
@class NewTabPageViewController;
@class UIViewController;
@protocol UserAccountImageUpdateDelegate;

@protocol NewTabPageComponentFactoryProtocol

// Coordinator for the ContentSuggestions.
- (ContentSuggestionsCoordinator*)contentSuggestionsCoordinatorForBrowser:
    (Browser*)browser;

// Metrics recorder for actions relating to the feed.
- (FeedMetricsRecorder*)feedMetricsRecorderForBrowser:(Browser*)browser;

// The header view controller containing the fake omnibox and logo.
- (NewTabPageHeaderViewController*)headerViewControllerForBrowser:
    (Browser*)browser;

// Mediator owned by the NewTabPageCoordinator
- (NewTabPageMediator*)NTPMediatorForBrowser:(Browser*)browser
                    identityDiscImageUpdater:
                        (id<UserAccountImageUpdateDelegate>)imageUpdater;

// View controller for the regular NTP.
- (NewTabPageViewController*)NTPViewController;

// Discover feed view controller.
- (UIViewController*)discoverFeedForBrowser:(Browser*)browser
                viewControllerConfiguration:
                    (DiscoverFeedViewControllerConfiguration*)
                        viewControllerConfiguration;

// Following feed view controller.
- (UIViewController*)followingFeedForBrowser:(Browser*)browser
                 viewControllerConfiguration:
                     (DiscoverFeedViewControllerConfiguration*)
                         viewControllerConfiguration
                                    sortType:(FollowingFeedSortType)sortType;

// Wrapper for the feed view controller.
- (FeedWrapperViewController*)
    feedWrapperViewControllerWithDelegate:
        (id<FeedWrapperViewControllerDelegate>)delegate
                       feedViewController:(UIViewController*)feedViewController;

// The header of the feed with the Following feed.
- (FeedHeaderViewController*)feedHeaderViewController;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COMPONENT_FACTORY_PROTOCOL_H_
