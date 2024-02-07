// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/history/core/browser/features.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/search_engines/search_terms_data.h"
#import "components/search_engines/template_url.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/query_suggestion_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/shortcuts_config.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/start_suggest_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_mediator.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
using RequestSource = SearchTermsData::RequestSource;

}  // namespace

@interface ContentSuggestionsMediator () <MostVisitedTilesMediatorDelegate,
                                          TabResumptionHelperDelegate>

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
// Section Info for the "Return to Recent Tab" section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* returnToRecentTabSectionInfo;
// Item for the "Return to Recent Tab" tile.
@property(nonatomic, strong)
    ContentSuggestionsReturnToRecentTabItem* returnToRecentTabItem;
// YES if the Return to Recent Tab tile is being shown.
@property(nonatomic, assign, getter=mostRecentTabStartSurfaceTileIsShowing)
    BOOL showMostRecentTabStartSurfaceTile;
// Browser reference.
@property(nonatomic, assign) Browser* browser;

// For testing-only
@property(nonatomic, assign) BOOL hasReceivedMagicStackResponse;

@end

@implementation ContentSuggestionsMediator {
  // Local State prefs.
  raw_ptr<PrefService> _localState;
  // The latest module ranking returned from the SegmentationService.
  NSArray<NSNumber*>* _magicStackOrderFromSegmentation;
  // YES if the module ranking has been received from the SegmentationService.
  BOOL _magicStackOrderFromSegmentationReceived;
  // The latest Magic Stack module order sent up to the consumer. This includes
  // any omissions due to filtering from `_magicStackOrderFromSegmentation` (or
  // `magicStackOrder:` if kSegmentationPlatformIosModuleRanker is disabled) and
  // any additions beyond `_magicStackOrderFromSegmentation` (e.g. Set Up List).
  NSArray<NSNumber*>* _latestMagicStackOrder;
  MostVisitedTilesMediator* _mostVisitedTilesMediator;
  SetUpListMediator* _setUpListMediator;
  TabResumptionMediator* _tabResumptionMediator;
}

#pragma mark - Public

- (instancetype)
         initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                   largeIconCache:(LargeIconCache*)largeIconCache
                  mostVisitedSite:(std::unique_ptr<ntp_tiles::MostVisitedSites>)
                                      mostVisitedSites
                      prefService:(PrefService*)prefService
                      syncService:(syncer::SyncService*)syncService
            authenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                    actionFactory:(BrowserActionFactory*)actionFactory
                          browser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
    _localState = GetApplicationContext()->GetLocalState();
    _articleForYouEnabled =
        prefService->FindPreference(prefs::kArticlesForYouEnabled);
    _contentSuggestionsPolicyEnabled =
        prefService->FindPreference(prefs::kNTPContentSuggestionsEnabled);

    _mostVisitedTilesMediator = [[MostVisitedTilesMediator alloc]
        initWithMostVisitedSite:std::move(mostVisitedSites)
                    prefService:prefService
               largeIconService:largeIconService
                 largeIconCache:largeIconCache
         URLLoadingBrowserAgent:UrlLoadingBrowserAgent::FromBrowser(browser)];
    _mostVisitedTilesMediator.delegate = self;
    _mostVisitedTilesMediator.contentSuggestionsDelegate = self.delegate;
    _mostVisitedTilesMediator.actionFactory = actionFactory;

    BOOL isSetupListEnabled = set_up_list_utils::IsSetUpListActive(_localState);
    if (isSetupListEnabled) {
      _setUpListMediator = [[SetUpListMediator alloc]
            initWithPrefService:prefService
                    syncService:syncService
                identityManager:identityManager
          authenticationService:authenticationService
                     sceneState:browser->GetSceneState()];
    }

    if (IsTabResumptionEnabled()) {
      _tabResumptionMediator =
          [[TabResumptionMediator alloc] initWithLocalState:_localState
                                                prefService:prefService
                                            identityManager:identityManager
                                                    browser:_browser];
      _tabResumptionMediator.delegate = self;
    }
  }

  return self;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosDiscoverFeedLastRefreshTime, 0);
  registry->RegisterInt64Pref(prefs::kIosDiscoverFeedLastUnseenRefreshTime, 0);
}

- (void)disconnect {
  [_setUpListMediator disconnect];
  _setUpListMediator = nil;
  [_mostVisitedTilesMediator disconnect];
  _mostVisitedTilesMediator = nil;
  [_tabResumptionMediator disconnect];
  _tabResumptionMediator = nil;
  _localState = nullptr;
}

