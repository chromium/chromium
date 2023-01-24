// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/bind.h"
#import "base/callback.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/ntp_snippets/category.h"
#import "components/ntp_snippets/category_info.h"
#import "components/ntp_tiles/metrics.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#import "components/search_engines/template_url.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp_tiles/most_visited_sites_observer_bridge.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/query_suggestion_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/content_suggestions/mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/content_suggestions/start_suggest_service_factory.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/ntp/feed_delegate.h"
#import "ios/chrome/browser/ui/ntp/metrics/metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/ntp_tile_saver.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;

// Maximum number of most visited tiles fetched.
const NSInteger kMaxNumMostVisitedTiles = 4;

}  // namespace

@interface ContentSuggestionsMediator () <MostVisitedSitesObserving,
                                          ReadingListModelBridgeObserver> {
  std::unique_ptr<ntp_tiles::MostVisitedSites> _mostVisitedSites;
  std::unique_ptr<ntp_tiles::MostVisitedSitesObserverBridge> _mostVisitedBridge;
  std::unique_ptr<ReadingListModelBridge> _readingListModelBridge;
  std::unique_ptr<StartSuggestServiceResponseBridge>
      _startSuggestServiceResponseBridge;
  StartSuggestService* _startSuggestService;
}

// Whether the contents section should be hidden completely.
// Don't use PrefBackedBoolean or PrefMember as this value needs to be checked
// when the Preference is updated.
@property(nonatomic, assign, readonly) BOOL contentSuggestionsEnabled;

// Don't use PrefBackedBoolean or PrefMember as those values needs to be checked
// when the Preference is updated.
// Whether the suggestions have been disabled in Chrome Settings.
@property(nonatomic, assign)
    const PrefService::Preference* articleForYouEnabled;
// Whether the suggestions have been disabled by a policy.
@property(nonatomic, assign)
    const PrefService::Preference* contentSuggestionsPolicyEnabled;

// Most visited items from the MostVisitedSites service currently displayed.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedItem*>* mostVisitedItems;
@property(nonatomic, strong)
    NSArray<ContentSuggestionsMostVisitedActionItem*>* actionButtonItems;
// Most visited items from the MostVisitedSites service (copied upon receiving
// the callback). Those items are up to date with the model.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedItem*>* freshMostVisitedItems;
// Section Info for the logo and omnibox section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* logoSectionInfo;
// Section Info for the "Return to Recent Tab" section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* returnToRecentTabSectionInfo;
// Item for the "Return to Recent Tab" tile.
@property(nonatomic, strong)
    ContentSuggestionsReturnToRecentTabItem* returnToRecentTabItem;
// Section Info for the Most Visited section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* mostVisitedSectionInfo;
// Whether the page impression has been recorded.
@property(nonatomic, assign) BOOL recordedPageImpression;
// Map the section information created to the relevant category.
@property(nonatomic, strong, nonnull)
    NSMutableDictionary<ContentSuggestionsCategoryWrapper*,
                        ContentSuggestionsSectionInformation*>*
        sectionInformationByCategory;
// Mediator fetching the favicons for the items.
@property(nonatomic, strong) ContentSuggestionsFaviconMediator* faviconMediator;
// Item for the reading list action item.  Reference is used to update the
// reading list count.
@property(nonatomic, strong)
    ContentSuggestionsMostVisitedActionItem* readingListItem;
// Number of unread items in reading list model.
@property(nonatomic, assign) NSInteger readingListUnreadCount;
// YES if the Return to Recent Tab tile is being shown.
@property(nonatomic, assign, getter=mostRecentTabStartSurfaceTileIsShowing)
    BOOL showMostRecentTabStartSurfaceTile;
// Whether the incognito mode is available.
@property(nonatomic, assign) BOOL incognitoAvailable;
// Recorder for the metrics related to the NTP.
@property(nonatomic, strong) NTPHomeMetrics* NTPMetrics;
// Browser reference.
@property(nonatomic, assign) Browser* browser;

@property(nonatomic, strong)
    NSMutableArray<QuerySuggestionConfig*>* trendingQueries;

@end

@implementation ContentSuggestionsMediator

#pragma mark - Public

