// Copyright 2017 The Chromium Authors
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
CGFloat doodleTopMargin(CGFloat topInset, UITraitCollection* traitCollection);
// Returns the proper margin to the bottom of the doodle for the search field.
CGFloat searchFieldTopMargin();
// Returns the proper width for the search field inside a view with a `width`.
// The SizeClass of the `traitCollection` of the view displaying the search
// field is used in the computation.
CGFloat searchFieldWidth(CGFloat superviewWidth,
                         UITraitCollection* traitCollection);
// Returns the expected height of the header. `logoIsShowing` is YES if showing
// the Google logo. `doodleIsShowing` is YES if the doodle is being shown.
CGFloat heightForLogoHeader(BOOL logoIsShowing,
                            BOOL doodleIsShowing,
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
// Configure the `lens_button`, adding it to the `search_tap_target` and
// constraining it.
void ConfigureLensButton(UIButton* lens_button, UIView* search_tap_target);

// Returns the nearest ancestor of `view` that is kind of `aClass`.
UIView* nearestAncestor(UIView* view, Class aClass);

}  // namespace content_suggestions

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
