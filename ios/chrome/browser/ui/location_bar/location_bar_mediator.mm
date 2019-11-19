// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/security_state/ios/security_state_utils.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/search_engines_util.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_consumer.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/common/origin_util.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LocationBarMediator () <CRWWebStateObserver,
                                   SearchEngineObserving,
                                   WebStateListObserving,
                                   OverlayPresenterObserving>

// The current web state associated with the toolbar.
@property(nonatomic, assign) web::WebState* webState;

// Whether the current default search engine supports search by image.
@property(nonatomic, assign) BOOL searchEngineSupportsSearchByImage;

// Whether an overlay is currently presented over the web content area.
@property(nonatomic, assign, getter=isWebContentAreaShowingOverlay)
    BOOL webContentAreaShowingOverlay;

// Whether an HTTP authentication dialog is currently presented over the
// web content area.
@property(nonatomic, assign, getter=isWebContentAreaShowingHTTPAuthDialog)
    BOOL webContentAreaShowingHTTPAuthDialog;

@end

@implementation LocationBarMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  std::unique_ptr<OverlayPresenterObserverBridge> _overlayObserver;
}

- (instancetype)initWithLocationBarModel:(LocationBarModel*)locationBarModel {
  DCHECK(locationBarModel);
  self = [super init];
  if (self) {
    _locationBarModel = locationBarModel;
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _overlayObserver = std::make_unique<OverlayPresenterObserverBridge>(self);
    _searchEngineSupportsSearchByImage = NO;
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)disconnect {
  self.webStateList = nullptr;
  self.webContentAreaOverlayPresenter = nullptr;
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webState:(web::WebState*)webState
    didPruneNavigationItemsWithCount:(size_t)pruned_item_count {
  DCHECK_EQ(_webState, webState);
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  // Currently, because of https://crbug.com/448486 , interstitials are not
  // commited navigations. This means that if a security interstitial (e.g. for
  // broken HTTPS) is shown, didFinishNavigation: is not called, and
  // didChangeVisibleSecurityState: is the only chance to update the URL.
  // Otherwise it would be preferable to only update the icon here.
  [self notifyConsumerOfChangedLocation];

  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webState = nullptr;
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request {
  self.webContentAreaShowingOverlay = YES;
  self.webContentAreaShowingHTTPAuthDialog =
      !!request->GetConfig<HTTPAuthOverlayRequestConfig>();
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  self.webContentAreaShowingOverlay = NO;
  self.webContentAreaShowingHTTPAuthDialog = NO;
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  DCHECK_EQ(_webStateList, webStateList);
  self.webState = newWebState;
  [self.consumer defocusOmnibox];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(self.templateURLService);
}

#pragma mark - Setters

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }

  _webState = webState;

  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());

    if (self.consumer) {
      [self notifyConsumerOfChangedLocation];
      [self notifyConsumerOfChangedSecurityIcon];
    }
  }
}

- (void)setConsumer:(id<LocationBarConsumer>)consumer {
  _consumer = consumer;
  if (self.webState) {
    [self notifyConsumerOfChangedLocation];
    [self notifyConsumerOfChangedSecurityIcon];
  }
  [consumer
      updateSearchByImageSupported:self.searchEngineSupportsSearchByImage];
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    self.webState = self.webStateList->GetActiveWebState();
    _webStateList->AddObserver(_webStateListObserver.get());
  } else {
    self.webState = nullptr;
  }
}

- (void)setWebContentAreaOverlayPresenter:
    (OverlayPresenter*)webContentAreaOverlayPresenter {
  if (_webContentAreaOverlayPresenter)
    _webContentAreaOverlayPresenter->RemoveObserver(_overlayObserver.get());

  _webContentAreaOverlayPresenter = webContentAreaOverlayPresenter;

  if (_webContentAreaOverlayPresenter)
    _webContentAreaOverlayPresenter->AddObserver(_overlayObserver.get());
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(templateURLService);
  _searchEngineObserver =
      std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
}

