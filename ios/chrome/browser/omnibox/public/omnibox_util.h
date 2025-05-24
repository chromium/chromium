// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_UTIL_H_
#define IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_UTIL_H_

#import <UIKit/UIKit.h>

#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/security_state/core/security_state.h"
#import "ios/chrome/browser/omnibox/public/omnibox_icon_type.h"
#include "ios/chrome/browser/omnibox/public/omnibox_suggestion_icon_util.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"

#pragma mark - Suggestion icons.

// Converts `type` to the appropriate icon type for this match type to show in
// the omnibox.
OmniboxSuggestionIconType GetOmniboxSuggestionIconTypeForAutocompleteMatchType(
    AutocompleteMatchType::Type type);

// Converts `type` to the appropriate icon for this type to show in the omnibox.
UIImage* GetOmniboxSuggestionIconForAutocompleteMatchType(
    AutocompleteMatchType::Type type);

// Converts Suggest proto icon `type` to the appropriate icon type to show in
// the omnibox.
OmniboxSuggestionIconType
GetOmniboxSuggestionIconTypeForSuggestTemplateInfoIconType(
    omnibox::SuggestTemplateInfo::IconType type);

// Converts Suggest proto icon `type` into the appropriate asset.
UIImage* GetOmniboxSuggestionIconForSuggestTemplateInfoIconType(
    omnibox::SuggestTemplateInfo::IconType type);

#pragma mark - Security icons.

// Returns the asset with "always template" rendering mode.
UIImage* GetLocationBarSecurityIcon(LocationBarSecurityIconType icon);

// Converts the `security_level` to an appropriate security icon type.
LocationBarSecurityIconType GetLocationBarSecurityIconTypeForSecurityState(
    security_state::SecurityLevel security_level);

// Converts the `security_level` to an appropriate icon in "always template"
// rendering mode.
UIImage* GetLocationBarSecurityIconForSecurityState(
    security_state::SecurityLevel security_level);

// Returns the icon for an offline page.
UIImage* GetLocationBarOfflineIcon();

#endif  // IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_UTIL_H_
