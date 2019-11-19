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
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/history/history_tab_helper.h"
#import "ios/chrome/browser/itunes_urls/itunes_urls_handler_tab_helper.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prerender/preload_controller_delegate.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/web/public/deprecated/crw_native_content.h"
#import "ios/web/public/deprecated/crw_native_content_holder.h"
#import "ios/web/public/deprecated/crw_web_controller_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/ui/java_script_dialog_presenter.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::WebStatePolicyDecider;

// Protocol used to cancel a scheduled preload request.
@protocol PreloadCancelling <NSObject>

// Schedules the current prerender to be cancelled during the next run of the
// event loop.
- (void)schedulePrerenderCancel;

@end

namespace {

// PrerenderFinalStatus values are used in the "Prerender.FinalStatus" histogram
// and new values needs to be kept in sync with histogram.xml.
enum PrerenderFinalStatus {
  PRERENDER_FINAL_STATUS_USED = 0,
  PRERENDER_FINAL_STATUS_MEMORY_LIMIT_EXCEEDED = 12,
  PRERENDER_FINAL_STATUS_CANCELLED = 32,
  PRERENDER_FINAL_STATUS_MAX = 52,
};

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
// The name of the histogram for recording time until a successful prerender.
const char kPrerenderStartToReleaseContentsTime[] =
    "Prerender.PrerenderStartToReleaseContentsTime";

// Is this install selected for this particular experiment.
bool IsPrerenderTabEvictionExperimentalGroup() {
  base::FieldTrial* trial =
      base::FieldTrialList::Find(kTabEvictionFieldTrialName);
  return trial && trial->group_name() == kPrerenderTabEvictionTrialGroup;
}

// Returns whether |url| can be prerendered.
bool CanPrerenderURL(const GURL& url) {
  // Prerendering is only enabled for http and https URLs.
  return url.is_valid() &&
         (url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme));
}

// Object used to schedule prerenders.
class PrerenderRequest {
 public:
  PrerenderRequest() {}
  PrerenderRequest(const GURL& url,
                   ui::PageTransition transition,
                   const web::Referrer& referrer)
      : url_(url), transition_(transition), referrer_(referrer) {}

  const GURL& url() const { return url_; }
  ui::PageTransition transition() const { return transition_; }
  const web::Referrer referrer() const { return referrer_; }

 private:
  const GURL url_;
  const ui::PageTransition transition_ = ui::PAGE_TRANSITION_LINK;
  const web::Referrer referrer_;
};

// A no-op JavaScriptDialogPresenter that cancels prerendering when the
// prerendered page attempts to show dialogs.
class PreloadJavaScriptDialogPresenter : public web::JavaScriptDialogPresenter {
 public:
  explicit PreloadJavaScriptDialogPresenter(
      id<PreloadCancelling> cancel_handler)
      : cancel_handler_(cancel_handler) {
    DCHECK(cancel_handler_);
  }

  // web::JavaScriptDialogPresenter:
  void RunJavaScriptDialog(web::WebState* web_state,
                           const GURL& origin_url,
                           web::JavaScriptDialogType dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           web::DialogClosedCallback callback) override {
    std::move(callback).Run(NO, nil);
    [cancel_handler_ schedulePrerenderCancel];
  }

  void CancelDialogs(web::WebState* web_state) override {}

 private:
  __weak id<PreloadCancelling> cancel_handler_ = nil;
};
}  // namespace

@interface PreloadController () <CRConnectionTypeObserverBridge,
                                 CRWWebStateDelegate,
                                 CRWWebStateObserver,
                                 CRWWebStatePolicyDecider,
                                 ManageAccountsDelegate,
                                 PrefObserverDelegate,
                                 PreloadCancelling> {
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegate;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<PrefObserverBridge> _observerBridge;
  std::unique_ptr<ConnectionTypeObserverBridge> _connectionTypeObserver;
  std::unique_ptr<web::WebStatePolicyDeciderBridge> _policyDeciderBridge;

  // The WebState used for prerendering.
  std::unique_ptr<web::WebState> _webState;

  // The scheduled request.
  std::unique_ptr<PrerenderRequest> _scheduledRequest;

  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // The dialog presenter.
  std::unique_ptr<web::JavaScriptDialogPresenter> _dialogPresenter;
}

// The ChromeBrowserState passed on initialization.
@property(nonatomic) ios::ChromeBrowserState* browserState;

