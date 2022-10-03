// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"

#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"
#import "ios/chrome/browser/ui/omnibox/popup/pedal_suggestion_wrapper.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_swift.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kOmniboxIconSize = 16;
}  // namespace

@interface OmniboxPopupMediator () <PedalSectionExtractorDelegate>

// Extracts pedals from AutocompleSuggestions.
@property(nonatomic, strong) PedalSectionExtractor* pedalSectionExtractor;
// List of suggestions without the pedal group. Used to debouce pedals.
@property(nonatomic, strong)
    NSArray<id<AutocompleteSuggestionGroup>>* nonPedalSuggestions;
// Index of the group containing AutocompleteSuggestion, first group to be
// highlighted on down arrow key.
@property(nonatomic, assign) NSInteger preselectedGroupIndex;

@end

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
    _pedalSectionExtractor = [[PedalSectionExtractor alloc] init];
    _pedalSectionExtractor.delegate = self;
    _preselectedGroupIndex = 0;
  }
  return self;
}

- (void)updateMatches:(const AutocompleteResult&)result {
  _currentResult.Reset();
  _currentResult.CopyFrom(result);
  self.nonPedalSuggestions = nil;

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
    (NSUInteger)visibleSuggestionCount {
  NSUInteger visibleSuggestions =
      MIN(visibleSuggestionCount, _currentResult.size());
  if (visibleSuggestions > 0) {
    // Groups visible suggestions by search vs url. Skip the first suggestion
    // because it's the omnibox content.
    [self groupCurrentSuggestionsFrom:1 to:visibleSuggestions];
  }
  // Groups hidden suggestions by search vs url.
  [self groupCurrentSuggestionsFrom:visibleSuggestions
                                 to:_currentResult.size()];

  NSArray<id<AutocompleteSuggestionGroup>>* groups = [self wrappedMatches];

  [self.consumer updateMatches:groups
      preselectedMatchGroupIndex:self.preselectedGroupIndex];

  [self loadModelImages];
}

#pragma mark - AutocompleteResultConsumerDelegate

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
               didSelectSuggestion:(id<AutocompleteSuggestion>)suggestion
                             inRow:(NSUInteger)row {
  if ([suggestion isKindOfClass:[PedalSuggestionWrapper class]]) {
    PedalSuggestionWrapper* pedalSuggestionWrapper =
        (PedalSuggestionWrapper*)suggestion;
    if (pedalSuggestionWrapper.innerPedal.action) {
      pedalSuggestionWrapper.innerPedal.action();
    }
  } else if ([suggestion isKindOfClass:[AutocompleteMatchFormatter class]]) {
    AutocompleteMatchFormatter* autocompleteMatchFormatter =
        (AutocompleteMatchFormatter*)suggestion;
    const AutocompleteMatch& match =
        autocompleteMatchFormatter.autocompleteMatch;

    // Don't log pastes in incognito.
    if (!self.incognito && match.type == AutocompleteMatchType::CLIPBOARD_URL) {
      [self.promoScheduler logUserPastedInOmnibox];
    }

    _delegate->OnMatchSelected(match, row, WindowOpenDisposition::CURRENT_TAB);
  } else {
    NOTREACHED() << "Suggestion type " << NSStringFromClass(suggestion.class)
                 << " not handled for selection.";
  }
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
    didTapTrailingButtonOnSuggestion:(id<AutocompleteSuggestion>)suggestion
                               inRow:(NSUInteger)row {
  if ([suggestion isKindOfClass:[AutocompleteMatchFormatter class]]) {
    AutocompleteMatchFormatter* autocompleteMatchFormatter =
        (AutocompleteMatchFormatter*)suggestion;
    const AutocompleteMatch& match =
        autocompleteMatchFormatter.autocompleteMatch;
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
  } else {
    NOTREACHED() << "Suggestion type " << NSStringFromClass(suggestion.class)
                 << " not handled for trailing button tap.";
  }
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
    didSelectSuggestionForDeletion:(id<AutocompleteSuggestion>)suggestion
                             inRow:(NSUInteger)row {
  if ([suggestion isKindOfClass:[AutocompleteMatchFormatter class]]) {
    AutocompleteMatchFormatter* autocompleteMatchFormatter =
        (AutocompleteMatchFormatter*)suggestion;
    const AutocompleteMatch& match =
        autocompleteMatchFormatter.autocompleteMatch;
    _delegate->OnMatchSelectedForDeletion(match);
  } else {
    NOTREACHED() << "Suggestion type " << NSStringFromClass(suggestion.class)
                 << " not handled for deletion.";
  }
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

#pragma mark - PedalSectionExtractorDelegate

// Removes the pedal group from suggestions. Pedal are removed from suggestions
// with a debouce timer in `PedalSectionExtractor`. When the timer ends the
// pedal group is removed.
- (void)invalidatePedals {
  if (self.nonPedalSuggestions) {
    [self.consumer updateMatches:self.nonPedalSuggestions
        preselectedMatchGroupIndex:0];
  }
}

#pragma mark - Private methods

// Wraps `match` with AutocompleteMatchFormatter.
- (id<AutocompleteSuggestion>)wrapMatch:(const AutocompleteMatch&)match {
  AutocompleteMatchFormatter* formatter =
      [AutocompleteMatchFormatter formatterWithMatch:match];
  formatter.starred = _delegate->IsStarredMatch(match);
  formatter.incognito = _incognito;
  formatter.defaultSearchEngineIsGoogle = self.defaultSearchEngineIsGoogle;
  formatter.pedalData = [self.pedalAnnotator pedalForMatch:match
                                                 incognito:_incognito];
  return formatter;
}

// Extracts tiles from AutocompleteMatch of type TILE_NAVSUGGEST.
- (id<AutocompleteSuggestionGroup>)extractTiles:
    (const AutocompleteMatch&)match {
  DCHECK(match.type == AutocompleteMatchType::TILE_NAVSUGGEST);
  DCHECK(base::FeatureList::IsEnabled(omnibox::kMostVisitedTiles));

  NSMutableArray<id<AutocompleteSuggestion>>* wrappedTiles =
      [[NSMutableArray alloc] init];

  for (const AutocompleteMatch::SuggestTile& tile : match.suggest_tiles) {
    AutocompleteMatch tileMatch = AutocompleteMatch(match);
    // TODO(crbug.com/1363546): replace with a new wrapper.
    tileMatch.destination_url = tile.url;
    tileMatch.fill_into_edit = base::UTF8ToUTF16(tile.url.spec());
    tileMatch.description = tile.title;
    [wrappedTiles addObject:[self wrapMatch:tileMatch]];
  }

  id<AutocompleteSuggestionGroup> tileGroup = [AutocompleteSuggestionGroupImpl
      groupWithTitle:nil
         suggestions:wrappedTiles
        displayStyle:SuggestionGroupDisplayStyleCarousel];

  return tileGroup;
}

// Unpacks AutocompleteMatch into wrapped AutocompleteSuggestion and
// AutocompleteSuggestionGroup. Sets `preselectedGroupIndex`.
- (NSArray<id<AutocompleteSuggestionGroup>>*)wrappedMatches {
  id<AutocompleteSuggestionGroup> tileGroup = nil;

  NSMutableArray<id<AutocompleteSuggestion>>* wrappedSuggestions =
      [[NSMutableArray alloc] init];

  size_t size = _currentResult.size();
  for (size_t i = 0; i < size; i++) {
    const AutocompleteMatch& match =
        ((const AutocompleteResult&)_currentResult).match_at((NSUInteger)i);
    if (match.type == AutocompleteMatchType::TILE_NAVSUGGEST) {
      DCHECK(!tileGroup) << "There should be only one TILE_NAVSUGGEST";
      tileGroup = [self extractTiles:match];
    } else {
      [wrappedSuggestions addObject:[self wrapMatch:match]];
    }
  }

  id<AutocompleteSuggestionGroup> pedalGroup =
      [self.pedalSectionExtractor extractPedals:wrappedSuggestions];
  id<AutocompleteSuggestionGroup> suggestionGroup =
      [AutocompleteSuggestionGroupImpl groupWithTitle:nil
                                          suggestions:wrappedSuggestions];

  NSMutableArray<id<AutocompleteSuggestionGroup>>* nonPedalGroups =
      [[NSMutableArray alloc] init];

  [nonPedalGroups addObject:suggestionGroup];
  if (tileGroup) {
    [nonPedalGroups addObject:tileGroup];
  }
  self.nonPedalSuggestions = nonPedalGroups;
  NSArray<id<AutocompleteSuggestionGroup>>* groups;
  if (pedalGroup) {
    groups = [@[ pedalGroup ] arrayByAddingObjectsFromArray:nonPedalGroups];
  } else {
    groups = nonPedalGroups;
  }
  self.preselectedGroupIndex = [groups indexOfObject:suggestionGroup];
  return groups;
}

- (void)groupCurrentSuggestionsFrom:(NSUInteger)begin to:(NSUInteger)end {
  DCHECK(begin <= _currentResult.size());
  DCHECK(end <= _currentResult.size());
  AutocompleteResult::GroupSuggestionsBySearchVsURL(
      std::next(_currentResult.begin(), begin),
      std::next(_currentResult.begin(), end));
}

@end
