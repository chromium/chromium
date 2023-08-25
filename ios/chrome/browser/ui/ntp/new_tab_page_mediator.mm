// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_mediator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/new_tab_page_state.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
// URL for 'Manage Activity' item in the Discover feed menu.
const char kFeedManageActivityURL[] =
    "https://myactivity.google.com/myactivity?product=50";
// URL for 'Manage Interests' item in the Discover feed menu.
const char kFeedManageInterestsURL[] =
    "https://google.com/preferences/interests";
// URL for 'Manage Hidden' item in the Discover feed menu.
const char kFeedManageHiddenURL[] =
    "https://google.com/preferences/interests/hidden";
// URL for 'Learn More' item in the Discover feed menu;
const char kFeedLearnMoreURL[] = "https://support.google.com/chrome/"
                                 "?p=new_tab&co=GENIE.Platform%3DiOS&oco=1";
}  // namespace

@interface NewTabPageMediator () <ChromeAccountManagerServiceObserver,
                                  IdentityManagerObserverBridgeDelegate,
                                  SearchEngineObserving> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // Listen for default search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  // Observes changes in identity and updates the Identity Disc.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  // Used to load URLs.
  UrlLoadingBrowserAgent* _URLLoader;
}

@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;
// Authentication Service to get the current user's avatar.
@property(nonatomic, assign) AuthenticationService* authService;
// This is the object that knows how to update the Identity Disc UI.
@property(nonatomic, weak) id<UserAccountImageUpdateDelegate> imageUpdater;
// Yes if the browser is currently in incognito mode.
@property(nonatomic, assign) BOOL isIncognito;
// DiscoverFeed Service to display the Feed.
@property(nonatomic, assign) DiscoverFeedService* discoverFeedService;

@end

@implementation NewTabPageMediator

// Synthesized from NewTabPageMutator.
@synthesize scrollPositionToSave = _scrollPositionToSave;

- (instancetype)
    initWithTemplateURLService:(TemplateURLService*)templateURLService
                     URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                   authService:(AuthenticationService*)authService
               identityManager:(signin::IdentityManager*)identityManager
         accountManagerService:
             (ChromeAccountManagerService*)accountManagerService
      identityDiscImageUpdater:(id<UserAccountImageUpdateDelegate>)imageUpdater
                   isIncognito:(BOOL)isIncognito
           discoverFeedService:(DiscoverFeedService*)discoverFeedService {
  self = [super init];
  if (self) {
    CHECK(accountManagerService);
    _templateURLService = templateURLService;
    _URLLoader = URLLoader;
    _authService = authService;
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(identityManager, self));
    // Listen for default search engine changes.
    _searchEngineObserver = std::make_unique<SearchEngineObserverBridge>(
        self, self.templateURLService);
    _imageUpdater = imageUpdater;
    _isIncognito = isIncognito;
    _discoverFeedService = discoverFeedService;
  }
  return self;
}

- (void)setUp {
  [self.headerConsumer
      setVoiceSearchIsEnabled:ios::provider::IsVoiceSearchEnabled()];

  self.templateURLService->Load();
  [self searchEngineChanged];

  [self updateAccountImage];
}

- (void)shutdown {
  _searchEngineObserver.reset();
  _identityObserverBridge.reset();
  _accountManagerServiceObserver.reset();
  self.accountManagerService = nil;
  self.discoverFeedService = nullptr;
}

- (void)handleFeedLearnMoreTapped {
  [self.feedMetricsRecorder recordHeaderMenuLearnMoreTapped];
  [self openMenuItemWebPage:GURL(kFeedLearnMoreURL)];
}

- (void)saveNTPStateForWebState:(web::WebState*)webState {
  NewTabPageTabHelper::FromWebState(webState)->SetNTPState(
      [[NewTabPageState alloc]
          initWithScrollPosition:self.scrollPositionToSave
                    selectedFeed:[self.feedControlDelegate selectedFeed]]);
}

- (void)restoreNTPStateForWebState:(web::WebState*)webState {
  NewTabPageState* ntpState =
      NewTabPageTabHelper::FromWebState(webState)->GetNTPState();
  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    [self.NTPContentDelegate updateForSelectedFeed:ntpState.selectedFeed];
  }

  if (ntpState.shouldScrollToTopOfFeed) {
    [self.consumer restoreScrollPositionToTopOfFeed];
    // Prevent next NTP from being scrolled to the top of feed.
    ntpState.shouldScrollToTopOfFeed = NO;
    NewTabPageTabHelper::FromWebState(webState)->SetNTPState(ntpState);
  } else {
    [self.consumer restoreScrollPosition:ntpState.scrollPosition];
  }
}

#pragma mark - FeedManagementNavigationDelegate

- (void)handleNavigateToActivity {
  [self.feedMetricsRecorder recordHeaderMenuManageActivityTapped];
  [self openMenuItemWebPage:GURL(kFeedManageActivityURL)];
}

- (void)handleNavigateToInterests {
  [self.feedMetricsRecorder recordHeaderMenuManageInterestsTapped];
  [self openMenuItemWebPage:GURL(kFeedManageInterestsURL)];
}

- (void)handleNavigateToHidden {
  [self.feedMetricsRecorder recordHeaderMenuManageHiddenTapped];
  [self openMenuItemWebPage:GURL(kFeedManageHiddenURL)];
}

- (void)handleNavigateToFollowedURL:(const GURL&)url {
  // TODO(crbug.com/1331102): Add metrics.
  [self openMenuItemWebPage:url];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  [self updateAccountImage];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  BOOL showLogo = NO;
  const TemplateURL* defaultURL =
      self.templateURLService->GetDefaultSearchProvider();
  if (defaultURL) {
    showLogo = defaultURL->GetEngineType(
                   self.templateURLService->search_terms_data()) ==
               SEARCH_ENGINE_GOOGLE;
  }
  [self.headerConsumer setLogoIsShowing:showLogo];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateAccountImage];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - Private

// Fetches and update user's avatar on NTP, or use default avatar if user is
// not signed in.
- (void)updateAccountImage {
  // Fetches user's identity from Authentication Service.
  id<SystemIdentity> identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (identity) {
    // Only show an avatar if the user is signed in.
    UIImage* image = self.accountManagerService->GetIdentityAvatarWithIdentity(
        identity, IdentityAvatarSize::SmallSize);
    [self.imageUpdater updateAccountImage:image
                                     name:identity.userFullName
                                    email:identity.userEmail];
  } else {
    [self.imageUpdater setSignedOutAccountImage];
  }
}

// Opens web page for a menu item in the NTP.
- (void)openMenuItemWebPage:(GURL)URL {
  _URLLoader->Load(UrlLoadParams::InCurrentTab(URL));
  // TODO(crbug.com/1085419): Add metrics.
}

@end
