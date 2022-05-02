// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#import "components/pref_registry/pref_registry_syncable.h"
#include "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#include "ios/chrome/browser/ntp_tiles/most_visited_sites_observer_bridge.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_parent_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/content_suggestions/mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_delegate.h"
#import "ios/chrome/browser/ui/ntp/metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#include "ios/chrome/browser/ui/ntp/ntp_tile_saver.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util_mac.h"

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
  std::unique_ptr<NotificationPromoWhatsNew> _notificationPromo;
  std::unique_ptr<ReadingListModelBridge> _readingListModelBridge;
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
// Parent Item for single cell layout.
@property(nonatomic, strong) ContentSuggestionsParentItem* parentItem;
// Section Info for the What's New promo section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* promoSectionInfo;
// Section Info for the Most Visited section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* mostVisitedSectionInfo;
// Section Info for the single cell parent item section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* singleCellSectionInfo;
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
// Whether the user already tapped on the NTP promo and therefore should be
// hidden.
@property(nonatomic, assign) BOOL shouldHidePromoAfterTap;
// Recorder for the metrics related to the NTP.
@property(nonatomic, strong) NTPHomeMetrics* NTPMetrics;
// Browser reference.
@property(nonatomic, assign) Browser* browser;

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
    if (IsSingleCellContentSuggestionsEnabled()) {
      _singleCellSectionInfo = SingleCellSectionInformation();
    } else {
      _promoSectionInfo = PromoSectionInformation();
      _mostVisitedSectionInfo = MostVisitedSectionInformation();
    }

    _notificationPromo = std::make_unique<NotificationPromoWhatsNew>(
        GetApplicationContext()->GetLocalState());
    _notificationPromo->Init();

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
  }
  return self;
}

// TODO: Whats this
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
  if (IsContentSuggestionsUIViewControllerMigrationEnabled()) {
    if (!self.consumer) {
      return;
    }
    if (_notificationPromo->CanShow()) {
      ContentSuggestionsWhatsNewItem* item =
          [[ContentSuggestionsWhatsNewItem alloc] initWithType:0];
      item.icon = _notificationPromo->GetIcon();
      item.text = base::SysUTF8ToNSString(_notificationPromo->promo_text());
      [self.consumer showWhatsNewViewWithConfig:item];
    }
    if (self.returnToRecentTabItem) {
      [self.consumer
          showReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
    }
    if ([self.mostVisitedItems count]) {
      [self.consumer setMostVisitedTilesWithConfigs:self.mostVisitedItems];
    }
    if (!ShouldHideShortcutsForStartSurface()) {
      [self.consumer setShortcutTilesWithConfigs:self.actionButtonItems];
    }
    return;
  }
  NSArray<ContentSuggestionsSectionInformation*>* sections =
      [self sectionsInfo];
  NSMutableDictionary<NSNumber*, NSArray*>* items =
      [[NSMutableDictionary alloc] init];
  for (ContentSuggestionsSectionInformation* section in sections) {
    items[@(section.sectionID)] = [self itemsForSectionInfo:section];
  }
  [self.collectionConsumer reloadDataWithSections:sections andItems:items];
}

- (void)blockMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlockedUrl(URL, true);
  [self useFreshMostVisited];
}

- (void)allowMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlockedUrl(URL, false);
  [self useFreshMostVisited];
}

- (NotificationPromoWhatsNew*)notificationPromo {
  return _notificationPromo.get();
}

