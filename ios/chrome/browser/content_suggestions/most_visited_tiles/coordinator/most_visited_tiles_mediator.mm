// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/coordinator/most_visited_tiles_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/cancelable_task_tracker.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/history/core/browser/history_service.h"
#import "components/ntp_tiles/features.h"
#import "components/ntp_tiles/metrics.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/ntp_tiles/tile_source.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/content_suggestions/coordinator/content_suggestions_delegate.h"
#import "ios/chrome/browser/content_suggestions/model/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/metrics.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tile_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_tile_saver.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_commands.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_consumer.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_menu_elements_provider.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp_tiles/model/most_visited_sites_observer_bridge.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Maximum number of most visited tiles fetched that are not pinned sites.
const NSInteger kMaxNumNonCustomMostVisitedTiles = 4;

// Maximum number of most visited tiles fetched.
const NSInteger kMaxNumMostVisitedTiles = 8;

// Size below which the provider returns a colored tile instead of an image.
const CGFloat kMagicStackMostVisitedFaviconMinimalSize = 18;

// Number of repeating visits of a site to trigger an in-product help.
const int kRepeatingVisitsToTriggerIPH = 3;

// Returns YES if an in-product help should be triggered, based on the result of
// querying visits of a URL.
BOOL ShouldTriggerIPHForURLVisits(history::QueryURLAndVisitsResult result) {
  if (!result.success || result.visits.size() < kRepeatingVisitsToTriggerIPH) {
    return NO;
  }
  base::Time earliest_visit_time = result.visits.back().visit_time;
  return (base::Time::Now() - earliest_visit_time) < base::Days(7);
}

// Fix up and validate the `url`. If the url is invalid, return an empty URL.
GURL GetValidUrl(NSString* urlString) {
  GURL fixedUpURL = url_formatter::FixupURL(base::SysNSStringToUTF8(urlString),
                                            std::string());
  if (fixedUpURL.IsStandard() || fixedUpURL.SchemeIs("chrome")) {
    return fixedUpURL;
  }
  return GURL();
}

}  // namespace

@interface MostVisitedTilesMediator () <ContentSuggestionsMenuElementsProvider,
                                        MostVisitedSitesObserving>
@end

@implementation MostVisitedTilesMediator {
  std::unique_ptr<ntp_tiles::MostVisitedSites> _mostVisitedSites;
  std::unique_ptr<ntp_tiles::MostVisitedSitesObserverBridge> _mostVisitedBridge;
  FaviconAttributesProvider* _mostVisitedAttributesProvider;
  std::map<GURL, FaviconCompletionHandler> _mostVisitedFetchFaviconCallbacks;
  NSMutableArray<MostVisitedItem*>* _freshMostVisitedItems;
  // Most visited items from the MostVisitedSites service currently displayed.
  MostVisitedTilesConfig* _mostVisitedConfig;
  // Whether incognito mode is available.
  BOOL _incognitoAvailable;
  BOOL _recordedPageImpression;
  raw_ptr<history::HistoryService> _historyService;
  raw_ptr<PrefService> _prefService;
  PrefChangeRegistrar _prefChangeRegistrar;
  raw_ptr<UrlLoadingBrowserAgent> _URLLoadingBrowserAgent;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  raw_ptr<feature_engagement::Tracker> _engagementTracker;
  LayoutGuideCenter* _layoutGuideCenter;
  // Tracker for cancellable tasks initiated by the mediator.
  base::CancelableTaskTracker _cancelableTaskTracker;
}

