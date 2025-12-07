// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"

#import "base/auto_reset.h"
#import "base/check.h"
#import "base/check_deref.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/device_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/prefs/pref_service.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#import "components/signin/ios/browser/manage_accounts_delegate.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"
#import "ios/chrome/browser/history/model/history_tab_helper.h"
#import "ios/chrome/browser/itunes_urls/model/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/prerender/model/prerender_browser_agent_delegate.h"
#import "ios/chrome/browser/prerender/model/prerender_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/web/model/load_timing_tab_helper.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/ui/java_script_dialog_presenter.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate.h"
#import "ios/web/public/web_state_observer.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/network_change_notifier.h"

namespace {

// The name of the histogram for recording final status (e.g. used/cancelled)
// of prerender requests.
constexpr char kPrerenderFinalStatusHistogramName[] = "Prerender.FinalStatus";

// Histogram to record that the load was complete when the prerender was used.
// Not recorded if the pre-render isn't used.
constexpr char kPrerenderLoadComplete[] = "Prerender.PrerenderLoadComplete";

// The name of the histogram for recording time until a successful prerender.
constexpr char kPrerenderPrerenderTimeSaved[] = "Prerender.PrerenderTimeSaved";

// Returns whether `url` can be pre-rendered.
bool CanPrerenderUrl(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

// Returns the delay before starting pre-rendering according to `policy`
base::TimeDelta DelayForPolicy(PrerenderBrowserAgent::PrerenderPolicy policy) {
  switch (policy) {
    case PrerenderBrowserAgent::PrerenderPolicy::kNoDelay:
      return base::TimeDelta();

    case PrerenderBrowserAgent::PrerenderPolicy::kDefaultDelay:
      return base::Milliseconds(500);
  }
}

// Returns the value of the NetworkPredictionSetting in `prefs`.
prerender_prefs::NetworkPredictionSetting NetworkPredictionSettingFromPrefs(
    PrefService* prefs) {
  using prerender_prefs::NetworkPredictionSetting;
  switch (prefs->GetInteger(prefs::kNetworkPredictionSetting)) {
    case base::to_underlying(NetworkPredictionSetting::kDisabled):
      return NetworkPredictionSetting::kDisabled;
    case base::to_underlying(NetworkPredictionSetting::kEnabledWifiOnly):
      return NetworkPredictionSetting::kEnabledWifiOnly;
    case base::to_underlying(NetworkPredictionSetting::kEnabledWifiAndCellular):
      return NetworkPredictionSetting::kEnabledWifiAndCellular;
    default:
      // If the value stored in the PrefService is unexpected, return
      // NetworkPredictionSetting::kDisabled (static_cast<...> would
      // lead to undefined behaviour when comparing the enum).
      return NetworkPredictionSetting::kDisabled;
  }
}

}  // anonymous namespace

// Used to observe UIApplicationDidReceiveMemoryWarningNotification from
// the NSNotificationCenter.
@interface PrerenderBrowserAgentMemoryWarningObservation : NSObject

// Will invoked `closure` when the NSNotificationCenter sends the
// UIApplicationDidReceiveMemoryWarningNotification. The closure
// will be called on the sequence the object is created.
- (instancetype)initWithClosure:(base::RepeatingClosure)closure
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@implementation PrerenderBrowserAgentMemoryWarningObservation {
  base::RepeatingClosure _closure;
}

- (instancetype)initWithClosure:(base::RepeatingClosure)closure {
  if ((self = [super init])) {
    // Since NSNotificationCenter may call the selector on a background
    // thread use base::BindPostTask(...) to ensure that the closure is
    // always called on the correct sequence.
    _closure = base::BindPostTask(
        base::SequencedTaskRunner::GetCurrentDefault(), std::move(closure));

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didReceiveMemoryWarning)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
  }
  return self;
}

- (void)didReceiveMemoryWarning {
  _closure.Run();
}

@end

// Information about a request.
class PrerenderBrowserAgent::RequestInfos {
 public:
  RequestInfos(const GURL& url,
               const web::Referrer& referrer,
               ui::PageTransition transition)
      : url_(url), referrer_(referrer), transition_(transition) {}

  ~RequestInfos() = default;

  const GURL& url() const { return url_; }
  const web::Referrer& referrer() const { return referrer_; }
  ui::PageTransition transition() const { return transition_; }

  web::NavigationManager::WebLoadParams load_params() const {
    web::NavigationManager::WebLoadParams params(url_);
    params.referrer = referrer_;
    params.transition_type = transition_;
    return params;
  }

 private:
  const GURL url_;
  const web::Referrer referrer_;
  const ui::PageTransition transition_;
};

