// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/ntp_tiles/features.h"
#import "components/ntp_tiles/metrics.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp_tiles/model/most_visited_sites_observer_bridge.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_saver.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_stack_view_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_stack_view_consumer_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Maximum number of most visited tiles fetched.
const NSInteger kMaxNumMostVisitedTiles = 4;

// Size below which the provider returns a colored tile instead of an image.
const CGFloat kMagicStackMostVisitedFaviconMinimalSize = 18;

}  // namespace

@interface MostVisitedTilesMediator () <MostVisitedSitesObserving,
                                        MostVisitedTilesStackViewConsumerSource,
                                        ContentSuggestionsMenuProvider>
@end

@implementation MostVisitedTilesMediator {
  std::unique_ptr<ntp_tiles::MostVisitedSites> _mostVisitedSites;
  std::unique_ptr<ntp_tiles::MostVisitedSitesObserverBridge> _mostVisitedBridge;
  FaviconAttributesProvider* _mostVisitedAttributesProvider;
  std::map<GURL, FaviconCompletionHandler> _mostVisitedFetchFaviconCallbacks;
  NSMutableArray<ContentSuggestionsMostVisitedItem*>* _freshMostVisitedItems;
  // Most visited items from the MostVisitedSites service currently displayed.
  MostVisitedTilesConfig* _mostVisitedConfig;
  // Whether incognito mode is available.
  BOOL _incognitoAvailable;
  BOOL _recordedPageImpression;
  raw_ptr<PrefService> _prefService;
  raw_ptr<UrlLoadingBrowserAgent> _URLLoadingBrowserAgent;
  // Consumer of model updates when MVTs are in the Magic Stack.
  id<MostVisitedTilesStackViewConsumer> _stackViewConsumer;
}

- (instancetype)
    initWithMostVisitedSite:
        (std::unique_ptr<ntp_tiles::MostVisitedSites>)mostVisitedSites
                prefService:(PrefService*)prefService
           largeIconService:(favicon::LargeIconService*)largeIconService
             largeIconCache:(LargeIconCache*)largeIconCache
     URLLoadingBrowserAgent:(UrlLoadingBrowserAgent*)URLLoadingBrowserAgent {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _URLLoadingBrowserAgent = URLLoadingBrowserAgent;
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
        std::make_unique<ntp_tiles::MostVisitedSitesObserverBridge>(self);
    _mostVisitedSites->AddMostVisitedURLsObserver(_mostVisitedBridge.get(),
                                                  kMaxNumMostVisitedTiles);
  }
  return self;
}

- (void)disconnect {
  _mostVisitedBridge.reset();
  _mostVisitedSites.reset();
  _mostVisitedAttributesProvider = nil;
}

+ (NSUInteger)maxSitesShown {
  return kMaxNumMostVisitedTiles;
}

- (void)refreshMostVisitedTiles {
  // Refresh in case there are new MVT to show.
  _mostVisitedSites->Refresh();
}

- (MostVisitedTilesConfig*)mostVisitedTilesConfig {
  return _mostVisitedConfig;
}

#pragma mark - MostVisitedSitesObserving

- (void)onMostVisitedURLsAvailable:
    (const ntp_tiles::NTPTilesVector&)mostVisited {
  // This is used by the content widget.
  content_suggestions_tile_saver::SaveMostVisitedToDisk(
      mostVisited, _mostVisitedAttributesProvider,
      app_group::ContentWidgetFaviconsFolder());

  _freshMostVisitedItems = [NSMutableArray array];
  int index = 0;
  for (const ntp_tiles::NTPTile& tile : mostVisited) {
    ContentSuggestionsMostVisitedItem* item = [self convertNTPTile:tile];
    item.commandHandler = self;
    item.incognitoAvailable = _incognitoAvailable;
    item.index = index;
    item.menuProvider = self;
    DCHECK(index < kShortcutMinimumIndex);
    index++;
    [_freshMostVisitedItems addObject:item];
  }

  [self useFreshMostVisited];

  if (mostVisited.size() && !_recordedPageImpression) {
    _recordedPageImpression = YES;
    [self recordMostVisitedTilesDisplayed];
    ntp_tiles::metrics::RecordPageImpression(mostVisited.size());
  }
}

