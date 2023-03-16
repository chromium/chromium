// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_mediator.h"

#import <memory>

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/logo_vendor.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/metrics/metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
                                  CRWWebStateObserver,
                                  IdentityManagerObserverBridgeDelegate,
                                  SearchEngineObserving> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
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
// Logo vendor to display the doodle on the NTP.
@property(nonatomic, strong) id<LogoVendor> logoVendor;
// This is the object that knows how to update the Identity Disc UI.
@property(nonatomic, weak) id<UserAccountImageUpdateDelegate> imageUpdater;

@end

@implementation NewTabPageMediator

- (instancetype)
            initWithWebState:(web::WebState*)webState
          templateURLService:(TemplateURLService*)templateURLService
                   URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                 authService:(AuthenticationService*)authService
             identityManager:(signin::IdentityManager*)identityManager
       accountManagerService:(ChromeAccountManagerService*)accountManagerService
                  logoVendor:(id<LogoVendor>)logoVendor
    identityDiscImageUpdater:(id<UserAccountImageUpdateDelegate>)imageUpdater {
  self = [super init];
  if (self) {
    _webState = webState;
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
    _logoVendor = logoVendor;
    _imageUpdater = imageUpdater;
  }
  return self;
}

- (void)dealloc {
  if (_webState && _webStateObserver) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

- (void)setUp {
  DCHECK(!_webStateObserver);

  _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  if (self.webState) {
    self.webState->AddObserver(_webStateObserver.get());
  }

  [self.contentSuggestionsHeaderConsumer setLogoVendor:self.logoVendor];
  [self.contentSuggestionsHeaderConsumer
      setVoiceSearchIsEnabled:ios::provider::IsVoiceSearchEnabled()];

  self.templateURLService->Load();
  [self searchEngineChanged];

  [self updateAccountImage];
}

- (void)shutdown {
  _searchEngineObserver.reset();
  if (_webState && _webStateObserver) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
  }
  _identityObserverBridge.reset();
  _accountManagerServiceObserver.reset();
  self.accountManagerService = nil;
}

- (void)saveContentOffsetForWebState:(web::WebState*)webState {
  if (webState->GetLastCommittedURL().DeprecatedGetOriginAsURL() !=
          kChromeUINewTabURL &&
      webState->GetVisibleURL().DeprecatedGetOriginAsURL() !=
          kChromeUINewTabURL) {
    // Do nothing if the current page is not the NTP.
    return;
  }

  CGFloat scrollPosition = [self.consumer scrollPosition];

  if ([self.suggestionsMediator mostRecentTabStartSurfaceTileIsShowing]) {
    // Return to Recent tab tile is only shown one time, so subtract it's
    // vertical space to preserve relative scroll position from top.
    CGFloat tileSectionHeight =
        ReturnToRecentTabHeight() +
        content_suggestions::kReturnToRecentTabSectionBottomMargin;
    if (scrollPosition > tileSectionHeight + [self.consumer pinnedOffsetY]) {
      scrollPosition -= tileSectionHeight;
    }
  }

  scrollPosition -= self.consumer.collectionShiftingOffset;

  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);

  if (NTPHelper) {
    NTPHelper->SaveNTPState(scrollPosition,
                            [self.feedControlDelegate selectedFeed]);
  }
}

// Opens web page for a menu item in the NTP.
- (void)openMenuItemWebPage:(GURL)URL {
  _URLLoader->Load(UrlLoadParams::InCurrentTab(URL));
  // TODO(crbug.com/1085419): Add metrics.
}

- (void)handleFeedManageActivityTapped {
  [self openMenuItemWebPage:GURL(kFeedManageActivityURL)];
  [self.feedMetricsRecorder recordHeaderMenuManageActivityTapped];
}

- (void)handleFeedManageInterestsTapped {
  [self openMenuItemWebPage:GURL(kFeedManageInterestsURL)];
  [self.feedMetricsRecorder recordHeaderMenuManageInterestsTapped];
}

- (void)handleFeedManageHiddenTapped {
  [self openMenuItemWebPage:GURL(kFeedManageHiddenURL)];
  [self.feedMetricsRecorder recordHeaderMenuManageHiddenTapped];
}

- (void)handleFeedLearnMoreTapped {
  [self openMenuItemWebPage:GURL(kFeedLearnMoreURL)];
  [self.feedMetricsRecorder recordHeaderMenuLearnMoreTapped];
}

- (void)handleVisitSiteFromFollowManagementList:(const GURL&)url {
  // TODO(crbug.com/1331102): Add metrics.
  [self openMenuItemWebPage:url];
}

#pragma mark - Properties.

- (void)setWebState:(web::WebState*)webState {
  if (_webState && _webStateObserver) {
    _webState->RemoveObserver(_webStateObserver.get());
    [self saveContentOffsetForWebState:_webState];
  }
  _webState = webState;
  [self.logoVendor setWebState:webState];
  if (_webState && _webStateObserver) {
    [self setContentOffsetForWebState:webState refreshFeedIfNeeded:NO];
    _webState->AddObserver(_webStateObserver.get());
  }
}

#pragma mark - CRWWebStateObserver

// Remove this once NTPCoordinator is started upon creation so
// setContentOffsetForWebState: can be called when the NTPCoordinator's WebState
// changes.
- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self setContentOffsetForWebState:webState refreshFeedIfNeeded:YES];
}

- (void)webStateWasHidden:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  DCHECK(self.contentSuggestionsHeaderConsumer);
  [self.consumer omniboxDidResignFirstResponder];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(self.webState, webState);
  self.webState = nullptr;
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
  [self.contentSuggestionsHeaderConsumer setLogoIsShowing:showLogo];
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

// Set the NTP scroll offset for the current navigation item. If
// `refreshFeedIfNeeded` is YES a feed refresh will be attempted.
- (void)setContentOffsetForWebState:(web::WebState*)webState
                refreshFeedIfNeeded:(BOOL)refreshFeedIfNeeded {
  if (webState->GetVisibleURL().DeprecatedGetOriginAsURL() !=
      kChromeUINewTabURL) {
    return;
  }

  CGFloat offsetFromSavedState;

  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper) {
    offsetFromSavedState = NTPHelper->ScrollPositionFromSavedState();
  } else {
    offsetFromSavedState = -CGFLOAT_MAX;
  }

  CGFloat minimumOffset = -[self.consumer heightAboveFeed];
  if (offsetFromSavedState > minimumOffset) {
    [self.consumer setSavedContentOffset:offsetFromSavedState];
  } else {
    // Remove this if NTPs are ever scoped back to the WebState.
    [self.consumer setContentOffsetToTop];
    // Refresh NTP content if there is is no saved scrolled state or when a new
    // NTP is opened. Since the same NTP is being shared across tabs, this
    // ensures that new content is being fetched.
    [self.suggestionsMediator refreshMostVisitedTiles];

    // Refresh DiscoverFeed unless in off-the-record NTP.
    if (!self.browser->GetBrowserState()->IsOffTheRecord() &&
        refreshFeedIfNeeded) {
      DiscoverFeedServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->RefreshFeed(FeedRefreshTrigger::kForegroundFeedVisibleOther);
    }
  }
}

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

@end
