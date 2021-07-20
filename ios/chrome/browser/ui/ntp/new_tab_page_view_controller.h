// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_controlling.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_supporting.h"

@class ContentSuggestionsHeaderViewController;
@class ContentSuggestionsViewController;
@class DiscoverFeedMetricsRecorder;
@class DiscoverFeedWrapperViewController;
@protocol NewTabPageContentDelegate;
@protocol OverscrollActionsControllerDelegate;
@class ViewRevealingVerticalPanHandler;

// View controller containing all the content presented on a standard,
// non-incognito new tab page.
@interface NewTabPageViewController
    : UIViewController <ContentSuggestionsCollectionControlling,
                        ThumbStripSupporting,
                        UIScrollViewDelegate>

// View controller wrapping the Discover feed.
@property(nonatomic, strong)
    DiscoverFeedWrapperViewController* discoverFeedWrapperViewController;

// Delegate for the overscroll actions.
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollDelegate;

// The content suggestions header, containing the fake omnibox and the doodle.
@property(nonatomic, weak)
    ContentSuggestionsHeaderViewController* headerController;

// Delegate for actions relating to the NTP content.
@property(nonatomic, weak) id<NewTabPageContentDelegate> ntpContentDelegate;

// The pan gesture handler to notify of scroll events happening in this view
// controller.
@property(nonatomic, weak) ViewRevealingVerticalPanHandler* panGestureHandler;

// Identity disc shown in the NTP.
// TODO(crbug.com/1170995): Remove once the Feed header properly supports
// ContentSuggestions.
@property(nonatomic, weak) UIButton* identityDiscButton;

// View controller representing the NTP content suggestions. These suggestions
// include the most visited site tiles, the shortcut tiles, the fake omnibox and
// the Google doodle.
@property(nonatomic, strong)
    UICollectionViewController* contentSuggestionsViewController;

// Discover Feed metrics recorder.
@property(nonatomic, strong)
    DiscoverFeedMetricsRecorder* discoverFeedMetricsRecorder;

// Initializes the new tab page view controller.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// Stops scrolling in the scroll view.
- (void)stopScrolling;

// Sets the feed collection contentOffset from the saved state to |offset| to
// set the initial scroll position.
- (void)setSavedContentOffset:(CGFloat)offset;

// Sets the feed collection contentOffset to the top of the page.
- (void)setContentOffsetToTop;

// Updates the ContentSuggestionsViewController and its header for the current
// layout.
// TODO(crbug.com/1170995): Remove once ContentSuggestions can be added as part
// of a header.
- (void)updateContentSuggestionForCurrentLayout;

// Returns the current height of the content suggestions content.
- (CGFloat)contentSuggestionsContentHeight;

// Scrolls up the collection view enough to focus the omnibox.
- (void)focusFakebox;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
