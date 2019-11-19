// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/mediator_util.h"

#include "base/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "components/ntp_snippets/category.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
ContentSuggestionsSectionInformation* EmptySectionInfo(
    ContentSuggestionsSectionID sectionID) {
  ContentSuggestionsSectionInformation* sectionInfo = [
      [ContentSuggestionsSectionInformation alloc] initWithSectionID:sectionID];
  sectionInfo.title = nil;
  sectionInfo.footerTitle = nil;
  sectionInfo.showIfEmpty = NO;
  sectionInfo.layout = ContentSuggestionsSectionLayoutCustom;

  return sectionInfo;
}
}  // namespace

ContentSuggestionsSectionID SectionIDForCategory(
    ntp_snippets::Category category) {
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES))
    return ContentSuggestionsSectionArticles;
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::READING_LIST))
    return ContentSuggestionsSectionReadingList;

  return ContentSuggestionsSectionUnknown;
}

ContentSuggestionsItem* ConvertSuggestion(
    const ntp_snippets::ContentSuggestion& contentSuggestion,
    ContentSuggestionsSectionInformation* sectionInfo,
    ntp_snippets::Category category) {
  ContentSuggestionsItem* suggestion = [[ContentSuggestionsItem alloc]
      initWithType:0
             title:base::SysUTF16ToNSString(contentSuggestion.title())
               url:contentSuggestion.url()];
  suggestion.metricsRecorded = NO;

  suggestion.publisher =
      base::SysUTF16ToNSString(contentSuggestion.publisher_name());
  suggestion.publishDate = contentSuggestion.publish_date();

  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.IDInSection =
      contentSuggestion.id().id_within_category();
  suggestion.suggestionIdentifier.sectionInfo = sectionInfo;

  suggestion.score = contentSuggestion.score();
  suggestion.fetchDate = contentSuggestion.fetch_date();

  if (category.IsKnownCategory(ntp_snippets::KnownCategories::READING_LIST)) {
    suggestion.faviconURL =
        contentSuggestion.reading_list_suggestion_extra()->favicon_page_url;
  }
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES)) {
    suggestion.hasImage = contentSuggestion.salient_image_url().is_valid();
    suggestion.readLaterAction = YES;
  }

  return suggestion;
}

ContentSuggestionsSectionInformation* SectionInformationFromCategoryInfo(
    const base::Optional<ntp_snippets::CategoryInfo>& categoryInfo,
    const ntp_snippets::Category& category,
    const BOOL expanded) {
  ContentSuggestionsSectionInformation* sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:SectionIDForCategory(category)];
  if (categoryInfo) {
    sectionInfo.layout = ContentSuggestionsSectionLayoutCard;
    sectionInfo.showIfEmpty = categoryInfo->show_if_empty();
    sectionInfo.emptyText =
        base::SysUTF16ToNSString(categoryInfo->no_suggestions_message());
    if (categoryInfo->additional_action() !=
        ntp_snippets::ContentSuggestionsAdditionalAction::NONE) {
      sectionInfo.footerTitle =
          l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE);
    }
    sectionInfo.title = base::SysUTF16ToNSString(categoryInfo->title());
    sectionInfo.expanded = expanded;
  }
  return sectionInfo;
}

ntp_snippets::ContentSuggestion::ID SuggestionIDForSectionID(
    ContentSuggestionsCategoryWrapper* category,
    const std::string& id_in_category) {
  return ntp_snippets::ContentSuggestion::ID(category.category, id_in_category);
}

ContentSuggestionsSectionInformation* LogoSectionInformation() {
  ContentSuggestionsSectionInformation* sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:ContentSuggestionsSectionLogo];
  sectionInfo.title = nil;
  sectionInfo.footerTitle = nil;
  sectionInfo.showIfEmpty = YES;
  sectionInfo.layout = ContentSuggestionsSectionLayoutCustom;

  return sectionInfo;
}

ContentSuggestionsSectionInformation* PromoSectionInformation() {
  return EmptySectionInfo(ContentSuggestionsSectionPromo);
}

ContentSuggestionsSectionInformation* MostVisitedSectionInformation() {
  return EmptySectionInfo(ContentSuggestionsSectionMostVisited);
}

ContentSuggestionsSectionInformation* LearnMoreSectionInformation() {
  return EmptySectionInfo(ContentSuggestionsSectionLearnMore);
}

ContentSuggestionsMostVisitedItem* ConvertNTPTile(
    const ntp_tiles::NTPTile& tile,
    ContentSuggestionsSectionInformation* sectionInfo) {
  ContentSuggestionsMostVisitedItem* suggestion =
      [[ContentSuggestionsMostVisitedItem alloc] initWithType:0];

  suggestion.title = base::SysUTF16ToNSString(tile.title);
  suggestion.URL = tile.url;
  suggestion.source = tile.source;
  suggestion.titleSource = tile.title_source;
  suggestion.accessibilityTraits = UIAccessibilityTraitButton;

  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.IDInSection = tile.url.spec();
  suggestion.suggestionIdentifier.sectionInfo = sectionInfo;

  return suggestion;
}

content_suggestions::StatusCode ConvertStatusCode(ntp_snippets::Status status) {
  switch (status.code) {
    case ntp_snippets::StatusCode::SUCCESS:
      return content_suggestions::StatusCodeSuccess;
      break;
    case ntp_snippets::StatusCode::TEMPORARY_ERROR:
      return content_suggestions::StatusCodeError;
      break;
    case ntp_snippets::StatusCode::PERMANENT_ERROR:
      return content_suggestions::StatusCodePermanentError;
      break;
    case ntp_snippets::StatusCode::STATUS_CODE_COUNT:
      NOTREACHED();
      return content_suggestions::StatusCodeError;
  }
}

ContentSuggestionsMostVisitedActionItem* BookmarkActionItem() {
  return [[ContentSuggestionsMostVisitedActionItem alloc]
      initWithCollectionShortcutType:NTPCollectionShortcutTypeBookmark];
}

ContentSuggestionsMostVisitedActionItem* ReadingListActionItem() {
  return [[ContentSuggestionsMostVisitedActionItem alloc]
      initWithCollectionShortcutType:NTPCollectionShortcutTypeReadingList];
}

ContentSuggestionsMostVisitedActionItem* RecentTabsActionItem() {
  return [[ContentSuggestionsMostVisitedActionItem alloc]
      initWithCollectionShortcutType:NTPCollectionShortcutTypeRecentTabs];
}

ContentSuggestionsMostVisitedActionItem* HistoryActionItem() {
  return [[ContentSuggestionsMostVisitedActionItem alloc]
      initWithCollectionShortcutType:NTPCollectionShortcutTypeHistory];
}