- (void)refreshMostVisitedTiles {
  // Refresh in case there are new MVT to show.
  [_mostVisitedTilesMediator refreshMostVisitedTiles];
}

- (void)setConsumer:(id<ContentSuggestionsConsumer>)consumer {
  _consumer = consumer;
  _mostVisitedTilesMediator.consumer = consumer;
  [self configureConsumer];
}

- (void)configureMostRecentTabItemWithWebState:(web::WebState*)webState
                                     timeLabel:(NSString*)timeLabel {
  // The most recent tab tile is replaced by the tab resume feature.
  if (IsTabResumptionEnabled()) {
    return;
  }

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
  const GURL& URL = webState->GetLastCommittedURL();
  if (!self.returnToRecentTabItem.icon) {
    driver->FetchFavicon(URL, false);
  }

  self.returnToRecentTabItem.title =
      l10n_util::GetNSString(IDS_IOS_RETURN_TO_RECENT_TAB_TITLE);
  self.returnToRecentTabItem.subtitle = [self
      constructReturnToRecentTabSubtitleWithPageTitle:base::SysUTF16ToNSString(
                                                          webState->GetTitle())
                                               forURL:URL
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

- (void)disableSetUpList {
  [_setUpListMediator disableSetUpList];
}

- (void)disableTabResumption {
  tab_resumption_prefs::DisableTabResumption(_localState);
  [self removeTabResumptionModule];
}

- (void)disableSafetyCheck:(ContentSuggestionsModuleType)type {
  safety_check_prefs::DisableSafetyCheckInMagicStack(_localState);

  MagicStackOrderChange change{MagicStackOrderChange::Type::kRemove};
  change.old_module = type;
  change.index = [self indexForMagicStackModule:type];
  CHECK(change.index != NSNotFound);
  [self.consumer updateMagicStackOrder:change];
}

- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type {
  [self.contentSuggestionsMetricsRecorder
      recordMagicStackModuleEngagementForType:type
                                      atIndex:
                                          [self indexForMagicStackModule:type]];
}

#pragma mark - ContentSuggestionsCommands

- (void)openMostRecentTab {
  [self.NTPMetricsDelegate recentTabTileOpened];
  [self.contentSuggestionsMetricsRecorder recordTabResumptionTabOpened];
  [IntentDonationHelper donateIntent:IntentType::kOpenLatestTab];
  [self hideRecentTabTile];
  WebStateList* webStateList = self.browser->GetWebStateList();
  web::WebState* webState =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(self.browser)
          ->most_recent_tab();
  if (!webState) {
    return;
  }
  int index = webStateList->GetIndexOfWebState(webState);
  webStateList->ActivateWebStateAt(index);
}

#pragma mark - ParcelTrackingMediatorDelegate

- (void)newParcelsAvailable {
  _latestMagicStackOrder =
      base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformIosModuleRanker)
          ? [self segmentationMagicStackOrder]
          : [self magicStackOrder];
  for (NSUInteger index = 0; index < [_latestMagicStackOrder count]; index++) {
    ContentSuggestionsModuleType type =
        (ContentSuggestionsModuleType)[_latestMagicStackOrder[index] intValue];
    if (type == ContentSuggestionsModuleType::kParcelTracking ||
        type == ContentSuggestionsModuleType::kParcelTrackingSeeMore) {
      MagicStackOrderChange change{MagicStackOrderChange::Type::kInsert};
      change.new_module = type;
      change.index = index;
      [self.consumer updateMagicStackOrder:change];
    }
  }

  [self.consumer showParcelTrackingItems:[self parcelTrackingItems]];
}

- (void)parcelTrackingDisabled {
  // Find all parcel tracking modules and remove them.
  for (NSUInteger i = 0; i < [_latestMagicStackOrder count]; i++) {
    ContentSuggestionsModuleType type =
        (ContentSuggestionsModuleType)[_latestMagicStackOrder[i] intValue];
    if (type == ContentSuggestionsModuleType::kParcelTracking ||
        type == ContentSuggestionsModuleType::kParcelTrackingSeeMore) {
      MagicStackOrderChange change{MagicStackOrderChange::Type::kRemove};
      change.old_module = type;
      change.index = [self indexForMagicStackModule:type];
      CHECK(change.index != NSNotFound);
      [self.consumer updateMagicStackOrder:change];
    }
  }
}

#pragma mark - StartSurfaceRecentTabObserving

- (void)mostRecentTabWasRemoved:(web::WebState*)webState {
  if (!IsTabResumptionEnabled()) {
    [self hideRecentTabTile];
  }
}

- (void)mostRecentTab:(web::WebState*)webState
    faviconUpdatedWithImage:(UIImage*)image {
  if (self.returnToRecentTabItem) {
    self.returnToRecentTabItem.icon = image;
    [self.consumer
        updateReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
}

- (void)mostRecentTab:(web::WebState*)webState
      titleWasUpdated:(NSString*)title {
  if (self.returnToRecentTabItem) {
    SceneState* scene = self.browser->GetSceneState();
    NSString* timeLabel = GetRecentTabTileTimeLabelForSceneState(scene);
    self.returnToRecentTabItem.subtitle = [self
        constructReturnToRecentTabSubtitleWithPageTitle:title
                                                 forURL:
                                                     webState
                                                         ->GetLastCommittedURL()
                                             timeString:timeLabel];
    [self.consumer
        updateReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
}

#pragma mark - TabResumptionHelperDelegate

- (void)tabResumptionHelperDidReceiveItem {
  CHECK(IsTabResumptionEnabled());
  if (!self.consumer ||
      tab_resumption_prefs::IsTabResumptionDisabled(_localState)) {
    return;
  }

  [self showTabResumptionWithItem:_tabResumptionMediator.itemConfig];
}

- (void)removeTabResumptionModule {
  [self.consumer hideTabResumption];
}

#pragma mark - Private

- (NSArray<ParcelTrackingItem*>*)parcelTrackingItems {
  return [self.parcelTrackingMediator parcelTrackingItemsToShow];
}

- (void)configureConsumer {
  if (!self.consumer) {
    return;
  }
  if (IsMagicStackEnabled()) {
    if (base::FeatureList::IsEnabled(
            segmentation_platform::features::
                kSegmentationPlatformIosModuleRanker)) {
      [self fetchMagicStackModuleRankingFromSegmentationPlatform];
    } else {
      _latestMagicStackOrder = [self magicStackOrder];
      [self.consumer setMagicStackOrder:_latestMagicStackOrder];
    }
    if (IsTabResumptionEnabled() && _tabResumptionMediator.itemConfig) {
      [self.consumer
          showTabResumptionWithItem:_tabResumptionMediator.itemConfig];
    }
  }
  if (self.returnToRecentTabItem) {
    [self.consumer
        showReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
  if (_mostVisitedTilesMediator.mostVisitedConfig) {
    [self.consumer
        setMostVisitedTilesConfig:_mostVisitedTilesMediator.mostVisitedConfig];
  }
  if ([self shouldShowSetUpList]) {
    [self showSetUpList];
  }
  // Show shorcuts if:
  // 1) Magic Stack is enabled (always show shortcuts in Magic Stack).
  // 2) The Set Up List and Magic Stack are not enabled (Set Up List replaced
  // Shortcuts).
  if ((IsMagicStackEnabled() || ![self shouldShowSetUpList])) {
    [self.consumer
        setShortcutTilesConfig:self.shortcutsMediator.shortcutsConfig];
  }

  if (IsSafetyCheckMagicStackEnabled() &&
      !safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState) &&
      self.safetyCheckMediator.safetyCheckState.runningState ==
          RunningSafetyCheckState::kDefault) {
    self.safetyCheckMediator.safetyCheckState.commandhandler =
        self.presentationDelegate;
    [self.consumer showSafetyCheck:self.safetyCheckMediator.safetyCheckState];
  }
  if (IsIOSParcelTrackingEnabled() &&
      !IsParcelTrackingDisabled(GetApplicationContext()->GetLocalState())) {
  }
}

// Creates a string containing the title and the time string.
// If `title` is empty, use the `URL` instead.
- (NSString*)constructReturnToRecentTabSubtitleWithPageTitle:
                 (NSString*)pageTitle
                                                      forURL:(const GURL&)URL
                                                  timeString:(NSString*)time {
  NSString* title = pageTitle;
  if (![title length]) {
    title = [self displayableURLFromURL:URL];
  }
  return [NSString stringWithFormat:@"%@%@", title, time];
}

// Formats the URL to be displayed in the recent tabs card.
- (NSString*)displayableURLFromURL:(const GURL&)URL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              URL));
}

// Returns an array that represents the order of the modules to be shown in the
// Magic Stack.
- (NSArray<NSNumber*>*)magicStackOrder {
  NSMutableArray* magicStackModules = [NSMutableArray array];
  if (IsTabResumptionEnabled() &&
      !tab_resumption_prefs::IsTabResumptionDisabled(_localState) &&
      _tabResumptionMediator.itemConfig) {
    [magicStackModules
        addObject:@(int(ContentSuggestionsModuleType::kTabResumption))];
  }
  if ([self shouldShowSetUpList]) {
    [self addSetUpListToMagicStackOrder:magicStackModules];
  }
  if (ShouldPutMostVisitedSitesInMagicStack()) {
    [magicStackModules
        addObject:@(int(ContentSuggestionsModuleType::kMostVisited))];
  }
  [magicStackModules
      addObject:@(int(ContentSuggestionsModuleType::kShortcuts))];

  if (IsSafetyCheckMagicStackEnabled() &&
      !safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState)) {
    [self addSafetyCheckToMagicStackOrder:magicStackModules];
  }

  if (IsIOSParcelTrackingEnabled() &&
      !IsParcelTrackingDisabled(GetApplicationContext()->GetLocalState())) {
    if ([[self parcelTrackingItems] firstObject].shouldShowSeeMore) {
      [magicStackModules
          addObject:@(int(
                        ContentSuggestionsModuleType::kParcelTrackingSeeMore))];
    } else {
      for (NSUInteger i = 0; i < [[self parcelTrackingItems] count]; i++) {
        // Magic Stack will show up to two modules to match the number of
        // parcels tracked.
        [magicStackModules
            addObject:@(int(ContentSuggestionsModuleType::kParcelTracking))];
      }
    }
  }

  return magicStackModules;
}

// Construct the Magic Stack module order from fetched results from
// Segmentation. This method adds on modules not included on the Segmentation
// side (e.g. Set Up List) and also filters out modules not ready or should not
// be presented.
- (NSArray<NSNumber*>*)segmentationMagicStackOrder {
  NSMutableArray<NSNumber*>* magicStackOrder = [NSMutableArray array];
  // Always add Set Up List at the front.
  if ([self shouldShowSetUpList]) {
    [self addSetUpListToMagicStackOrder:magicStackOrder];
  }
  for (NSNumber* moduleNumber : _magicStackOrderFromSegmentation) {
    ContentSuggestionsModuleType moduleType =
        (ContentSuggestionsModuleType)[moduleNumber intValue];
    switch (moduleType) {
      case ContentSuggestionsModuleType::kMostVisited:
        if (ShouldPutMostVisitedSitesInMagicStack()) {
          [magicStackOrder addObject:moduleNumber];
        }
        break;
      case ContentSuggestionsModuleType::kTabResumption:
        if (!IsTabResumptionEnabled() ||
            tab_resumption_prefs::IsTabResumptionDisabled(_localState) ||
            !_tabResumptionMediator.itemConfig) {
          break;
        }
        // If ShouldHideIrrelevantModules() is enabled and it is not ranked as
        // the first two modules, do not add it to the Magic Stack.
        if (ShouldHideIrrelevantModules() && [magicStackOrder count] > 1) {
          break;
        }
        [magicStackOrder addObject:moduleNumber];
        break;
      case ContentSuggestionsModuleType::kSafetyCheck:
      case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
      case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
        if (!IsSafetyCheckMagicStackEnabled() ||
            safety_check_prefs::IsSafetyCheckInMagicStackDisabled(
                _localState)) {
          break;
        }
        // If ShouldHideIrrelevantModules() is enabled and it is not the first
        // ranked module, do not add it to the Magic Stack.
        if (!ShouldHideIrrelevantModules() || [magicStackOrder count] == 0) {
          [self addSafetyCheckToMagicStackOrder:magicStackOrder];
        }
        break;
      case ContentSuggestionsModuleType::kShortcuts:
        [magicStackOrder addObject:moduleNumber];
        break;
      case ContentSuggestionsModuleType::kParcelTracking:
        if (IsIOSParcelTrackingEnabled() &&
            !IsParcelTrackingDisabled(
                GetApplicationContext()->GetLocalState())) {
          if ([[self parcelTrackingItems] firstObject].shouldShowSeeMore) {
            [magicStackOrder addObject:@(int(ContentSuggestionsModuleType::
                                                 kParcelTrackingSeeMore))];
          } else {
            for (NSUInteger i = 0; i < [[self parcelTrackingItems] count];
                 i++) {
              // Magic Stack will show up to two modules to match the number of
              // parcels tracked.
              [magicStackOrder addObject:moduleNumber];
            }
          }
        }
        break;
      default:
        // These module types should not have been added by the logic
        // receiving the order list from Segmentation.
        NOTREACHED();
        break;
    }
  }
  return magicStackOrder;
}

- (void)fetchMagicStackModuleRankingFromSegmentationPlatform {
  auto input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  int mvt_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kMostVisitedTilesFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          mvt_freshness_impression_count));
  int shortcuts_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kShortcutsFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          shortcuts_freshness_impression_count));
  int safety_check_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kSafetyCheckFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          safety_check_freshness_impression_count));
  int tab_resumption_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kTabResumptionFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          tab_resumption_freshness_impression_count));
  int parcel_tracking_freshness_impression_count = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness);
  input_context->metadata_args.emplace(
      segmentation_platform::kParcelTrackingFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          parcel_tracking_freshness_impression_count));
  __weak ContentSuggestionsMediator* weakSelf = self;
  segmentation_platform::PredictionOptions options;
  options.on_demand_execution = true;
  self.segmentationService->GetClassificationResult(
      segmentation_platform::kIosModuleRankerKey, options, input_context,
      base::BindOnce(
          ^(const segmentation_platform::ClassificationResult& result) {
            weakSelf.hasReceivedMagicStackResponse = YES;
            [weakSelf didReceiveSegmentationServiceResult:result];
          }));
}

