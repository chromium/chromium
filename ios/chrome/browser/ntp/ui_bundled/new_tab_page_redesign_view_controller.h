// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_REDESIGN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_REDESIGN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_delegate.h"

@class NewTabPageHeaderView;
@protocol NewTabPageMutator;
@protocol NewTabPageContentDelegate;

// View controller shell for the New Tab Page Redesign.
@interface NewTabPageRedesignViewController
    : UIViewController <NewTabPageConsumer,
                        NewTabPageHeaderViewDelegate,
                        SearchEngineLogoConsumer>

// Delegate for actions relating to the NTP content.
@property(nonatomic, weak) id<NewTabPageContentDelegate> NTPContentDelegate;

// The mutator to provide updates to the NTP mediator.
@property(nonatomic, weak) id<NewTabPageMutator> mutator;

// The search engine/Doodle logo view.
@property(nonatomic, strong) UIView* searchEngineLogoView;

// The Most Visited Tiles (MVTs) view controller.
@property(nonatomic, strong) UIViewController* mostVisitedViewController;

// Sets the feed view controller to embed in the redesign bottom sheet.
- (void)setFeedViewController:(UIViewController*)feedViewController;

// `YES` if the omnibox should be focused on when the view appears for voice
// over.
@property(nonatomic, assign) BOOL focusAccessibilityOmniboxWhenViewAppears;

// Whether this NTP has fully appeared.
@property(nonatomic, assign) BOOL viewDidAppear;

// Properties conformed to by NewTabPageConsumer.
@property(nonatomic, assign) BOOL mostVisitedVisible;
@property(nonatomic, assign) BOOL magicStackVisible;

// Indicates to the receiver to update its state to focus the omnibox.
- (void)focusOmnibox;

// Clears state and delegates.
- (void)invalidate;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_REDESIGN_VIEW_CONTROLLER_H_
