// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/prerender/preload_controller.h"

#include "base/ios/device_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_service.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/history/history_tab_helper.h"
#import "ios/chrome/browser/itunes_urls/itunes_urls_handler_tab_helper.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prerender/preload_controller_delegate.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#include "ios/chrome/browser/ui/prerender_final_status.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_state/web_state_observer_bridge.h"
#import "ios/web/public/web_state/web_state_policy_decider_bridge.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::WebStatePolicyDecider;

namespace {
// Delay before starting to prerender a URL.
const NSTimeInterval kPrerenderDelay = 0.5;

// The finch experiment to turn off prerendering as a field trial.
const char kTabEvictionFieldTrialName[] = "TabEviction";
// The associated group.
const char kPrerenderTabEvictionTrialGroup[] = "NoPrerendering";
// The name of the histogram for recording final status (e.g. used/cancelled)
// of prerender requests.
const char kPrerenderFinalStatusHistogramName[] = "Prerender.FinalStatus";
// The name of the histogram for recording the number of successful prerenders.
const char kPrerendersPerSessionCountHistogramName[] =
    "Prerender.PrerendersPerSessionCount";

// Is this install selected for this particular experiment.
bool IsPrerenderTabEvictionExperimentalGroup() {
  base::FieldTrial* trial =
      base::FieldTrialList::Find(kTabEvictionFieldTrialName);
  return trial && trial->group_name() == kPrerenderTabEvictionTrialGroup;
}

}  // namespace

@interface PreloadController (PrivateMethods)

// Returns YES if prerendering is enabled.
- (BOOL)isPrerenderingEnabled;

// Returns YES if the |url| is valid for prerendering.
- (BOOL)shouldPreloadURL:(const GURL&)url;

// Called to start any scheduled prerendering requests.
- (void)startPrerender;

// Destroys the preview Tab and resets |prerenderURL_| to the empty URL.
- (void)destroyPreviewContents;

// Schedules the current prerender to be cancelled during the next run of the
// event loop.
- (void)schedulePrerenderCancel;

// Removes any scheduled prerender requests and resets |scheduledURL| to the
// empty URL.
- (void)removeScheduledPrerenderRequests;

@end

@interface PreloadController ()<CRWWebStateObserver,
                                CRWWebStatePolicyDecider,
                                ManageAccountsDelegate,
                                PrefObserverDelegate>
@end

@implementation PreloadController {
  ios::ChromeBrowserState* browserState_;  // Weak.

  // The WebState used for prerendering.
  std::unique_ptr<web::WebState> webState_;

  // The WebStateDelegateBridge used to register self as a CRWWebStateDelegate
  // with the pre-rendered WebState.
  std::unique_ptr<web::WebStateDelegateBridge> webStateDelegate_;

  // The WebStateObserverBridge used to register self as a WebStateObserver
  // with the pre-rendered WebState.
  std::unique_ptr<web::WebStateObserverBridge> webStateObserver_;

  // The URL that is prerendered in |webState_|.  This can be different from
  // the value returned by WebState last committed navigation item, for example
  // in cases where there was a redirect.
  //
  // When choosing whether or not to use a prerendered Tab,
  // BrowserViewController compares the URL being loaded by the omnibox with the
  // URL of the prerendered Tab.  Comparing against the Tab's currently URL
  // could return false negatives in cases of redirect, hence the need to store
  // the originally prerendered URL.
  GURL prerenderedURL_;

  // The URL that is scheduled to be prerendered, its associated transition and
  // referrer. |scheduledTransition_| and |scheduledReferrer_| are not valid
  // when |scheduledURL_| is empty.
  GURL scheduledURL_;
  ui::PageTransition scheduledTransition_;
  web::Referrer scheduledReferrer_;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> observerBridge_;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar prefChangeRegistrar_;
  // Observer for the WWAN setting.  Contains a valid object only if the
  // instant setting is set to wifi-only.
  std::unique_ptr<ConnectionTypeObserverBridge> connectionTypeObserverBridge_;

  // Whether or not the preference is enabled.
  BOOL enabled_;
  // Whether or not prerendering is only when on wifi.
  BOOL wifiOnly_;
  // Whether or not the current connection is using WWAN.
  BOOL usingWWAN_;

  // Number of successful prerenders (i.e. the user viewed the prerendered page)
  // during the lifetime of this controller.
  int successfulPrerendersPerSessionCount_;

  // Bridge to provide navigation policies for |webState_|.
  std::unique_ptr<web::WebStatePolicyDeciderBridge> policyDeciderBridge_;
}

