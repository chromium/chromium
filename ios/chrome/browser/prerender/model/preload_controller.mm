// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/preload_controller.h"

#import <UIKit/UIKit.h>

#import "base/check_op.h"
#import "base/ios/device_util.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/browser/history/model/history_tab_helper.h"
#import "ios/chrome/browser/itunes_urls/model/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/prerender/model/preload_controller_delegate.h"
#import "ios/chrome/browser/prerender/model/prerender_pref.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/ui/java_script_dialog_presenter.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/page_transition_types.h"

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
// The name of the histogram for recording time until a successful prerender.
const char kPrerenderPrerenderTimeSaved[] = "Prerender.PrerenderTimeSaved";
// Histogram to record that the load was complete when the prerender was used.
// Not recorded if the pre-render isn't used.
const char kPrerenderLoadComplete[] = "Prerender.PrerenderLoadComplete";

// Is this install selected for this particular experiment.
bool IsPrerenderTabEvictionExperimentalGroup() {
  base::FieldTrial* trial =
      base::FieldTrialList::Find(kTabEvictionFieldTrialName);
  return trial && trial->group_name() == kPrerenderTabEvictionTrialGroup;
}

// Returns true if the primary account is subject to parental controls and the
// URL filtering control has been enabled.
bool IsSubjectToParentalControls(ProfileIOS* profile) {
  return profile && supervised_user::IsSubjectToParentalControls(profile);
}

// Returns whether `url` can be prerendered.
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
  void RunJavaScriptAlertDialog(web::WebState* web_state,
                                const GURL& origin_url,
                                NSString* message_text,
                                base::OnceClosure callback) override {
    std::move(callback).Run();
    [cancel_handler_ schedulePrerenderCancel];
  }

  void RunJavaScriptConfirmDialog(
      web::WebState* web_state,
      const GURL& origin_url,
      NSString* message_text,
      base::OnceCallback<void(bool success)> callback) override {
    std::move(callback).Run(/*success=*/false);
    [cancel_handler_ schedulePrerenderCancel];
  }

  void RunJavaScriptPromptDialog(
      web::WebState* web_state,
      const GURL& origin_url,
      NSString* message_text,
      NSString* default_prompt_text,
      base::OnceCallback<void(NSString* user_input)> callback) override {
    std::move(callback).Run(/*user_input=*/nil);
    [cancel_handler_ schedulePrerenderCancel];
  }

  void CancelDialogs(web::WebState* web_state) override {}

 private:
  __weak id<PreloadCancelling> cancel_handler_ = nil;
};

class PreloadManageAccountsDelegate : public ManageAccountsDelegate {
 public:
  explicit PreloadManageAccountsDelegate(id<PreloadCancelling> canceler)
      : canceler_(canceler) {}
  ~PreloadManageAccountsDelegate() override {}

  void OnRestoreGaiaCookies() override { [canceler_ schedulePrerenderCancel]; }
  void OnManageAccounts() override { [canceler_ schedulePrerenderCancel]; }
  void OnAddAccount() override { [canceler_ schedulePrerenderCancel]; }
  void OnShowConsistencyPromo(const GURL& url,
                              web::WebState* webState) override {
    [canceler_ schedulePrerenderCancel];
  }
  void OnGoIncognito(const GURL& url) override {
    [canceler_ schedulePrerenderCancel];
  }

 private:
  __weak id<PreloadCancelling> canceler_;
};

}  // namespace

@interface PreloadController () <CRConnectionTypeObserverBridge,
                                 CRWWebStateDelegate,
                                 CRWWebStateObserver,
                                 CRWWebStatePolicyDecider,
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

  // A weak pointer to the webState that will be replaced with the prerendered
  // one. This is needed by `startPrerender` to build the new webstate with the
  // same sessions.
  base::WeakPtr<web::WebState> _webStateToReplace;

  std::unique_ptr<PreloadManageAccountsDelegate> _manageAccountsDelegate;
}

// The ProfileIOS passed on initialization.
@property(nonatomic) ProfileIOS* profile;

// Redefine property as readwrite.  The URL that is prerendered in `_webState`.
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

// Network prediction settings.
@property(nonatomic)
    prerender_prefs::NetworkPredictionSetting networkPredictionSetting;