- (void)didReceiveSegmentationServiceResult:
    (const segmentation_platform::ClassificationResult&)result {
  CHECK(IsMagicStackEnabled());
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    return;
  }

  NSMutableArray* magicStackOrder = [NSMutableArray array];
  for (const std::string& label : result.ordered_labels) {
    if (label == segmentation_platform::kMostVisitedTiles) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kMostVisited))];
    } else if (label == segmentation_platform::kShortcuts) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kShortcuts))];
    } else if (label == segmentation_platform::kSafetyCheck) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kSafetyCheck))];
    } else if (label == segmentation_platform::kTabResumption) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kTabResumption))];
    } else if (label == segmentation_platform::kParcelTracking) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kParcelTracking))];
    }
  }
  _magicStackOrderFromSegmentationReceived = YES;
  _magicStackOrderFromSegmentation = magicStackOrder;
  _latestMagicStackOrder = [self segmentationMagicStackOrder];
  [self.consumer setMagicStackOrder:_latestMagicStackOrder];
}

- (void)addSetUpListToMagicStackOrder:(NSMutableArray*)order {
    if ([_setUpListMediator allItemsComplete]) {
      [order addObject:@(int(ContentSuggestionsModuleType::kSetUpListAllSet))];
    } else if (set_up_list_utils::ShouldShowCompactedSetUpListModule()) {
      [order
          addObject:@(int(ContentSuggestionsModuleType::kCompactedSetUpList))];
    } else {
      for (SetUpListItemViewData* model in _setUpListMediator.setUpListItems) {
        [order
            addObject:@(int(SetUpListModuleTypeForSetUpListType(model.type)))];
      }
    }
}