- (instancetype)
    initWithMostVisitedSite:
        (std::unique_ptr<ntp_tiles::MostVisitedSites>)mostVisitedSites
             historyService:(history::HistoryService*)historyService
                prefService:(PrefService*)prefService
           largeIconService:(favicon::LargeIconService*)largeIconService
             largeIconCache:(LargeIconCache*)largeIconCache
     URLLoadingBrowserAgent:(UrlLoadingBrowserAgent*)URLLoadingBrowserAgent
      accountManagerService:(ChromeAccountManagerService*)accountManagerService
          engagementTracker:(feature_engagement::Tracker*)engagementTracker
          layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  self = [super init];
  if (self) {
    CHECK(historyService);
    CHECK(engagementTracker);
    _prefService = prefService;
    _prefChangeRegistrar.Init(_prefService);
    _URLLoadingBrowserAgent = URLLoadingBrowserAgent;
    _accountManagerService = accountManagerService;
    _historyService = historyService;
    _engagementTracker = engagementTracker;
    _layoutGuideCenter = layoutGuideCenter;
    _incognitoAvailable = !IsIncognitoModeDisabled(prefService);
    _mostVisitedAttributesProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kMagicStackFaviconWidth
             minFaviconSize:kMagicStackMostVisitedFaviconMinimalSize
           largeIconService:largeIconService];
    // Set a cache only for the Most Visited provider, as the cache is
    // overwritten for every new results and the size of the favicon fetched for
    // the suggestions is much smaller.
    _mostVisitedAttributesProvider.cache = largeIconCache;

    _mostVisitedSites = std::move(mostVisitedSites);
    _mostVisitedBridge =
        std::make_unique<ntp_tiles::MostVisitedSitesObserverBridge>(
            self, _mostVisitedSites.get());
    if (IsContentSuggestionsCustomizable()) {
      _mostVisitedSites->EnableTileTypes(
          ntp_tiles::MostVisitedSites::EnableTileTypesOptions()
              .with_top_sites(true)
              .with_custom_links(true));
    }
    _mostVisitedSites->AddMostVisitedURLsObserver(
        _mostVisitedBridge.get(), [MostVisitedTilesMediator maxSitesShown],
        kMaxNumNonCustomMostVisitedTiles);
  }
  return self;
}

- (void)disconnect {
  _cancelableTaskTracker.TryCancelAll();
  _mostVisitedBridge.reset();
  _mostVisitedSites.reset();
  _mostVisitedAttributesProvider = nil;
  _historyService = nullptr;
  _engagementTracker = nullptr;
  _accountManagerService = nullptr;
  _URLLoadingBrowserAgent = nullptr;
  _prefChangeRegistrar.RemoveAll();
  _prefService = nullptr;
}

+ (NSUInteger)maxSitesShown {
  return IsContentSuggestionsCustomizable() ? kMaxNumMostVisitedTiles
                                            : kMaxNumNonCustomMostVisitedTiles;
}

- (void)refreshMostVisitedTiles {
  // Refresh in case there are new MVT to show.
  _mostVisitedSites->Refresh();
}

- (void)disableModule {
  _prefService->SetBoolean(ntp_tiles::prefs::kMostVisitedHomeModuleEnabled,
                           false);
}

- (MostVisitedTilesConfig*)mostVisitedTilesConfig {
  return _mostVisitedConfig;
}

#pragma mark - MostVisitedSitesObserving

- (void)mostVisitedSites:(ntp_tiles::MostVisitedSites*)mostVisitedSites
          didUpdateTiles:(const ntp_tiles::NTPTilesVector&)tiles {
  // This is used by the shortcuts widget.
  content_suggestions_tile_saver::SaveMostVisitedToDisk(
      tiles, _mostVisitedAttributesProvider,
      app_group::ShortcutsWidgetFaviconsFolder(), _accountManagerService);

  _freshMostVisitedItems = [NSMutableArray array];
  int index = 0;
  for (const ntp_tiles::NTPTile& tile : tiles) {
    MostVisitedItem* item = [self convertNTPTile:tile];
    item.commandHandler = self;
    item.incognitoAvailable = _incognitoAvailable;
    item.index = index;
    item.menuElementsProvider = self;
    index++;
    [_freshMostVisitedItems addObject:item];
  }

  [self useFreshMostVisited];

  if (tiles.size() && !_recordedPageImpression) {
    _recordedPageImpression = YES;
    [self recordMostVisitedTilesDisplayed];
    ntp_tiles::metrics::RecordPageImpression(tiles.size());
  }
}

