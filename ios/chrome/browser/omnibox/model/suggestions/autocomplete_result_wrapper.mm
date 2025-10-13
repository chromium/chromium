// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_result_wrapper.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_classification.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/aim/model/aim_availability.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_match_formatter.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_result_wrapper_delegate.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/omnibox/model/suggestions/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/omnibox/model/suggestions/pedal_section_extractor.h"
#import "ios/chrome/browser/omnibox/model/suggestions/suggest_action.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "net/base/apple/url_conversions.h"

@interface AutocompleteResultWrapper () <PedalSectionExtractorDelegate,
                                         SearchEngineObserving>
@end

@implementation AutocompleteResultWrapper {
  /// Search engine observer.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  /// Whether the default search engine is Google.
  BOOL _defaultSearchEngineIsGoogle;
  /// Extracts pedals from AutocompleSuggestions.
  PedalSectionExtractor* _pedalSectionExtractor;
  /// List of suggestions without the pedal group. Used to debounce pedals.
  NSArray<id<AutocompleteSuggestionGroup>>* _nonPedalSuggestionsGroups;
  /// The omnibox client.
  base::WeakPtr<OmniboxClient> _omniboxClient;
  /// The autocomplete client.
  base::WeakPtr<AutocompleteProviderClient> _autocompleteProviderClient;
}

- (instancetype)initWithOmniboxClient:(OmniboxClient*)omniboxClient
           autocompleteProviderClient:
               (AutocompleteProviderClient*)autocompleteProviderClient {
  self = [super init];
  if (self) {
    _omniboxClient = omniboxClient->AsWeakPtr();
    _autocompleteProviderClient = autocompleteProviderClient->GetWeakPtr();
    _pedalSectionExtractor = [[PedalSectionExtractor alloc] init];
    _pedalSectionExtractor.delegate = self;
  }
  return self;
}

- (void)disconnect {
  _searchEngineObserver.reset();
  _omniboxClient = nullptr;
  _autocompleteProviderClient = nullptr;
}

- (NSArray<id<AutocompleteSuggestionGroup>>*)wrapAutocompleteResultInGroups:
    (const AutocompleteResult&)autocompleteResult {
  NSMutableArray<id<AutocompleteSuggestionGroup>>* groups =
      [[NSMutableArray alloc] init];

  // Group the suggestions by the section Id.
  NSMutableArray<AutocompleteMatchFormatter*>* allMatches =
      [self wrapMatchesFromResult:autocompleteResult];
  NSArray<id<AutocompleteSuggestionGroup>>* allGroups =
      [self groupSuggestions:allMatches
          usingACResultAsHeaderMap:autocompleteResult];
  [groups addObjectsFromArray:allGroups];

  // Before inserting pedals above all, back up non-pedal suggestions for
  // debouncing.
  _nonPedalSuggestionsGroups = [NSArray arrayWithArray:groups];

  // Get pedals, if any. They go at the very top of the list.
  id<AutocompleteSuggestionGroup> pedalSuggestionsGroup =
      [_pedalSectionExtractor extractPedals:allMatches];
  if (pedalSuggestionsGroup) {
    [groups insertObject:pedalSuggestionsGroup atIndex:0];
  }

  return groups;
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  if (self.templateURLService) {
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    [self searchEngineChanged];
  } else {
    _searchEngineObserver.reset();
  }
}

#pragma mark - PedalSectionExtractorDelegate