// Adds the Safety Check module to `order` based on the current Safety Check
// state.
- (void)addSafetyCheckToMagicStackOrder:(NSMutableArray*)order {
  CHECK(IsSafetyCheckMagicStackEnabled());

  int checkIssuesCount =
      CheckIssuesCount(self.safetyCheckMediator.safetyCheckState);

  if (checkIssuesCount > 2) {
    [order addObject:@(int(ContentSuggestionsModuleType::
                               kSafetyCheckMultiRowOverflow))];
  } else if (checkIssuesCount > 1) {
    [order
        addObject:@(int(ContentSuggestionsModuleType::kSafetyCheckMultiRow))];
  } else {
    [order addObject:@(int(ContentSuggestionsModuleType::kSafetyCheck))];
  }
}

// Returns YES if the conditions are right to display the Set Up List.
- (BOOL)shouldShowSetUpList {
  if (!set_up_list_utils::IsSetUpListActive(_localState)) {
    return NO;
  }

  if (!_setUpListMediator || _setUpListMediator.setUpListItems.count == 0) {
    return NO;
  }

  return YES;
}

// Returns an array of all possible items in the Set Up List.
- (NSArray<SetUpListItemViewData*>*)allSetUpListItems {
  return [_setUpListMediator allItems];
}

// Sends the SetUpList items up to the consumer.
- (void)showSetUpList {
  _setUpListMediator.consumer = self.consumer;
  _setUpListMediator.commandHandler = self.presentationDelegate;
  _setUpListMediator.delegate = self.delegate;
  _setUpListMediator.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  [_setUpListMediator showSetUpList];
}