// Whether or not the current connection is using WWAN.
@property(nonatomic, assign) BOOL isOnCellularNetwork;

// Tracks the time of the last attempt to load a prerender URL. Used for UMA
// reporting of load durations.
@property(nonatomic) base::TimeTicks startTime;

// Whether the load was completed or not.
@property(nonatomic, assign) BOOL loadCompleted;
// The time between the start of the load and the completion (only valid if the
// load completed).
@property(nonatomic) base::TimeDelta completionTime;

// Called to start any scheduled prerendering requests.
- (void)startPrerender;

// Destroys the preview Tab and resets `prerenderURL_` to the empty URL.
- (void)destroyPreviewContents;

// Removes any scheduled prerender requests and resets `scheduledURL` to the
// empty URL.
- (void)removeScheduledPrerenderRequests;

// Records metric on a successful prerender.
- (void)recordReleaseMetrics;

@end

@implementation PreloadController

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  DCHECK(profile);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if ((self = [super init])) {
    _profile = profile;
    _networkPredictionSetting =
        static_cast<prerender_prefs::NetworkPredictionSetting>(
            _profile->GetPrefs()->GetInteger(prefs::kNetworkPredictionSetting));
    _isOnCellularNetwork = net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType());
    _webStateDelegate = std::make_unique<web::WebStateDelegateBridge>(self);
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _observerBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(_profile->GetPrefs());
    _observerBridge->ObserveChangesForPreference(
        prefs::kNetworkPredictionSetting, &_prefChangeRegistrar);
    _dialogPresenter = std::make_unique<PreloadJavaScriptDialogPresenter>(self);
    _manageAccountsDelegate =
        std::make_unique<PreloadManageAccountsDelegate>(self);
    if (_networkPredictionSetting ==
        prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly) {
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
  [self cancelPrerender];
}

#pragma mark - Accessors

- (const GURL&)scheduledURL {
  return _scheduledRequest ? _scheduledRequest->url() : GURL::EmptyGURL();
}

- (BOOL)isEnabled {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  if (IsPrerenderTabEvictionExperimentalGroup() ||
      ios::device_util::IsSingleCoreDevice() ||
      !ios::device_util::RamIsAtLeast512Mb() ||
      net::NetworkChangeNotifier::IsOffline() ||
      IsSubjectToParentalControls(_profile)) {
    return false;
  }

  switch (self.networkPredictionSetting) {
    case prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly: {
      return !self.isOnCellularNetwork;
    }

    case prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular: {
      return true;
    }

    case prerender_prefs::NetworkPredictionSetting::kDisabled: {
      return false;
    }
  }
}

- (void)setLoadCompleted:(BOOL)loadCompleted {
  if (_loadCompleted == loadCompleted) {
    return;
  }

  _loadCompleted = loadCompleted;

  if (loadCompleted) {
    self.completionTime = base::TimeTicks::Now() - self.startTime;
  } else {
    self.completionTime = base::TimeDelta();
  }
}

#pragma mark - Public

- (void)profileDestroyed {
  [self cancelPrerender];
  _connectionTypeObserver.reset();
}

- (void)prerenderURL:(const GURL&)url
            referrer:(const web::Referrer&)referrer
          transition:(ui::PageTransition)transition
     currentWebState:(web::WebState*)currentWebState
         immediately:(BOOL)immediately {
  // TODO(crbug.com/40534385): If CanPrerenderURL() returns false, should we
  // cancel any scheduled prerender requests?
  if (!self.enabled || !CanPrerenderURL(url)) {
    return;
  }

  // Ignore this request if there is already a scheduled request for the same
  // URL; or, if there is no scheduled request, but the currently prerendered
  // page matches this URL.
  if (url == self.scheduledURL ||
      (self.scheduledURL.is_empty() && url == self.prerenderedURL)) {
    return;
  }

  [self removeScheduledPrerenderRequests];
  _webStateToReplace = currentWebState->GetWeakPtr();
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
  if (!_webState) {
    return nullptr;
  }

  [self recordReleaseMetrics];
  [self removeScheduledPrerenderRequests];

  // Use the helper function to properly release the web::WebState.
  std::unique_ptr<web::WebState> webState =
      [self releasePrerenderContentsInternal];

  // The WebState will be converted to a proper tab. Record navigations that
  // happened during pre-rendering to the HistoryService.
  HistoryTabHelper::FromWebState(webState.get())
      ->SetDelayHistoryServiceNotification(false);

  return webState;
}