- (void)setCollectionConsumer:
    (id<ContentSuggestionsCollectionConsumer>)collectionConsumer {
  _collectionConsumer = collectionConsumer;
  self.faviconMediator.collectionConsumer = collectionConsumer;
  [self reloadAllData];
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
  DCHECK(IsStartSurfaceEnabled());
  self.returnToRecentTabSectionInfo = ReturnToRecentTabSectionInformation();
  if (!self.returnToRecentTabItem) {
    self.returnToRecentTabItem =
        [[ContentSuggestionsReturnToRecentTabItem alloc] initWithType:0];
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
  NSString* subtitle = [NSString
      stringWithFormat:@"%@%@", base::SysUTF16ToNSString(webState->GetTitle()),
                       timeLabel];
  self.returnToRecentTabItem.subtitle = subtitle;
  self.showMostRecentTabStartSurfaceTile = YES;
  NSArray<CSCollectionViewItem*>* items =
      [self itemsForSectionInfo:self.returnToRecentTabSectionInfo];
  if (IsContentSuggestionsUIViewControllerMigrationEnabled()) {
    [self.consumer
        showReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  } else {
    [self.collectionConsumer
        addSection:self.returnToRecentTabSectionInfo
         withItems:items
        completion:^{
          [self.discoverFeedDelegate returnToRecentTabWasAdded];
        }];
  }
}

- (void)hideRecentTabTile {
  DCHECK(IsStartSurfaceEnabled());
  if (self.showMostRecentTabStartSurfaceTile) {
    self.showMostRecentTabStartSurfaceTile = NO;
    self.returnToRecentTabItem = nil;
    if (IsContentSuggestionsUIViewControllerMigrationEnabled()) {
      [self.consumer hideReturnToRecentTabTile];
    } else {
      if (IsSingleCellContentSuggestionsEnabled()) {
        [self reloadAllData];
      } else {
        [self.collectionConsumer
            clearSection:self.returnToRecentTabSectionInfo];
      }
    }
  }
}

- (void)hidePromo {
  self.shouldHidePromoAfterTap = YES;
  if (IsContentSuggestionsUIViewControllerMigrationEnabled()) {
    [self.consumer hideWhatsNewView];
  } else {
    // By reloading data, checking |notificationPromo| will remove the promo
    // view.
    [self reloadAllData];
  }
}

#pragma mark - ContentSuggestionsCommands

- (void)openMostVisitedItem:(CollectionViewItem*)item
                    atIndex:(NSInteger)mostVisitedIndex {
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  if (NTPHelper && NTPHelper->IgnoreLoadRequests())
    return;

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

// TODO(crbug.com/761096) : Promo handling should be tested.
- (void)handlePromoTapped {
  NotificationPromoWhatsNew* notificationPromo = _notificationPromo.get();
  DCHECK(notificationPromo);
  notificationPromo->HandleClosed();
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_PROMO];
  if (IsSingleCellContentSuggestionsEnabled()) {
    [self hidePromo];
  }

  if (notificationPromo->IsURLPromo()) {
    UrlLoadParams params = UrlLoadParams::InNewTab(notificationPromo->url());
    params.append_to = kCurrentTab;
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
    return;
  }

  if (notificationPromo->IsChromeCommandPromo()) {
    // "What's New" promo that runs a command can be added here by calling
    // self.dispatcher.
    if (notificationPromo->command() == kSetDefaultBrowserCommand) {
      base::RecordAction(
          base::UserMetricsAction("IOS.DefaultBrowserNTPPromoTapped"));
      [[UIApplication sharedApplication]
                    openURL:
                        [NSURL URLWithString:UIApplicationOpenSettingsURLString]
                    options:{}
          completionHandler:nil];
      return;
    }

    DCHECK_EQ(kTestWhatsNewCommand, notificationPromo->command())
        << "Promo command is not valid.";
    return;
  }
  NOTREACHED() << "Promo type is neither URL or command.";
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

#pragma mark - StartSurfaceRecentTabObserving

- (void)mostRecentTabWasRemoved:(web::WebState*)web_state {
  DCHECK(IsStartSurfaceEnabled());
  [self hideRecentTabTile];
}

- (void)mostRecentTabFaviconUpdatedWithImage:(UIImage*)image {
  if (self.returnToRecentTabItem) {
    self.returnToRecentTabItem.icon = image;
    if (IsContentSuggestionsUIViewControllerMigrationEnabled()) {
      [self.consumer
          updateReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
    } else {
      if (IsSingleCellContentSuggestionsEnabled()) {
        self.parentItem.returnToRecentItem = self.returnToRecentTabItem;
        [self.collectionConsumer itemHasChanged:self.parentItem];
      } else {
        [self.collectionConsumer itemHasChanged:self.returnToRecentTabItem];
      }
    }
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
    if (!IsSingleCellContentSuggestionsEnabled() ||
        IsContentSuggestionsUIViewControllerMigrationEnabled()) {
      [self.faviconMediator fetchFaviconForMostVisited:item];
    } else {
      [self.faviconMediator fetchFaviconForMostVisited:item
                                            parentItem:self.parentItem];
    }
    [self.freshMostVisitedItems addObject:item];
  }

  if (!IsSingleNtpEnabled() && [self.mostVisitedItems count] > 0) {
    // If some content is already displayed to the user, do not update without a
    // user action.
    return;
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
      if (!IsSingleCellContentSuggestionsEnabled() ||
          IsContentSuggestionsUIViewControllerMigrationEnabled()) {
        [self.faviconMediator fetchFaviconForMostVisited:item];
      } else {
        [self.faviconMediator fetchFaviconForMostVisited:item
                                              parentItem:self.parentItem];
      }
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
  if (IsContentSuggestionsUIViewControllerMigrationEnabled()) {
    [self.consumer setMostVisitedTilesWithConfigs:self.mostVisitedItems];
  } else {
    // All data needs to be reloaded in order to force a re-layout, this is
    // cheaper since the Feed is not part of this ViewController when Discover
    // is enabled.
    [self reloadAllData];
  }
    // TODO(crbug.com/1170995): Potentially remove once ContentSuggestions can
    // be added as part of a header.
    [self.discoverFeedDelegate contentSuggestionsWasUpdated];
}

- (NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo {
  NSMutableArray<ContentSuggestionsSectionInformation*>* sectionsInfo =
      [NSMutableArray array];

  if (!IsContentSuggestionsHeaderMigrationEnabled()) {
    [sectionsInfo addObject:self.logoSectionInfo];
  }

  if (IsSingleCellContentSuggestionsEnabled()) {
    [sectionsInfo addObject:self.singleCellSectionInfo];
  } else {
    if (self.showMostRecentTabStartSurfaceTile) {
      DCHECK(IsStartSurfaceEnabled());
      [sectionsInfo addObject:self.returnToRecentTabSectionInfo];
    }

    if (_notificationPromo->CanShow()) {
      [sectionsInfo addObject:self.promoSectionInfo];
    }

    [sectionsInfo addObject:self.mostVisitedSectionInfo];
  }

  return sectionsInfo;
}

- (NSArray<CSCollectionViewItem*>*)itemsForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  NSMutableArray<CSCollectionViewItem*>* convertedSuggestions =
      [NSMutableArray array];

  if (sectionInfo == self.logoSectionInfo) {
    // Section empty on purpose.
  } else if (sectionInfo == self.promoSectionInfo) {
    if (_notificationPromo->CanShow()) {
      ContentSuggestionsWhatsNewItem* item =
          [[ContentSuggestionsWhatsNewItem alloc] initWithType:0];
      item.icon = _notificationPromo->GetIcon();
      item.text = base::SysUTF8ToNSString(_notificationPromo->promo_text());
      [convertedSuggestions addObject:item];
    }
  } else if (sectionInfo == self.returnToRecentTabSectionInfo) {
    DCHECK(IsStartSurfaceEnabled());
    [convertedSuggestions addObject:self.returnToRecentTabItem];
  } else if (sectionInfo == self.mostVisitedSectionInfo) {
    [convertedSuggestions addObjectsFromArray:self.mostVisitedItems];
    if (!ShouldHideShortcutsForStartSurface()) {
      [convertedSuggestions addObjectsFromArray:self.actionButtonItems];
    }
  } else if (sectionInfo == self.singleCellSectionInfo) {
    self.parentItem = [[ContentSuggestionsParentItem alloc] initWithType:0];
    if (_notificationPromo->CanShow() && !self.shouldHidePromoAfterTap) {
      ContentSuggestionsWhatsNewItem* item =
          [[ContentSuggestionsWhatsNewItem alloc] initWithType:0];
      item.icon = _notificationPromo->GetIcon();
      item.text = base::SysUTF8ToNSString(_notificationPromo->promo_text());
      self.parentItem.whatsNewItem = item;
    }
    if (self.showMostRecentTabStartSurfaceTile) {
      self.parentItem.returnToRecentItem = self.returnToRecentTabItem;
    }
    self.parentItem.mostVisitedItems = self.mostVisitedItems;
    if (!ShouldHideShortcutsForStartSurface()) {
      self.parentItem.shortcutsItems = self.actionButtonItems;
    }
    [convertedSuggestions addObject:self.parentItem];
  }

  return convertedSuggestions;
}

// Opens the |URL| in a new tab |incognito| or not. |originPoint| is the origin
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
// with a |URL|.
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

#pragma mark - Properties

- (NSArray<ContentSuggestionsMostVisitedActionItem*>*)actionButtonItems {
  if (!_actionButtonItems) {
    self.readingListItem = ReadingListActionItem();
    self.readingListItem.count = self.readingListUnreadCount;
    _actionButtonItems = @[
      BookmarkActionItem(), self.readingListItem, RecentTabsActionItem(),
      HistoryActionItem()
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
    if (IsContentSuggestionsUIViewControllerMigrationEnabled()) {
      [self.consumer updateReadingListCount:self.readingListUnreadCount];
    } else {
      [self.collectionConsumer itemHasChanged:self.readingListItem];
    }
  }
}

@end