- (void)mostVisitedSites:(ntp_tiles::MostVisitedSites*)mostVisitedSites
    didUpdateFaviconForURL:(const GURL&)siteURL {
  // This is used by the shortcuts widget.
  content_suggestions_tile_saver::UpdateSingleFavicon(
      siteURL, _mostVisitedAttributesProvider,
      app_group::ShortcutsWidgetFaviconsFolder(), _accountManagerService);

  for (MostVisitedItem* item in _mostVisitedConfig.mostVisitedItems) {
    if (item.URL == siteURL) {
      FaviconCompletionHandler completion =
          _mostVisitedFetchFaviconCallbacks[siteURL];
      if (completion) {
        [_mostVisitedAttributesProvider
            fetchFaviconAttributesForURL:siteURL
                              completion:completion];
      }
      return;
    }
  }
}

#pragma mark - ContentSuggestionsImageDataSource

- (void)fetchFaviconForURL:(const GURL&)URL
                completion:(FaviconCompletionHandler)completion {
  _mostVisitedFetchFaviconCallbacks[URL] = completion;
  [_mostVisitedAttributesProvider fetchFaviconAttributesForURL:URL
                                                    completion:completion];
}

#pragma mark - MostVisitedTilesCommands

- (void)mostVisitedTileTapped:(UIGestureRecognizer*)sender {
  MostVisitedTileView* mostVisitedView =
      static_cast<MostVisitedTileView*>(sender.view);
  MostVisitedItem* mostVisitedItem =
      base::apple::ObjCCastStrict<MostVisitedItem>(
          mostVisitedView.configuration);

  [self logMostVisitedOpening:mostVisitedItem atIndex:mostVisitedItem.index];

  UrlLoadParams params = UrlLoadParams::InCurrentTab(mostVisitedItem.URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  _URLLoadingBrowserAgent->Load(params);
}

- (void)openNewTabWithMostVisitedItem:(MostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index
                            fromPoint:(CGPoint)point {
  if (incognito && IsIncognitoModeDisabled(_prefService)) {
    // This should only happen when the policy changes while the option is
    // presented.
    return;
  }
  [self logMostVisitedOpening:item atIndex:index];
  [self openNewTabWithURL:item.URL incognito:incognito originPoint:point];
}

- (void)openNewTabWithMostVisitedItem:(MostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index {
  if (incognito && IsIncognitoModeDisabled(_prefService)) {
    // This should only happen when the policy changes while the option is
    // presented.
    return;
  }
  [self logMostVisitedOpening:item atIndex:index];
  [self openNewTabWithURL:item.URL incognito:incognito originPoint:CGPointZero];
}

- (void)openNewTabWithMostVisitedItem:(MostVisitedItem*)item
                            incognito:(BOOL)incognito {
  [self openNewTabWithMostVisitedItem:item
                            incognito:incognito
                              atIndex:item.index];
}

- (void)pinOrUnpinMostVisited:(MostVisitedItem*)item {
  GURL url = item.URL;
  __weak MostVisitedTilesMediator* weakSelf = self;
  if (_mostVisitedSites->HasCustomLink(url)) {
    // Remove the custom link.
    if (_mostVisitedSites->DeleteCustomLink(url)) {
      [self showSnackbarWithMessage:
                l10n_util::GetNSString(
                    IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_SNACKBAR_UNPINNED)
                         undoAction:^{
                           [weakSelf undoLastPinAction];
                           RecordSnackbarUndoUserAction(/*undo_pin=*/NO);
                         }];
    }
    return;
  }
  if (!_mostVisitedSites->AddCustomLink(url,
                                        base::SysNSStringToUTF16(item.title))) {
    return;
  }
  _engagementTracker->NotifyEvent(
      feature_engagement::events::kIOSPinMVTSiteUsed);
  // Show snackbar message.
  [self showSnackbarWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_SNACKBAR_PINNED)
                     undoAction:^{
                       [weakSelf undoLastPinAction];
                       RecordSnackbarUndoUserAction(/*undo_pin=*/YES);
                     }];
}

- (void)removeMostVisited:(MostVisitedItem*)item {
  [self.contentSuggestionsMetricsRecorder recordMostVisitedTileRemoved];
  [self blockMostVisitedURL:item.URL];
  __weak MostVisitedTilesMediator* weakSelf = self;
  [self showSnackbarWithMessage:l10n_util::GetNSString(
                                    IDS_IOS_NEW_TAB_MOST_VISITED_ITEM_REMOVED)
                     undoAction:^{
                       [weakSelf allowMostVisitedURL:item.URL];
                     }];
}

- (void)moveMostVisitedItem:(MostVisitedItem*)item toIndex:(NSUInteger)index {
  _mostVisitedSites->ReorderCustomLink(item.URL, index);
  RecordReorderUserAction();
}

- (void)openModalToAddPinnedSite {
  [self.contentSuggestionsHandler showPinnedSiteCreator];
  RecordAddSiteUserAction();
}

- (void)openModalToEditPinnedSite:(MostVisitedItem*)item {
  [self.contentSuggestionsHandler showPinnedSiteEditorForItem:item];
}

#pragma mark - ContentSuggestionsMenuProvider

- (NSArray<UIMenuElement*>*)defaultContextMenuElementsForItem:
                                (MostVisitedItem*)item
                                                     fromView:(UIView*)view {
  // Record that this context menu was shown to the user.
  RecordMenuShown(kMenuScenarioHistogramMostVisitedEntry);

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  CGPoint centerPoint = [view.superview convertPoint:view.center toView:nil];

  __weak MostVisitedTilesMediator* weakSelf = self;
  [menuElements addObject:[self.actionFactory actionToOpenInNewTabWithBlock:^{
                  [weakSelf openNewTabWithMostVisitedItem:item
                                                incognito:NO
                                                  atIndex:item.index
                                                fromPoint:centerPoint];
                }]];

  UIAction* incognitoAction =
      [self.actionFactory actionToOpenInNewIncognitoTabWithBlock:^{
        [weakSelf openNewTabWithMostVisitedItem:item
                                      incognito:YES
                                        atIndex:item.index
                                      fromPoint:centerPoint];
      }];

  if (IsIncognitoModeDisabled(_prefService)) {
    // Disable the "Open in Incognito" option if the incognito mode is
    // disabled.
    incognitoAction.attributes = UIMenuElementAttributesDisabled;
  }

  [menuElements addObject:incognitoAction];

  if (base::ios::IsMultipleScenesSupported()) {
    UIAction* newWindowAction = [self.actionFactory
        actionToOpenInNewWindowWithURL:item.URL
                        activityOrigin:WindowActivityContentSuggestionsOrigin];
    [menuElements addObject:newWindowAction];
  }

  CrURL* URL = [[CrURL alloc] initWithGURL:item.URL];
  [menuElements addObject:[self.actionFactory actionToCopyURL:URL]];

  [menuElements addObject:[self.actionFactory actionToShareWithBlock:^{
                  [weakSelf.contentSuggestionsDelegate shareURL:item.URL
                                                          title:item.title
                                                       fromView:view];
                }]];
  if (item.isPinned) {
    CHECK(IsContentSuggestionsCustomizable(), base::NotFatalUntil::M148);
    [menuElements
        addObject:[self.actionFactory
                      actionToEditPinnedSiteOnMostVisitedTileWithBlock:^{
                        [weakSelf openModalToEditPinnedSite:item];
                      }]];
    [menuElements addObject:[self.actionFactory
                                actionToUnpinSiteFromMostVisitedTileWithBlock:^{
                                  [weakSelf pinOrUnpinMostVisited:item];
                                }]];
  } else {
    if (IsContentSuggestionsCustomizable()) {
      [menuElements addObject:[self.actionFactory
                                  actionToPinSiteToMostVisitedTileWithBlock:^{
                                    [weakSelf pinOrUnpinMostVisited:item];
                                  }]];
    }
    [menuElements addObject:[self.actionFactory actionToRemoveWithBlock:^{
                    [weakSelf removeMostVisited:item];
                  }]];
  }

  return menuElements;
}

#pragma mark - MostVisitedTilesPinnedSiteMutator

- (PinnedSiteMutationResult)addPinnedSiteWithTitle:(NSString*)title
                                               URL:(NSString*)URL {
  GURL fixedUpURL = GetValidUrl(URL);
  if (fixedUpURL.is_empty()) {
    return PinnedSiteMutationResult::kURLInvalid;
  }
  if (!_mostVisitedSites->AddCustomLink(fixedUpURL,
                                        base::SysNSStringToUTF16(title))) {
    return PinnedSiteMutationResult::kURLExisted;
  }
  _engagementTracker->NotifyEvent(
      feature_engagement::events::kIOSPinMVTSiteUsed);
  __weak MostVisitedTilesMediator* weakSelf = self;
  [self showSnackbarWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_SNACKBAR_ADDED_AND_PINNED)
                     undoAction:^{
                       [weakSelf undoLastPinAction];
                       RecordSnackbarUndoUserAction(/*undo_pin=*/YES);
                     }];
  return PinnedSiteMutationResult::kSuccess;
}

- (PinnedSiteMutationResult)editPinnedSiteForURL:(NSString*)oldURL
                                       withTitle:(NSString*)title
                                             URL:(NSString*)newURL {
  GURL newKeyURL = GetValidUrl(newURL);
  if (newKeyURL.is_empty()) {
    return PinnedSiteMutationResult::kURLInvalid;
  }
  GURL oldKeyURL = GURL(base::SysNSStringToUTF8(oldURL));
  if (oldKeyURL == newKeyURL) {
    // Do not provide the new URL if only the title is changing.
    newKeyURL = GURL();
  }
  if (!_mostVisitedSites->UpdateCustomLink(oldKeyURL, newKeyURL,
                                           base::SysNSStringToUTF16(title))) {
    return PinnedSiteMutationResult::kURLExisted;
  }
  __weak MostVisitedTilesMediator* weakSelf = self;
  [self showSnackbarWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_SNACKBAR_EDITS_SAVED)
                     undoAction:^{
                       [weakSelf undoLastPinAction];
                     }];
  return PinnedSiteMutationResult::kSuccess;
}