@synthesize prerenderedURL = prerenderedURL_;
@synthesize delegate = delegate_;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if ((self = [super init])) {
    browserState_ = browserState;
    enabled_ =
        browserState_->GetPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled);
    wifiOnly_ = browserState_->GetPrefs()->GetBoolean(
        prefs::kNetworkPredictionWifiOnly);
    usingWWAN_ = net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType());
    webStateDelegate_ = std::make_unique<web::WebStateDelegateBridge>(self);
    webStateObserver_ = std::make_unique<web::WebStateObserverBridge>(self);
    observerBridge_ = std::make_unique<PrefObserverBridge>(self);
    prefChangeRegistrar_.Init(browserState_->GetPrefs());
    observerBridge_->ObserveChangesForPreference(
        prefs::kNetworkPredictionEnabled, &prefChangeRegistrar_);
    observerBridge_->ObserveChangesForPreference(
        prefs::kNetworkPredictionWifiOnly, &prefChangeRegistrar_);
    if (enabled_ && wifiOnly_) {
      connectionTypeObserverBridge_ =
          std::make_unique<ConnectionTypeObserverBridge>(self);
    }

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didReceiveMemoryWarning)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
  }
  return self;
}

- (void)browserStateDestroyed {
  [self cancelPrerender];
  connectionTypeObserverBridge_.reset();
}

- (void)dealloc {
  UMA_HISTOGRAM_COUNTS_1M(kPrerendersPerSessionCountHistogramName,
                          successfulPrerendersPerSessionCount_);
  [self cancelPrerender];
}

- (void)prerenderURL:(const GURL&)url
            referrer:(const web::Referrer&)referrer
          transition:(ui::PageTransition)transition
         immediately:(BOOL)immediately {
  // TODO(crbug.com/754050): If shouldPrerenderURL returns false, should we
  // cancel any scheduled prerender requests?
  if (![self isPrerenderingEnabled] || ![self shouldPreloadURL:url])
    return;

  // Ignore this request if there is already a scheduled request for the same
  // URL; or, if there is no scheduled request, but the currently prerendered
  // page matches this URL.
  if (url == scheduledURL_ ||
      (scheduledURL_.is_empty() && url == prerenderedURL_)) {
    return;
  }

  [self removeScheduledPrerenderRequests];
  scheduledURL_ = url;
  scheduledTransition_ = transition;
  scheduledReferrer_ = referrer;

  NSTimeInterval delay = immediately ? 0.0 : kPrerenderDelay;
  [self performSelector:@selector(startPrerender)
             withObject:nil
             afterDelay:delay];
}

- (void)cancelPrerender {
  [self cancelPrerenderForReason:PRERENDER_FINAL_STATUS_CANCELLED];
}

- (void)cancelPrerenderForReason:(PrerenderFinalStatus)reason {
  [self removeScheduledPrerenderRequests];
  [self destroyPreviewContentsForReason:reason];
}

- (BOOL)isWebStatePrerendered:(web::WebState*)webState {
  return webState && webState_.get() == webState;
}

- (std::unique_ptr<web::WebState>)releasePrerenderContents {
  successfulPrerendersPerSessionCount_++;
  UMA_HISTOGRAM_ENUMERATION(kPrerenderFinalStatusHistogramName,
                            PRERENDER_FINAL_STATUS_USED,
                            PRERENDER_FINAL_STATUS_MAX);
  [self removeScheduledPrerenderRequests];
  prerenderedURL_ = GURL();

  if (!webState_)
    return nullptr;

  // Move the pre-rendered WebState to a local variable so that it will no
  // longer be considered as pre-rendering (otherwise tab helpers may early
  // exist when invoked).
  std::unique_ptr<web::WebState> webState = std::move(webState_);
  DCHECK(![self isWebStatePrerendered:webState.get()]);

  Tab* tab = LegacyTabHelper::GetTabForWebState(webState.get());
  [[tab webController] setNativeProvider:nil];

  webState->SetShouldSuppressDialogs(false);
  webState->RemoveObserver(webStateObserver_.get());
  webState->SetDelegate(nullptr);
  policyDeciderBridge_.reset();
  HistoryTabHelper::FromWebState(webState.get())
      ->SetDelayHistoryServiceNotification(false);

  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              browserState_)) {
    accountConsistencyService->RemoveWebStateHandler(webState.get());
  }

  if ([tab loadFinished]) {
    [[OmniboxGeolocationController sharedInstance] finishPageLoadForTab:tab
                                                            loadSuccess:YES];
  }

  return webState;
}

