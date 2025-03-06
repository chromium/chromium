// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_popup_mediator.h"

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/omnibox/browser/actions/omnibox_action_concepts.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_classification.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/remote_suggestions_service.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/strings/grit/components_strings.h"
#import "components/variations/variations_associated_data.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_match_wrapper.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_controller_observer_bridge.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/carousel/carousel_item.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/carousel/carousel_item_menu_provider.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_popup_mediator+Testing.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/pedal_section_extractor.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/pedal_suggestion_wrapper.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/popup_debug_info_consumer.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/popup_swift.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/remote_suggestions_service_observer_bridge.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/row/actions/suggest_action.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_omnibox_consumer.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kOmniboxIconSize = 16;
/// Maximum number of suggest tile types we want to record. Anything beyond this
/// will be reported in the overflow bucket.
const NSUInteger kMaxSuggestTileTypePosition = 15;
}  // namespace

@interface OmniboxPopupMediator () <PedalSectionExtractorDelegate>

// FET reference.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;
/// Extracts pedals from AutocompleSuggestions.
@property(nonatomic, strong) PedalSectionExtractor* pedalSectionExtractor;
/// List of suggestions without the pedal group. Used to debouce pedals.
@property(nonatomic, strong)
    NSArray<id<AutocompleteSuggestionGroup>>* nonPedalSuggestions;
/// Holds the currently displayed pedals group, if any.
@property(nonatomic, strong) id<AutocompleteSuggestionGroup> currentPedals;
/// Index of the group containing AutocompleteSuggestion, first group to be
/// highlighted on down arrow key.
@property(nonatomic, assign) NSInteger preselectedGroupIndex;

// Autocomplete controller backing this mediator.
// It is observed through OmniboxPopupViewIOS.
@property(nonatomic, assign) AutocompleteController* autocompleteController;

// Remote suggestions service backing `autocompleteController`. Observed in
// debug mode.
@property(nonatomic, assign) RemoteSuggestionsService* remoteSuggestionsService;

// Whether the omnibox has a thumbnail.
@property(nonatomic, assign) BOOL hasThumbnail;

@end

@implementation OmniboxPopupMediator {
  // Fetcher for Answers in Suggest images.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;

  std::unique_ptr<AutocompleteControllerObserverBridge>
      _autocompleteObserverBridge;
  std::unique_ptr<RemoteSuggestionsServiceObserverBridge>
      _remoteSuggestionsServiceObserverBridge;

  /// Holds cached images keyed by their URL. The cache is purged when the popup
  /// is closed.
  NSCache<NSString*, UIImage*>* _cachedImages;
}
@synthesize consumer = _consumer;
@synthesize incognito = _incognito;
@synthesize presenter = _presenter;

- (instancetype)
             initWithFetcher:
                 (std::unique_ptr<image_fetcher::ImageDataFetcher>)imageFetcher
               faviconLoader:(FaviconLoader*)faviconLoader
      autocompleteController:(AutocompleteController*)autocompleteController
    remoteSuggestionsService:(RemoteSuggestionsService*)remoteSuggestionsService
                     tracker:(feature_engagement::Tracker*)tracker {
  self = [super init];
  if (self) {
    DCHECK(autocompleteController);
    _imageFetcher = std::move(imageFetcher);
    _faviconLoader = faviconLoader;
    _open = NO;
    _pedalSectionExtractor = [[PedalSectionExtractor alloc] init];
    _pedalSectionExtractor.delegate = self;
    _preselectedGroupIndex = 0;
    _autocompleteController = autocompleteController;
    _remoteSuggestionsService = remoteSuggestionsService;
    _tracker = tracker;
    _cachedImages = [[NSCache alloc] init];
  }
  return self;
}

- (void)disconnect {
  [self.autocompleteMatchWrapper disconnect];
  if (_remoteSuggestionsServiceObserverBridge) {
    self.remoteSuggestionsService->RemoveObserver(
        _remoteSuggestionsServiceObserverBridge.get());
    _remoteSuggestionsServiceObserverBridge.reset();
  }
}

