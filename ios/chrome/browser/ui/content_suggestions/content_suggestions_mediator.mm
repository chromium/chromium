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
#import "components/feature_engagement/public/event_constants.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/start_suggest_service_factory.h"
#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
using RequestSource = SearchTermsData::RequestSource;

}  // namespace

@interface ContentSuggestionsMediator ()
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

@end

@implementation ContentSuggestionsMediator {
  // Local State prefs.
  raw_ptr<PrefService> _localState;
}

#pragma mark - Public

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
    _localState = GetApplicationContext()->GetLocalState();
  }

  return self;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosDiscoverFeedLastRefreshTime, 0);
  registry->RegisterInt64Pref(prefs::kIosDiscoverFeedLastUnseenRefreshTime, 0);
}

- (void)disconnect {
  _localState = nullptr;
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

#pragma mark - Private

- (void)configureConsumer {
  if (!self.consumer) {
    return;
  }
  if (IsMagicStackEnabled()) {
    [self.magicStackRankingModel fetchLatestMagicStackRanking];
  }
  if (self.returnToRecentTabItem) {
    [self.consumer
        showReturnToRecentTabTileWithConfig:self.returnToRecentTabItem];
  }
  if (!ShouldPutMostVisitedSitesInMagicStack() &&
      self.mostVisitedTilesMediator.mostVisitedConfig) {
    [self.consumer setMostVisitedTilesConfig:self.mostVisitedTilesMediator
                                                 .mostVisitedConfig];
  }
  if (!IsMagicStackEnabled()) {
    if ([self.setUpListMediator shouldShowSetUpList]) {
      [self.setUpListMediator showSetUpList];
    } else {
      [self.consumer
          setShortcutTilesConfig:self.shortcutsMediator.shortcutsConfig];
    }
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

- (void)setConsumer:(id<ContentSuggestionsConsumer>)consumer {
  _consumer = consumer;
  [self configureConsumer];
}

@end