- (void)connectionTypeChanged:(net::NetworkChangeNotifier::ConnectionType)type {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  usingWWAN_ = net::NetworkChangeNotifier::IsConnectionCellular(type);
  if (wifiOnly_ && usingWWAN_)
    [self cancelPrerender];
}

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kNetworkPredictionEnabled ||
      preferenceName == prefs::kNetworkPredictionWifiOnly) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    // The logic is simpler if both preferences changes are handled equally.
    enabled_ =
        browserState_->GetPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled);
    wifiOnly_ = browserState_->GetPrefs()->GetBoolean(
        prefs::kNetworkPredictionWifiOnly);

    if (wifiOnly_ && enabled_) {
      if (!connectionTypeObserverBridge_.get()) {
        usingWWAN_ = net::NetworkChangeNotifier::IsConnectionCellular(
            net::NetworkChangeNotifier::GetConnectionType());
        connectionTypeObserverBridge_.reset(
            new ConnectionTypeObserverBridge(self));
      }
      if (usingWWAN_) {
        [self cancelPrerender];
      }
    } else if (enabled_) {
      connectionTypeObserverBridge_.reset();
    } else {
      [self cancelPrerender];
      connectionTypeObserverBridge_.reset();
    }
  }
}

- (void)didReceiveMemoryWarning {
  [self cancelPrerenderForReason:PRERENDER_FINAL_STATUS_MEMORY_LIMIT_EXCEEDED];
}

#pragma mark -
#pragma mark CRWNativeContentProvider implementation

- (BOOL)hasControllerForURL:(const GURL&)url {
  if (!webState_)
    return NO;

  return [delegate_ preloadHasNativeControllerForURL:url];
}

// Override the CRWNativeContentProvider methods to cancel any prerenders that
// require native content.
- (id<CRWNativeContent>)controllerForURL:(const GURL&)url
                                webState:(web::WebState*)webState {
  [self schedulePrerenderCancel];
  return nil;
}

- (CGFloat)nativeContentHeaderHeightForWebState:(web::WebState*)webState {
  return [delegate_ nativeContentHeaderHeightForPreloadController:self
                                                         webState:webState];
}

#pragma mark -
#pragma mark Private Methods

- (BOOL)isPrerenderingEnabled {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return !IsPrerenderTabEvictionExperimentalGroup() && enabled_ &&
         !ios::device_util::IsSingleCoreDevice() &&
         ios::device_util::RamIsAtLeast512Mb() && (!wifiOnly_ || !usingWWAN_);
}

- (BOOL)shouldPreloadURL:(const GURL&)url {
  return url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme);
}