- (instancetype)
         initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                   largeIconCache:(LargeIconCache*)largeIconCache
                  mostVisitedSite:(std::unique_ptr<ntp_tiles::MostVisitedSites>)
                                      mostVisitedSites
                 readingListModel:(ReadingListModel*)readingListModel
                      prefService:(PrefService*)prefService
    isGoogleDefaultSearchProvider:(BOOL)isGoogleDefaultSearchProvider
                          browser:(Browser*)browser {
  self = [super init];
  if (self) {
    _incognitoAvailable = !IsIncognitoModeDisabled(prefService);
    _articleForYouEnabled =
        prefService->FindPreference(prefs::kArticlesForYouEnabled);
    _contentSuggestionsPolicyEnabled =
        prefService->FindPreference(prefs::kNTPContentSuggestionsEnabled);

    _sectionInformationByCategory = [[NSMutableDictionary alloc] init];

    _faviconMediator = [[ContentSuggestionsFaviconMediator alloc]
        initWithLargeIconService:largeIconService
                  largeIconCache:largeIconCache];

    _logoSectionInfo = LogoSectionInformation();
    _mostVisitedSectionInfo = MostVisitedSectionInformation();

    _mostVisitedSites = std::move(mostVisitedSites);
    _mostVisitedBridge =
        std::make_unique<ntp_tiles::MostVisitedSitesObserverBridge>(self);
    _mostVisitedSites->AddMostVisitedURLsObserver(_mostVisitedBridge.get(),
                                                  kMaxNumMostVisitedTiles);

    _readingListModelBridge =
        std::make_unique<ReadingListModelBridge>(self, readingListModel);
    _browser = browser;
    _NTPMetrics = [[NTPHomeMetrics alloc]
        initWithBrowserState:_browser->GetBrowserState()];

    if (IsTrendingQueriesModuleEnabled()) {
      _startSuggestService = StartSuggestServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState(), true);
      _startSuggestServiceResponseBridge =
          std::make_unique<StartSuggestServiceResponseBridge>(self);
    }
  }
  return self;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosDiscoverFeedLastRefreshTime, 0);
}

- (void)disconnect {
  _mostVisitedBridge.reset();
  _mostVisitedSites.reset();
}

- (void)refreshMostVisitedTiles {
  // Refresh in case there are new MVT to show.
  _mostVisitedSites->RefreshTiles();
  _mostVisitedSites->Refresh();
}

- (void)reloadAllData {
  if (!self.consumer) {
    return;
  }
  if (self.returnToRecentTabItem) {
    [self.consumer
        showReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
  if ([self.mostVisitedItems count]) {
    [self.consumer setMostVisitedTilesWithConfigs:self.mostVisitedItems];
  }
  if (!ShouldHideShortcutsForTrendingQueries()) {
    [self.consumer setShortcutTilesWithConfigs:self.actionButtonItems];
  }
  if (IsTrendingQueriesModuleEnabled()) {
    [self fetchTrendingQueriesIfApplicable];
  }
}

- (void)blockMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlockedUrl(URL, true);
  [self useFreshMostVisited];
}

- (void)allowMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlockedUrl(URL, false);
  [self useFreshMostVisited];
}

- (void)setConsumer:(id<ContentSuggestionsConsumer>)consumer {
  _consumer = consumer;
  self.faviconMediator.consumer = consumer;
  [self reloadAllData];
}

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
  self.NTPMetrics.webState = self.webState;
}

- (void)setShowingStartSurface:(BOOL)showingStartSurface {
  _showingStartSurface = showingStartSurface;
  self.NTPMetrics.showingStartSurface = showingStartSurface;
}

+ (NSUInteger)maxSitesShown {
  return kMaxNumMostVisitedTiles;
}