// Represents a request.
template <typename WebStatePtr>
class PrerenderBrowserAgent::Request {
 public:
  Request(WebStatePtr web_state_ptr, RequestInfos infos)
      : web_state_(std::move(web_state_ptr)), infos_(std::move(infos)) {}

  ~Request() = default;

  web::WebState* web_state() const { return web_state_.get(); }
  const RequestInfos& infos() const { return infos_; }

  WebStatePtr Release() { return std::move(web_state_); }

 private:
  WebStatePtr web_state_;
  RequestInfos infos_;
};

// WebStateDelegate used by PrerenderBrowserAgent.
class PrerenderBrowserAgent::Delegate final
    : public web::WebStateDelegate,
      public web::JavaScriptDialogPresenter {
 public:
  Delegate(web::WebState* web_state, PrerenderBrowserAgent* agent)
      : web_state_(CHECK_DEREF(web_state)), agent_(CHECK_DEREF(agent)) {
    web_state_->SetDelegate(this);
  }

  ~Delegate() final { web_state_->SetDelegate(nullptr); }

  // web::WebStateDelegate implementation.
  web::WebState* CreateNewWebState(web::WebState* web_state,
                                   const GURL& url,
                                   const GURL& opener_url,
                                   bool user_initiated) final {
    agent_->ScheduleCancelPrerender();
    return nullptr;
  }

  web::JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(
      web::WebState* web_state) final {
    return this;
  }

  void OnAuthRequired(web::WebState* web_state,
                      NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      AuthCallback callback) final {
    agent_->ScheduleCancelPrerender();
    std::move(callback).Run(nil, nil);
  }

  UIView* GetWebViewContainer(web::WebState* web_state) final {
    return [agent_->delegate_ webViewContainer];
  }

  // web::JavaScriptDialogPresenter implementation.
  void RunJavaScriptAlertDialog(web::WebState* web_state,
                                const url::Origin& origin,
                                NSString* message_text,
                                base::OnceClosure callback) final {
    agent_->ScheduleCancelPrerender();
    std::move(callback).Run();
  }

  void RunJavaScriptConfirmDialog(
      web::WebState* web_state,
      const url::Origin& origin,
      NSString* message_text,
      base::OnceCallback<void(bool)> callback) final {
    agent_->ScheduleCancelPrerender();
    std::move(callback).Run(/*success=*/false);
  }

  void RunJavaScriptPromptDialog(
      web::WebState* web_state,
      const url::Origin& origin,
      NSString* message_text,
      NSString* default_prompt_text,
      base::OnceCallback<void(NSString*)> callback) final {
    agent_->ScheduleCancelPrerender();
    std::move(callback).Run(/*user_input=*/nil);
  }

  void CancelDialogs(web::WebState* web_state) override {}

 private:
  const raw_ref<web::WebState> web_state_;
  const raw_ref<PrerenderBrowserAgent> agent_;
};

// WebStateObserver used by PrerenderBrowserAgent.
class PrerenderBrowserAgent::Observer final : public web::WebStateObserver {
 public:
  Observer(web::WebState* web_state, PrerenderBrowserAgent* agent)
      : web_state_(CHECK_DEREF(web_state)),
        agent_(CHECK_DEREF(agent)),
        start_time_(base::TimeTicks::Now()) {
    web_state_->AddObserver(this);
  }

  ~Observer() final { web_state_->RemoveObserver(this); }

  // Returns whether the load was successfully completed.
  bool load_completed() const { return load_completed_; }

  // Returns the time saved by pre-rendering.
  base::TimeDelta time_saved() const {
    if (load_completed_) {
      return time_saved_;
    }

    return base::TimeTicks::Now() - start_time_;
  }

  // Whether the pre-render should be cancelled based on the mime type of
  // the content.
  bool ShouldCancelPrerenderForMimeType(std::string_view mime_type) const {
    // Cancel pre-rendering if response is
    //  -  "application/octet-stream" can be a video which should not be
    //      played for pre-rendered tab (see https://crbug.com/436813)
    //
    //  - "application/pdf" as starting in iOS 13.0, PDFs get focus on load,
    //    preventing typing in the omnibox (see https://crbug.com/1017352).
    return mime_type == kBinaryDataMimeType ||
           mime_type == kAdobePortableDocumentFormatMimeType;
  }