- (void)startPrerender {
  // Destroy any existing prerenders before starting a new one.
  [self destroyPreviewContents];
  prerenderedURL_ = scheduledURL_;
  scheduledURL_ = GURL();

  DCHECK(prerenderedURL_.is_valid());
  if (!prerenderedURL_.is_valid()) {
    [self destroyPreviewContents];
    return;
  }

  web::WebState::CreateParams createParams(browserState_);
  webState_ = web::WebState::Create(createParams);
  // Add the preload controller as a policyDecider before other tab helpers, so
  // that it can block the navigation if needed before other policy deciders
  // execute thier side effects (eg. AppLauncherTabHelper launching app).
  policyDeciderBridge_ =
      std::make_unique<web::WebStatePolicyDeciderBridge>(webState_.get(), self);
  AttachTabHelpers(webState_.get(), /*for_prerender=*/true);

  Tab* tab = LegacyTabHelper::GetTabForWebState(webState_.get());
  DCHECK(tab);

  [[tab webController] setNativeProvider:self];

  webState_->SetDelegate(webStateDelegate_.get());
  webState_->AddObserver(webStateObserver_.get());
  webState_->SetShouldSuppressDialogs(true);
  webState_->SetWebUsageEnabled(true);

  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              browserState_)) {
    accountConsistencyService->SetWebStateHandler(webState_.get(), self);
  }

  HistoryTabHelper::FromWebState(webState_.get())
      ->SetDelayHistoryServiceNotification(true);

  web::NavigationManager::WebLoadParams loadParams(prerenderedURL_);
  loadParams.referrer = scheduledReferrer_;
  loadParams.transition_type = scheduledTransition_;
  if ([delegate_ preloadShouldUseDesktopUserAgent]) {
    loadParams.user_agent_override_option =
        web::NavigationManager::UserAgentOverrideOption::DESKTOP;
  }
  webState_->GetNavigationManager()->LoadURLWithParams(loadParams);

  // LoadIfNecessary is needed because the view is not created (but needed) when
  // loading the page. TODO(crbug.com/705819): Remove this call.
  webState_->GetNavigationManager()->LoadIfNecessary();
}

- (void)destroyPreviewContents {
  [self destroyPreviewContentsForReason:PRERENDER_FINAL_STATUS_CANCELLED];
}

- (void)destroyPreviewContentsForReason:(PrerenderFinalStatus)reason {
  if (!webState_)
    return;

  UMA_HISTOGRAM_ENUMERATION(kPrerenderFinalStatusHistogramName, reason,
                            PRERENDER_FINAL_STATUS_MAX);

  Tab* tab = LegacyTabHelper::GetTabForWebState(webState_.get());
  [[tab webController] setNativeProvider:nil];
  webState_->RemoveObserver(webStateObserver_.get());
  webState_->SetDelegate(nullptr);
  webState_.reset();

  prerenderedURL_ = GURL();
}

- (void)schedulePrerenderCancel {
  // TODO(crbug.com/228550): Instead of cancelling the prerender, should we mark
  // it as failed instead?  That way, subsequent prerender requests for the same
  // URL will not kick off new prerenders.
  [self removeScheduledPrerenderRequests];
  [self performSelector:@selector(cancelPrerender) withObject:nil afterDelay:0];
}

- (void)removeScheduledPrerenderRequests {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  scheduledURL_ = GURL();
}

#pragma mark - CRWWebStateDelegate

- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler {
  DCHECK([self isWebStatePrerendered:webState]);
  [self schedulePrerenderCancel];
  if (handler) {
    handler(nil, nil);
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState_.get());
  [tab notifyTabOfUrlMayStartLoading:navigation->GetUrl()];
}

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  DCHECK_EQ(webState, webState_.get());
  // Cancel prerendering if response is "application/octet-stream". It can be a
  // video file which should not be played from preload tab. See issue at
  // http://crbug.com/436813 for more details.
  const std::string& mimeType = webState->GetContentsMimeType();
  if (mimeType == "application/octet-stream")
    [self schedulePrerenderCancel];
}

- (void)webStateDidSuppressDialog:(web::WebState*)webState {
  DCHECK_EQ(webState, webState_.get());
  [self schedulePrerenderCancel];
}

#pragma mark - ManageAccountsDelegate

- (void)onManageAccounts {
  [self schedulePrerenderCancel];
}

- (void)onAddAccount {
  [self schedulePrerenderCancel];
}

- (void)onGoIncognito:(const GURL&)url {
  [self schedulePrerenderCancel];
}

#pragma mark - CRWWebStatePolicyDecider

- (BOOL)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(const WebStatePolicyDecider::RequestInfo&)info {
  GURL requestURL = net::GURLWithNSURL(request.URL);
  // Don't allow preloading for requests that are handled by opening another
  // application or by presenting a native UI.
  if (AppLauncherTabHelper::IsAppUrl(requestURL) ||
      ITunesUrlsHandlerTabHelper::CanHandleUrl(requestURL)) {
    [self schedulePrerenderCancel];
    return NO;
  }
  return YES;
}
@end
