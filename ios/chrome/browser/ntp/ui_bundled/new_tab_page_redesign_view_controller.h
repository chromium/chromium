// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_REDESIGN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_REDESIGN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_consumer.h"
#import "ios/chrome/browser/content_suggestions/ui/user_account_image_update_delegate.h"
#import "ios/chrome/browser/location_bar/ui_bundled/fakebox_buttons_snapshot_provider.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_delegate.h"

@class LayoutGuideCenter;
@class NewTabPageHeaderView;
@protocol NewTabPageMutator;
@protocol NewTabPageContentDelegate;
@protocol NewTabPageHeaderCommands;
@protocol NewTabPageShortcutsHandler;

// View controller shell for the New Tab Page Redesign.
@interface NewTabPageRedesignViewController
    : UIViewController <ContentSuggestionsConsumer,
                        FakeboxButtonsSnapshotProvider,
                        NewTabPageConsumer,
                        NewTabPageHeaderConsumer,
                        NewTabPageHeaderViewDelegate,
                        SearchEngineLogoConsumer,
                        UserAccountImageUpdateDelegate>

// Handler for header commands.
@property(nonatomic, weak) id<NewTabPageHeaderCommands> headerCommandsHandler;

// Layout guide center for referencing views.
@property(nonatomic, weak) LayoutGuideCenter* layoutGuideCenter;

// Delegate for actions relating to the NTP content.
@property(nonatomic, weak) id<NewTabPageContentDelegate> NTPContentDelegate;

// The mutator to provide updates to the NTP mediator.
@property(nonatomic, weak) id<NewTabPageMutator> mutator;

// Handles the actions for the NTP shortcuts, like Lens or voice search.
@property(nonatomic, weak) id<NewTabPageShortcutsHandler> NTPShortcutsHandler;

// Indicates whether the lens button should use the "New" badge.
@property(nonatomic, assign) BOOL useNewBadgeForLensButton;

// The search engine/Doodle logo view.
@property(nonatomic, strong) UIView* searchEngineLogoView;

// The Magic Stack view controller.
@property(nonatomic, strong) UIViewController* magicStackViewController;

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
