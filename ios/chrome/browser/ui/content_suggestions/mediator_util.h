// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_

#include <string>

#include "base/bind.h"
#include "base/optional.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/status.h"
#include "components/ntp_tiles/ntp_tile.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"

namespace ntp_snippets {
class Category;
}

@class CollectionViewItem;
@class ContentSuggestionsCategoryWrapper;
@class ContentSuggestionsItem;
@class ContentSuggestionsMostVisitedActionItem;
@class ContentSuggestionsMostVisitedItem;

// Returns the section ID for this |category|.
ContentSuggestionsSectionID SectionIDForCategory(
    ntp_snippets::Category category);

// Converts a ntp_snippets::ContentSuggestion to a CollectionViewItem.
ContentSuggestionsItem* ConvertSuggestion(
    const ntp_snippets::ContentSuggestion& contentSuggestion,
    ContentSuggestionsSectionInformation* sectionInfo,
    ntp_snippets::Category category);

// Returns a SectionInformation for a |category|, filled with the
// |categoryInfo| and |expanded|.
ContentSuggestionsSectionInformation* SectionInformationFromCategoryInfo(
    const base::Optional<ntp_snippets::CategoryInfo>& categoryInfo,
    const ntp_snippets::Category& category,
    const BOOL expanded);

// Returns a ntp_snippets::ID based on a Objective-C Category and the ID in the
// category.
ntp_snippets::ContentSuggestion::ID SuggestionIDForSectionID(
    ContentSuggestionsCategoryWrapper* category,
    const std::string& id_in_category);

// Creates and returns a SectionInfo for the section containing the "Return to
// Recent Tab" tile for the Start Surface.
ContentSuggestionsSectionInformation* ReturnToRecentTabSectionInformation();

// Creates and returns a SectionInfo for the section containing the logo and
// omnibox.
ContentSuggestionsSectionInformation* LogoSectionInformation();

// Creates and returns a SectionInfo for the What's New promo section.
ContentSuggestionsSectionInformation* PromoSectionInformation();

// Creates and returns a SectionInfo for the Most Visited section.
ContentSuggestionsSectionInformation* MostVisitedSectionInformation();

// Creates and returns a SectionInfo for the Learn More section.
ContentSuggestionsSectionInformation* LearnMoreSectionInformation();

// Creates and returns a SectionInfo for the Discover feed section.
ContentSuggestionsSectionInformation* DiscoverSectionInformation(
    BOOL isGoogleDefaultSearchProvider);

// Converts a ntp_tiles::NTPTile |tile| to a ContentSuggestionsMostVisitedItem
// with a |sectionInfo|.
ContentSuggestionsMostVisitedItem* ConvertNTPTile(
    const ntp_tiles::NTPTile& tile,
    ContentSuggestionsSectionInformation* sectionInfo);

content_suggestions::StatusCode ConvertStatusCode(ntp_snippets::Status status);

// Creates and returns a Bookmarks action item.
ContentSuggestionsMostVisitedActionItem* BookmarkActionItem();

// Creates and returns a Reading List action item.
ContentSuggestionsMostVisitedActionItem* ReadingListActionItem();

// Creates and returns a Recent Tabs action item.
ContentSuggestionsMostVisitedActionItem* RecentTabsActionItem();

// Creates and returns a Histry action item.
ContentSuggestionsMostVisitedActionItem* HistoryActionItem();

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MEDIATOR_UTIL_H_
