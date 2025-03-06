// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/autocomplete_match_wrapper.h"

#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_match_formatter.h"
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

- (AutocompleteMatchFormatter*)wrapMatch:(const AutocompleteMatch&)match
                              fromResult:(const AutocompleteResult&)result
                               isStarred:(BOOL)isStarred {
  AutocompleteMatchFormatter* formatter =
      [AutocompleteMatchFormatter formatterWithMatch:match];
  formatter.starred = isStarred;
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

@end
