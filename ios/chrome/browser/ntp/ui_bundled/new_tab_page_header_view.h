// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@class GradientView;
@class TabGroupIndicatorView;

// Header view for the NTP. The header view contains all views that are
// displayed above the list of most visited sites, which includes the
// primary toolbar, doodle, and fake omnibox.
@interface NewTabPageHeaderView : UIView

// Initializes the view with the Lens button new badge status.
- (instancetype)initWithUseNewBadgeForLensButton:(BOOL)useNewBadgeForLensButton
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns the toolbar view.
@property(nonatomic, readonly) UIView* toolBarView;

// The Identity Disc showing the current user's avatar on NTP.
@property(nonatomic, strong) UIView* identityDiscView;

// The entrypoint for the Home customization menu.
@property(nonatomic, strong) UIButton* customizationMenuButton;

// Voice search button.
@property(nonatomic, strong, readonly) UIButton* voiceSearchButton;

// The button that opens Lens. May be nil if Lens is not enabled.
@property(nonatomic, strong, readonly) UIButton* lensButton;

// Fake cancel button, used for animations. Hidden by default.
@property(nonatomic, strong) UIView* cancelButton;
// Fake omnibox, used for animations. Hidden by default.
@property(nonatomic, strong) UIView* omnibox;

@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarLeadingConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarTrailingConstraint;
@property(nonatomic, strong) GradientView* fakeLocationBar;
@property(nonatomic, strong) UILabel* searchHintLabel;

// View that contains tab group information.
@property(nonatomic, weak) TabGroupIndicatorView* tabGroupIndicatorView;

// `YES` if Google is the default search engine.
@property(nonatomic, assign) BOOL isGoogleDefaultSearchEngine;

// Should be set to YES if an animation will run that requires animating the
// font scale, for example, during a fakebox defocus animation.
@property(nonatomic, assign) BOOL allowFontScaleAnimation;

// Adds the separator to the searchField. Must be called after the searchField
// is added as a subview.
- (void)addSeparatorToSearchField:(UIView*)searchField;

// Adds the `toolbarView` to the view implementing this protocol.
// Can only be added once.
- (void)addToolbarView:(UIView*)toolbarView;

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

// Update buttons for the user interface style.
- (void)updateButtonsForUserInterfaceStyle:(UIUserInterfaceStyle)style;

// Adds views necessary to customize the NTP search box.
- (void)addViewsToSearchField:(UIView*)searchField;

// Highlights the fake omnibox.
- (void)setFakeboxHighlighted:(BOOL)highlighted;

// Hides the buttons within the fakebox.
- (void)hideFakeboxButtons;

// Shows the buttons within the fakebox.
- (void)showFakeboxButtons;

// Shows account disc particle error badge.
- (void)setIdentityDiscErrorBadge;

// Removes account disc particle error badge.
- (void)removeIdentityDiscErrorBadge;

// Sets the Home customization menu entrypoint with a conditional "new feature"
// badge.
- (void)setCustomizationMenuButton:(UIButton*)customizationMenuButton
                      withNewBadge:(BOOL)hasNewBadge;

// Hides the new feature badge on the Home customization menu's entrypoint.
- (void)hideBadgeOnCustomizationMenu;

// Updates the `tabGroupIndicatorView` availability.
// `offset` represents the scroll view's y `offset`.
- (void)updateTabGroupIndicatorAvailabilityWithOffset:(CGFloat)offset;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_H_
