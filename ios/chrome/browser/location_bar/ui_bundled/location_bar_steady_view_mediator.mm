// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view_consumer.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"

@interface LocationBarSteadyViewMediator () <CRWWebStateObserver,
                                             WebStateListObserving,
                                             OverlayPresenterObserving>

// Whether an overlay is currently presented over the web content area.
@property(nonatomic, assign, getter=isWebContentAreaShowingOverlay)
    BOOL webContentAreaShowingOverlay;

// Whether an HTTP authentication dialog is currently presented over the
// web content area.
@property(nonatomic, assign, getter=isWebContentAreaShowingHTTPAuthDialog)
    BOOL webContentAreaShowingHTTPAuthDialog;

// The location bar model used by this mediator to extract the current URL and
// the security state.
@property(nonatomic, assign, readonly) LocationBarModel* locationBarModel;

@property(nonatomic, readonly) web::WebState* currentWebState;

@end

@implementation LocationBarSteadyViewMediator {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;
  std::unique_ptr<OverlayPresenterObserverBridge> _overlayObserver;
}

- (instancetype)initWithLocationBarModel:(LocationBarModel*)locationBarModel {
  DCHECK(locationBarModel);
  self = [super init];
  if (self) {
    _locationBarModel = locationBarModel;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _overlayObserver = std::make_unique<OverlayPresenterObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  CHECK(!self.webStateList);
}

- (void)disconnect {
  _forwarder = nullptr;
  _webStateObserver = nullptr;

  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateList = nullptr;
  }
  _webStateListObserver = nullptr;

  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->RemoveObserver(_overlayObserver.get());
    _webContentAreaOverlayPresenter = nullptr;
  }
  _overlayObserver = nullptr;
}

#pragma mark - Setters

- (void)setConsumer:(id<LocationBarSteadyViewConsumer>)consumer {
  _consumer = consumer;
  if (self.webStateList) {
    [self notifyConsumerOfChangedLocation];
    [self notifyConsumerOfChangedSecurityIcon];
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _forwarder = nullptr;
  }

  _webStateList = webStateList;

  if (_webStateList) {
    _webStateList->AddObserver(_webStateListObserver.get());
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        _webStateList, _webStateObserver.get());
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

- (void)setWebContentAreaOverlayPresenter:
    (OverlayPresenter*)webContentAreaOverlayPresenter {
  if (_webContentAreaOverlayPresenter)
    _webContentAreaOverlayPresenter->RemoveObserver(_overlayObserver.get());

  _webContentAreaOverlayPresenter = webContentAreaOverlayPresenter;

  if (_webContentAreaOverlayPresenter)
    _webContentAreaOverlayPresenter->AddObserver(_overlayObserver.get());
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self notifyConsumerOfChangedLocation];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
  __weak __typeof(self) weakSelf = self;

  self.tracker->AddOnInitializedCallback(base::BindRepeating(^(bool success) {
    if (!success) {
      return;
    }
    [weakSelf.consumer attemptShowingLensOverlayIPH];
  }));
  // Records the leading icon type when the document changes to avoid too many
  // recording. Don't record on NTP as the leading icon is not visible.
  if (navigation && !navigation->IsSameDocument() &&
      !IsURLNewTabPage(navigation->GetUrl())) {
    [self.consumer recordLensOverlayAvailability];
  }
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self notifyConsumerOfChangedLocation];
  [self notifyConsumerOfChangedSecurityIcon];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  // Currently, because of https://crbug.com/448486 , interstitials are not
  // commited navigations. This means that if a security interstitial (e.g. for
  // broken HTTPS) is shown, didFinishNavigation: is not called, and
  // didChangeVisibleSecurityState: is the only chance to update the URL.
  // Otherwise it would be preferable to only update the icon here.
  [self notifyConsumerOfChangedLocation];

  [self notifyConsumerOfChangedSecurityIcon];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change()) {
    [self notifyConsumerOfChangedLocation];
    [self notifyConsumerOfChangedSecurityIcon];
  }
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  self.webContentAreaShowingOverlay = YES;
  self.webContentAreaShowingHTTPAuthDialog =
      !!request->GetConfig<HTTPAuthOverlayRequestConfig>();
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  self.webContentAreaShowingOverlay = NO;
  self.webContentAreaShowingHTTPAuthDialog = NO;
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  self.webContentAreaOverlayPresenter = nullptr;
}

#pragma mark - private

- (web::WebState*)currentWebState {
  return self.webStateList ? self.webStateList->GetActiveWebState() : nullptr;
}

- (void)notifyConsumerOfChangedLocation {
  [self.consumer updateLocationText:[self currentLocationString]
                           clipTail:[self locationShouldClipTail]];
  GURL URL =
      self.currentWebState ? self.currentWebState->GetVisibleURL() : GURL();
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
  std::u16string string = self.locationBarModel->GetURLForDisplay();
  return base::SysUTF16ToNSString(string);
}

// Data URLs (data://) should have their tail clipped when presented; while for
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

  return GetLocationBarSecurityIconForSecurityState(
      self.locationBarModel->GetSecurityLevel());
}

// Returns a location icon for offline pages.
- (UIImage*)imageForOfflinePage {
  return GetLocationBarOfflineIcon();
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

  if (!self.currentWebState) {
    return NO;
  }

  const GURL& URL = self.currentWebState->GetLastCommittedURL();

  // Enable sharing when the current page url is valid and the url is not app
  // specific (the url's scheme is `chrome`) except when:
  // 1. The page url represents a chrome's download path `chrome://downloads`.
  // 2. The page url is a reference to an external file
  // `chrome://external-file`.
  return URL.is_valid() &&
         (UrlIsDownloadedFile(URL) || UrlIsExternalFileReference(URL) ||
          !web::GetWebClient()->IsAppSpecificURL(URL));
}

@end