- (void)setSearchEngineSupportsSearchByImage:
    (BOOL)searchEngineSupportsSearchByImage {
  BOOL supportChanged =
      _searchEngineSupportsSearchByImage != searchEngineSupportsSearchByImage;
  _searchEngineSupportsSearchByImage = searchEngineSupportsSearchByImage;
  if (supportChanged) {
    [self.consumer
        updateSearchByImageSupported:searchEngineSupportsSearchByImage];
  }
}

- (void)setWebContentAreaShowingOverlay:(BOOL)webContentAreaShowingOverlay {
  if (_webContentAreaShowingOverlay == webContentAreaShowingOverlay)
    return;
  _webContentAreaShowingOverlay = webContentAreaShowingOverlay;
  [self.consumer updateLocationShareable:[self isSharingEnabled]];
}

- (void)setWebContentAreaShowingHTTPAuthDialog:
    (BOOL)webContentAreaShowingHTTPAuthDialog {
  if (_webContentAreaShowingHTTPAuthDialog ==
      webContentAreaShowingHTTPAuthDialog) {
    return;
  }
  _webContentAreaShowingHTTPAuthDialog = webContentAreaShowingHTTPAuthDialog;
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

#pragma mark - private

- (void)notifyConsumerOfChangedLocation {
  [self.consumer updateLocationText:[self currentLocationString]
                           clipTail:[self locationShouldClipTail]];
  GURL URL = self.webState->GetVisibleURL();
  BOOL isNTP = IsURLNewTabPage(URL);
  if (isNTP) {
    [self.consumer updateAfterNavigatingToNTP];
  }
  [self.consumer updateLocationShareable:[self isSharingEnabled]];
}

- (void)notifyConsumerOfChangedSecurityIcon {
  [self.consumer updateLocationIcon:[self currentLocationIcon]
                 securityStatusText:[self securityStatusText]];
}

#pragma mark Location helpers

- (NSString*)currentLocationString {
  if (self.webContentAreaShowingHTTPAuthDialog)
    return l10n_util::GetNSString(IDS_IOS_LOCATION_BAR_SIGN_IN);
  base::string16 string = self.locationBarModel->GetURLForDisplay();
  return base::SysUTF16ToNSString(string);
}

// Some URLs (data://) should have their tail clipped when presented; while for
// others (http://) it would be more appropriate to clip the head.
- (BOOL)locationShouldClipTail {
  if (self.webContentAreaShowingHTTPAuthDialog)
    return YES;
  GURL url = self.locationBarModel->GetURL();
  return url.SchemeIs(url::kDataScheme);
}

#pragma mark Security status icon helpers

- (UIImage*)currentLocationIcon {
  if (!self.locationBarModel->ShouldDisplayURL() ||
      self.webContentAreaShowingHTTPAuthDialog) {
    return nil;
  }

  if (self.locationBarModel->IsOfflinePage()) {
    return [self imageForOfflinePage];
  }

  security_state::SecurityLevel securityLevel =
      self.locationBarModel->GetSecurityLevel();

  bool shouldDowngrade = security_state::ShouldDowngradeNeutralStyling(
      securityLevel, self.locationBarModel->GetURL(),
      base::BindRepeating(&web::IsOriginSecure));

  return GetLocationBarSecurityIconForSecurityState(securityLevel,
                                                    shouldDowngrade);
}

// Returns a location icon for offline pages.
- (UIImage*)imageForOfflinePage {
  return [[UIImage imageNamed:@"location_bar_offline"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

// The status text associated with the current location icon.
- (NSString*)securityStatusText {
  if (self.webContentAreaShowingHTTPAuthDialog)
    return nil;
  return base::SysUTF16ToNSString(
      self.locationBarModel->GetSecureAccessibilityText());
}

#pragma mark Shareability helpers

- (BOOL)isSharingEnabled {
  // Page sharing requires JavaScript execution, which is paused while overlays
  // are displayed over the web content area.
  if (self.webContentAreaShowingOverlay)
    return NO;

  const GURL& URL = self.webState->GetLastCommittedURL();
  return URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL);
}

@end