#pragma mark - Private

// Replaces the Most Visited items currently displayed by the most recent ones.
- (void)useFreshMostVisited {
  base::ListValue oldMostVisitedSites =
      _prefService->GetList(prefs::kIosLatestMostVisitedSites).Clone();
  base::ListValue freshMostVisitedSites;
  for (MostVisitedItem* item in _freshMostVisitedItems) {
    freshMostVisitedSites.Append(item.URL.spec());
  }

  // Don't check for a change in the Most Visited Sites if the device doesn't
  // have any saved sites to begin with. This will not log for users with no
  // top sites that have a new top site, but the benefit of not logging for
  // new installs outweighs it.
  if (!oldMostVisitedSites.empty()) {
    [self lookForNewMostVisitedSite:freshMostVisitedSites
                oldMostVisitedSites:oldMostVisitedSites];
  }
  _prefService->SetList(prefs::kIosLatestMostVisitedSites,
                        std::move(freshMostVisitedSites));

  _mostVisitedConfig = [[MostVisitedTilesConfig alloc]
      initWithLayoutGuideCenter:_layoutGuideCenter];
  _mostVisitedConfig.imageDataSource = self;
  _mostVisitedConfig.commandHandler = self;
  _mostVisitedConfig.mostVisitedItems = _freshMostVisitedItems;

  [self.consumer setMostVisitedTilesConfig:_mostVisitedConfig];
  [self.contentSuggestionsDelegate contentSuggestionsWasUpdated];
  if (IsContentSuggestionsCustomizable()) {
    [self maybeDisplayIPH];
  }
}