- (void)invalidatePedals {
  if (_nonPedalSuggestionsGroups) {
    [self.delegate autocompleteResultWrapper:self
                         didInvalidatePedals:_nonPedalSuggestionsGroups];
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  TemplateURLService* templateURLService = self.templateURLService;
  _defaultSearchEngineIsGoogle =
      templateURLService && templateURLService->GetDefaultSearchProvider() &&
      templateURLService->GetDefaultSearchProvider()->GetEngineType(
          templateURLService->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
}

#pragma mark - Private

/// Wraps `match` with AutocompleteMatchFormatter.
- (AutocompleteMatchFormatter*)wrapMatch:(const AutocompleteMatch&)match
                              fromResult:(const AutocompleteResult&)result {
  AutocompleteMatchFormatter* formatter =
      [AutocompleteMatchFormatter formatterWithMatch:match];
  formatter.starred = [self isStarredMatch:match];
  formatter.incognito = self.incognito;
  formatter.defaultSearchEngineIsGoogle = _defaultSearchEngineIsGoogle;
  formatter.pedalData = [self.pedalAnnotator pedalForMatch:match];
  formatter.isMultimodal = self.hasThumbnail;

  if (formatter.suggestionGroupId) {
    omnibox::GroupId groupId =
        static_cast<omnibox::GroupId>(formatter.suggestionGroupId.intValue);
    omnibox::GroupSection sectionId =
        result.GetSectionForSuggestionGroup(groupId);
    formatter.suggestionSectionId =
        [NSNumber numberWithInt:static_cast<int>(sectionId)];
  }

  NSMutableArray* actions = [[NSMutableArray alloc] init];

  for (auto& action : match.actions) {
    SuggestAction* suggestAction =
        [SuggestAction actionWithOmniboxAction:action.get()];

    if (!suggestAction) {
      continue;
    }

    switch (suggestAction.type) {
      case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CALL: {
        BOOL hasDialApp = [[UIApplication sharedApplication]
            canOpenURL:net::NSURLWithGURL(suggestAction.actionURI)];
        if (hasDialApp) {
          [actions addObject:suggestAction];
        }
        break;
      }
      case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS:
      case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS:
        [actions addObject:suggestAction];
        break;
      default:
        break;
    }
  }

  formatter.actionsInSuggest = actions;

  return formatter;
}

/// Wraps the autocomplete results from the given AutocompleteResult object into
/// an array of AutocompleteSuggestion objects.
- (NSMutableArray<AutocompleteMatchFormatter*>*)wrapMatchesFromResult:
    (const AutocompleteResult&)autocompleteResult {
  NSMutableArray<AutocompleteMatchFormatter*>* wrappedMatches =
      [[NSMutableArray alloc] init];

  // TODO(crbug.com/439796782): If multiple matches with `TILE_NAVSUGGEST` are
  // present in the autocomplete result, process only the first on the one.
  BOOL tileNavSuggestHandled = NO;
  for (size_t i = 0; i < autocompleteResult.size(); i++) {
    const AutocompleteMatch& match = autocompleteResult.match_at((NSUInteger)i);
    if (match.type == AutocompleteMatchType::TILE_NAVSUGGEST) {
      if (tileNavSuggestHandled) {
        continue;
      }
      tileNavSuggestHandled = YES;
      DCHECK(match.type == AutocompleteMatchType::TILE_NAVSUGGEST);
      for (const AutocompleteMatch::SuggestTile& tile : match.suggest_tiles) {
        AutocompleteMatch tileMatch = AutocompleteMatch(match);
        tileMatch.destination_url = tile.url;
        tileMatch.fill_into_edit = base::UTF8ToUTF16(tile.url.spec());
        tileMatch.description = tile.title;
        tileMatch.description_class = ClassifyTermMatches(
            {}, tileMatch.description.length(), 0, ACMatchClassification::NONE);
#if DCHECK_IS_ON()
        tileMatch.Validate();
#endif  // DCHECK_IS_ON()
        AutocompleteMatchFormatter* formatter =
            [self wrapMatch:tileMatch fromResult:autocompleteResult];
        [wrappedMatches addObject:formatter];
      }
    } else {
      [wrappedMatches addObject:[self wrapMatch:match
                                     fromResult:autocompleteResult]];
    }
  }

  return wrappedMatches;
}

/// Take a list of suggestions and break it into groups determined by sectionId
/// field. Use `headerMap` to extract group names.
- (NSArray<id<AutocompleteSuggestionGroup>>*)
            groupSuggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
    usingACResultAsHeaderMap:(const AutocompleteResult&)headerMap {
  __block NSMutableArray<id<AutocompleteSuggestion>>* currentGroup =
      [[NSMutableArray alloc] init];
  NSMutableArray<id<AutocompleteSuggestionGroup>>* groups =
      [[NSMutableArray alloc] init];

  if (suggestions.count == 0) {
    return @[];
  }

  id<AutocompleteSuggestion> firstSuggestion = suggestions.firstObject;

  __block NSNumber* currentSectionId = firstSuggestion.suggestionSectionId;
  __block NSNumber* currentGroupId = firstSuggestion.suggestionGroupId;

  [currentGroup addObject:firstSuggestion];

  void (^startNewGroup)() = ^{
    if (currentGroup.count == 0) {
      return;
    }

    NSString* groupTitle =
        currentGroupId
            ? base::SysUTF16ToNSString(headerMap.GetHeaderForSuggestionGroup(
                  static_cast<omnibox::GroupId>([currentGroupId intValue])))
            : nil;
    SuggestionGroupDisplayStyle displayStyle =
        SuggestionGroupDisplayStyleDefault;

    if (base::FeatureList::IsEnabled(
            omnibox::kMostVisitedTilesHorizontalRenderGroup)) {
      omnibox::GroupConfig_RenderType renderType =
          headerMap.GetRenderTypeForSuggestionGroup(
              static_cast<omnibox::GroupId>([currentGroupId intValue]));
      displayStyle = (renderType == omnibox::GroupConfig_RenderType_HORIZONTAL)
                         ? SuggestionGroupDisplayStyleCarousel
                         : SuggestionGroupDisplayStyleDefault;
    } else if (currentSectionId &&
               static_cast<omnibox::GroupSection>(currentSectionId.intValue) ==
                   omnibox::SECTION_MOBILE_MOST_VISITED) {
      displayStyle = SuggestionGroupDisplayStyleCarousel;
    }

    SuggestionGroupType groupType =
        SuggestionGroupType::kUnspecifiedSuggestionGroup;

    if (displayStyle == SuggestionGroupDisplayStyleCarousel) {
      groupType = SuggestionGroupType::kMVTilesSuggestionGroup;
    }

    [groups
        addObject:[AutocompleteSuggestionGroupImpl groupWithTitle:groupTitle
                                                      suggestions:currentGroup
                                                     displayStyle:displayStyle
                                                             type:groupType]];
    currentGroup = [[NSMutableArray alloc] init];
  };

  for (NSUInteger i = 1; i < suggestions.count; i++) {
    id<AutocompleteSuggestion> suggestion = suggestions[i];
    if ((!suggestion.suggestionSectionId && !currentSectionId) ||
        [suggestion.suggestionSectionId isEqual:currentSectionId]) {
      [currentGroup addObject:suggestion];
    } else {
      startNewGroup();
      currentGroupId = suggestion.suggestionGroupId;
      currentSectionId = suggestion.suggestionSectionId;
      [currentGroup addObject:suggestion];
    }
  }
  startNewGroup();

  return groups;
}

/// Whether `match` is a starred/bookmarked match.
- (BOOL)isStarredMatch:(const AutocompleteMatch&)match {
  if (_omniboxClient) {
    auto* bookmark_model = _omniboxClient->GetBookmarkModel();
    return bookmark_model &&
           bookmark_model->IsBookmarked(match.destination_url);
  }
  return NO;
}

@end