- (void)setDebugInfoConsumer:
    (id<PopupDebugInfoConsumer,
        RemoteSuggestionsServiceObserver,
        AutocompleteControllerObserver>)debugInfoConsumer {
  DCHECK(experimental_flags::IsOmniboxDebuggingEnabled());

  _autocompleteObserverBridge =
      std::make_unique<AutocompleteControllerObserverBridge>(debugInfoConsumer);
  self.autocompleteController->AddObserver(_autocompleteObserverBridge.get());

  // Observe the remote suggestions service if it's available. It might not
  // be available e.g. in incognito.
  if (self.remoteSuggestionsService) {
    _remoteSuggestionsServiceObserverBridge =
        std::make_unique<RemoteSuggestionsServiceObserverBridge>(
            debugInfoConsumer, self.remoteSuggestionsService);
    self.remoteSuggestionsService->AddObserver(
        _remoteSuggestionsServiceObserverBridge.get());
  }

  _debugInfoConsumer = debugInfoConsumer;
}

- (void)setOpen:(BOOL)open {
  // When closing the popup.
  if (_open && !open) {
    [_cachedImages removeAllObjects];
    [_debugInfoConsumer removeAllObjects];
  }
  _open = open;
}

#pragma mark - OmniboxPopupControllerDelegate

- (void)popupControllerDidUpdateSuggestions:
            (OmniboxPopupController*)popupController
                             hasSuggestions:(BOOL)hasSuggestions
                                 isFocusing:(BOOL)isFocusing {
  self.nonPedalSuggestions = nil;
  self.currentPedals = nil;

  [self.consumer newResultsAvailable];

  if (self.debugInfoConsumer) {
    DCHECK(experimental_flags::IsOmniboxDebuggingEnabled());

    [self.debugInfoConsumer
        setVariationIDString:
            base::SysUTF8ToNSString(
                variations::VariationsIdsProvider::GetInstance()
                    ->GetTriggerVariationsString())];
  }
  self.open = hasSuggestions;
  [self.presenter updatePopupOnFocus:isFocusing];
}

- (void)popupController:(OmniboxPopupController*)popupController
         didSortResults:(const AutocompleteResult&)result {
  NSArray<id<AutocompleteSuggestionGroup>>* groups =
      [self wrappedMatches:result];

  [self.consumer updateMatches:groups
      preselectedMatchGroupIndex:self.preselectedGroupIndex];
}

- (void)popupController:(OmniboxPopupController*)popupController
    didUpdateTextAlignment:(NSTextAlignment)alignment {
  [self.consumer setTextAlignment:alignment];
}

- (void)popupController:(OmniboxPopupController*)popupController
    didUpdateSemanticContentAttribute:
        (UISemanticContentAttribute)semanticContentAttribute {
  [self.consumer setSemanticContentAttribute:semanticContentAttribute];
}

- (void)popupController:(OmniboxPopupController*)popupController
    didUpdateHasThumbnail:(BOOL)hasThumbnail {
  self.hasThumbnail = hasThumbnail;
  self.autocompleteMatchWrapper.hasThumbnail = hasThumbnail;
}

#pragma mark - AutocompleteResultDataSource

- (void)requestResultsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount {
  [self.popupController
      requestSuggestionsWithVisibleSuggestionCount:visibleSuggestionCount];
}

#pragma mark - AutocompleteResultConsumerDelegate

