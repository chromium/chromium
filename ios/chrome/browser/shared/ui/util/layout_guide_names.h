// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_LAYOUT_GUIDE_NAMES_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_LAYOUT_GUIDE_NAMES_H_

#import <Foundation/Foundation.h>

typedef NSString GuideName;

// The list of well-known UILayoutGuides.  When adding a new guide to the app,
// create a constant for it below. Because these constants are in the global
// namespace, all guide names should end in 'Guide', for clarity.

// A guide that is constrained to match the frame of the tab's content area.
extern GuideName* const kContentAreaGuide;
// A guide that is constrained to match the frame of the primary toolbar. This
// follows the frame of the primary toolbar even when that frame shrinks due to
// fullscreen. It does not include the tab strip on iPad.
extern GuideName* const kPrimaryToolbarGuide;
// A guide that is constrained to match the frame of the secondary toolbar.
extern GuideName* const kSecondaryToolbarGuide;
// A guide that is constrained to match the frame of the omnibox when it's in
// the primary toolbar.
extern GuideName* const kTopOmniboxGuide;
// A guide that is constrained to match the frame of the leading image view in
// the omnibox.
extern GuideName* const kOmniboxLeadingImageGuide;
// A guide that is constrained to match the frame of the text field in the
// omnibox.
extern GuideName* const kOmniboxTextFieldGuide;
// A guide that is constrained to match the frame of the back button's image.
extern GuideName* const kBackButtonGuide;
// A guide that is constrained to match the frame of the forward button's image.
extern GuideName* const kForwardButtonGuide;
// A guide that is constrained to match the frame of the NewTab button.
extern GuideName* const kNewTabButtonGuide;
// A guide that is constrained to match the frame of the Share button.
extern GuideName* const kShareButtonGuide;
// A guide that is constrained to match the frame of the TabSwitcher button's
// image.
extern GuideName* const kTabSwitcherGuide;
// A guide that is constrained to match the frame of the ToolsMenu button.
extern GuideName* const kToolsMenuGuide;
// A guide that is constrained to match the frame of the last-tapped voice
// search button.
extern GuideName* const kVoiceSearchButtonGuide;
// A guide that is constrained to present the feed IPH on a view.
extern GuideName* const kFeedIPHNamedGuide;
// A guide that is constrained to match the frame of the bottom toolbar in the
// tab grid.
extern GuideName* const kTabGridBottomToolbarGuide;
// A guide that is constrained to match the frame of the first Autofill result.
extern GuideName* const kAutofillFirstSuggestionGuide;
// A guide that is constrained to match the frame of the Lens button in the
// omnibox keyboard accessory view.
extern GuideName* const kLensKeyboardButtonGuide;
// A guide that is constrained to match the frame of the Magic Stack on the NTP.
extern GuideName* const kMagicStackGuide;
// A guide that is constrained to match the frame of the Contextual Panel's
// entrypoint when it is large, otherwise this stays nil.
extern GuideName* const kContextualPanelLargeEntrypointGuide;
// A guide that is constrained to match the frame of the Lens icon in the NTP's
// Fakebox.
extern GuideName* const kFakeboxLensIconGuide;
// A guide that is constrained to match the frame of the lens overlay
// entrypoint.
extern GuideName* const kLensOverlayEntrypointGuide;

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_LAYOUT_GUIDE_NAMES_H_