// Shows the tab resumption tile with the given `item` configuration.
- (void)showTabResumptionWithItem:(TabResumptionItem*)item {
  if (tab_resumption_prefs::IsLastOpenedURL(
          item.tabURL, self.browser->GetBrowserState()->GetPrefs())) {
    return;
  }

  _latestMagicStackOrder =
      base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformIosModuleRanker)
          ? [self segmentationMagicStackOrder]
          : [self magicStackOrder];
  if ([self isMagicStackOrderReady]) {
    // Only indicate the need for an explicit insertion if the tab resumption
    // item was received after building the initial Magic Stack order or getting
    // the Magic Stack Order from Segmentation.
    NSUInteger insertionIndex = [self
        indexForMagicStackModule:ContentSuggestionsModuleType::kTabResumption];
    if (insertionIndex == NSNotFound) {
      return;
    }
    // Only continue on to insert Tab Resumption after `isMagicStackOrderReady`
    // if it is in the Magic Stack order
    MagicStackOrderChange change{MagicStackOrderChange::Type::kInsert,
                                 ContentSuggestionsModuleType::kTabResumption};
    change.index = insertionIndex;
    [self.consumer updateMagicStackOrder:change];
  }
  [self.consumer showTabResumptionWithItem:item];
}

// Returns the index rank of `moduleType`.
// Callers of this need to handle a NSNotFound return case and do nothing in
// that case.
- (NSUInteger)indexForMagicStackModule:
    (ContentSuggestionsModuleType)moduleType {
  NSUInteger index = [_latestMagicStackOrder indexOfObject:@(int(moduleType))];
  if (index == NSNotFound) {
    // It is possible that a feature is enabled but the segmentation model being
    // used didn't return the feature's module (i.e. first browser session after
    // enabling a feature, so the latest model will not be downloaded until the
    // following session since experiment models are tied via finch). It is also
    // possible that the segmentation result has not returned yet.
    CHECK(base::FeatureList::IsEnabled(
        segmentation_platform::features::kSegmentationPlatformIosModuleRanker));
  }
  return index;
}