#pragma mark - Internal

// Helper function that return ownership of the internal web::WebState instance
// disconnecting the observers attached to it for preloading, ... Needs to be
// called before destroying the WebState or before converting it to a tab.
- (std::unique_ptr<web::WebState>)releasePrerenderContentsInternal {
  DCHECK(_webState);

  self.prerenderedURL = GURL();
  self.startTime = base::TimeTicks();
  self.loadCompleted = NO;

  // Move the pre-rendered WebState to a local variable so that it will no
  // longer be considered as pre-rendering (otherwise tab helpers may early
  // exist when invoked).
  std::unique_ptr<web::WebState> webState = std::move(_webState);
  DCHECK(![self isWebStatePrerendered:webState.get()]);

  webState->RemoveObserver(_webStateObserver.get());
  crash_report_helper::StopMonitoringURLsForPreloadWebState(webState.get());
  webState->SetDelegate(nullptr);
  _policyDeciderBridge.reset();

  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForProfile(self.profile)) {
    accountConsistencyService->RemoveWebStateHandler(webState.get());
  }

  return webState;
}

#pragma mark - CRConnectionTypeObserverBridge

- (void)connectionTypeChanged:(net::NetworkChangeNotifier::ConnectionType)type {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  self.isOnCellularNetwork =
      net::NetworkChangeNotifier::IsConnectionCellular(type);
  if (self.networkPredictionSetting ==
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly &&
      self.isOnCellularNetwork) {
    [self cancelPrerender];
  }
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

- (UIView*)webViewContainerForWebState:(web::WebState*)webState {
  return [self.delegate webViewContainer];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(webState, _webState.get());
  if ([self shouldCancelPreloadForMimeType:webState->GetContentsMimeType()]) {
    [self schedulePrerenderCancel];
  }
}

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  DCHECK_EQ(webState, _webState.get());
  // The load should have been cancelled when the navigation finishes, but this
  // makes sure that we didn't miss one.
  if ([self shouldCancelPreloadForMimeType:webState->GetContentsMimeType()]) {
    [self schedulePrerenderCancel];
  } else if (loadSuccess) {
    self.loadCompleted = YES;
  }
}

#pragma mark - CRWWebStatePolicyDecider

- (void)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(WebStatePolicyDecider::RequestInfo)requestInfo
           decisionHandler:(PolicyDecisionHandler)decisionHandler {
  GURL requestURL = net::GURLWithNSURL(request.URL);
  // Don't allow preloading for requests that are handled by opening another
  // application or by presenting a native UI.
  if (AppLauncherTabHelper::IsAppUrl(requestURL) ||
      ITunesUrlsHandlerTabHelper::CanHandleUrl(requestURL)) {
    [self schedulePrerenderCancel];
    decisionHandler(WebStatePolicyDecider::PolicyDecision::Cancel());
    return;
  }
  decisionHandler(WebStatePolicyDecider::PolicyDecision::Allow());
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kNetworkPredictionSetting) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    // The logic is simpler if both preferences changes are handled equally.
    self.networkPredictionSetting =
        static_cast<prerender_prefs::NetworkPredictionSetting>(
            self.profile->GetPrefs()->GetInteger(
                prefs::kNetworkPredictionSetting));

    switch (self.networkPredictionSetting) {
      case prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly: {
        if (!_connectionTypeObserver.get()) {
          self.isOnCellularNetwork =
              net::NetworkChangeNotifier::IsConnectionCellular(
                  net::NetworkChangeNotifier::GetConnectionType());
          _connectionTypeObserver =
              std::make_unique<ConnectionTypeObserverBridge>(self);
        }
        if (self.isOnCellularNetwork) {
          [self cancelPrerender];
        }
        break;
      }

      case prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular: {
        _connectionTypeObserver.reset();
        break;
      }

      case prerender_prefs::NetworkPredictionSetting::kDisabled: {
        [self cancelPrerender];
        _connectionTypeObserver.reset();
        break;
      }
    }
  }
}