// Redefine property as readwrite.  The URL that is prerendered in |_webState|.
// This can be different from the value returned by WebState last committed
// navigation item, for example in cases where there was a redirect.
//
// When choosing whether or not to use a prerendered Tab,
// BrowserViewController compares the URL being loaded by the omnibox with the
// URL of the prerendered Tab.  Comparing against the Tab's currently URL
// could return false negatives in cases of redirect, hence the need to store
// the originally prerendered URL.
@property(nonatomic, readwrite, assign) GURL prerenderedURL;

// The URL in the currently scheduled prerender request, or an empty one if
// there is no prerender scheduled.
@property(nonatomic, readonly) const GURL& scheduledURL;

// Whether or not the preference is enabled.
@property(nonatomic, getter=isPreferenceEnabled) BOOL preferenceEnabled;

// Whether or not prerendering is only when on wifi.
@property(nonatomic, getter=isWifiOnly) BOOL wifiOnly;

// Whether or not the current connection is using WWAN.
@property(nonatomic, getter=isUsingWWAN) BOOL usingWWAN;

// Number of successful prerenders (i.e. the user viewed the prerendered page)
// during the lifetime of this controller.
@property(nonatomic) NSUInteger successfulPrerendersPerSessionCount;

// Tracks the time of the last attempt to load a prerender URL. Used for UMA
// reporting of load durations.
@property(nonatomic) base::TimeTicks startTime;

// Called to start any scheduled prerendering requests.
- (void)startPrerender;

// Destroys the preview Tab and resets |prerenderURL_| to the empty URL.
- (void)destroyPreviewContents;

// Removes any scheduled prerender requests and resets |scheduledURL| to the
// empty URL.
- (void)removeScheduledPrerenderRequests;

// Records metric on a successful prerender.
- (void)recordReleaseMetrics;

@end

@implementation PreloadController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if ((self = [super init])) {
    _browserState = browserState;
    _preferenceEnabled =
        _browserState->GetPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled);
    _wifiOnly = _browserState->GetPrefs()->GetBoolean(
        prefs::kNetworkPredictionWifiOnly);
    _usingWWAN = net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType());
    _webStateDelegate = std::make_unique<web::WebStateDelegateBridge>(self);
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _observerBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(_browserState->GetPrefs());
    _observerBridge->ObserveChangesForPreference(
        prefs::kNetworkPredictionEnabled, &_prefChangeRegistrar);
    _observerBridge->ObserveChangesForPreference(
        prefs::kNetworkPredictionWifiOnly, &_prefChangeRegistrar);
    _dialogPresenter = std::make_unique<PreloadJavaScriptDialogPresenter>(self);
    if (_preferenceEnabled && _wifiOnly) {
      _connectionTypeObserver =
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

- (void)dealloc {
  UMA_HISTOGRAM_COUNTS_1M(kPrerendersPerSessionCountHistogramName,
                          self.successfulPrerendersPerSessionCount);
  [self cancelPrerender];
}

#pragma mark - Accessors

- (const GURL&)scheduledURL {
  return _scheduledRequest ? _scheduledRequest->url() : GURL::EmptyGURL();
}

- (BOOL)isEnabled {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return !IsPrerenderTabEvictionExperimentalGroup() && self.preferenceEnabled &&
         !ios::device_util::IsSingleCoreDevice() &&
         ios::device_util::RamIsAtLeast512Mb() &&
         !net::NetworkChangeNotifier::IsOffline() &&
         (!self.wifiOnly || !self.usingWWAN);
}

#pragma mark - Public

- (void)browserStateDestroyed {
  [self cancelPrerender];
  _connectionTypeObserver.reset();
}

- (void)prerenderURL:(const GURL&)url
            referrer:(const web::Referrer&)referrer
          transition:(ui::PageTransition)transition
         immediately:(BOOL)immediately {
  // TODO(crbug.com/754050): If CanPrerenderURL() returns false, should we
  // cancel any scheduled prerender requests?
  if (!self.enabled || !CanPrerenderURL(url))
    return;

  // Ignore this request if there is already a scheduled request for the same
  // URL; or, if there is no scheduled request, but the currently prerendered
  // page matches this URL.
  if (url == self.scheduledURL ||
      (self.scheduledURL.is_empty() && url == self.prerenderedURL)) {
    return;
  }

  [self removeScheduledPrerenderRequests];
  _scheduledRequest =
      std::make_unique<PrerenderRequest>(url, transition, referrer);

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
  return webState && _webState.get() == webState;
}

- (std::unique_ptr<web::WebState>)releasePrerenderContents {
  if (!_webState ||
      _webState->GetNavigationManager()->IsRestoreSessionInProgress())
    return nullptr;

  self.successfulPrerendersPerSessionCount++;
  [self recordReleaseMetrics];
  [self removeScheduledPrerenderRequests];
  self.prerenderedURL = GURL();
  self.startTime = base::TimeTicks();

  // Move the pre-rendered WebState to a local variable so that it will no
  // longer be considered as pre-rendering (otherwise tab helpers may early
  // exist when invoked).
  std::unique_ptr<web::WebState> webState = std::move(_webState);
  DCHECK(![self isWebStatePrerendered:webState.get()]);

  web_deprecated::SetNativeProvider(webState.get(), nil);
  webState->RemoveObserver(_webStateObserver.get());
  breakpad::StopMonitoringURLsForWebState(webState.get());
  webState->SetDelegate(nullptr);
  _policyDeciderBridge.reset();
  HistoryTabHelper::FromWebState(webState.get())
      ->SetDelayHistoryServiceNotification(false);

  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              self.browserState)) {
    accountConsistencyService->RemoveWebStateHandler(webState.get());
  }

  if (!webState->IsLoading()) {
    [[OmniboxGeolocationController sharedInstance]
        finishPageLoadForWebState:webState.get()
                      loadSuccess:YES];
  }

  return webState;
}

