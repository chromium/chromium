// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller_delegate.h"

@class ContentSuggestionsViewController;
@class FeedHeaderViewController;
@class FeedMetricsRecorder;
@class FeedWrapperViewController;
typedef NS_ENUM(NSInteger, FeedLayoutUpdateType);
@protocol HelpCommands;
@class MagicStackCollectionViewController;
@protocol NewTabPageContentDelegate;
@class NewTabPageHeaderViewController;
@protocol NewTabPageMutator;
@protocol OverscrollActionsControllerDelegate;

// View controller containing all the content presented on a standard,
// non-incognito new tab page.
@interface NewTabPageViewController
    : UIViewController <NewTabPageConsumer,
                        NewTabPageHeaderViewControllerDelegate,
                        UIScrollViewDelegate>

// View controller wrapping the feed.
@property(nonatomic, strong)
    FeedWrapperViewController* feedWrapperViewController;

// Delegate for the overscroll actions.
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollDelegate;

// The NTP header, containing the fake omnibox and the doodle.
@property(nonatomic, weak) NewTabPageHeaderViewController* headerViewController;

// Delegate for actions relating to the NTP content.
@property(nonatomic, weak) id<NewTabPageContentDelegate> NTPContentDelegate;

// The view controller representing the content suggestions.
@property(nonatomic, strong)
    ContentSuggestionsViewController* contentSuggestionsViewController;

@property(nonatomic, strong)
    MagicStackCollectionViewController* magicStackCollectionView;

// Feed metrics recorder.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

// Whether or not the feed is visible.
@property(nonatomic, assign) BOOL feedVisible;

// The view controller representing the NTP feed header.
@property(nonatomic, weak) FeedHeaderViewController* feedHeaderViewController;

// The view controller representing the Feed top section (between the feed
// header and the feed collection).
@property(nonatomic, weak) UIViewController* feedTopSectionViewController;

// In-product help handle for displaying IPH bubbles relating to the NTP.
@property(nonatomic, weak) id<HelpCommands> helpHandler;

// Whether or not this NTP has fully appeared for the first time yet. This value
// remains YES if viewDidAppear has been called.
@property(nonatomic, assign) BOOL viewDidAppear;

// Whether the NTP should initially be scrolled into the feed.
@property(nonatomic, assign) BOOL shouldScrollIntoFeed;

// `YES` if the omnibox should be focused on when the view appears for voice
// over.
@property(nonatomic, assign) BOOL focusAccessibilityOmniboxWhenViewAppears;

// The mutator to provide updates to the NTP mediator.
@property(nonatomic, weak) id<NewTabPageMutator> mutator;

// Whether or not the fake omnibox is pinned to the top of the NTP.
@property(nonatomic, readonly) BOOL isFakeboxPinned;

// Layout guide for NTP modules.
@property(nonatomic, readonly) UILayoutGuide* moduleLayoutGuide;

// `YES` if the NTP is currently visible.
@property(nonatomic, assign) BOOL NTPVisible;

// Initializes the new tab page view controller.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Indicates to the receiver to update its state to focus the omnibox.
- (void)focusOmnibox;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// Lays out content above feed and adjusts content suggestions.
- (void)updateNTPLayout;

// Signals to the ViewController that the height above the feed needs to be
// recalculated. Usually called in response to an event that happens after
// all the content has been loaded (example: a UI element expanding). Keeps
// the scroll position from the top the same.
- (void)updateHeightAboveFeed;

// Returns whether the NTP is scrolled to the top or not.
- (BOOL)isNTPScrolledToTop;

// Lays out and re-configures the NTP content after changing the containing
// collection view, such as when changing feeds.
- (void)layoutContentInParentCollectionView;

// Resets hierarchy of views and view controllers.
- (void)resetViewHierarchy;

// Resets any relevant NTP states due for a content reload.
- (void)resetStateUponReload;

// Sets the feed collection contentOffset to the top of the page. Resets fake
// omnibox back to initial state.
- (void)setContentOffsetToTop;

// Sets the NTP collection view's scroll position to `contentOffset`, unless it
// is beyond the top of the feed. In that case, sets the scroll position to the
// top of the feed.
- (void)setContentOffsetToTopOfFeedOrLess:(CGFloat)contentOffset;

// Checks the content size of the feed and updates the bottom content inset to
// ensure the feed is still scrollable to the minimum height.
- (void)updateFeedInsetsForMinimumHeight;

// Updates the scroll position to account for the feed promo being removed.
- (void)updateScrollPositionForFeedTopSectionClosed;

// Signals that the feed has completed its updates (i.e. loading cards).
- (void)feedLayoutDidEndUpdatesWithType:(FeedLayoutUpdateType)type;

// Clears state and delegates.
- (void)invalidate;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