- (void)configureMostRecentTabItemWithWebState:(web::WebState*)webState
                                     timeLabel:(NSString*)timeLabel {
  self.returnToRecentTabSectionInfo = ReturnToRecentTabSectionInformation();
  if (!self.returnToRecentTabItem) {
    self.returnToRecentTabItem =
        [[ContentSuggestionsReturnToRecentTabItem alloc] init];
  }

  // Retrieve favicon associated with the page.
  favicon::WebFaviconDriver* driver =
      favicon::WebFaviconDriver::FromWebState(webState);
  if (driver->FaviconIsValid()) {
    gfx::Image favicon = driver->GetFavicon();
    if (!favicon.IsEmpty()) {
      self.returnToRecentTabItem.icon = favicon.ToUIImage();
    }
  }
  if (!self.returnToRecentTabItem.icon) {
    driver->FetchFavicon(webState->GetLastCommittedURL(), false);
  }

  self.returnToRecentTabItem.title =
      l10n_util::GetNSString(IDS_IOS_RETURN_TO_RECENT_TAB_TITLE);
  self.returnToRecentTabItem.subtitle = [self
      constructReturnToRecentTabSubtitleWithPageTitle:base::SysUTF16ToNSString(
                                                          webState->GetTitle())
                                           timeString:timeLabel];
  self.showMostRecentTabStartSurfaceTile = YES;
  [self.consumer
      showReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
}

- (void)hideRecentTabTile {
  if (self.showMostRecentTabStartSurfaceTile) {
    self.showMostRecentTabStartSurfaceTile = NO;
    self.returnToRecentTabItem = nil;
    [self.consumer hideReturnToRecentTabTile];
  }
}

#pragma mark - ContentSuggestionsCommands

- (void)openMostVisitedItem:(NSObject*)item
                    atIndex:(NSInteger)mostVisitedIndex {
  if ([item isKindOfClass:[ContentSuggestionsMostVisitedActionItem class]]) {
    [self.NTPMetrics recordContentSuggestionsActionForType:
                         IOSContentSuggestionsActionType::kShortcuts];
    ContentSuggestionsMostVisitedActionItem* mostVisitedItem =
        base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedActionItem>(
            item);
    switch (mostVisitedItem.collectionShortcutType) {
      case NTPCollectionShortcutTypeBookmark:
        base::RecordAction(base::UserMetricsAction("MobileNTPShowBookmarks"));
        LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
        [self.dispatcher showBookmarksManager];
        break;
      case NTPCollectionShortcutTypeReadingList:
        base::RecordAction(base::UserMetricsAction("MobileNTPShowReadingList"));
        [self.dispatcher showReadingList];
        break;
      case NTPCollectionShortcutTypeRecentTabs:
        base::RecordAction(base::UserMetricsAction("MobileNTPShowRecentTabs"));
        [self.dispatcher showRecentTabs];
        break;
      case NTPCollectionShortcutTypeHistory:
        base::RecordAction(base::UserMetricsAction("MobileNTPShowHistory"));
        [self.dispatcher showHistory];
        break;
      case NTPCollectionShortcutTypeWhatsNew:
        SetWhatsNewUsed();
        base::RecordAction(base::UserMetricsAction("MobileNTPShowWhatsNew"));
        [self.dispatcher showWhatsNew];
        break;
      case NTPCollectionShortcutTypeCount:
        NOTREACHED();
        break;
    }
    return;
  }

  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);

  [self logMostVisitedOpening:mostVisitedItem atIndex:mostVisitedIndex];

  UrlLoadParams params = UrlLoadParams::InCurrentTab(mostVisitedItem.URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

- (void)loadSuggestedQuery:(QuerySuggestionConfig*)config {
  UMA_HISTOGRAM_ENUMERATION("IOS.TrendingQueries", config.index,
                            kMaxTrendingQueries);
  [self.NTPMetrics recordContentSuggestionsActionForType:
                       IOSContentSuggestionsActionType::kTrendingQuery];
  UrlLoadParams params = UrlLoadParams::InCurrentTab(config.URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_LINK;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

- (void)openMostRecentTab {
  [self.NTPMetrics recordContentSuggestionsActionForType:
                       IOSContentSuggestionsActionType::kReturnToRecentTab];
  base::RecordAction(
      base::UserMetricsAction("IOS.StartSurface.OpenMostRecentTab"));
  [self hideRecentTabTile];
  WebStateList* web_state_list = self.browser->GetWebStateList();
  web::WebState* web_state =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(self.browser)
          ->most_recent_tab();
  if (!web_state) {
    return;
  }
  int index = web_state_list->GetIndexOfWebState(web_state);
  web_state_list->ActivateWebStateAt(index);
}

#pragma mark - ContentSuggestionsGestureCommands

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index
                            fromPoint:(CGPoint)point {
  if (incognito &&
      IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs())) {
    // This should only happen when the policy changes while the option is
    // presented.
    return;
  }
  [self logMostVisitedOpening:item atIndex:index];
  [self openNewTabWithURL:item.URL incognito:incognito originPoint:point];
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index {
  if (incognito &&
      IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs())) {
    // This should only happen when the policy changes while the option is
    // presented.
    return;
  }
  [self logMostVisitedOpening:item atIndex:index];
  [self openNewTabWithURL:item.URL incognito:incognito originPoint:CGPointZero];
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito {
  [self openNewTabWithMostVisitedItem:item
                            incognito:incognito
                              atIndex:item.index];
}

- (void)removeMostVisited:(ContentSuggestionsMostVisitedItem*)item {
  base::RecordAction(base::UserMetricsAction("MostVisited_UrlBlacklisted"));
  [self blockMostVisitedURL:item.URL];
  [self showMostVisitedUndoForURL:item.URL];
}

#pragma mark - StartSuggestServiceDelegateBridge

- (void)suggestionsReceived:(std::vector<QuerySuggestion>)suggestions {
  self.trendingQueries = [NSMutableArray array];
  int index = 0;
  for (QuerySuggestion query : suggestions) {
    if (index == kMaxTrendingQueries) {
      break;
    }
    QuerySuggestionConfig* suggestion = [[QuerySuggestionConfig alloc] init];
    suggestion.URL = query.destination_url;
    suggestion.query = base::SysUTF16ToNSString(query.query);
    suggestion.index = index;
    index++;
    [self.trendingQueries addObject:suggestion];
  }
  [self.consumer setTrendingQueriesWithConfigs:self.trendingQueries];
}

#pragma mark - StartSurfaceRecentTabObserving

- (void)mostRecentTabWasRemoved:(web::WebState*)web_state {
  [self hideRecentTabTile];
}

- (void)mostRecentTabFaviconUpdatedWithImage:(UIImage*)image {
  if (self.returnToRecentTabItem) {
    self.returnToRecentTabItem.icon = image;
    [self.consumer
        updateReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
}

- (void)mostRecentTabTitleWasUpdated:(NSString*)title {
  if (self.returnToRecentTabItem) {
    SceneState* scene =
        SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
    NSString* time_label = GetRecentTabTileTimeLabelForSceneState(scene);
    self.returnToRecentTabItem.subtitle =
        [self constructReturnToRecentTabSubtitleWithPageTitle:title
                                                   timeString:time_label];
    [self.consumer
        updateReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
}

#pragma mark - MostVisitedSitesObserving

- (void)onMostVisitedURLsAvailable:
    (const ntp_tiles::NTPTilesVector&)mostVisited {
  // This is used by the content widget.
  ntp_tile_saver::SaveMostVisitedToDisk(
      mostVisited, self.faviconMediator.mostVisitedAttributesProvider,
      app_group::ContentWidgetFaviconsFolder());

  self.freshMostVisitedItems = [NSMutableArray array];
  int index = 0;
  for (const ntp_tiles::NTPTile& tile : mostVisited) {
    ContentSuggestionsMostVisitedItem* item =
        ConvertNTPTile(tile, self.mostVisitedSectionInfo);
    item.commandHandler = self;
    item.incognitoAvailable = self.incognitoAvailable;
    item.index = index;
    DCHECK(index < kShortcutMinimumIndex);
    index++;
    [self.faviconMediator fetchFaviconForMostVisited:item];
    [self.freshMostVisitedItems addObject:item];
  }

  [self useFreshMostVisited];

  if (mostVisited.size() && !self.recordedPageImpression) {
    self.recordedPageImpression = YES;
    [self.faviconMediator setMostVisitedDataForLogging:mostVisited];
    ntp_tiles::metrics::RecordPageImpression(mostVisited.size());
  }
}

- (void)onIconMadeAvailable:(const GURL&)siteURL {
  // This is used by the content widget.
  ntp_tile_saver::UpdateSingleFavicon(
      siteURL, self.faviconMediator.mostVisitedAttributesProvider,
      app_group::ContentWidgetFaviconsFolder());

  for (ContentSuggestionsMostVisitedItem* item in self.mostVisitedItems) {
    if (item.URL == siteURL) {
      [self.faviconMediator fetchFaviconForMostVisited:item];
      return;
    }
  }
}

#pragma mark - ContentSuggestionsMetricsRecorderDelegate

- (ContentSuggestionsCategoryWrapper*)categoryWrapperForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  return [[self.sectionInformationByCategory allKeysForObject:sectionInfo]
      firstObject];
}

#pragma mark - Private

// Replaces the Most Visited items currently displayed by the most recent ones.
- (void)useFreshMostVisited {
  self.mostVisitedItems = self.freshMostVisitedItems;
  [self.consumer setMostVisitedTilesWithConfigs:self.mostVisitedItems];

  [self.feedDelegate contentSuggestionsWasUpdated];
}

// Opens the `URL` in a new tab `incognito` or not. `originPoint` is the origin
// of the new tab animation if the tab is opened in background, in window
// coordinates.
- (void)openNewTabWithURL:(const GURL&)URL
                incognito:(BOOL)incognito
              originPoint:(CGPoint)originPoint {
  // Open the tab in background if it is non-incognito only.
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.SetInBackground(!incognito);
  params.in_incognito = incognito;
  params.append_to = kCurrentTab;
  params.origin_point = originPoint;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

// Logs a histogram due to a Most Visited item being opened.
- (void)logMostVisitedOpening:(ContentSuggestionsMostVisitedItem*)item
                      atIndex:(NSInteger)mostVisitedIndex {
  [self.NTPMetrics
      recordAction:new_tab_page_uma::ACTION_OPENED_MOST_VISITED_ENTRY];
  [self.NTPMetrics recordContentSuggestionsActionForType:
                       IOSContentSuggestionsActionType::kMostVisitedTile];
  base::RecordAction(base::UserMetricsAction("MobileNTPMostVisited"));
  RecordNTPTileClick(mostVisitedIndex, item.source, item.titleSource,
                     item.attributes, GURL());
}

// Shows a snackbar with an action to undo the removal of the most visited item
// with a `URL`.
- (void)showMostVisitedUndoForURL:(GURL)URL {
  GURL copiedURL = URL;

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  __weak ContentSuggestionsMediator* weakSelf = self;
  action.handler = ^{
    ContentSuggestionsMediator* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf allowMostVisitedURL:copiedURL];
  };
  action.title = l10n_util::GetNSString(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  action.accessibilityIdentifier = @"Undo";

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage
      messageWithText:l10n_util::GetNSString(
                          IDS_IOS_NEW_TAB_MOST_VISITED_ITEM_REMOVED)];
  message.action = action;
  message.category = @"MostVisitedUndo";
  [self.dispatcher showSnackbarMessage:message];
}

- (void)fetchTrendingQueriesIfApplicable {
  PrefService* pref_service =
      ChromeBrowserState::FromBrowserState(self.browser->GetBrowserState())
          ->GetPrefs();
  BOOL isFeedVisible =
      (pref_service->GetBoolean(prefs::kArticlesForYouEnabled) &&
       pref_service->GetBoolean(prefs::kNTPContentSuggestionsEnabled) &&
       !IsFeedAblationEnabled()) &&
      pref_service->GetBoolean(feed::prefs::kArticlesListVisible);
  if (ShouldOnlyShowTrendingQueriesForDisabledFeed() && isFeedVisible) {
    // Notify consumer with empty array so it knows to remove the module.
    [self.consumer setTrendingQueriesWithConfigs:@[]];
    return;
  }

  // Fetch Trending Queries
  TemplateURLRef::SearchTermsArgs args;
  args.request_source = TemplateURLRef::NON_SEARCHBOX_NTP;
  _startSuggestService->FetchSuggestions(
      args,
      base::BindOnce(&StartSuggestServiceResponseBridge::OnSuggestionsReceived,
                     _startSuggestServiceResponseBridge->AsWeakPtr()),
      self.showingStartSurface);
}

- (NSString*)constructReturnToRecentTabSubtitleWithPageTitle:
                 (NSString*)pageTitle
                                                  timeString:(NSString*)time {
  return [NSString stringWithFormat:@"%@%@", pageTitle, time];
}

- (BOOL)shouldShowWhatsNewActionItem {
  if (!IsWhatsNewEnabled() || WasWhatsNewUsed()) {
    return NO;
  }

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  BOOL isSignedIn =
      authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);

  return !isSignedIn;
}

#pragma mark - Properties

- (NSArray<ContentSuggestionsMostVisitedActionItem*>*)actionButtonItems {
  if (!_actionButtonItems) {
    self.readingListItem = ReadingListActionItem();
    self.readingListItem.count = self.readingListUnreadCount;
    _actionButtonItems = @[
      [self shouldShowWhatsNewActionItem] ? WhatsNewActionItem()
                                          : BookmarkActionItem(),
      self.readingListItem, RecentTabsActionItem(), HistoryActionItem()
    ];
    for (ContentSuggestionsMostVisitedActionItem* item in _actionButtonItems) {
      item.accessibilityTraits = UIAccessibilityTraitButton;
    }
  }
  return _actionButtonItems;
}

- (void)setCommandHandler:
    (id<ContentSuggestionsCommands, ContentSuggestionsGestureCommands>)
        commandHandler {
  if (_commandHandler == commandHandler)
    return;

  _commandHandler = commandHandler;

  for (ContentSuggestionsMostVisitedItem* item in self.freshMostVisitedItems) {
    item.commandHandler = commandHandler;
  }
}

- (BOOL)contentSuggestionsEnabled {
  return self.articleForYouEnabled->GetValue()->GetBool() &&
         self.contentSuggestionsPolicyEnabled->GetValue()->GetBool();
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self readingListModelDidApplyChanges:model];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  self.readingListUnreadCount = model->unread_size();
  if (self.readingListItem) {
    self.readingListItem.count = self.readingListUnreadCount;
    [self.consumer updateShortcutTileConfig:self.readingListItem];
  }
}

@end
