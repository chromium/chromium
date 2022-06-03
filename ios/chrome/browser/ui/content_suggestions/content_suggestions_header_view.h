// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

// Header view for the NTP. The header view contains all views that are
// displayed above the list of most visited sites, which includes the
// primary toolbar, doodle, and fake omnibox.
@interface ContentSuggestionsHeaderView : UIView

// Returns the toolbar view.
@property(nonatomic, readonly) UIView* toolBarView;

// The Identity Disc showing the current user's avatar on NTP.
@property(nonatomic, strong) UIView* identityDiscView;

// Voice search button.
@property(nonatomic, strong, readonly) UIButton* voiceSearchButton;

// Fake cancel button, used for animations. Hidden by default.
@property(nonatomic, strong) UIView* cancelButton;
// Fake omnibox, used for animations. Hidden by default.
@property(nonatomic, strong) UIView* omnibox;

@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarLeadingConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarTrailingConstraint;
@property(nonatomic, strong) UIView* fakeLocationBar;
@property(nonatomic, strong) UILabel* searchHintLabel;

// Adds the separator to the searchField. Must be called after the searchField
// is added as a subview.
- (void)addSeparatorToSearchField:(UIView*)searchField;

// Adds the |toolbarView| to the view implementing this protocol.
// Can only be added once.
- (void)addToolbarView:(UIView*)toolbarView;

// Returns the progress of the search field position along
// |ntp_header::kAnimationDistance| as the offset changes.
- (CGFloat)searchFieldProgressForOffset:(CGFloat)offset
                         safeAreaInsets:(UIEdgeInsets)safeAreaInsets;

// Changes the constraints of searchField based on its initialFrame and the
// scroll view's y |offset|. Also adjust the alpha values for |_searchBoxBorder|
// and |_shadow| and the constant values for the |constraints|.|screenWidth| is
// the width of the screen, including the space outside the safe area. The
// |safeAreaInsets| is relative to the view used to calculate the |width|.
- (void)updateSearchFieldWidth:(NSLayoutConstraint*)widthConstraint
                        height:(NSLayoutConstraint*)heightConstraint
                     topMargin:(NSLayoutConstraint*)topMarginConstraint
                     forOffset:(CGFloat)offset
                   screenWidth:(CGFloat)screenWidth
                safeAreaInsets:(UIEdgeInsets)safeAreaInsets;

// Adds views necessary to customize the NTP search box.
- (void)addViewsToSearchField:(UIView*)searchField;

// Highlights the fake omnibox.
- (void)setFakeboxHighlighted:(BOOL)highlighted;

// Updates the different constraints using |topSafeAreaInset|. This is needed
// because sometimes the safe area isn't correctly updated. See
// crbug.com/1041831.
- (void)updateForTopSafeAreaInset:(CGFloat)topSafeAreaInset;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_H_
