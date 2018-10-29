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
                     hintLabel:(UILabel*)hintLabel
            subviewConstraints:(NSArray*)constraints
                     forOffset:(CGFloat)offset
                   screenWidth:(CGFloat)screenWidth
                safeAreaInsets:(UIEdgeInsets)safeAreaInsets;

// Adds views necessary to customize the NTP search box.
- (void)addViewsToSearchField:(UIView*)searchField;

// Highlights the fake omnibox.
- (void)setFakeboxHighlighted:(BOOL)highlighted;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_H_
