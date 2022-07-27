// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/image_fetcher/core/image_data_fetcher.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_swift.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kOmniboxIconSize = 16;
}  // namespace

@implementation OmniboxPopupMediator {
  // Fetcher for Answers in Suggest images.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;

  OmniboxPopupMediatorDelegate* _delegate;  // weak

  AutocompleteResult _currentResult;
}
@synthesize consumer = _consumer;
@synthesize hasResults = _hasResults;
@synthesize incognito = _incognito;
@synthesize open = _open;
@synthesize presenter = _presenter;

- (instancetype)initWithFetcher:
                    (std::unique_ptr<image_fetcher::ImageDataFetcher>)
                        imageFetcher
                  faviconLoader:(FaviconLoader*)faviconLoader
                       delegate:(OmniboxPopupMediatorDelegate*)delegate {
  self = [super init];
  if (self) {
    DCHECK(delegate);
    _delegate = delegate;
    _imageFetcher = std::move(imageFetcher);
    _faviconLoader = faviconLoader;
    _open = NO;
  }
  return self;
}

- (void)updateMatches:(const AutocompleteResult&)result {
  _currentResult.Reset();
  _currentResult.CopyFrom(result);

  self.hasResults = !_currentResult.empty();
  if (base::FeatureList::IsEnabled(omnibox::kAdaptiveSuggestionsCount)) {
    [self.consumer newResultsAvailable];
  } else {
    // Avoid calling consumer visible size and set all suggestions as visible to
    // get only one grouping.
    [self requestResultsWithVisibleSuggestionCount:_currentResult.size()];
  }
}

- (void)updateWithResults:(const AutocompleteResult&)result {
  [self updateMatches:result];
  self.open = !result.empty();
  [self.presenter updatePopup];
}

- (void)setTextAlignment:(NSTextAlignment)alignment {
  [self.consumer setTextAlignment:alignment];
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  [self.consumer setSemanticContentAttribute:semanticContentAttribute];
}

#pragma mark - AutocompleteResultDataSource

- (void)requestResultsWithVisibleSuggestionCount:
    (NSInteger)visibleSuggestionCount {
  size_t visibleSuggestions =
      MIN(visibleSuggestionCount, (NSInteger)_currentResult.size());
  if (visibleSuggestions > 0) {
    // Groups visible suggestions by search vs url. Skip the first suggestion
    // because it's the omnibox content.
    [self groupCurrentSuggestionsFrom:1 to:visibleSuggestions];
  }
  // Groups hidden suggestions by search vs url.
  [self groupCurrentSuggestionsFrom:visibleSuggestions
                                 to:_currentResult.size()];

  NSArray<id<AutocompleteSuggestion>>* matches = [self wrappedMatches];

  [self.consumer updateMatches:@[ [AutocompleteSuggestionGroupImpl
                                   groupWithTitle:nil
                                      suggestions:matches] ]
      preselectedMatchGroupIndex:0];

  [self loadModelImages];
}

#pragma mark - AutocompleteResultConsumerDelegate