  // web::WebStateObserver implementation.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* context) final {
    if (ShouldCancelPrerenderForMimeType(web_state_->GetContentsMimeType())) {
      agent_->ScheduleCancelPrerender();
    }
  }

  void PageLoaded(web::WebState* web_state,
                  web::PageLoadCompletionStatus status) final {
    if (ShouldCancelPrerenderForMimeType(web_state_->GetContentsMimeType())) {
      agent_->ScheduleCancelPrerender();
      return;
    }

    load_completed_ = status == web::PageLoadCompletionStatus::SUCCESS;
    time_saved_ = base::TimeTicks::Now() - start_time_;
  }

 private:
  const raw_ref<web::WebState> web_state_;
  const raw_ref<PrerenderBrowserAgent> agent_;

  const base::TimeTicks start_time_;
  base::TimeDelta time_saved_;
  bool load_completed_ = false;
};

// WebStatePolicyDecider used by PrerenderBrowserAgent.
class PrerenderBrowserAgent::PolicyDecider final
    : public web::WebStatePolicyDecider {
 public:
  PolicyDecider(web::WebState* web_state, PrerenderBrowserAgent* agent)
      : web::WebStatePolicyDecider(web_state), agent_(CHECK_DEREF(agent)) {}

  // WebStatePolicyDecided implementation.
  void ShouldAllowRequest(NSURLRequest* request,
                          RequestInfo request_info,
                          PolicyDecisionCallback callback) final {
    // Don't allow preloading for requests that are handled by opening another
    // application of by presenting a native UI.
    const GURL url = net::GURLWithNSURL(request.URL);
    if (!AppLauncherTabHelper::IsAppUrl(url) &&
        !ITunesUrlsHandlerTabHelper::CanHandleUrl(url)) {
      std::move(callback).Run(PolicyDecision::Allow());
      return;
    }

    std::move(callback).Run(PolicyDecision::Cancel());
    agent_->ScheduleCancelPrerender();
  }

 private:
  const raw_ref<PrerenderBrowserAgent> agent_;
};

// ManageAccountsDelegate used by PrerenderBrowserAgent.
class PrerenderBrowserAgent::ManageAccountsDelegate final
    : public ::ManageAccountsDelegate {
 public:
  explicit ManageAccountsDelegate(PrerenderBrowserAgent* agent)
      : agent_(CHECK_DEREF(agent)) {}

  // ManageAccountsDelegate implementation.
  void OnRestoreGaiaCookies() final { agent_->ScheduleCancelPrerender(); }
  void OnManageAccounts(const GURL& url) final {
    agent_->ScheduleCancelPrerender();
  }
  void OnAddAccount(const GURL& url, const std::string& prefilled_email) final {
    agent_->ScheduleCancelPrerender();
  }
  void OnShowConsistencyPromo(const GURL& url, web::WebState* webState) final {
    agent_->ScheduleCancelPrerender();
  }
  void OnGoIncognito(const GURL& url) final {
    agent_->ScheduleCancelPrerender();
  }

 private:
  const raw_ref<PrerenderBrowserAgent> agent_;
};

// PrerenderFinalStatus values are used in the "Prerender.FinalStatus" histogram
// and new values needs to be kept in sync with histogram.xml.
enum class PrerenderBrowserAgent::PrerenderFinalStatus {
  kUsed = 0,
  kMemoryLimitExceeded = 12,
  kCancelled = 32,
  kNotAllowed = 63,
  kMaxValue = kNotAllowed,
};

PrerenderBrowserAgent::PrerenderBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  registrar_.Init(browser_->GetProfile()->GetPrefs());
  registrar_.Add(prefs::kNetworkPredictionSetting,
                 base::BindRepeating(
                     &PrerenderBrowserAgent::OnNetworkPredictionSettingChanged,
                     weak_ptr_factory_.GetWeakPtr()));

  nsnotification_registration_ =
      [[PrerenderBrowserAgentMemoryWarningObservation alloc]
          initWithClosure:base::BindRepeating(
                              &PrerenderBrowserAgent::CancelPrerenderInternal,
                              weak_ptr_factory_.GetWeakPtr(),
                              PrerenderFinalStatus::kMemoryLimitExceeded)];
}

PrerenderBrowserAgent::~PrerenderBrowserAgent() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  CancelPrerender();
}

void PrerenderBrowserAgent::SetDelegate(
    id<PrerenderBrowserAgentDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = delegate;
}

void PrerenderBrowserAgent::StartPrerender(const GURL& url,
                                           const web::Referrer& referrer,
                                           ui::PageTransition transition,
                                           PrerenderPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If `url` is already pre-rendered, or a request to pre-render `url` is
  // already scheduled, then there is nothing to do. Return immediately.
  if (IsPrerenderdOrScheduled(url)) {
    return;
  }

  // If there is no active WebState or the url cannot be pre-rendered, then
  // cancel any prerendering and ignore the request.
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (!web_state || !CanPrerenderUrl(url) || !Enabled()) {
    CancelPrerender();
    return;
  }

  scheduled_request_ = std::make_unique<Request<base::WeakPtr<web::WebState>>>(
      web_state->GetWeakPtr(), RequestInfos(url, referrer, transition));
  timer_.Start(FROM_HERE, DelayForPolicy(policy),
               base::BindOnce(&PrerenderBrowserAgent::StartPendingRequest,
                              weak_ptr_factory_.GetWeakPtr()));
}