// Logs a histogram due to a Most Visited item being opened.
- (void)logMostVisitedOpening:(MostVisitedItem*)item
                      atIndex:(NSInteger)mostVisitedIndex {
  [self.NTPActionsDelegate mostVisitedTileOpened];
  [self.contentSuggestionsMetricsRecorder
      recordMostVisitedTileOpened:item
                          atIndex:mostVisitedIndex];
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
  params.append_to = OpenPosition::kCurrentTab;
  params.origin_point = originPoint;
  _URLLoadingBrowserAgent->Load(params);
}

- (void)blockMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlockedUrl(URL, true);
  [self useFreshMostVisited];
}

- (void)allowMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlockedUrl(URL, false);
  [self useFreshMostVisited];
}

// Updates `prefs::kIosSyncSegmentsNewTabPageDisplayCount` with the number of
// remaining New Tab Page displays that include synced history in the Most
// Visited Tiles.
- (void)recordMostVisitedTilesDisplayed {
  const int displayCount =
      _prefService->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount) +
      1;

  _prefService->SetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount,
                           displayCount);
}

// Logs a User Action if `freshMostVisitedSites` has at least one site that
// isn't in `oldMostVisitedSites`.
- (void)lookForNewMostVisitedSite:(const base::ListValue&)freshMostVisitedSites
              oldMostVisitedSites:(const base::ListValue&)oldMostVisitedSites {
  for (const auto& freshSiteURLValue : freshMostVisitedSites) {
    BOOL freshSiteInOldList = NO;
    for (const auto& oldSiteURLValue : oldMostVisitedSites) {
      if (freshSiteURLValue.GetString() == oldSiteURLValue.GetString()) {
        freshSiteInOldList = YES;
        break;
      }
    }
    if (!freshSiteInOldList) {
      // Reset impressions since freshness.
      _prefService->SetInteger(
          prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, 0);
      base::RecordAction(
          base::UserMetricsAction("IOSMostVisitedTopSitesChanged"));
      return;
    }
  }
}