#pragma mark - CRConnectionTypeObserverBridge

- (void)connectionTypeChanged:(net::NetworkChangeNotifier::ConnectionType)type {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  self.usingWWAN = net::NetworkChangeNotifier::IsConnectionCellular(type);
  if (self.wifiOnly && self.usingWWAN)
    [self cancelPrerender];
}

#pragma mark - CRWWebStateDelegate

- (web::WebState*)webState:(web::WebState*)webState
    createNewWebStateForURL:(const GURL&)URL
                  openerURL:(const GURL&)openerURL
            initiatedByUser:(BOOL)initiatedByUser {
  DCHECK([self isWebStatePrerendered:webState]);
  [self schedulePrerenderCancel];
  return nil;
}

- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState {
  DCHECK([self isWebStatePrerendered:webState]);
  return _dialogPresenter.get();
}

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
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(webState, _webState.get());
  if ([self shouldCancelPreloadForMimeType:webState->GetContentsMimeType()])
    [self schedulePrerenderCancel];
}

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  DCHECK_EQ(webState, _webState.get());
  // The load should have been cancelled when the navigation finishes, but this
  // makes sure that we didn't miss one.
  if ([self shouldCancelPreloadForMimeType:webState->GetContentsMimeType()])
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

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kNetworkPredictionEnabled ||
      preferenceName == prefs::kNetworkPredictionWifiOnly) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    // The logic is simpler if both preferences changes are handled equally.
    self.preferenceEnabled = self.browserState->GetPrefs()->GetBoolean(
        prefs::kNetworkPredictionEnabled);
    self.wifiOnly = self.browserState->GetPrefs()->GetBoolean(
        prefs::kNetworkPredictionWifiOnly);

    if (self.wifiOnly && self.preferenceEnabled) {
      if (!_connectionTypeObserver.get()) {
        self.usingWWAN = net::NetworkChangeNotifier::IsConnectionCellular(
            net::NetworkChangeNotifier::GetConnectionType());
        _connectionTypeObserver.reset(new ConnectionTypeObserverBridge(self));
      }
      if (self.usingWWAN) {
        [self cancelPrerender];
      }
    } else if (self.preferenceEnabled) {
      _connectionTypeObserver.reset();
    } else {
      [self cancelPrerender];
      _connectionTypeObserver.reset();
    }
  }
}

#pragma mark - PreloadCancelling

- (void)schedulePrerenderCancel {
  // TODO(crbug.com/228550): Instead of cancelling the prerender, should we mark
  // it as failed instead?  That way, subsequent prerender requests for the same
  // URL will not kick off new prerenders.
  [self removeScheduledPrerenderRequests];
  [self performSelector:@selector(cancelPrerender) withObject:nil afterDelay:0];
}

#pragma mark - Cancellation Helpers