bool PrerenderBrowserAgent::ValidatePrerender(const GURL& url,
                                              ui::PageTransition transition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ensure that the pre-render is cancelled when this method complete.
  base::ScopedClosureRunner cancel_prerender_on_cleanup(base::BindOnce(
      &PrerenderBrowserAgent::CancelPrerender, weak_ptr_factory_.GetWeakPtr()));

  if (!prerender_request_ || prerender_request_->infos().url() != url) {
    return false;
  }

  WebStateList* web_state_list = browser_->GetWebStateList();
  const int active_index = web_state_list->active_index();
  if (active_index == WebStateList::kInvalidIndex) {
    return false;
  }

  web::WebState* active_web_state = web_state_list->GetWebStateAt(active_index);
  CHECK(active_web_state);

  std::unique_ptr<web::WebState> new_web_state =
      ReleasePrerender(PrerenderFinalStatus::kUsed);
  if (!new_web_state) {
    return false;
  }

  // Due to some security workarounds inside //ios/web, sometimes a restored
  // WebState may mark new navigations as renderer initiated instead of browser
  // initiated. As a result "visible url" of the preloaded WebState will be the
  // "last committed url" and not "url typed by the user". As there navigations
  // are uncommitted, and make the omnibox (or NTP) look stange, drop them.
  // See crbug.com/1020497 for the strange UI, and crbug.com/1010765 for the
  // triggering security fixes.
  if (active_web_state->GetVisibleURL() == new_web_state->GetVisibleURL()) {
    return false;
  }

  // Remove the PrerenderTabHelper as the WebState will become a real tab.
  PrerenderTabHelper::RemoveFromWebState(new_web_state.get());

  // The WebState will be converted to a proper tab. Record navigations that
  // happened during pre-rendering to the HistoryService.
  if (HistoryTabHelper* tab_helper =
          HistoryTabHelper::FromWebState(new_web_state.get())) {
    tab_helper->SetDelayHistoryServiceNotification(false);
  }

  {
    base::AutoReset<bool> scoped_reset(&loading_prerender_, true);
    web_state_list->ReplaceWebStateAt(active_index, std::move(new_web_state));
    active_web_state = web_state_list->GetWebStateAt(active_index);
  }

  if (PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED)) {
    LoadTimingTabHelper::FromWebState(active_web_state)
        ->DidPromotePrerenderTab();
  }

  // There is no need to call CancelPrerender(), clear the ScopedClosureRunner.
  std::ignore = cancel_prerender_on_cleanup.Release();
  return true;
}

bool PrerenderBrowserAgent::IsInsertingPrerender() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return loading_prerender_;
}

void PrerenderBrowserAgent::CancelPrerender() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelPrerenderInternal(PrerenderFinalStatus::kCancelled);
}

bool PrerenderBrowserAgent::Enabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ProfileIOS* profile = browser_->GetProfile();
  if (ios::device_util::IsSingleCoreDevice() ||
      !ios::device_util::RamIsAtLeast512Mb() ||
      net::NetworkChangeNotifier::IsOffline() ||
      supervised_user::IsSubjectToParentalControls(profile)) {
    return false;
  }

  switch (NetworkPredictionSettingFromPrefs(profile->GetPrefs())) {
    case prerender_prefs::NetworkPredictionSetting::kDisabled:
      return false;

    case prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly:
      return !net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType());

    case prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular:
      return true;
  }
}

bool PrerenderBrowserAgent::IsPrerenderdOrScheduled(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (scheduled_request_) {
    if (scheduled_request_->infos().url() == url) {
      return true;
    }
  }

  if (prerender_request_) {
    if (prerender_request_->infos().url() == url) {
      return true;
    }
  }

  return false;
}

