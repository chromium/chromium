// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_

#import <UIKit/UIKit.h>

namespace content_suggestions {

extern const CGFloat kHintTextScale;

// Bottom margin for the Return to Recent Tab tile.
extern const CGFloat kReturnToRecentTabSectionBottomMargin;

// Returns the proper height for the doodle. `logo_is_showing` is YES if showing
// the Google logo. `doodle_is_showing` is YES if the doodle is showing. The
// SizeClass of the `trait_collection` of the view displaying the doodle is used
// in the computation.
CGFloat DoodleHeight(BOOL logo_is_showing,
                     BOOL doodle_is_showing,
                     UITraitCollection* trait_collection);
// Returns the proper margin to the top of the header for the doodle.
CGFloat DoodleTopMargin(CGFloat top_inset, UITraitCollection* trait_collection);
// Returns the height of the separator line below the omnibox.
CGFloat HeaderSeparatorHeight();
// Returns the proper margin to the bottom of the doodle for the search field.
CGFloat SearchFieldTopMargin();
// Returns the height of the Fake Omnibox on Home when it is not scrolled.
CGFloat FakeOmniboxHeight();
// Returns the height of the Fake Omnibox on Home when it is pinned / scrolled.
CGFloat PinnedFakeOmniboxHeight();
// Returns the height of the fake toolbar shown when the fake omnibox is pinned.
CGFloat FakeToolbarHeight();
// Returns the proper width for the search field inside a view with a `width`.
// The SizeClass of the `traitCollection` of the view displaying the search
// field is used in the computation.
CGFloat SearchFieldWidth(CGFloat width, UITraitCollection* trait_collection);
// Returns the expected height of the header. `logo_is_showing` is YES if
// showing the Google logo. `doodle_is_showing` is YES if the doodle is being
// shown.
CGFloat HeightForLogoHeader(BOOL logo_is_showing,
                            BOOL doodle_is_showing,
                            UITraitCollection* trait_collection);
// Returns the bottom padding for the header. This represents the spacing
// between the fake omnibox and the content suggestions tiles.
CGFloat HeaderBottomPadding();
// Configure the `search_hint_label` for the fake omnibox.  `hintLabelContainer`
// is added to the `search_tab_target` with autolayout and `search_hint_label`
// is added to `hintLabelContainer` with autoresizing.  This is done due to the
// way `search_hint_label` is later tranformed.
void ConfigureSearchHintLabel(UILabel* search_hint_label,
                              UIView* search_tab_target);
// Configure the `voice_search_button` appearance.
void ConfigureVoiceSearchButton(UIButton* voice_search_button,
                                BOOL use_color_icon);
// Configure the `lens_button` appearance.
void ConfigureLensButtonAppearance(UIButton* lens_button,
                                   BOOL use_new_badge,
                                   BOOL use_color_icon);
// Configure the `lens_button` new badge's alpha.
void ConfigureLensButtonWithNewBadgeAlpha(UIButton* lens_button,
                                          CGFloat new_badge_alpha);

// Returns the nearest ancestor of `view` that is kind of `of_class`.
UIView* NearestAncestor(UIView* view, Class of_class);

// Returns the color of the search hint label in the fakebox.
UIColor* SearchHintLabelColor();

// Returns the string ID to be used to get the title of the SetUpList.
int SetUpListTitleStringID();

// Returns the localized title to be used for the SetUpList.
NSString* SetUpListTitleString();

}  // namespace content_suggestions

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
