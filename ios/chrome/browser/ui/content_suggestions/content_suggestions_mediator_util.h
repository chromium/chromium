// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_

#include "components/ntp_tiles/ntp_tile.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"

@class ContentSuggestionsMostVisitedItem;

// Creates and returns a SectionInfo for the section containing the "Return to
// Recent Tab" tile for the Start Surface.
ContentSuggestionsSectionInformation* ReturnToRecentTabSectionInformation();

// Creates and returns a SectionInfo for the single cell parent item.
ContentSuggestionsSectionInformation* SingleCellSectionInformation();

// Converts a ntp_tiles::NTPTile `tile` to a ContentSuggestionsMostVisitedItem
// with a `sectionInfo`.
ContentSuggestionsMostVisitedItem* ConvertNTPTile(
    const ntp_tiles::NTPTile& tile);

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_