void PrerenderBrowserAgent::StartPendingRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(scheduled_request_);
  CHECK(CanPrerenderUrl(scheduled_request_->infos().url()));
  auto request = std::exchange(scheduled_request_, nullptr);
  CancelPrerenderInternal(PrerenderFinalStatus::kCancelled);

  // If the WebState has been destroyed since the request was scheduled, there
  // is nothing to do (as we just destroyed the previous pre-rendering if any).
  web::WebState* active_web_state = request->web_state();
  if (!active_web_state) {
    return;
  }

  // To avoid losing the navigation history when the user navigates to the
  // pre-rendered tab, clone the tab that will be replaced, and start the
  // pre-rendered navigation in the new tab.
  CHECK(!prerender_request_);
  prerender_request_ =
      std::make_unique<Request<std::unique_ptr<web::WebState>>>(
          active_web_state->Clone(), request->infos());
  web::WebState* web_state = prerender_request_->web_state();

  // Create the delegate, observer and policy decider before any tab helper
  // to ensure they will be the first notified of the respective changes to
  // the WebState and can act before any tab helper can have any side-effect
  // (e.g. AppLauncherTabHelper launching an external application).
  web_state_delegate_ = std::make_unique<Delegate>(web_state, this);
  web_state_observer_ = std::make_unique<Observer>(web_state, this);
  policy_decider_ = std::make_unique<PolicyDecider>(web_state, this);

  // Create the PrerenderTabHelper before any other TabHelpers to ensure
  // they all correctly see this WebState as used for pre-rendering.
  PrerenderTabHelper::CreateForWebState(web_state, this);

  AttachTabHelpers(web_state, TabHelperFilter::kPrerender);
  crash_report_helper::MonitorURLsForPreloadWebState(web_state);

  if (AccountConsistencyService* service =
          ios::AccountConsistencyServiceFactory::GetForProfile(
              browser_->GetProfile())) {
    if (!manage_accounts_delegate_) {
      manage_accounts_delegate_ =
          std::make_unique<ManageAccountsDelegate>(this);
    }
    service->SetWebStateHandler(web_state, manage_accounts_delegate_.get());
  }

  if (HistoryTabHelper* tab_helper =
          HistoryTabHelper::FromWebState(web_state)) {
    tab_helper->SetDelayHistoryServiceNotification(true);
  }

  web_state->SetWebUsageEnabled(true);
  web_state->SetKeepRenderProcessAlive(true);

  // LoadIfNecessary() is needed because the view is not created (but needed)
  // when loading the page. TODO(crbug.com/41309809): remove this call.
  web::NavigationManager* manager = web_state->GetNavigationManager();
  manager->LoadURLWithParams(prerender_request_->infos().load_params());
  manager->LoadIfNecessary();
}

void PrerenderBrowserAgent::ScheduleCancelPrerender() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelScheduledRequest();
  timer_.Start(FROM_HERE, base::TimeDelta(),
               base::BindOnce(&PrerenderBrowserAgent::CancelPrerender,
                              weak_ptr_factory_.GetWeakPtr()));
}

void PrerenderBrowserAgent::CancelScheduledRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (scheduled_request_) {
    scheduled_request_.reset();
    timer_.Stop();
  }
}

void PrerenderBrowserAgent::CancelPrerenderInternal(
    PrerenderFinalStatus reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelScheduledRequest();
  DestroyPrerender(reason);
}

void PrerenderBrowserAgent::DestroyPrerender(PrerenderFinalStatus reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!prerender_request_) {
    return;
  }

  // Dropping the std::unique_ptr<...> causes the destruction of the WebState
  // object used for pre-render.
  std::ignore = ReleasePrerender(reason);
}

std::unique_ptr<web::WebState> PrerenderBrowserAgent::ReleasePrerender(
    PrerenderFinalStatus reason) {
  CHECK(prerender_request_);
  CHECK(web_state_observer_);

  base::UmaHistogramEnumeration(kPrerenderFinalStatusHistogramName, reason);
  if (reason == PrerenderFinalStatus::kUsed) {
    base::UmaHistogramBoolean(kPrerenderLoadComplete,
                              web_state_observer_->load_completed());
    base::UmaHistogramTimes(kPrerenderPrerenderTimeSaved,
                            web_state_observer_->time_saved());
  }

  auto request = std::exchange(prerender_request_, nullptr);
  std::unique_ptr<web::WebState> web_state = request->Release();

  crash_report_helper::StopMonitoringURLsForPreloadWebState(web_state.get());
  if (AccountConsistencyService* service =
          ios::AccountConsistencyServiceFactory::GetForProfile(
              browser_->GetProfile())) {
    service->RemoveWebStateHandler(web_state.get());
  }

  policy_decider_.reset();
  web_state_observer_.reset();
  web_state_delegate_.reset();

  return web_state;
}

void PrerenderBrowserAgent::OnNetworkPredictionSettingChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!Enabled()) {
    CancelPrerender();
  }
}

void PrerenderBrowserAgent::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!Enabled()) {
    CancelPrerender();
  }
}
