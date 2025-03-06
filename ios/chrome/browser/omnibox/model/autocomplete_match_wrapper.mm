// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/autocomplete_match_wrapper.h"

#import "base/strings/utf_string_conversions.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_classification.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_match_wrapper_delegate.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/row/actions/suggest_action.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "net/base/apple/url_conversions.h"

@interface AutocompleteMatchWrapper () <SearchEngineObserving>
@end

@implementation AutocompleteMatchWrapper {
  /// Search engine observer.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  /// Whether the default search engine is Google.
  BOOL _defaultSearchEngineIsGoogle;
}

- (void)disconnect {
  _searchEngineObserver.reset();
}

- (NSMutableArray<AutocompleteMatchFormatter*>*)wrapMatchesFromResult:
    (const AutocompleteResult&)autocompleteResult {
  NSMutableArray<AutocompleteMatchFormatter*>* wrappedMatches =
      [[NSMutableArray alloc] init];
  for (size_t i = 0; i < autocompleteResult.size(); i++) {
    const AutocompleteMatch& match = autocompleteResult.match_at((NSUInteger)i);
    if (match.type == AutocompleteMatchType::TILE_NAVSUGGEST) {
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
  formatter.starred = [self.delegate isStarredMatch:match];
  formatter.incognito = self.isIncognito;
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

    if (suggestAction.type != omnibox::ActionInfo_ActionType_CALL) {
      [actions addObject:suggestAction];
      continue;
    }

    BOOL hasDialApp = [[UIApplication sharedApplication]
        canOpenURL:net::NSURLWithGURL(suggestAction.actionURI)];
    if (hasDialApp) {
      [actions addObject:suggestAction];
    }
  }

  formatter.actionsInSuggest = actions;

  return formatter;
}

@end