#pragma mark - PreloadCancelling

- (void)schedulePrerenderCancel {
  // TODO(crbug.com/41005198): Instead of cancelling the prerender, should we
  // mark it as failed instead?  That way, subsequent prerender requests for the
  // same URL will not kick off new prerenders.
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
  return mimeType == kBinaryDataMimeType || mimeType == "application/pdf";
}

- (void)removeScheduledPrerenderRequests {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  _scheduledRequest = nullptr;
  _webStateToReplace.reset();
}

#pragma mark - Prerender Helpers

- (void)startPrerender {
  // Destroy any existing prerenders before starting a new one.
  [self destroyPreviewContents];
  self.prerenderedURL = self.scheduledURL;
  std::unique_ptr<PrerenderRequest> request = std::move(_scheduledRequest);

  web::WebState* webStateToReplace = _webStateToReplace.get();
  _webStateToReplace.reset();

  // TODO(crbug.com/40726702): The correct way is to always get the
  // webStateToReplace from the delegate. however this is not possible because
  // there is only one delegate per profile.
  if (!webStateToReplace) {
    webStateToReplace = [self.delegate webStateToReplace];
  }

  if (!self.prerenderedURL.is_valid() || !webStateToReplace) {
    [self destroyPreviewContents];
    return;
  }

  // To avoid losing the navigation history when the user navigates to the
  // pre-rendered tab, clone the tab that will be replaced, and start the
  // pre-rendered navigation in the new tab.
  _webState = webStateToReplace->Clone();

  // Add the preload controller as a policyDecider before other tab helpers, so
  // that it can block the navigation if needed before other policy deciders
  // execute thier side effects (eg. AppLauncherTabHelper launching app).
  _policyDeciderBridge =
      std::make_unique<web::WebStatePolicyDeciderBridge>(_webState.get(), self);
  AttachTabHelpers(_webState.get(), TabHelperFilter::kPrerender);

  _webState->SetDelegate(_webStateDelegate.get());
  _webState->AddObserver(_webStateObserver.get());
  crash_report_helper::MonitorURLsForPreloadWebState(_webState.get());
  _webState->SetWebUsageEnabled(true);

  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForProfile(self.profile)) {
    accountConsistencyService->SetWebStateHandler(
        _webState.get(), _manageAccountsDelegate.get());
  }

  HistoryTabHelper::FromWebState(_webState.get())
      ->SetDelayHistoryServiceNotification(true);

  web::NavigationManager::WebLoadParams loadParams(self.prerenderedURL);
  loadParams.referrer = request->referrer();
  loadParams.transition_type = request->transition();
  _webState->SetKeepRenderProcessAlive(true);
  _webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  // LoadIfNecessary is needed because the view is not created (but needed) when
  // loading the page. TODO(crbug.com/41309809): Remove this call.
  _webState->GetNavigationManager()->LoadIfNecessary();

  self.startTime = base::TimeTicks::Now();
  self.loadCompleted = NO;
}

#pragma mark - Teardown Helpers

- (void)destroyPreviewContents {
  [self destroyPreviewContentsForReason:PRERENDER_FINAL_STATUS_CANCELLED];
}

- (void)destroyPreviewContentsForReason:(PrerenderFinalStatus)reason {
  if (!_webState) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kPrerenderFinalStatusHistogramName, reason,
                            PRERENDER_FINAL_STATUS_MAX);

  // Dropping the std::unique_ptr<...> cause the destruction
  // of the WebState object used for pre-render.
  std::unique_ptr<web::WebState> destroyed =
      [self releasePrerenderContentsInternal];
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

  UMA_HISTOGRAM_BOOLEAN(kPrerenderLoadComplete, self.loadCompleted);

  if (self.loadCompleted) {
    DCHECK_NE(base::TimeDelta(), self.completionTime);
    UMA_HISTOGRAM_TIMES(kPrerenderPrerenderTimeSaved, self.completionTime);
  } else {
    DCHECK_NE(base::TimeTicks(), self.startTime);
    UMA_HISTOGRAM_TIMES(kPrerenderPrerenderTimeSaved,
                        base::TimeTicks::Now() - self.startTime);
  }
}

@end
