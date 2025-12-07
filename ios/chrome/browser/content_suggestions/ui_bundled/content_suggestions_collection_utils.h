// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_

#import <UIKit/UIKit.h>

enum class SearchEngineLogoState;

namespace content_suggestions {

extern const CGFloat kHintTextScale;

// Bottom margin for the Return to Recent Tab tile.
extern const CGFloat kReturnToRecentTabSectionBottomMargin;

// Returns the proper height for the doodle, based on `logo_state`. The
// SizeClass of the `trait_collection` of the view displaying the doodle is used
// in the computation.
CGFloat DoodleHeight(SearchEngineLogoState logo_state,
                     UITraitCollection* trait_collection);
// Returns the proper margin to the top of the header for the doodle.
CGFloat DoodleTopMargin(SearchEngineLogoState logo_state,
                        UITraitCollection* trait_collection);
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
// Returns the expected height of the header, based on `logo_state`.
CGFloat HeightForLogoHeader(SearchEngineLogoState logo_state,
                            UITraitCollection* trait_collection);
// Returns the bottom padding for the header. This represents the spacing
// between the fake omnibox and the content suggestions tiles.
CGFloat HeaderBottomPadding(UITraitCollection* trait_collection);
// Configure the `search_hint_label` for the fake omnibox.  `hintLabelContainer`
// is added to the `search_tab_target` with autolayout and `search_hint_label`
// is added to `hintLabelContainer` with autoresizing.  This is done due to the
// way `search_hint_label` is later tranformed.
// Uses `placeholder_text` as the placeholder.
void ConfigureSearchHintLabel(UILabel* search_hint_label,
                              UIView* search_tab_target,
                              NSString* placeholder_text);
// Configure the `voice_search_button` appearance.
void ConfigureVoiceSearchButton(UIButton* voice_search_button,
                                BOOL use_color_icon);
// Configure the `lens_button` appearance. If `new_badge_color` is nil, falls
// back to the default badge color.
void ConfigureLensButtonAppearance(UIButton* lens_button,
                                   BOOL use_new_badge,
                                   BOOL use_color_icon,
                                   UIColor* new_badge_color);
// Configure the `lens_button` new badge's alpha.
void ConfigureLensButtonWithNewBadgeAlpha(UIButton* lens_button,
                                          CGFloat new_badge_alpha);

// Configure the `mia_button` appearance.
void ConfigureMIAButton(UIButton* mia_button, BOOL use_color_icon);

// Returns the nearest ancestor of `view` that is kind of `of_class`.
UIView* NearestAncestor(UIView* view, Class of_class);

// Returns the color of the search hint label in the fakebox.
UIColor* SearchHintLabelColor();

// Returns the color to use for the Lens and Voice icons in the Fakebox.
UIColor* DefaultIconTintColorWithAIMAllowed(bool aim_allowed);

}  // namespace content_suggestions

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