- (BOOL)shouldCancelPreloadForMimeType:(std::string)mimeType {
  // Cancel prerendering if response is "application/octet-stream". It can be a
  // video file which should not be played from preload tab. See issue at
  // http://crbug.com/436813 for more details.
  // On iOS 13, PDF are getting focused when loaded, preventing the user from
  // typing in the omnibox. See crbug.com/1017352.
  return mimeType == "application/octet-stream" ||
         mimeType == "application/pdf";
}

- (void)removeScheduledPrerenderRequests {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  _scheduledRequest = nullptr;
}

#pragma mark - Prerender Helpers

- (void)startPrerender {
  // Destroy any existing prerenders before starting a new one.
  [self destroyPreviewContents];
  self.prerenderedURL = self.scheduledURL;
  std::unique_ptr<PrerenderRequest> request = std::move(_scheduledRequest);

  web::WebState* webStateToReplace = [self.delegate webStateToReplace];
  if (!self.prerenderedURL.is_valid() || !webStateToReplace) {
    [self destroyPreviewContents];
    return;
  }

  web::WebState::CreateParams createParams(self.browserState);
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    _webState = web::WebState::CreateWithStorageSession(
        createParams, webStateToReplace->BuildSessionStorage());
  } else {
    _webState = web::WebState::Create(createParams);
  }

  // Add the preload controller as a policyDecider before other tab helpers, so
  // that it can block the navigation if needed before other policy deciders
  // execute thier side effects (eg. AppLauncherTabHelper launching app).
  _policyDeciderBridge =
      std::make_unique<web::WebStatePolicyDeciderBridge>(_webState.get(), self);
  AttachTabHelpers(_webState.get(), /*for_prerender=*/true);

  web_deprecated::SetNativeProvider(_webState.get(), nil);

  _webState->SetDelegate(_webStateDelegate.get());
  _webState->AddObserver(_webStateObserver.get());
  breakpad::MonitorURLsForWebState(_webState.get());
  _webState->SetWebUsageEnabled(true);

  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              self.browserState)) {
    accountConsistencyService->SetWebStateHandler(_webState.get(), self);
  }

  HistoryTabHelper::FromWebState(_webState.get())
      ->SetDelayHistoryServiceNotification(true);

  web::NavigationManager::WebLoadParams loadParams(self.prerenderedURL);
  loadParams.referrer = request->referrer();
  loadParams.transition_type = request->transition();
  if ([self.delegate preloadShouldUseDesktopUserAgent]) {
    loadParams.user_agent_override_option =
        web::NavigationManager::UserAgentOverrideOption::DESKTOP;
  }
  _webState->SetKeepRenderProcessAlive(true);
  _webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  // LoadIfNecessary is needed because the view is not created (but needed) when
  // loading the page. TODO(crbug.com/705819): Remove this call.
  _webState->GetNavigationManager()->LoadIfNecessary();

  self.startTime = base::TimeTicks::Now();
}

#pragma mark - Teardown Helpers

- (void)destroyPreviewContents {
  [self destroyPreviewContentsForReason:PRERENDER_FINAL_STATUS_CANCELLED];
}

- (void)destroyPreviewContentsForReason:(PrerenderFinalStatus)reason {
  if (!_webState)
    return;

  UMA_HISTOGRAM_ENUMERATION(kPrerenderFinalStatusHistogramName, reason,
                            PRERENDER_FINAL_STATUS_MAX);

  web_deprecated::SetNativeProvider(_webState.get(), nil);
  _webState->RemoveObserver(_webStateObserver.get());
  breakpad::StopMonitoringURLsForWebState(_webState.get());
  _webState->SetDelegate(nullptr);
  _webState.reset();

  self.prerenderedURL = GURL();
  self.startTime = base::TimeTicks();
}

#pragma mark - Notification Helpers

- (void)didReceiveMemoryWarning {
  [self cancelPrerenderForReason:PRERENDER_FINAL_STATUS_MEMORY_LIMIT_EXCEEDED];
}

#pragma mark - Metrics Helpers

- (void)recordReleaseMetrics {
  UMA_HISTOGRAM_ENUMERATION(kPrerenderFinalStatusHistogramName,
                            PRERENDER_FINAL_STATUS_USED,
                            PRERENDER_FINAL_STATUS_MAX);

  DCHECK_NE(base::TimeTicks(), self.startTime);
  UMA_HISTOGRAM_TIMES(kPrerenderStartToReleaseContentsTime,
                      base::TimeTicks::Now() - self.startTime);
}

@end
