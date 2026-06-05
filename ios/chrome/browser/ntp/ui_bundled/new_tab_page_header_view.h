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
@protocol NewTabPageControllerDelegate;
@protocol NewTabPageMutator;
@protocol HelpCommands;
@protocol FakeboxFocuser;
@class OmniboxContainerView;
@class SearchEngineLogoMediator;
enum class SearchEngineLogoState;
@class TabGroupIndicatorView;

// Header view for the NTP. The header view contains all views that are
// displayed above the list of most visited sites, which includes the
// primary toolbar, doodle, and fake omnibox.
@interface NewTabPageHeaderView : UIView <UserAccountImageUpdateDelegate,
                                          SearchEngineLogoConsumer,
                                          NewTabPageHeaderConsumer,
                                          FakeboxButtonsSnapshotProvider>

// Returns the toolbar view.
@property(nonatomic, readonly) UIView* toolBarView;

// The Identity Disc showing the current user's avatar on NTP.
@property(nonatomic, strong) NTPIdentityDiscButton* identityDiscButton;

// The entrypoint for the Home customization menu.
@property(nonatomic, strong) UIButton* customizationMenuButton;

// The layout guide center for the current scene. Owned by this view's owning
// view controller.
@property(nonatomic, weak) LayoutGuideCenter* layoutGuideCenter;

// View that contains tab group information.
@property(nonatomic, weak) TabGroupIndicatorView* tabGroupIndicatorView;

// Sets whether Google is the default search engine.
- (void)setIsGoogleDefaultSearchEngine:(BOOL)isGoogleDefaultSearchEngine;

// Name of the default search engine. Used for the omnibox placeholder text.
@property(nonatomic, copy) NSString* placeholderText;

// Should be set to YES if an animation will run that requires animating the
// font scale, for example, during a fakebox defocus animation.
@property(nonatomic, assign) BOOL allowFontScaleAnimation;

// Handles the actions for the NTP shortcuts, like Lens or voice search.
@property(nonatomic, weak) id<NewTabPageShortcutsHandler> NTPShortcutsHandler;

// Handler for dispatched commands.
@property(nonatomic, weak) id<NewTabPageHeaderCommands> commandHandler;

// Delegate for toolbar actions.
@property(nonatomic, weak) id<NewTabPageControllerDelegate> toolbarDelegate;

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

// The mediator for the search engine logo.
@property(nonatomic, strong) SearchEngineLogoMediator* searchEngineLogoMediator;

// The logo state.
@property(nonatomic, assign) SearchEngineLogoState logoState;

// Initializes the view with the Lens and customization menu badge status.
- (instancetype)initWithUseNewBadgeForLensButton:(BOOL)useNewBadgeForLensButton
                 useNewBadgeForCustomizationMenu:
                     (BOOL)useNewBadgeForCustomizationMenu
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Adds the separator to the searchField. Must be called after the searchField
// is added as a subview.
- (void)addSeparatorToSearchField:(UIView*)searchField;

// Adds the `toolbarView` to the view implementing this protocol.
// Can only be added once.
- (void)addToolbarView:(UIView*)toolbarView;

// Sets up the subviews (fake omnibox, tap view) after properties are set.
- (void)setupSubviews;

// Returns the progress of the search field position along
// `ntp_header::kAnimationDistance` as the offset changes.
- (CGFloat)searchFieldProgressForOffset:(CGFloat)offset;

// Changes the constraints of searchField based on its initialFrame and the
// scroll view's y `offset`. Also adjust the alpha values for `_searchBoxBorder`
// and `_shadow` and the constant values for the `constraints`. `screenWidth` is
// the width of the screen, including the space outside the safe area. The
// `safeAreaInsets` is relative to the view used to calculate the `width`.
- (void)updateSearchFieldWidth:(NSLayoutConstraint*)widthConstraint
                        height:(NSLayoutConstraint*)heightConstraint
                     topMargin:(NSLayoutConstraint*)topMarginConstraint
                     forOffset:(CGFloat)offset
                   screenWidth:(CGFloat)screenWidth
                safeAreaInsets:(UIEdgeInsets)safeAreaInsets;

// Sets the Home customization menu entrypoint with a conditional "new feature"
// badge.
- (void)setCustomizationMenuButton:(UIButton*)customizationMenuButton
                      withNewBadge:(BOOL)hasNewBadge;

// Hides the new feature badge on the Home customization menu's entrypoint.
- (void)hideBadgeOnCustomizationMenu;

// Updates the `tabGroupIndicatorView` availability.
// `offset` represents the scroll view's y `offset`.
- (void)updateTabGroupIndicatorAvailabilityWithOffset:(CGFloat)offset;

// Returns a snapshot view of the fakebox's buttons to be used during focus
// and defocus animations.
- (UIView*)fakeboxButtonsSnapshot;

// Whether to show the plus button.
- (BOOL)shouldShowPlusButton;

// Resets the resizing of this view for scroll progress in split toolbar mode.
// Should be called on rotations.
- (void)resetSplitToolbarResizing;

// Animation to expand this header in response to focusing the omnibox.
- (void)expandHeaderForFocus;

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