- (void)autocompleteResultConsumerDidChangeTraitCollection:
    (id<AutocompleteResultConsumer>)sender {
  [self.presenter updatePopupAfterTraitCollectionChange];
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
               didSelectSuggestion:(id<AutocompleteSuggestion>)suggestion
                             inRow:(NSUInteger)row {
  [self logPedalShownForCurrentResult];

  // Log the suggest actions that were shown and not used.
  if (suggestion.actionsInSuggest.count == 0) {
    [self logActionsInSuggestShownForCurrentResult];
  }

  if ([suggestion isKindOfClass:[PedalSuggestionWrapper class]]) {
    PedalSuggestionWrapper* pedalSuggestionWrapper =
        (PedalSuggestionWrapper*)suggestion;
    if (pedalSuggestionWrapper.innerPedal.action) {
      base::UmaHistogramEnumeration(
          "Omnibox.SuggestionUsed.Pedal",
          (OmniboxPedalId)pedalSuggestionWrapper.innerPedal.type,
          OmniboxPedalId::TOTAL_COUNT);
      if ((OmniboxPedalId)pedalSuggestionWrapper.innerPedal.type ==
          OmniboxPedalId::MANAGE_PASSWORDS) {
        base::UmaHistogramEnumeration(
            "PasswordManager.ManagePasswordsReferrer",
            password_manager::ManagePasswordsReferrer::kOmniboxPedalSuggestion);
      }
      pedalSuggestionWrapper.innerPedal.action();
    }
  } else if ([suggestion isKindOfClass:[AutocompleteMatchFormatter class]]) {
    AutocompleteMatchFormatter* autocompleteMatchFormatter =
        (AutocompleteMatchFormatter*)suggestion;
    const AutocompleteMatch& match =
        autocompleteMatchFormatter.autocompleteMatch;

    // A search using clipboard link or text is activity that should indicate a
    // user that would be interested in setting the browser as the default.
    if (match.type == AutocompleteMatchType::CLIPBOARD_URL) {
      default_browser::NotifyOmniboxURLCopyPasteAndNavigate(
          self.incognito, self.tracker, self.sceneState);
    }
    if (match.type == AutocompleteMatchType::CLIPBOARD_TEXT) {
      default_browser::NotifyOmniboxTextCopyPasteAndNavigate(self.tracker);
    }

    if (!self.incognito &&
        match.type == AutocompleteMatchType::TILE_NAVSUGGEST) {
      [self logSelectedAutocompleteTile:match];
    }
    [self.popupController
        selectMatchForOpening:match
                        inRow:row
                       openIn:WindowOpenDisposition::CURRENT_TAB];
  } else {
    DUMP_WILL_BE_NOTREACHED()
        << "Suggestion type " << NSStringFromClass(suggestion.class)
        << " not handled for selection.";
  }
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
         didSelectSuggestionAction:(SuggestAction*)action
                        suggestion:(id<AutocompleteSuggestion>)suggestion
                             inRow:(NSUInteger)row {
  OmniboxActionInSuggest::RecordShownAndUsedMetrics(action.type,
                                                    true /* used */);

  switch (action.type) {
    case omnibox::ActionInfo_ActionType_CALL: {
      NSURL* URL = net::NSURLWithGURL(action.actionURI);
      __weak __typeof__(self) weakSelf = self;
      [[UIApplication sharedApplication] openURL:URL
                                         options:@{}
                               completionHandler:^(BOOL success) {
                                 if (success) {
                                   [weakSelf callActionTapped];
                                 }
                               }];
      break;
    }
    case omnibox::ActionInfo_ActionType_DIRECTIONS: {
      NSURL* URL = net::NSURLWithGURL(action.actionURI);

      if (IsGoogleMapsAppInstalled() && !self.incognito) {
        [[UIApplication sharedApplication] openURL:URL
                                           options:@{}
                                 completionHandler:nil];
      } else {
        [self openNewTabWithSuggestAction:action];
      }
      break;
    }
    case omnibox::ActionInfo_ActionType_REVIEWS: {
      [self openNewTabWithSuggestAction:action];
      break;
    }
    default:
      break;
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
      [self.popupController
          selectMatchForOpening:match
                          inRow:row
                         openIn:WindowOpenDisposition::SWITCH_TO_TAB];
    } else {
      if (AutocompleteMatch::IsSearchType(match.type)) {
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxRefineSuggestion.Search"));
      } else {
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxRefineSuggestion.Url"));
      }
      [self.popupController selectMatchForAppending:match];
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
    [self.popupController selectMatchForDeletion:match];
  } else {
    DUMP_WILL_BE_NOTREACHED()
        << "Suggestion type " << NSStringFromClass(suggestion.class)
        << " not handled for deletion.";
  }
}

- (void)autocompleteResultConsumerDidScroll:
    (id<AutocompleteResultConsumer>)sender {
  [self.popupController onScroll];
}

#pragma mark AutocompleteResultConsumerDelegate Private

/// Logs selected tile index and type.
- (void)logSelectedAutocompleteTile:(const AutocompleteMatch&)match {
  DCHECK(match.type == AutocompleteMatchType::TILE_NAVSUGGEST);
  for (size_t i = 0; i < match.suggest_tiles.size(); ++i) {
    const AutocompleteMatch::SuggestTile& tile = match.suggest_tiles[i];
    // AutocompleteMatch contains all tiles, find the tile corresponding to the
    // match. See how tiles are unwrapped in `extractMatches`.
    if (match.destination_url == tile.url) {
      // Log selected tile index. Note: When deleting a tile, the index may
      // shift, this is not taken into account.
      base::UmaHistogramExactLinear("Omnibox.SuggestTiles.SelectedTileIndex", i,
                                    kMaxSuggestTileTypePosition);
      int tileType =
          tile.is_search ? SuggestTileType::kSearch : SuggestTileType::kURL;
      base::UmaHistogramExactLinear("Omnibox.SuggestTiles.SelectedTileType",
                                    tileType, SuggestTileType::kCount);
      return;
    }
  }
}

#pragma mark - ImageFetcher

- (void)fetchImage:(GURL)imageURL completion:(void (^)(UIImage*))completion {
  NSString* URL = base::SysUTF8ToNSString(imageURL.spec());
  UIImage* cachedImage = [_cachedImages objectForKey:URL];
  if (cachedImage) {
    completion(cachedImage);
    return;
  }
  __weak NSCache<NSString*, UIImage*>* weakCachedImages = _cachedImages;
  auto callback =
      base::BindOnce(^(const std::string& image_data,
                       const image_fetcher::RequestMetadata& metadata) {
        NSData* data = [NSData dataWithBytes:image_data.data()
                                      length:image_data.size()];

        UIImage* image = [UIImage imageWithData:data
                                          scale:[UIScreen mainScreen].scale];
        if (image) {
          [weakCachedImages setObject:image forKey:URL];
        }
        completion(image);
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
        if (attributes.faviconImage && !attributes.usesDefaultImage) {
          completion(attributes.faviconImage);
        }
      });
}

#pragma mark - PedalSectionExtractorDelegate

/// Removes the pedal group from suggestions. Pedal are removed from suggestions
/// with a debouce timer in `PedalSectionExtractor`. When the timer ends the
/// pedal group is removed.
- (void)invalidatePedals {
  if (self.nonPedalSuggestions) {
    self.currentPedals = nil;
    [self.consumer updateMatches:self.nonPedalSuggestions
        preselectedMatchGroupIndex:0];
  }
}

#pragma mark - Private methods

- (void)logPedalShownForCurrentResult {
  for (PedalSuggestionWrapper* pedalMatch in self.currentPedals.suggestions) {
    base::UmaHistogramEnumeration("Omnibox.PedalShown",
                                  (OmniboxPedalId)pedalMatch.innerPedal.type,
                                  OmniboxPedalId::TOTAL_COUNT);
  }
}

/// Log action in suggest shown but not used for the current result.
- (void)logActionsInSuggestShownForCurrentResult {
  for (id<AutocompleteSuggestionGroup> group in self.nonPedalSuggestions) {
    for (id<AutocompleteSuggestion> suggestion in group.suggestions) {
      if (suggestion.actionsInSuggest.count == 0) {
        continue;
      }
      for (SuggestAction* action in suggestion.actionsInSuggest) {
        OmniboxActionInSuggest::RecordShownAndUsedMetrics(action.type,
                                                          false /* used */);
      }
    }
  }
}

/// Extract normal (non-tile) matches from `autocompleteResult`.
- (NSMutableArray<id<AutocompleteSuggestion>>*)extractMatches:
    (const AutocompleteResult&)autocompleteResult {
  NSMutableArray<id<AutocompleteSuggestion>>* wrappedMatches =
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
        AutocompleteMatchFormatter* formatter = [self.autocompleteMatchWrapper
             wrapMatch:tileMatch
            fromResult:autocompleteResult
             isStarred:[self.popupController isStarredMatch:match]];
        [wrappedMatches addObject:formatter];
      }
    } else {
      [wrappedMatches addObject:[self.autocompleteMatchWrapper
                                     wrapMatch:match
                                    fromResult:autocompleteResult
                                     isStarred:[self.popupController
                                                   isStarredMatch:match]]];
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

    [groups addObject:[AutocompleteSuggestionGroupImpl
                          groupWithTitle:groupTitle
                             suggestions:currentGroup
                            displayStyle:displayStyle]];
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

/// Unpacks AutocompleteMatch into wrapped AutocompleteSuggestion and
/// AutocompleteSuggestionGroup. Sets `preselectedGroupIndex`.
- (NSArray<id<AutocompleteSuggestionGroup>>*)wrappedMatches:
    (const AutocompleteResult&)autocompleteResult {
  NSMutableArray<id<AutocompleteSuggestionGroup>>* groups =
      [[NSMutableArray alloc] init];

  // Group the suggestions by the section Id.
  NSMutableArray<id<AutocompleteSuggestion>>* allMatches =
      [self extractMatches:autocompleteResult];
  NSArray<id<AutocompleteSuggestionGroup>>* allGroups =
      [self groupSuggestions:allMatches
          usingACResultAsHeaderMap:autocompleteResult];
  [groups addObjectsFromArray:allGroups];

  // Before inserting pedals above all, back up non-pedal suggestions for
  // debouncing.
  self.nonPedalSuggestions = groups;

  // Get pedals, if any. They go at the very top of the list.
  self.currentPedals = [self.pedalSectionExtractor extractPedals:allMatches];
  if (self.currentPedals) {
    [groups insertObject:self.currentPedals atIndex:0];
  }

  // Preselect the verbatim match. It's the top match, unless we inserted pedals
  // and pushed it one section down.
  self.preselectedGroupIndex = self.currentPedals ? MIN(1, groups.count) : 0;

  return groups;
}

- (void)callActionTapped {
  [self.popupController onCallAction];
}

#pragma mark - CarouselItemMenuProvider

/// Context Menu for carousel `item` in `view`.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForCarouselItem:(CarouselItem*)carouselItem
                                   fromView:(UIView*)view {
  __weak __typeof(self) weakSelf = self;
  __weak CarouselItem* weakItem = carouselItem;
  GURL copyURL = carouselItem.URL.gurl;

  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    DCHECK(weakSelf);

    __typeof(self) strongSelf = weakSelf;
    BrowserActionFactory* actionFactory = strongSelf.mostVisitedActionFactory;

    // Record that this context menu was shown to the user.
    RecordMenuShown(kMenuScenarioHistogramOmniboxMostVisitedEntry);

    NSMutableArray<UIMenuElement*>* menuElements =
        [[NSMutableArray alloc] init];

    [menuElements addObject:[actionFactory actionToOpenInNewTabWithURL:copyURL
                                                            completion:nil]];

    UIAction* incognitoAction =
        [actionFactory actionToOpenInNewIncognitoTabWithURL:copyURL
                                                 completion:nil];

    if (!self.allowIncognitoActions) {
      // Disable the "Open in Incognito" option if the incognito mode is
      // disabled.
      incognitoAction.attributes = UIMenuElementAttributesDisabled;
    }

    [menuElements addObject:incognitoAction];

    if (base::ios::IsMultipleScenesSupported()) {
      UIAction* newWindowAction = [actionFactory
          actionToOpenInNewWindowWithURL:copyURL
                          activityOrigin:
                              WindowActivityContentSuggestionsOrigin];
      [menuElements addObject:newWindowAction];
    }

    CrURL* URL = [[CrURL alloc] initWithGURL:copyURL];
    [menuElements addObject:[actionFactory actionToCopyURL:URL]];

    [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                    [weakSelf.sharingDelegate popupMediator:weakSelf
                                                   shareURL:copyURL
                                                      title:carouselItem.title
                                                 originView:view];
                  }]];

    [menuElements addObject:[actionFactory actionToRemoveWithBlock:^{
                    [weakSelf removeMostVisitedForURL:copyURL
                                     withCarouselItem:weakItem];
                  }]];

    return [UIMenu menuWithTitle:@"" children:menuElements];
  };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (NSArray<UIAccessibilityCustomAction*>*)
    accessibilityActionsForCarouselItem:(CarouselItem*)carouselItem
                               fromView:(UIView*)view {
  __weak __typeof(self) weakSelf = self;
  __weak CarouselItem* weakItem = carouselItem;
  __weak UIView* weakView = view;
  GURL copyURL = carouselItem.URL.gurl;

  NSMutableArray* actions = [[NSMutableArray alloc] init];

  {  // Open in new tab
    UIAccessibilityCustomActionHandler openInNewTabBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          [weakSelf openNewTabWithMostVisitedItem:weakItem incognito:NO];
          return YES;
        };
    UIAccessibilityCustomAction* openInNewTab =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(
                              IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
            actionHandler:openInNewTabBlock];
    [actions addObject:openInNewTab];
  }
  {  // Remove
    UIAccessibilityCustomActionHandler removeBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          [weakSelf removeMostVisitedForURL:copyURL withCarouselItem:weakItem];
          return YES;
        };
    UIAccessibilityCustomAction* removeMostVisited =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(
                              IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)
            actionHandler:removeBlock];
    [actions addObject:removeMostVisited];
  }
  if (self.allowIncognitoActions) {  // Open in new incognito tab
    UIAccessibilityCustomActionHandler openInNewIncognitoTabBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          [weakSelf openNewTabWithMostVisitedItem:weakItem incognito:YES];
          return YES;
        };
    UIAccessibilityCustomAction* openInIncognitoNewTab =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(
                              IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
            actionHandler:openInNewIncognitoTabBlock];
    [actions addObject:openInIncognitoNewTab];
  }
  if (base::ios::IsMultipleScenesSupported()) {  // Open in new window
    UIAccessibilityCustomActionHandler openInNewWindowBlock = ^BOOL(
        UIAccessibilityCustomAction*) {
      NSUserActivity* activity =
          ActivityToLoadURL(WindowActivityContentSuggestionsOrigin, copyURL);
      [weakSelf.applicationCommandsHandler openNewWindowWithActivity:activity];
      return YES;
    };
    UIAccessibilityCustomAction* newWindowAction =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(
                              IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW)
            actionHandler:openInNewWindowBlock];
    [actions addObject:newWindowAction];
  }
  {  // Copy
    UIAccessibilityCustomActionHandler copyBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          StoreURLInPasteboard(copyURL);
          return YES;
        };
    UIAccessibilityCustomAction* copyAction =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(IDS_IOS_COPY_LINK_ACTION_TITLE)
            actionHandler:copyBlock];
    [actions addObject:copyAction];
  }
  {  // Share
    UIAccessibilityCustomActionHandler shareBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          [weakSelf.sharingDelegate popupMediator:weakSelf
                                         shareURL:copyURL
                                            title:weakItem.title
                                       originView:weakView];
          return YES;
        };
    UIAccessibilityCustomAction* shareAction =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL)
            actionHandler:shareBlock];
    [actions addObject:shareAction];
  }

  return actions;
}