- (void)autocompleteResultConsumerCancelledHighlighting:
    (id<AutocompleteResultConsumer>)sender {
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                   didHighlightRow:(NSUInteger)row
                         inSection:(NSUInteger)section {
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                      didSelectRow:(NSUInteger)row
                         inSection:(NSUInteger)section {
  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, `match` and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);

  // Don't log pastes in incognito.
  if (!self.incognito && match.type == AutocompleteMatchType::CLIPBOARD_URL) {
    [self.promoScheduler logUserPastedInOmnibox];
  }

  _delegate->OnMatchSelected(match, row, WindowOpenDisposition::CURRENT_TAB);
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
        didTapTrailingButtonForRow:(NSUInteger)row
                         inSection:(NSUInteger)section {
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);

  if (match.has_tab_match.value_or(false)) {
    _delegate->OnMatchSelected(match, row,
                               WindowOpenDisposition::SWITCH_TO_TAB);
  } else {
    if (AutocompleteMatch::IsSearchType(match.type)) {
      base::RecordAction(
          base::UserMetricsAction("MobileOmniboxRefineSuggestion.Search"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("MobileOmniboxRefineSuggestion.Url"));
    }
    _delegate->OnMatchSelectedForAppending(match);
  }
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
           didSelectRowForDeletion:(NSUInteger)row
                         inSection:(NSUInteger)section {
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);
  _delegate->OnMatchSelectedForDeletion(match);
}

- (void)autocompleteResultConsumerDidScroll:
    (id<AutocompleteResultConsumer>)sender {
  _delegate->OnScroll();
}

- (void)loadModelImages {
  for (PopupMatchSection* section in self.model.sections) {
    for (PopupMatch* match in section.matches) {
      PopupImage* popupImage = match.image;
      switch (popupImage.icon.iconType) {
        case OmniboxIconTypeSuggestionIcon:
          break;
        case OmniboxIconTypeImage: {
          [self fetchImage:popupImage.icon.imageURL.gurl
                completion:^(UIImage* image) {
                  popupImage.iconUIImageFromURL = image;
                }];
          break;
        }
        case OmniboxIconTypeFavicon: {
          [self fetchFavicon:popupImage.icon.imageURL.gurl
                  completion:^(UIImage* image) {
                    popupImage.iconUIImageFromURL = image;
                  }];
          break;
        }
      }
    }
  }
}

#pragma mark - ImageFetcher

- (void)fetchImage:(GURL)imageURL completion:(void (^)(UIImage*))completion {
  auto callback =
      base::BindOnce(^(const std::string& image_data,
                       const image_fetcher::RequestMetadata& metadata) {
        NSData* data = [NSData dataWithBytes:image_data.data()
                                      length:image_data.size()];
        if (data) {
          UIImage* image = [UIImage imageWithData:data
                                            scale:[UIScreen mainScreen].scale];
          completion(image);
        } else {
          completion(nil);
        }
      });

  _imageFetcher->FetchImageData(imageURL, std::move(callback),
                                NO_TRAFFIC_ANNOTATION_YET);
}

#pragma mark - FaviconRetriever

- (void)fetchFavicon:(GURL)pageURL completion:(void (^)(UIImage*))completion {
  if (!self.faviconLoader) {
    return;
  }

  self.faviconLoader->FaviconForPageUrl(
      pageURL, kOmniboxIconSize, kOmniboxIconSize,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        if (attributes.faviconImage && !attributes.usesDefaultImage)
          completion(attributes.faviconImage);
      });
}

#pragma mark - Private methods

- (NSArray<id<AutocompleteSuggestion>>*)wrappedMatches {
  NSMutableArray<id<AutocompleteSuggestion>>* wrappedMatches =
      [[NSMutableArray alloc] init];

  size_t size = _currentResult.size();
  for (size_t i = 0; i < size; i++) {
    const AutocompleteMatch& match =
        ((const AutocompleteResult&)_currentResult).match_at((NSUInteger)i);
    AutocompleteMatchFormatter* formatter =
        [AutocompleteMatchFormatter formatterWithMatch:match];
    formatter.starred = _delegate->IsStarredMatch(match);
    formatter.incognito = _incognito;
    formatter.defaultSearchEngineIsGoogle = self.defaultSearchEngineIsGoogle;
    formatter.pedalData = [self.pedalAnnotator pedalForMatch:match
                                                   incognito:_incognito];
    [wrappedMatches addObject:formatter];
  }

  return wrappedMatches;
}

- (void)groupCurrentSuggestionsFrom:(NSUInteger)begin to:(NSUInteger)end {
  AutocompleteResult::GroupSuggestionsBySearchVsURL(
      std::next(_currentResult.begin(), begin),
      std::next(_currentResult.begin(), end));
}

@end
