// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui/user_account_image_update_delegate.h"
#import "ios/chrome/browser/location_bar/ui_bundled/fakebox_buttons_snapshot_provider.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_delegate.h"

@class FakeLocationBarView;
@class NTPIdentityDiscButton;
@class LayoutGuideCenter;
@class NewTabPageColorPalette;
@protocol NewTabPageShortcutsHandler;
@protocol NewTabPageHeaderCommands;
@protocol NewTabPageMutator;
@protocol HelpCommands;
@protocol FakeboxFocuser;
@class OmniboxContainerView;
enum class SearchEngineLogoState;
@class TabGroupIndicatorView;

// Header view for the NTP. The header view contains all views that are
// displayed above the list of most visited sites, which includes the
// primary toolbar, doodle, and fake omnibox.
@interface NewTabPageHeaderView : UIView <FakeboxButtonsSnapshotProvider,
                                          NewTabPageHeaderConsumer,
                                          SearchEngineLogoConsumer,
                                          UserAccountImageUpdateDelegate>
// Returns the toolbar view.
@property(nonatomic, readonly) UIView* toolBarView;

// The layout guide center for the current scene. Owned by this view's owning
// view controller.
@property(nonatomic, weak) LayoutGuideCenter* layoutGuideCenter;

// View that contains tab group information.
@property(nonatomic, weak) TabGroupIndicatorView* tabGroupIndicatorView;

// Sets whether Google is the default search engine.
@property(nonatomic, assign) BOOL isGoogleDefaultSearchEngine;

// Name of the default search engine. Used for the omnibox placeholder text.
@property(nonatomic, copy) NSString* placeholderText;

// Should be set to YES if an animation will run that requires animating the
// font scale, for example, during a fakebox defocus animation.
@property(nonatomic, assign) BOOL allowFontScaleAnimation;

// Handles the actions for the NTP shortcuts, like Lens or voice search.
@property(nonatomic, weak) id<NewTabPageShortcutsHandler> NTPShortcutsHandler;

// Handler for dispatched commands.
@property(nonatomic, weak) id<NewTabPageHeaderCommands> commandHandler;

// The target for scribble events as forwarded by the NTP fakebox.
@property(nonatomic, weak) UIResponder<UITextInput>* scribbleForwardingTarget;

// The scroll progress of the fakebox animation (0.0 to 1.0).
@property(nonatomic, assign, readonly) CGFloat scrollProgress;

// Delegate for header view actions.
@property(nonatomic, weak) id<NewTabPageHeaderViewDelegate> delegate;

// The mutator for the NTP.
@property(nonatomic, weak) id<NewTabPageMutator> mutator;

// In-product help handler.
@property(nonatomic, weak) id<HelpCommands> helpHandler;

// Fakebox focus handler.
@property(nonatomic, weak) id<FakeboxFocuser> fakeboxFocuserHandler;

// Whether the NTP is currently showing.
@property(nonatomic, assign, getter=isShowing) BOOL showing;

// The search engine logo view.
@property(nonatomic, weak) UIView* searchEngineLogoView;

// Initializes the view with the Lens and customization menu badge status.
- (instancetype)initWithUseNewBadgeForLensButton:(BOOL)useNewBadgeForLensButton
                 useNewBadgeForCustomizationMenu:
                     (BOOL)useNewBadgeForCustomizationMenu
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;


// Sets up the subviews (fake omnibox, tap view) after properties are set.
- (void)setupSubviews;

// Hides the new feature badge on the Home customization menu's entrypoint.
- (void)hideBadgeOnCustomizationMenu;

// Animation to expand this header in response to focusing the omnibox.
- (void)expandHeaderForFocus;

// Reverts the effects of expanding the header.
- (void)revertHeaderExpansionOnUnfocus;

// Updates the fake omnibox layout for the given scroll offset.
- (void)updateFakeOmniboxForOffset:(CGFloat)offset
                       screenWidth:(CGFloat)screenWidth
                    safeAreaInsets:(UIEdgeInsets)safeAreaInsets
            animateScrollAnimation:(BOOL)animateScrollAnimation;

// Updates the fake omnibox layout for the given width.
- (void)updateFakeOmniboxForWidth:(CGFloat)width;

// Layouts the header view.
- (void)layoutHeader;

// Returns the height of the header.
- (CGFloat)headerHeight;

// Notifies the view that it appeared.
- (void)didAppear;

// Sends notification to focus the accessibility of the omnibox.
- (void)focusAccessibilityOnOmnibox;

// Configure the header after the focus omnibox animation has completed.
- (void)completeHeaderFakeOmniboxFocusAnimationWithFinalPosition:
    (UIViewAnimatingPosition)finalPosition;

// Resets fakebox state when omnibox ends editing.
- (void)omniboxDidEndEditing;

// Returns the view containing the fake omnibox.
- (UIView*)fakeOmniboxView;

// Returns the Y value to use for the scroll view's contentOffset when scrolling
// the omnibox to the top of the screen.
- (CGFloat)pinnedOffsetY;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_H_
