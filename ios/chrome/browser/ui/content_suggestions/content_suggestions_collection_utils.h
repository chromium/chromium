// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_

#import <UIKit/UIKit.h>

namespace content_suggestions {

extern const int kSearchFieldBackgroundColor;

extern const CGFloat kHintTextScale;

// Bottom margin for the Return to Recent Tab tile.
extern const CGFloat kReturnToRecentTabSectionBottomMargin;

// Returns the proper height for the doodle. `logoIsShowing` is YES if showing
// the Google logo. `doodleisShowing` is YES if the doodle is showing. The
// SizeClass of the `traitCollection` of the view displaying the doodle is used
// in the computation.
CGFloat doodleHeight(BOOL logoIsShowing,
                     BOOL doodleIsShowing,
                     UITraitCollection* traitCollection);
// Returns the proper margin to the top of the header for the doodle.
// If `toolbarPresent` is true, the top margin include a space to display the
// toolbar.  Adds `topInset` to non-RxR displays. The SizeClass of the
// `traitCollection` of the view displaying the doodle is used.
CGFloat doodleTopMargin(BOOL toolbarPresent,
                        CGFloat topInset,
                        UITraitCollection* traitCollection);
// Returns the proper margin to the bottom of the doodle for the search field.
CGFloat searchFieldTopMargin();
// Returns the proper width for the search field inside a view with a `width`.
// The SizeClass of the `traitCollection` of the view displaying the search
// field is used in the computation.
CGFloat searchFieldWidth(CGFloat superviewWidth,
                         UITraitCollection* traitCollection);
// TODO(crbug.com/761817): Remove `toolbarPresent` once the transition to the
// new architecture is completed.
// Returns the expected height of the header. `logoIsShowing` is YES if showing
// the Google logo. `doodleIsShowing` is YES if the doodle is being shown.
// `promoCanShow` represents whether a what's new promo can be displayed.
// `toolbarPresent` represent whether the height should take into account a
// space to show the toolbar. The SizeClass of the `traitCollection` of the view
// displaying the logo is used in the computation.
CGFloat heightForLogoHeader(BOOL logoIsShowing,
                            BOOL doodleIsShowing,
                            BOOL promoCanShow,
                            BOOL toolbarPresent,
                            CGFloat topInset,
                            UITraitCollection* traitCollection);
// Returns the bottom padding for the header. This represents the spacing
// between the fake omnibox and the content suggestions tiles.
CGFloat headerBottomPadding();
// Configure the `searchHintLabel` for the fake omnibox.  `hintLabelContainer`
// is added to the `searchTapTarget` with autolayout and `searchHintLabel` is
// added to `hintLabelContainer` with autoresizing.  This is done due to the
// way searchHintLabel is later tranformed.
void configureSearchHintLabel(UILabel* searchHintLabel,
                              UIView* searchTapTarget);
// Configure the `voiceSearchButton`, adding it to the `searchTapTarget` and
// constraining it.
void configureVoiceSearchButton(UIButton* voiceSearchButton,
                                UIView* searchTapTarget);

// Returns the nearest ancestor of `view` that is kind of `aClass`.
UIView* nearestAncestor(UIView* view, Class aClass);

}  // namespace content_suggestions

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