#pragma mark CarouselItemMenuProvider Private

/// Blocks `URL` so it won't appear in most visited URLs.
- (void)blockMostVisitedURL:(GURL)URL {
  scoped_refptr<history::TopSites> top_sites = [self.protocolProvider topSites];
  if (top_sites) {
    top_sites->AddBlockedUrl(URL);
  }
}

/// Blocks `URL` in most visited sites and hides `CarouselItem` if it still
/// exist.
- (void)removeMostVisitedForURL:(GURL)URL
               withCarouselItem:(CarouselItem*)carouselItem {
  if (!carouselItem) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("MostVisited_UrlBlocklisted_Omnibox"));
  [self blockMostVisitedURL:URL];
  [self.carouselItemConsumer deleteCarouselItem:carouselItem];
}

/// Opens `carouselItem` in a new tab.
/// `incognito`: open in incognito tab.
- (void)openNewTabWithMostVisitedItem:(CarouselItem*)carouselItem
                            incognito:(BOOL)incognito {
  DCHECK(self.applicationCommandsHandler);
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:carouselItem.URL.gurl
                                      inIncognito:incognito];
  [self.applicationCommandsHandler openURLInNewTab:command];
}

/// Opens suggestAction in a new tab.
- (void)openNewTabWithSuggestAction:(SuggestAction*)suggestAction {
  DCHECK(self.applicationCommandsHandler);
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:suggestAction.actionURI
                                      inIncognito:NO];
  [self.applicationCommandsHandler openURLInNewTab:command];
}

@end