- (void)onIconMadeAvailable:(const GURL&)siteURL {
  // This is used by the content widget.
  content_suggestions_tile_saver::UpdateSingleFavicon(
      siteURL, _mostVisitedAttributesProvider,
      app_group::ContentWidgetFaviconsFolder());

  for (ContentSuggestionsMostVisitedItem* item in _mostVisitedConfig
           .mostVisitedItems) {
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
  ContentSuggestionsMostVisitedTileView* mostVisitedView =
      static_cast<ContentSuggestionsMostVisitedTileView*>(sender.view);
  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::apple::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(
          mostVisitedView.config);

  [self logMostVisitedOpening:mostVisitedItem atIndex:mostVisitedItem.index];

  UrlLoadParams params = UrlLoadParams::InCurrentTab(mostVisitedItem.URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  _URLLoadingBrowserAgent->Load(params);
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
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

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
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

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito {
  [self openNewTabWithMostVisitedItem:item
                            incognito:incognito
                              atIndex:item.index];
}

- (void)removeMostVisited:(ContentSuggestionsMostVisitedItem*)item {
  [self.contentSuggestionsMetricsRecorder recordMostVisitedTileRemoved];
  [self blockMostVisitedURL:item.URL];
  [self showMostVisitedUndoForURL:item.URL];
}

#pragma mark - ContentSuggestionsMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (ContentSuggestionsMostVisitedItem*)item
                                                      fromView:(UIView*)view {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        MostVisitedTilesMediator* strongSelf = weakSelf;
        if (!strongSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }
        return [strongSelf contextMenuActionProviderForItem:item fromView:view];
      };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - MostVisitedTilesStackViewConsumerSource

- (void)addConsumer:(id<MostVisitedTilesStackViewConsumer>)consumer {
  if (_stackViewConsumer == consumer) {
    return;
  }
  _stackViewConsumer = consumer;
}

#pragma mark - Private

- (UIMenu*)contextMenuActionProviderForItem:
               (ContentSuggestionsMostVisitedItem*)item
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

  [menuElements addObject:[self.actionFactory actionToRemoveWithBlock:^{
                  [weakSelf removeMostVisited:item];
                }]];

  return [UIMenu menuWithTitle:@"" children:menuElements];
}

// Replaces the Most Visited items currently displayed by the most recent ones.
- (void)useFreshMostVisited {
    const base::Value::List& oldMostVisitedSites =
        _prefService->GetList(prefs::kIosLatestMostVisitedSites);
    base::Value::List freshMostVisitedSites;
    for (ContentSuggestionsMostVisitedItem* item in _freshMostVisitedItems) {
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

    _mostVisitedConfig = [[MostVisitedTilesConfig alloc] init];
    _mostVisitedConfig.imageDataSource = self;
    _mostVisitedConfig.commandHandler = self;
    _mostVisitedConfig.mostVisitedItems = _freshMostVisitedItems;
    _mostVisitedConfig.consumerSource = self;
    if (ShouldPutMostVisitedSitesInMagicStack()) {
      if ([_freshMostVisitedItems count] == 0) {
        [self.delegate removeMostVisitedTilesModule];
      } else if (!oldMostVisitedSites.empty()) {
        [_stackViewConsumer updateWithConfig:_mostVisitedConfig];
      } else {
        [self.delegate didReceiveInitialMostVistedTiles];
      }
    } else {
      [self.consumer setMostVisitedTilesConfig:_mostVisitedConfig];
      [self.contentSuggestionsDelegate contentSuggestionsWasUpdated];
    }
}

// Logs a histogram due to a Most Visited item being opened.
- (void)logMostVisitedOpening:(ContentSuggestionsMostVisitedItem*)item
                      atIndex:(NSInteger)mostVisitedIndex {
  [self.NTPActionsDelegate mostVisitedTileOpened];
  if (ShouldPutMostVisitedSitesInMagicStack()) {
    [self.delegate logMagicStackEngagementForType:ContentSuggestionsModuleType::
                                                      kMostVisited];
  }
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

// Shows a snackbar with an action to undo the removal of the most visited item
// with a `URL`.
- (void)showMostVisitedUndoForURL:(GURL)URL {
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  __weak MostVisitedTilesMediator* weakSelf = self;
  action.handler = ^{
    [weakSelf allowMostVisitedURL:URL];
  };
  action.title = l10n_util::GetNSString(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  action.accessibilityIdentifier = @"Undo";

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = CreateSnackbarMessage(
      l10n_util::GetNSString(IDS_IOS_NEW_TAB_MOST_VISITED_ITEM_REMOVED));
  message.action = action;
  message.category = @"MostVisitedUndo";
  [self.snackbarHandler showSnackbarMessage:message];
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
- (void)
    lookForNewMostVisitedSite:(const base::Value::List&)freshMostVisitedSites
          oldMostVisitedSites:(const base::Value::List&)oldMostVisitedSites {
  for (auto const& freshSiteURLValue : freshMostVisitedSites) {
    BOOL freshSiteInOldList = NO;
    for (auto const& oldSiteURLValue : oldMostVisitedSites) {
      if (freshSiteURLValue.GetString() == oldSiteURLValue.GetString()) {
        freshSiteInOldList = YES;
        break;
      }
    }
    if (!freshSiteInOldList) {
      // Reset impressions since freshness.
      GetApplicationContext()->GetLocalState()->SetInteger(
          prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, 0);
      base::RecordAction(
          base::UserMetricsAction("IOSMostVisitedTopSitesChanged"));
      return;
    }
  }
}

// Converts a ntp_tiles::NTPTile `tile` to a ContentSuggestionsMostVisitedItem
// with a `sectionInfo`.
- (ContentSuggestionsMostVisitedItem*)convertNTPTile:
    (const ntp_tiles::NTPTile&)tile {
  ContentSuggestionsMostVisitedItem* suggestion =
      [[ContentSuggestionsMostVisitedItem alloc] init];

  suggestion.title = base::SysUTF16ToNSString(tile.title);
  suggestion.URL = tile.url;
  suggestion.source = tile.source;
  suggestion.titleSource = tile.title_source;

  return suggestion;
}

@end