// Returns NO if client is expecting the order from Segmentation and it has not
// returned yet.
- (BOOL)isMagicStackOrderReady {
  if (base::FeatureList::IsEnabled(segmentation_platform::features::
                                       kSegmentationPlatformIosModuleRanker)) {
    return _magicStackOrderFromSegmentationReceived;
  }
  return YES;
}

#pragma mark - Properties

- (void)setCommandHandler:(id<ContentSuggestionsCommands>)commandHandler {
  if (_commandHandler == commandHandler)
    return;

  _commandHandler = commandHandler;
}

- (void)setDispatcher:
    (id<ApplicationCommands, BrowserCoordinatorCommands, SnackbarCommands>)
        dispatcher {
  _dispatcher = dispatcher;
  _mostVisitedTilesMediator.snackbarHandler = _dispatcher;
}

- (void)setNTPMetricsDelegate:(id<NewTabPageMetricsDelegate>)delegate {
  _NTPMetricsDelegate = delegate;
  _mostVisitedTilesMediator.NTPMetricsDelegate = delegate;
  _tabResumptionMediator.NTPMetricsDelegate = delegate;
}

- (void)setContentSuggestionsMetricsRecorder:
    (ContentSuggestionsMetricsRecorder*)contentSuggestionsMetricsRecorder {
  _contentSuggestionsMetricsRecorder = contentSuggestionsMetricsRecorder;
  _tabResumptionMediator.contentSuggestionsMetricsRecorder =
      contentSuggestionsMetricsRecorder;
}

- (BOOL)contentSuggestionsEnabled {
  return self.articleForYouEnabled->GetValue()->GetBool() &&
         self.contentSuggestionsPolicyEnabled->GetValue()->GetBool();
}

@end
