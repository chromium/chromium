// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_

#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/status.h"
#include "components/ntp_tiles/ntp_tile.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"

@class ContentSuggestionsMostVisitedActionItem;
@class ContentSuggestionsMostVisitedItem;

// Creates and returns a SectionInfo for the section containing the "Return to
// Recent Tab" tile for the Start Surface.
ContentSuggestionsSectionInformation* ReturnToRecentTabSectionInformation();

// Creates and returns a SectionInfo for the section containing the logo and
// omnibox.
ContentSuggestionsSectionInformation* LogoSectionInformation();

// Creates and returns a SectionInfo for the Most Visited section.
ContentSuggestionsSectionInformation* MostVisitedSectionInformation();

// Creates and returns a SectionInfo for the single cell parent item.
ContentSuggestionsSectionInformation* SingleCellSectionInformation();

// Converts a ntp_tiles::NTPTile `tile` to a ContentSuggestionsMostVisitedItem
// with a `sectionInfo`.
ContentSuggestionsMostVisitedItem* ConvertNTPTile(
    const ntp_tiles::NTPTile& tile,
    ContentSuggestionsSectionInformation* sectionInfo);

// Creates and returns a Bookmarks action item.
ContentSuggestionsMostVisitedActionItem* BookmarkActionItem();

// Creates and returns a Reading List action item.
ContentSuggestionsMostVisitedActionItem* ReadingListActionItem();

// Creates and returns a Recent Tabs action item.
ContentSuggestionsMostVisitedActionItem* RecentTabsActionItem();

// Creates and returns a History action item.
ContentSuggestionsMostVisitedActionItem* HistoryActionItem();

// Creates and returns a Whats New action item.
ContentSuggestionsMostVisitedActionItem* WhatsNewActionItem();

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_