// Undo the last action that adds/removes/edits a pinned site.
- (void)undoLastPinAction {
  CHECK(IsContentSuggestionsCustomizable());
  _mostVisitedSites->UndoCustomLinkAction();
}

// Converts a ntp_tiles::NTPTile `tile` to a MostVisitedItem
// with a `sectionInfo`.
- (MostVisitedItem*)convertNTPTile:(const ntp_tiles::NTPTile&)tile {
  MostVisitedItem* suggestion = [[MostVisitedItem alloc] init];

  suggestion.title = base::SysUTF16ToNSString(tile.title);
  suggestion.URL = tile.url;
  suggestion.source = tile.source;
  suggestion.titleSource = tile.title_source;

  return suggestion;
}

// Display a snackbar with `message` and an "undo" button, invoking `undoAction`
// on tap.
- (void)showSnackbarWithMessage:(NSString*)message
                     undoAction:(ProceduralBlock)undoAction {
  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.handler = undoAction;
  action.title = l10n_util::GetNSString(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  SnackbarMessage* snackbar = [[SnackbarMessage alloc] initWithTitle:message];
  snackbar.action = action;
  [self.snackbarHandler showSnackbarMessage:snackbar];
}

// Display an in-product help on the first tile of the most visited tiles if
// conditions are met.
- (void)maybeDisplayIPH {
  if (_freshMostVisitedItems.firstObject &&
      _freshMostVisitedItems.firstObject.source !=
          ntp_tiles::TileSource::TOP_SITES) {
    // The order of the items are pinned sites, most visited sites and popular
    // sites. If this happens, either the user does not have a list of most
    // visited sites, or has already pinned a site.
    return;
  }
  if (!_engagementTracker->WouldTriggerHelpUI(
          feature_engagement::kIPHiOSPinMostVisitedSiteFeature)) {
    // If the in-product help is not eligible as determined by the in-product
    // help view, return directly without consulting history service.
    return;
  }
  id<HelpCommands> helpHandler = self.helpHandler;
  auto presentIPHForRepeatingVisits =
      ^void(history::QueryURLAndVisitsResult result) {
        if (ShouldTriggerIPHForURLVisits(result)) {
          [helpHandler presentInProductHelpWithType:InProductHelpType::
                                                        kPinSiteToMostVisited];
        }
      };
  for (MostVisitedItem* item in _freshMostVisitedItems) {
    if (item.source == ntp_tiles::TileSource::TOP_SITES) {
      _historyService->GetMostRecentVisitsForGurl(
          item.URL, /*max_visits=*/kRepeatingVisitsToTriggerIPH,
          history::VisitQuery404sPolicy::kInclude404s,
          base::BindOnce(presentIPHForRepeatingVisits),
          &_cancelableTaskTracker);
    }
  }
}

@end
