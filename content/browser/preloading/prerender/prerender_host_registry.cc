// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_host_registry.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/preloading/prerender/devtools_prerender_attempt.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/preloading/prerender/prerender_navigation_utils.h"
#include "content/browser/preloading/prerender/prerender_new_tab_handle.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {

bool IsBackground(Visibility visibility) {
  // PrerenderHostRegistry treats HIDDEN and OCCLUDED as background.
  switch (visibility) {
    case Visibility::HIDDEN:
    case Visibility::OCCLUDED:
      return true;
    case Visibility::VISIBLE:
      return false;
  }
}

// Returns true when it is allowed to activate a prerendered page in a
// background tab.
bool IsAllowedToActivateInBackgroundForTesting() {
  // Now it is allowed to activate a prerendered page in a background only on
  // macOS for running web platform tests. See comments on the flag definition
  // for more details.
#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(
          features::kPrerender2AllowActivationInBackground)) {
    return true;
  }
#endif
  return false;
}

bool DeviceHasEnoughMemoryForPrerender() {
  // This method disallows prerendering on low-end devices if the
  // kPrerender2MemoryControls feature is enabled.
  if (!base::FeatureList::IsEnabled(blink::features::kPrerender2MemoryControls))
    return true;

  // On Android, Prerender2 is only enabled for 2GB+ high memory devices.  The
  // default threshold value is set to 1700 MB to account for all 2GB devices
  // which report lower RAM due to carveouts.
  // Previously used the same default threshold as the back/forward cache. See
  // comments in DeviceHasEnoughMemoryForBackForwardCache().
  // TODO(crbug.com/40277975): experiment with 1200 MB threshold like
  // back/forward cache.
  static constexpr int kDefaultMemoryThresholdMb =
#if BUILDFLAG(IS_ANDROID)
      1700;
#else
      0;
#endif

  // The default is overridable by field trial param.
  int memory_threshold_mb = base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kPrerender2MemoryControls,
      blink::features::kPrerender2MemoryThresholdParamName,
      kDefaultMemoryThresholdMb);

  return base::SysInfo::AmountOfPhysicalMemoryMB() > memory_threshold_mb;
}

base::MemoryPressureListener::MemoryPressureLevel
GetCurrentMemoryPressureLevel() {
  // Ignore the memory pressure event if the memory control is disabled.
  if (!base::FeatureList::IsEnabled(
          blink::features::kPrerender2MemoryControls)) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  auto* monitor = base::MemoryPressureMonitor::Get();
  if (!monitor) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }
  return monitor->GetCurrentPressureLevel();
}

// Create a resource request for `back_url` that only checks whether the
// resource is in the HTTP cache.
std::unique_ptr<network::SimpleURLLoader> CreateHttpCacheQueryingResourceLoad(
    const GURL& back_url) {
  url::Origin origin = url::Origin::Create(back_url);
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, origin, origin,
      net::SiteForCookies::FromOrigin(origin));
  network::ResourceRequest::TrustedParams trusted_params;
  trusted_params.isolation_info = isolation_info;

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = back_url;
  request->load_flags =
      net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
  request->trusted_params = trusted_params;
  request->site_for_cookies = trusted_params.isolation_info.site_for_cookies();
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->skip_service_worker = true;
  request->do_not_prompt_for_login = true;

  CHECK(!request->SendsCookies());
  CHECK(!request->SavesCookies());
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("back_navigation_cache_query",
                                          R"(
          semantics {
            sender: "Prerender"
            description:
              "This is not actually a network request. It is used internally "
              "by the browser to determine if the HTTP cache would be used if "
              "the user were to navigate back in session history. It only "
              "checks the cache and does not hit the network."
            trigger:
              "When the user performs an action that would suggest that they "
              "intend to navigate back soon. Examples include hovering the "
              "mouse over the back button and the start of a gestural back "
              "navigation."
            user_data {
              type: NONE
            }
            data: "None. The request doesn't hit the network."
            destination: LOCAL
            internal {
              contacts {
                email: "chrome-brapp-loading@chromium.org"
              }
            }
            last_reviewed: "2023-03-24"
          }
          policy {
            cookies_allowed: NO
            setting:
              "This is not controlled by a setting."
            policy_exception_justification: "This is not a network request."
        })");

  return network::SimpleURLLoader::Create(std::move(request),
                                          traffic_annotation);
}

// Returns true if the given navigation is meant to be predicted by a predictor
// related to session history (e.g. hovering over the back button could have
// predicted the navigation).
bool IsNavigationInSessionHistoryPredictorDomain(NavigationHandle* handle) {
  CHECK(handle->IsInPrimaryMainFrame());
  CHECK(!handle->IsSameDocument());

  if (handle->IsRendererInitiated()) {
    return false;
  }

  // Note that currently the only predictors are for back navigations of a
  // single step, however we still include all session history navigations in
  // the domain. The preloading of back navigations could generalize to session
  // history navigations of other offsets, but we haven't explored this due to
  // the higher usage of the back button compared to the forward button or
  // history menu.
  if (!(handle->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK)) {
    return false;
  }

  if (handle->IsPost()) {
    return false;
  }

  if (handle->IsServedFromBackForwardCache()) {
    return false;
  }

  if (!handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Note that even though the current predictors do not handle session history
  // navigations that are same-site or which don't use the HTTP cache, they are
  // still included in the domain.
  return true;
}

PreloadingEligibility ToEligibility(PrerenderFinalStatus status) {
  switch (status) {
    case PrerenderFinalStatus::kActivated:
    case PrerenderFinalStatus::kDestroyed:
      NOTREACHED();
    case PrerenderFinalStatus::kLowEndDevice:
      return PreloadingEligibility::kLowMemory;
    case PrerenderFinalStatus::kInvalidSchemeRedirect:
      NOTREACHED();
    case PrerenderFinalStatus::kInvalidSchemeNavigation:
      return PreloadingEligibility::kHttpOrHttpsOnly;
    case PrerenderFinalStatus::kNavigationRequestBlockedByCsp:
    case PrerenderFinalStatus::kMainFrameNavigation:
    case PrerenderFinalStatus::kMojoBinderPolicy:
    case PrerenderFinalStatus::kRendererProcessCrashed:
    case PrerenderFinalStatus::kRendererProcessKilled:
    case PrerenderFinalStatus::kDownload:
    case PrerenderFinalStatus::kTriggerDestroyed:
    case PrerenderFinalStatus::kNavigationNotCommitted:
    case PrerenderFinalStatus::kNavigationBadHttpStatus:
    case PrerenderFinalStatus::kClientCertRequested:
    case PrerenderFinalStatus::kNavigationRequestNetworkError:
    case PrerenderFinalStatus::kCancelAllHostsForTesting:
    case PrerenderFinalStatus::kDidFailLoad:
    case PrerenderFinalStatus::kStop:
    case PrerenderFinalStatus::kSslCertificateError:
    case PrerenderFinalStatus::kLoginAuthRequested:
    case PrerenderFinalStatus::kUaChangeRequiresReload:
    case PrerenderFinalStatus::kBlockedByClient:
    case PrerenderFinalStatus::kMixedContent:
      NOTREACHED();
    case PrerenderFinalStatus::kTriggerBackgrounded:
      return PreloadingEligibility::kHidden;
    case PrerenderFinalStatus::kMemoryLimitExceeded:
      NOTREACHED();
    case PrerenderFinalStatus::kDataSaverEnabled:
      return PreloadingEligibility::kDataSaverEnabled;
    case PrerenderFinalStatus::kTriggerUrlHasEffectiveUrl:
      return PreloadingEligibility::kHasEffectiveUrl;
    case PrerenderFinalStatus::kActivatedBeforeStarted:
    case PrerenderFinalStatus::kInactivePageRestriction:
    case PrerenderFinalStatus::kStartFailed:
    case PrerenderFinalStatus::kTimeoutBackgrounded:
    case PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation:
      NOTREACHED();
    case PrerenderFinalStatus::kCrossSiteNavigationInInitialNavigation:
      return PreloadingEligibility::kCrossOrigin;
    case PrerenderFinalStatus::
        kSameSiteCrossOriginRedirectNotOptInInInitialNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginNavigationNotOptInInInitialNavigation:
    case PrerenderFinalStatus::kActivationNavigationParameterMismatch:
    case PrerenderFinalStatus::kActivatedInBackground:
    case PrerenderFinalStatus::kEmbedderHostDisallowed:
    case PrerenderFinalStatus::kActivationNavigationDestroyedBeforeSuccess:
    case PrerenderFinalStatus::kTabClosedByUserGesture:
    case PrerenderFinalStatus::kTabClosedWithoutUserGesture:
    case PrerenderFinalStatus::kPrimaryMainFrameRendererProcessCrashed:
    case PrerenderFinalStatus::kPrimaryMainFrameRendererProcessKilled:
    case PrerenderFinalStatus::kActivationFramePolicyNotCompatible:
      NOTREACHED();
    case PrerenderFinalStatus::kPreloadingDisabled:
      return PreloadingEligibility::kPreloadingDisabled;
    case PrerenderFinalStatus::kBatterySaverEnabled:
      return PreloadingEligibility::kBatterySaverEnabled;
    case PrerenderFinalStatus::kActivatedDuringMainFrameNavigation:
      NOTREACHED();
    case PrerenderFinalStatus::kPreloadingUnsupportedByWebContents:
      return PreloadingEligibility::kPreloadingUnsupportedByWebContents;
    case PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation:
    case PrerenderFinalStatus::kCrossSiteNavigationInMainFrameNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginNavigationNotOptInInMainFrameNavigation:
      NOTREACHED();
    case PrerenderFinalStatus::kMemoryPressureOnTrigger:
      return PreloadingEligibility::kMemoryPressure;
    case PrerenderFinalStatus::kMemoryPressureAfterTriggered:
      NOTREACHED();
    case PrerenderFinalStatus::kPrerenderingDisabledByDevTools:
      return PreloadingEligibility::kPreloadingDisabledByDevTools;
    case PrerenderFinalStatus::kSpeculationRuleRemoved:
    case PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts:
    case PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded:
    case PrerenderFinalStatus::kMaxNumOfRunningNonEagerPrerendersExceeded:
    case PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded:
      NOTREACHED();
    case PrerenderFinalStatus::kPrerenderingUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kRedirectedPrerenderingUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kActivationUrlHasEffectiveUrl:
      return PreloadingEligibility::kHasEffectiveUrl;
    case PrerenderFinalStatus::kJavaScriptInterfaceAdded:
    case PrerenderFinalStatus::kJavaScriptInterfaceRemoved:
    case PrerenderFinalStatus::kAllPrerenderingCanceled:
    case PrerenderFinalStatus::kWindowClosed:
    case PrerenderFinalStatus::kOtherPrerenderedPageActivated:
      NOTREACHED();
    case PrerenderFinalStatus::kSlowNetwork:
      return PreloadingEligibility::kSlowNetwork;
    case PrerenderFinalStatus::kV8OptimizerDisabled:
      return PreloadingEligibility::kV8OptimizerDisabled;
  }

  NOTREACHED();
}

// Represents a contract and ensures that the given prerender attempt is started
// as a PrerenderHost or rejected with a reason. It is allowed to use it only in
// PrerenderHostRegistry::CreateAndStartHost.
//
// TODO(kenoss): Add emits of Preload.prerenderStatusUpdated.
class PrerenderHostBuilder {
 public:
  explicit PrerenderHostBuilder(PreloadingAttempt* attempt);
  ~PrerenderHostBuilder();

  PrerenderHostBuilder(const PrerenderHostBuilder&) = delete;
  PrerenderHostBuilder& operator=(const PrerenderHostBuilder&) = delete;
  PrerenderHostBuilder(PrerenderHostBuilder&&) = delete;
  PrerenderHostBuilder& operator=(PrerenderHostBuilder&&) = delete;

  // The following methods consumes this class.
  std::unique_ptr<PrerenderHost> Build(const PrerenderAttributes& attributes,
                                       WebContentsImpl& prerender_web_contents);
  void RejectAsNotEligible(const PrerenderAttributes& attributes,
                           PrerenderFinalStatus status);
  void RejectAsDuplicate();
  void RejectAsFailure(const PrerenderAttributes& attributes,
                       PrerenderFinalStatus status);
  void RejectDueToHoldback();

  void SetHoldbackOverride(PreloadingHoldbackStatus status);
  bool CheckIfShouldHoldback();

  // Public only for exceptional case.
  // TODO(crbug.com/40904828): Make this private again.
  void Drop();

 private:
  bool IsDropped() const;

  // Use raw pointer as PrerenderHostBuilder is alive only during
  // PrerenderHostRegistry::CreateAndStartHost(), and PreloadingAttempt should
  // outlive the function.
  raw_ptr<PreloadingAttempt> attempt_;
  std::unique_ptr<DevToolsPrerenderAttempt> devtools_attempt_;
};

PrerenderHostBuilder::PrerenderHostBuilder(PreloadingAttempt* attempt)
    : attempt_(attempt),
      devtools_attempt_(std::make_unique<DevToolsPrerenderAttempt>()) {}

PrerenderHostBuilder::~PrerenderHostBuilder() {
  CHECK(IsDropped());
}

void PrerenderHostBuilder::Drop() {
  attempt_ = nullptr;
  devtools_attempt_.reset();
}

bool PrerenderHostBuilder::IsDropped() const {
  return devtools_attempt_ == nullptr;
}

std::unique_ptr<PrerenderHost> PrerenderHostBuilder::Build(
    const PrerenderAttributes& attributes,
    WebContentsImpl& prerender_web_contents) {
  CHECK(!IsDropped());

  auto prerender_host = std::make_unique<PrerenderHost>(
      attributes, prerender_web_contents,
      attempt_ ? attempt_->GetWeakPtr() : nullptr,
      std::move(devtools_attempt_));

  Drop();

  return prerender_host;
}

void PrerenderHostBuilder::RejectAsNotEligible(
    const PrerenderAttributes& attributes,
    PrerenderFinalStatus status) {
  CHECK(!IsDropped());

  if (attempt_) {
    attempt_->SetEligibility(ToEligibility(status));
  }

  devtools_attempt_->SetFailureReason(attributes, status);

  RecordFailedPrerenderFinalStatus(PrerenderCancellationReason(status),
                                   attributes);

  Drop();
}

bool PrerenderHostBuilder::CheckIfShouldHoldback() {
  CHECK(!IsDropped());

  // Assigns the holdback status in the attempt it was not overridden earlier.
  return attempt_ && attempt_->ShouldHoldback();
}

void PrerenderHostBuilder::RejectDueToHoldback() {
  CHECK(!IsDropped());

  // If DevTools is opened, holdbacks are force-disabled. So, we don't need to
  // report this case to DevTools.

  Drop();
}

void PrerenderHostBuilder::RejectAsDuplicate() {
  CHECK(!IsDropped());

  if (attempt_) {
    attempt_->SetTriggeringOutcome(PreloadingTriggeringOutcome::kDuplicate);
  }

  // No need to report DevTools nor UMA; just removing duplicates.

  Drop();
}

void PrerenderHostBuilder::SetHoldbackOverride(
    PreloadingHoldbackStatus status) {
  if (!attempt_) {
    return;
  }
  attempt_->SetHoldbackStatus(status);
}

void PrerenderHostBuilder::RejectAsFailure(
    const PrerenderAttributes& attributes,
    PrerenderFinalStatus status) {
  CHECK(!IsDropped());

  if (attempt_) {
    attempt_->SetFailureReason(ToPreloadingFailureReason(status));
  }

  devtools_attempt_->SetFailureReason(attributes, status);

  RecordFailedPrerenderFinalStatus(PrerenderCancellationReason(status),
                                   attributes);

  Drop();
}

bool IsSlowNetwork(WebContents* web_contents) {
  static const base::TimeDelta kSlowNetworkThreshold =
      features::kSuppressesPrerenderingOnSlowNetworkThreshold.Get();
  return web_contents && web_contents->GetBrowserContext() &&
         web_contents->GetBrowserContext()
             ->GetClientHintsControllerDelegate() &&
         web_contents->GetBrowserContext()
             ->GetClientHintsControllerDelegate()
             ->GetNetworkQualityTracker() &&
         web_contents->GetBrowserContext()
                 ->GetClientHintsControllerDelegate()
                 ->GetNetworkQualityTracker()
                 ->GetHttpRTT() > kSlowNetworkThreshold;
}

}  // namespace

const char kMaxNumOfRunningSpeculationRulesEagerPrerenders[] =
    "max_num_of_running_speculation_rules_eager_prerenders";
const char kMaxNumOfRunningSpeculationRulesNonEagerPrerenders[] =
    "max_num_of_running_speculation_rules_non_eager_prerenders";
const char kMaxNumOfRunningEmbedderPrerenders[] =
    "max_num_of_running_embedder_prerenders";

PrerenderHostRegistry::PrerenderHostRegistry(WebContents& web_contents)
    : memory_pressure_listener_(
          FROM_HERE,
          base::BindRepeating(&PrerenderHostRegistry::OnMemoryPressure,
                              base::Unretained(this))) {
  Observe(&web_contents);
}

PrerenderHostRegistry::~PrerenderHostRegistry() {
  // This function is called by WebContentsImpl's dtor, so web_contents() should
  // not be a null ptr at this moment.
  CHECK(web_contents());

  PrerenderFinalStatus final_status =
      web_contents()->GetClosedByUserGesture()
          ? PrerenderFinalStatus::kTabClosedByUserGesture
          : PrerenderFinalStatus::kTabClosedWithoutUserGesture;

  // Here we have to delete the prerender hosts synchronously, to ensure the
  // FrameTrees would not access the WebContents.
  CancelAllHosts(final_status);
  Observe(nullptr);
  for (Observer& obs : observers_)
    obs.OnRegistryDestroyed();
}

void PrerenderHostRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrerenderHostRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

FrameTreeNodeId PrerenderHostRegistry::CreateAndStartHost(
    const PrerenderAttributes& attributes,
    PreloadingAttempt* attempt) {
  std::string recorded_url =
      attributes.initiator_origin.has_value()
          ? attributes.initiator_origin.value().GetURL().spec()
          : "(empty_url)";

  TRACE_EVENT2("navigation", "PrerenderHostRegistry::CreateAndStartHost",
               "attributes", attributes, "initiator_origin", recorded_url);

  // The initiator WebContents can be different from the WebContents that will
  // host a prerendered page only when the prerender-in-new-tab runs.
  CHECK(attributes.initiator_web_contents);
  auto& initiator_web_contents =
      static_cast<WebContentsImpl&>(*attributes.initiator_web_contents);
  auto& prerender_web_contents = static_cast<WebContentsImpl&>(*web_contents());
  CHECK(&initiator_web_contents == &prerender_web_contents ||
        base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab));

  FrameTreeNodeId frame_tree_node_id;

  {
    RenderFrameHostImpl* initiator_rfh =
        attributes.IsBrowserInitiated()
            ? nullptr
            : RenderFrameHostImpl::FromFrameToken(
                  attributes.initiator_process_id,
                  attributes.initiator_frame_token.value());

    // Ensure observers are notified that a trigger occurred.
    absl::Cleanup notify_trigger = [this, &attributes] {
      NotifyTrigger(attributes.prerendering_url);
    };

    auto builder = PrerenderHostBuilder(attempt);

    // We don't know the root cause, but there is a case this is null.
    //
    // TODO(crbug.com/40904828): Continue investigation and fix the root
    // cause.
    if (initiator_web_contents.GetDelegate() == nullptr) {
      // Note that return without consuming `builder` is exceptional.
      builder.Drop();
      return FrameTreeNodeId();
    }

    // Check the about://flags toggle.
    if (!base::FeatureList::IsEnabled(blink::features::kPrerender2)) {
      builder.RejectAsNotEligible(attributes,
                                  PrerenderFinalStatus::kPreloadingDisabled);
      return FrameTreeNodeId();
    }

    // Check whether preloading is enabled. If it is not enabled, report the
    // reason.
    switch (initiator_web_contents.GetDelegate()->IsPrerender2Supported(
        initiator_web_contents)) {
      case PreloadingEligibility::kEligible:
        // nop
        break;
      case PreloadingEligibility::kPreloadingDisabled:
        builder.RejectAsNotEligible(attributes,
                                    PrerenderFinalStatus::kPreloadingDisabled);
        return FrameTreeNodeId();
      case PreloadingEligibility::kDataSaverEnabled:
        builder.RejectAsNotEligible(attributes,
                                    PrerenderFinalStatus::kDataSaverEnabled);
        return FrameTreeNodeId();
      case PreloadingEligibility::kBatterySaverEnabled:
        builder.RejectAsNotEligible(attributes,
                                    PrerenderFinalStatus::kBatterySaverEnabled);
        return FrameTreeNodeId();
      case PreloadingEligibility::kPreloadingUnsupportedByWebContents:
        builder.RejectAsNotEligible(
            attributes,
            PrerenderFinalStatus::kPreloadingUnsupportedByWebContents);
        return FrameTreeNodeId();
      default:
        NOTREACHED();
    }

    // Don't prerender when the initiator is in the background and its type is
    // `kEmbedder`, as current implementation doesn't use `pending_prerenders_`
    // when kEmbedder.
    // If the trigger type is speculation rules, nothing should be done here and
    // then prerender host will be created and its id will be enqueued to
    // `pending_prerenders_`. The visibility of the initiator will be considered
    // when trying to pop from `pending_prerenders_` on `StartPrerendering()`.
    if (attributes.trigger_type == PreloadingTriggerType::kEmbedder &&
        initiator_web_contents.GetVisibility() == Visibility::HIDDEN) {
      builder.RejectAsNotEligible(attributes,
                                  PrerenderFinalStatus::kTriggerBackgrounded);
      return FrameTreeNodeId();
    }

    // Don't prerender on low-end devices.
    if (!DeviceHasEnoughMemoryForPrerender()) {
      builder.RejectAsNotEligible(attributes,
                                  PrerenderFinalStatus::kLowEndDevice);
      return FrameTreeNodeId();
    }

    // Don't prerender under critical memory pressure.
    switch (GetCurrentMemoryPressureLevel()) {
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
        break;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
        builder.RejectAsNotEligible(
            attributes, PrerenderFinalStatus::kMemoryPressureOnTrigger);
        return FrameTreeNodeId();
    }

    // Disable prerendering on slow network.
    static const bool kSuppressesPrerenderingOnSlowNetworkIsEnabled =
        base::FeatureList::IsEnabled(
            features::kSuppressesPrerenderingOnSlowNetwork);
    if (kSuppressesPrerenderingOnSlowNetworkIsEnabled &&
        IsSlowNetwork(web_contents())) {
      builder.RejectAsNotEligible(attributes,
                                  PrerenderFinalStatus::kSlowNetwork);
      return FrameTreeNodeId();
    }

    // Allow prerendering only for same-site. The initiator origin is nullopt
    // when prerendering is initiated by the browser (not by a renderer using
    // Speculation Rules API). In that case, skip this same-site check.
    // TODO(crbug.com/40168192): Support cross-site prerendering.
    if (!attributes.IsBrowserInitiated() &&
        !prerender_navigation_utils::IsSameSite(
            attributes.prerendering_url, attributes.initiator_origin.value())) {
      builder.RejectAsNotEligible(
          attributes,
          PrerenderFinalStatus::kCrossSiteNavigationInInitialNavigation);
      return FrameTreeNodeId();
    }

    // Allow prerendering only HTTP(S) scheme URLs. For redirection, this will
    // be checked in PrerenderNavigationThrottle::WillStartOrRedirectRequest().
    if (!attributes.prerendering_url.SchemeIsHTTPOrHTTPS()) {
      builder.RejectAsNotEligible(
          attributes, PrerenderFinalStatus::kInvalidSchemeNavigation);
      return FrameTreeNodeId();
    }

    // Disallow all pages that have an effective URL like hosted apps and NTP.
    auto* browser_context = prerender_web_contents.GetBrowserContext();
    if (SiteInstanceImpl::HasEffectiveURL(browser_context,
                                          initiator_web_contents.GetURL())) {
      builder.RejectAsNotEligible(
          attributes, PrerenderFinalStatus::kTriggerUrlHasEffectiveUrl);
      return FrameTreeNodeId();
    }
    if (SiteInstanceImpl::HasEffectiveURL(browser_context,
                                          attributes.prerendering_url)) {
      builder.RejectAsNotEligible(
          attributes, PrerenderFinalStatus::kPrerenderingUrlHasEffectiveUrl);
      return FrameTreeNodeId();
    }

    if (initiator_rfh && initiator_rfh->frame_tree() &&
        !devtools_instrumentation::IsPrerenderAllowed(
            *initiator_rfh->frame_tree())) {
      builder.RejectAsNotEligible(
          attributes, PrerenderFinalStatus::kPrerenderingDisabledByDevTools);
      return FrameTreeNodeId();
    }

    // Don't start prerendering when the V8 optimizer is disabled by the site
    // settings. This is because prerendering a page that has the COOP crashes
    // when the V8 optimizer is disabled. See https://crbug.com/40076091 for
    // details.
    if (GetContentClient()->browser()->AreV8OptimizationsDisabledForSite(
            web_contents()->GetBrowserContext(), attributes.prerendering_url)) {
      builder.RejectAsNotEligible(attributes,
                                  PrerenderFinalStatus::kV8OptimizerDisabled);
      return FrameTreeNodeId();
    }

    // Once all eligibility checks are completed, set the status to kEligible.
    if (attempt)
      attempt->SetEligibility(PreloadingEligibility::kEligible);

    // Normally CheckIfShouldHoldback() computes the holdback status based on
    // PreloadingConfig. In special cases, we call SetHoldbackOverride() to
    // override that processing.
    bool has_devtools_open =
        initiator_rfh &&
        RenderFrameDevToolsAgentHost::GetFor(initiator_rfh) != nullptr;

    if (has_devtools_open) {
      // Never holdback when DevTools is opened, to avoid web developer
      // frustration.
      builder.SetHoldbackOverride(PreloadingHoldbackStatus::kAllowed);
    } else if (attributes.holdback_status_override !=
               PreloadingHoldbackStatus::kUnspecified) {
      // The caller (e.g. from chrome/) is allowed to specify a holdback that
      // overrides the default logic.
      builder.SetHoldbackOverride(attributes.holdback_status_override);
    }

    // Check if the attempt is held back either due to the check above or via
    // PreloadingConfig.
    if (builder.CheckIfShouldHoldback()) {
      builder.RejectDueToHoldback();
      return FrameTreeNodeId();
    }

    // Ignore prerendering requests for the same URL.
    for (auto& iter : prerender_host_by_frame_tree_node_id_) {
      if (iter.second->GetInitialUrl() == attributes.prerendering_url) {
        builder.RejectAsDuplicate();
        return FrameTreeNodeId();
      }
    }

    // Under kPrerender2InNewTab, CreateAndStartHost will be called in
    // the newly created WebContents’s PrerenderHostRegistry for new tab
    // triggers, rather than in initiator WebContents’s registry, while
    // it is called in initiator ones for normal triggers. In either
    // case, we want to control the limit based on the initiator
    // WebContents.
    //
    // TODO(crbug.com/40235847): Enqueue the request exceeding the number limit
    // until the forerunners are cancelled, and suspend starting a new prerender
    // when the number reaches the limit.
    if (!initiator_web_contents.GetPrerenderHostRegistry()
             ->IsAllowedToStartPrerenderingForTrigger(attributes.trigger_type,
                                                      attributes.eagerness)) {
      // The reason we don't consider limit exceeded as an ineligibility
      // reason is because we can't replicate the behavior in our other
      // experiment groups for analysis. To prevent this we set
      // TriggeringOutcome to kFailure and look into the failure reason to
      // learn more.
      PrerenderFinalStatus final_status;
      switch (GetPrerenderLimitGroup(attributes.trigger_type,
                                     attributes.eagerness)) {
        case PrerenderLimitGroup::kSpeculationRulesEager:
          final_status =
              PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded;
          break;
        case PrerenderLimitGroup::kSpeculationRulesNonEager:
          final_status =
              PrerenderFinalStatus::kMaxNumOfRunningNonEagerPrerendersExceeded;
          break;
        case PrerenderLimitGroup::kEmbedder:
          final_status =
              PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded;
          break;
      }
      builder.RejectAsFailure(attributes, final_status);
      return FrameTreeNodeId();
    }

    auto prerender_host = builder.Build(attributes, prerender_web_contents);
    frame_tree_node_id = prerender_host->frame_tree_node_id();

    CHECK(!base::Contains(prerender_host_by_frame_tree_node_id_,
                          frame_tree_node_id));
    prerender_host_by_frame_tree_node_id_[frame_tree_node_id] =
        std::move(prerender_host);

    if (GetPrerenderLimitGroup(attributes.trigger_type, attributes.eagerness) ==
        PrerenderLimitGroup::kSpeculationRulesNonEager) {
      non_eager_prerender_host_id_by_arrival_order_.push_back(
          frame_tree_node_id);
    }
  }

  switch (attributes.trigger_type) {
    case PreloadingTriggerType::kSpeculationRule:
    case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
    case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
      pending_prerenders_.push_back(frame_tree_node_id);
      if (running_prerender_host_id_.is_null()) {
        // Start the initial prerendering navigation of the pending request in
        // the head of the queue if there's no running prerender and the
        // initiator is in the foreground. If the initiator page is in the
        // background, `StartPrerendering` will return a corresponding
        // frame_tree_node_id if allowed by
        // `PrerenderCanBeStartedWhenInitiatorIsInBackground`.
        if (IsBackground(initiator_web_contents.GetVisibility()) &&
            !initiator_web_contents.GetPrerenderHostRegistry()
                 ->PrerenderCanBeStartedWhenInitiatorIsInBackground()) {
          // Cancel if it is prerender-into-new-tab.
          // TODO(crbug.com/350785853): Add queue
          // mechanism and update test expectation.
          if (web_contents() != &initiator_web_contents) {
            return FrameTreeNodeId();
          }
          break;
        }
        FrameTreeNodeId started_frame_tree_node_id =
            StartPrerendering(FrameTreeNodeId());
        CHECK(started_frame_tree_node_id == frame_tree_node_id ||
              started_frame_tree_node_id.is_null());
        frame_tree_node_id = started_frame_tree_node_id;
      }
      break;
    case PreloadingTriggerType::kEmbedder:
      // The prerendering request from embedder should have high-priority
      // because embedder prediction is more likely for the user to visit. Hold
      // the return value of `StartPrerendering` because the requested prerender
      // might be cancelled due to some restrictions and a null FrameTreeNodeId
      // should be returned in that case.
      frame_tree_node_id = StartPrerendering(frame_tree_node_id);
      break;
  }

  return frame_tree_node_id;
}

FrameTreeNodeId PrerenderHostRegistry::CreateAndStartHostForNewTab(
    const PrerenderAttributes& attributes,
    const PreloadingPredictor& creating_predictor,
    const PreloadingPredictor& enacting_predictor,
    PreloadingConfidence confidence) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab));
  CHECK(IsSpeculationRuleType(attributes.trigger_type));
  std::string recorded_url =
      attributes.initiator_origin.has_value()
          ? attributes.initiator_origin.value().GetURL().spec()
          : "(empty_url)";
  TRACE_EVENT2("navigation",
               "PrerenderHostRegistry::CreateAndStartHostForNewTab",
               "attributes", attributes, "initiator_origin", recorded_url);

  auto handle = std::make_unique<PrerenderNewTabHandle>(
      attributes, *web_contents()->GetBrowserContext());
  FrameTreeNodeId prerender_host_id = handle->StartPrerendering(
      creating_predictor, enacting_predictor, confidence);
  if (prerender_host_id.is_null()) {
    return FrameTreeNodeId();
  }
  prerender_new_tab_handle_by_frame_tree_node_id_[prerender_host_id] =
      std::move(handle);

  if (GetPrerenderLimitGroup(attributes.trigger_type, attributes.eagerness) ==
      PrerenderLimitGroup::kSpeculationRulesNonEager) {
    non_eager_prerender_host_id_by_arrival_order_.push_back(prerender_host_id);
  }
  return prerender_host_id;
}

FrameTreeNodeId PrerenderHostRegistry::StartPrerendering(
    FrameTreeNodeId frame_tree_node_id) {
  // TODO(crbug.com/40260412): Don't start prerendering if the current
  // memory pressure level is critical, and then retry prerendering when the
  // memory pressure level goes down.

  if (frame_tree_node_id.is_null()) {
    CHECK(running_prerender_host_id_.is_null());

    while (!pending_prerenders_.empty()) {
      FrameTreeNodeId host_id = pending_prerenders_.front();

      // Skip a cancelled request.
      auto found = prerender_host_by_frame_tree_node_id_.find(host_id);
      if (found == prerender_host_by_frame_tree_node_id_.end()) {
        // Remove the cancelled request from the pending queue.
        pending_prerenders_.pop_front();
        continue;
      }
      PrerenderHost* prerender_host = found->second.get();

      // The initiator WebContents should be alive as it cancels all the
      // prerendering requests during destruction.
      CHECK(prerender_host->initiator_web_contents());

      WebContentsImpl* initiator_web_contents = static_cast<WebContentsImpl*>(
          prerender_host->initiator_web_contents().get());
      if (IsBackground(initiator_web_contents->GetVisibility())) {
        // The pending prerender triggered by the background tab will be started
        // according to the conditions in
        // `PrerenderCanBeStartedWhenInitiatorIsInBackground`.
        if (!initiator_web_contents->GetPrerenderHostRegistry()
                 ->PrerenderCanBeStartedWhenInitiatorIsInBackground()) {
          return FrameTreeNodeId();
        }
      }

      // Found the request to run.
      pending_prerenders_.pop_front();
      frame_tree_node_id = host_id;
      break;
    }

    if (frame_tree_node_id.is_null()) {
      return FrameTreeNodeId();
    }
  }

  auto prerender_host_it =
      prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  CHECK(prerender_host_it != prerender_host_by_frame_tree_node_id_.end());
  PrerenderHost& prerender_host = *prerender_host_it->second;
  devtools_instrumentation::WillInitiatePrerender(
      prerender_host.GetPrerenderFrameTree());
  if (!prerender_host.StartPrerendering()) {
    CancelHost(frame_tree_node_id, PrerenderFinalStatus::kStartFailed);
    return FrameTreeNodeId();
  }

  switch (prerender_host_by_frame_tree_node_id_[frame_tree_node_id]
              ->trigger_type()) {
    case PreloadingTriggerType::kSpeculationRule:
    case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
    case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
      // Update the `running_prerender_host_id` to the starting prerender's id.
      running_prerender_host_id_ = frame_tree_node_id;
      break;
    case PreloadingTriggerType::kEmbedder:
      // `running_prerender_host_id` only tracks the id for speculation rules
      // trigger, so we also don't update it in the case of embedder.
      break;
  }

  RecordPrerenderTriggered(
      prerender_host_by_frame_tree_node_id_[frame_tree_node_id]
          ->initiator_ukm_id());
  return frame_tree_node_id;
}

std::set<FrameTreeNodeId> PrerenderHostRegistry::CancelHosts(
    const std::vector<FrameTreeNodeId>& frame_tree_node_ids,
    const PrerenderCancellationReason& reason) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::CancelHosts",
               "frame_tree_node_ids", frame_tree_node_ids);

  // Cancel must not be requested during activation.
  CHECK(!reserved_prerender_host_);

  std::set<FrameTreeNodeId> cancelled_ids;

  for (FrameTreeNodeId host_id : frame_tree_node_ids) {
    if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)) {
      if (CancelHostInternal(host_id, reason) ||
          CancelNewTabHostInternal(host_id, reason)) {
        cancelled_ids.insert(host_id);
      }
    } else {
      CHECK(prerender_new_tab_handle_by_frame_tree_node_id_.empty());
      if (CancelHostInternal(host_id, reason)) {
        cancelled_ids.insert(host_id);
      }
    }
  }

  // Start another prerender if the running prerender is cancelled.
  if (running_prerender_host_id_.is_null()) {
    StartPrerendering(FrameTreeNodeId());
  }

  return cancelled_ids;
}

bool PrerenderHostRegistry::CancelHost(FrameTreeNodeId frame_tree_node_id,
                                       PrerenderFinalStatus final_status) {
  return CancelHost(frame_tree_node_id,
                    PrerenderCancellationReason(final_status));
}

bool PrerenderHostRegistry::CancelHost(
    FrameTreeNodeId frame_tree_node_id,
    const PrerenderCancellationReason& reason) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::CancelHost",
               "frame_tree_node_id", frame_tree_node_id);
  std::set<FrameTreeNodeId> cancelled_ids =
      CancelHosts({frame_tree_node_id}, reason);
  return !cancelled_ids.empty();
}

void PrerenderHostRegistry::CancelHostsForTriggers(
    std::vector<PreloadingTriggerType> trigger_types,
    const PrerenderCancellationReason& reason) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::CancelHostsForTrigger",
               "trigger_type", trigger_types[0]);

  std::vector<FrameTreeNodeId> ids_to_be_deleted;

  for (auto& iter : prerender_host_by_frame_tree_node_id_) {
    if (base::Contains(trigger_types, iter.second->trigger_type())) {
      ids_to_be_deleted.push_back(iter.first);
    }
  }
  if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)) {
    for (auto& iter : prerender_new_tab_handle_by_frame_tree_node_id_) {
      if (base::Contains(trigger_types, iter.second->trigger_type())) {
        // Prerendering into a new tab can be triggered by speculation rules
        // only.
        CHECK(IsSpeculationRuleType(iter.second->trigger_type()));
        ids_to_be_deleted.push_back(iter.first);
      }
    }
  } else {
    CHECK(prerender_new_tab_handle_by_frame_tree_node_id_.empty());
  }

  CancelHosts(ids_to_be_deleted, reason);
}

void PrerenderHostRegistry::CancelAllHosts(PrerenderFinalStatus final_status) {
  // Cancel must not be requested during activation.
  CHECK(!reserved_prerender_host_);

  PrerenderCancellationReason reason(final_status);

  while (!prerender_host_by_frame_tree_node_id_.empty()) {
    CancelHostInternal(prerender_host_by_frame_tree_node_id_.begin()->first,
                       reason);
  }

  if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)) {
    while (!prerender_new_tab_handle_by_frame_tree_node_id_.empty()) {
      CancelNewTabHostInternal(
          prerender_new_tab_handle_by_frame_tree_node_id_.begin()->first,
          reason);
    }
  } else {
    CHECK(prerender_new_tab_handle_by_frame_tree_node_id_.empty());
  }

  pending_prerenders_.clear();
}

bool PrerenderHostRegistry::CancelHostInternal(
    FrameTreeNodeId frame_tree_node_id,
    const PrerenderCancellationReason& reason) {
  // Look up the id in the non-reserved host map.
  auto iter = prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (iter == prerender_host_by_frame_tree_node_id_.end()) {
    return false;
  }

  if (running_prerender_host_id_ == frame_tree_node_id) {
    running_prerender_host_id_ = FrameTreeNodeId();
  }

  // Remove the prerender host from the host map so that it's not used for
  // activation during asynchronous deletion.
  std::unique_ptr<PrerenderHost> prerender_host = std::move(iter->second);
  prerender_host_by_frame_tree_node_id_.erase(iter);

  reason.ReportMetrics(prerender_host->GetHistogramSuffix());

  NotifyCancel(prerender_host->frame_tree_node_id(), reason);

  // Under kPrerender2InNewTab, if the host we are attempting to cancel is the
  // new-tab host and initiator WebContents's PrerenderHostRegistry for this
  // host is still alive, invoke the initiator WebContents's
  // CancelNewTabHostInternal to destroy PrerenderNewTabHandle and WebContents
  // that this new-tab host belongs to. This will eventually destroy `this`, so
  // it should be performed asynchronously.
  if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)) {
    CHECK(prerender_host->initiator_web_contents());
    WebContentsImpl* initiator_web_contents = static_cast<WebContentsImpl*>(
        prerender_host->initiator_web_contents().get());
    if (web_contents() != initiator_web_contents &&
        !initiator_web_contents->IsBeingDestroyed()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              base::IgnoreResult(
                  &PrerenderHostRegistry::CancelNewTabHostInternal),
              initiator_web_contents->GetPrerenderHostRegistry()->GetWeakPtr(),
              frame_tree_node_id,
              PrerenderCancellationReason(reason.final_status())));
    }
  }

  // Asynchronously delete the prerender host.
  ScheduleToDeleteAbandonedHost(std::move(prerender_host), reason);
  return true;
}

bool PrerenderHostRegistry::CancelNewTabHostInternal(
    FrameTreeNodeId frame_tree_node_id,
    const PrerenderCancellationReason& reason) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab));

  // Look up the id in the prerender-in-new-tab handle map.
  auto iter =
      prerender_new_tab_handle_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (iter == prerender_new_tab_handle_by_frame_tree_node_id_.end()) {
    return false;
  }

  // The host should be driven by PrerenderHostRegistry associated with
  // the new tab.
  CHECK_NE(running_prerender_host_id_, frame_tree_node_id);

  std::unique_ptr<PrerenderNewTabHandle> handle = std::move(iter->second);
  prerender_new_tab_handle_by_frame_tree_node_id_.erase(iter);
  NotifyCancel(frame_tree_node_id, reason);
  handle->CancelPrerendering(reason);
  return true;
}

FrameTreeNodeId PrerenderHostRegistry::FindPotentialHostToActivate(
    NavigationRequest& navigation_request) {
  TRACE_EVENT2(
      "navigation", "PrerenderHostRegistry::FindPotentialHostToActivate",
      "navigation_url", navigation_request.GetURL().spec(), "render_frame_host",
      navigation_request.frame_tree_node()->current_frame_host());

  // Disallow activation when the navigation is for a nested browsing context
  // (e.g., iframes, fenced frames). This is because nested browsing contexts
  // such as iframes are supposed to be created in the parent's browsing context
  // group and can script with the parent, but prerendered pages are created in
  // new browsing context groups.
  //
  // Also, disallow activation when the navigation happens in the prerendering
  // frame tree.
  if (!navigation_request.IsInPrimaryMainFrame()) {
    return FrameTreeNodeId();
  }

  // Collect hosts that can match the navigation request.
  std::vector<PrerenderHost*> matchable_hosts;
  // First, collect hosts that can exactly match or match with the
  // No-Vary-Search header.
  for (const auto& [host_id, host] : prerender_host_by_frame_tree_node_id_) {
    if (host->IsUrlMatch(navigation_request.GetURL())) {
      matchable_hosts.push_back(host.get());
    }
  }
  // Then, collect hosts that can match with the No-Vary-Search hint.
  for (const auto& [host_id, host] : prerender_host_by_frame_tree_node_id_) {
    if (host->IsNoVarySearchHintUrlMatch(navigation_request.GetURL())) {
      matchable_hosts.push_back(host.get());
    }
  }
  if (matchable_hosts.empty()) {
    return FrameTreeNodeId();
  }
  // Use the first match. This prioritizes the exact match or No-Vary-Search
  // header match than No-Vary-Search hint match.
  PrerenderHost* host = *matchable_hosts.begin();

  base::UmaHistogramCounts100(
      "Prerender.Experimental.MatchableHostCountOnActivation",
      matchable_hosts.size());

  // Disallow activation when the navigation URL has an effective URL like
  // hosted apps and NTP.
  if (SiteInstanceImpl::HasEffectiveURL(web_contents()->GetBrowserContext(),
                                        navigation_request.GetURL())) {
    CancelHost(host->frame_tree_node_id(),
               PrerenderFinalStatus::kActivationUrlHasEffectiveUrl);
    return FrameTreeNodeId();
  }

  // Cannot activate if prerendering navigation has not started yet.
  if (!host->GetInitialNavigationId().has_value()) {
    CancelHost(host->frame_tree_node_id(),
               PrerenderFinalStatus::kActivatedBeforeStarted);
    return FrameTreeNodeId();
  }

  return CanNavigationActivateHost(navigation_request, *host)
             ? host->frame_tree_node_id()
             : FrameTreeNodeId();
}

FrameTreeNodeId PrerenderHostRegistry::ReserveHostToActivate(
    NavigationRequest& navigation_request,
    FrameTreeNodeId expected_host_id) {
  RenderFrameHostImpl* render_frame_host =
      navigation_request.frame_tree_node()->current_frame_host();
  TRACE_EVENT2("navigation", "PrerenderHostRegistry::ReserveHostToActivate",
               "navigation_url", navigation_request.GetURL().spec(),
               "render_frame_host", render_frame_host);

  // These should be ensured in `FindPotentialHostToActivate()`. See the
  // corresponding checks in the function for details.
  CHECK(navigation_request.IsInPrimaryMainFrame());
  CHECK(!SiteInstanceImpl::HasEffectiveURL(web_contents()->GetBrowserContext(),
                                           navigation_request.GetURL()));

  // Choose the host that NavigationRequest expects.
  //
  // Note that other prerendered pages may match this NavigationRequest but
  // we shouldn't activate them. NavigationRequest makes sure that the
  // expected prerendered page is ready for activation by waiting for
  // PrerenderCommitDeferringCondition before this point, while the new
  // matched pages may not be ready for activation yet.
  auto it = prerender_host_by_frame_tree_node_id_.find(expected_host_id);
  if (it == prerender_host_by_frame_tree_node_id_.end()) {
    return FrameTreeNodeId();
  }

  PrerenderHost& host_ref = *it->second;

  // The expected host does not match. This can happen when the host matches
  // based on the No-Vary-Search hint but actually it does not match based on
  // the No-Vary-Search header.
  std::optional<UrlMatchType> match_type =
      host_ref.IsUrlMatch(navigation_request.GetURL());
  if (!match_type.has_value()) {
    return FrameTreeNodeId();
  }

  if (!CanNavigationActivateHost(navigation_request, host_ref)) {
    return FrameTreeNodeId();
  }

  FrameTreeNodeId host_id = host_ref.frame_tree_node_id();

  // Disallow activation when ongoing navigations exist. It can happen when the
  // main frame navigation starts after PrerenderCommitDeferringCondition posts
  // a task to resume activation and before the activation is completed.
  auto& prerender_frame_tree = host_ref.GetPrerenderFrameTree();
  if (prerender_frame_tree.root()->HasNavigation()) {
    CancelHost(host_id,
               PrerenderFinalStatus::kActivatedDuringMainFrameNavigation);
    return FrameTreeNodeId();
  }

  // Remove the host from the map of non-reserved hosts.
  std::unique_ptr<PrerenderHost> host =
      std::move(prerender_host_by_frame_tree_node_id_[host_id]);
  prerender_host_by_frame_tree_node_id_.erase(host_id);
  CHECK_EQ(host_id, host->frame_tree_node_id());
  CHECK(host->IsUrlMatch(navigation_request.GetURL()));

  if (match_type.value() == UrlMatchType::kNoVarySearch) {
    // Count use of No-Vary-Search header in prerender.
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        web_contents()->GetPrimaryMainFrame(),
        blink::mojom::WebFeature::kNoVarySearchPrerender);
  }
  // Reserve the host for activation.
  CHECK(!reserved_prerender_host_);
  reserved_prerender_host_ = std::move(host);

  return host_id;
}

RenderFrameHostImpl* PrerenderHostRegistry::GetRenderFrameHostForReservedHost(
    FrameTreeNodeId frame_tree_node_id) {
  if (!reserved_prerender_host_)
    return nullptr;

  CHECK_EQ(frame_tree_node_id, reserved_prerender_host_->frame_tree_node_id());

  return reserved_prerender_host_->GetPrerenderedMainFrameHost();
}

std::unique_ptr<StoredPage> PrerenderHostRegistry::ActivateReservedHost(
    FrameTreeNodeId frame_tree_node_id,
    NavigationRequest& navigation_request) {
  CHECK(reserved_prerender_host_);
  CHECK_EQ(frame_tree_node_id, reserved_prerender_host_->frame_tree_node_id());

  std::unique_ptr<PrerenderHost> prerender_host =
      std::move(reserved_prerender_host_);
  return prerender_host->Activate(navigation_request);
}

void PrerenderHostRegistry::OnActivationFinished(
    FrameTreeNodeId frame_tree_node_id) {
  // OnActivationFinished() should not be called for non-reserved hosts.
  CHECK(!base::Contains(prerender_host_by_frame_tree_node_id_,
                        frame_tree_node_id));

  if (!reserved_prerender_host_) {
    // The activation finished successfully and has already activated the
    // reserved host.
    return;
  }

  // The activation navigation is cancelled before activating the prerendered
  // page, which means the activation failed.
  CHECK_EQ(frame_tree_node_id, reserved_prerender_host_->frame_tree_node_id());

  // TODO(crbug.com/40243805): Monitor the final status metric and see
  // whether it could be possible.
  ScheduleToDeleteAbandonedHost(
      std::move(reserved_prerender_host_),
      PrerenderCancellationReason(
          PrerenderFinalStatus::kActivationNavigationDestroyedBeforeSuccess));
}

PrerenderHost* PrerenderHostRegistry::FindNonReservedHostById(
    FrameTreeNodeId frame_tree_node_id) {
  auto id_iter = prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (id_iter == prerender_host_by_frame_tree_node_id_.end())
    return nullptr;
  return id_iter->second.get();
}

bool PrerenderHostRegistry::HasReservedHost() const {
  return !!reserved_prerender_host_;
}

std::unique_ptr<WebContentsImpl>
PrerenderHostRegistry::TakePreCreatedWebContentsForNewTabIfExists(
    const mojom::CreateNewWindowParams& create_new_window_params,
    const WebContents::CreateParams& web_contents_create_params) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab));

  // Don't serve a prerendered page if the window needs the opener or is created
  // for non-regular navigations.
  if (!create_new_window_params.opener_suppressed ||
      create_new_window_params.is_form_submission ||
      create_new_window_params.pip_options) {
    return nullptr;
  }

  for (auto& iter : prerender_new_tab_handle_by_frame_tree_node_id_) {
    std::unique_ptr<WebContentsImpl> web_contents =
        iter.second->TakeWebContentsIfAvailable(create_new_window_params,
                                                web_contents_create_params);
    if (web_contents) {
      prerender_new_tab_handle_by_frame_tree_node_id_.erase(iter);
      return web_contents;
    }
  }
  return nullptr;
}

std::vector<FrameTree*> PrerenderHostRegistry::GetPrerenderFrameTrees() {
  std::vector<FrameTree*> result;
  for (auto& i : prerender_host_by_frame_tree_node_id_) {
    result.push_back(&i.second->GetPrerenderFrameTree());
  }
  if (reserved_prerender_host_)
    result.push_back(&reserved_prerender_host_->GetPrerenderFrameTree());

  return result;
}

PrerenderHost* PrerenderHostRegistry::FindHostByUrlForTesting(
    const GURL& prerendering_url) {
  for (auto& iter : prerender_host_by_frame_tree_node_id_) {
    if (iter.second->IsUrlMatch(prerendering_url)) {
      return iter.second.get();
    }
  }
  return nullptr;
}

bool PrerenderHostRegistry::HasNewTabHandleByIdForTesting(
    FrameTreeNodeId frame_tree_node_id) {
  return prerender_new_tab_handle_by_frame_tree_node_id_.contains(
      frame_tree_node_id);
}

void PrerenderHostRegistry::CancelAllHostsForTesting() {
  CHECK(!reserved_prerender_host_)
      << "It is not possible to cancel a reserved host, so they must not exist "
         "when trying to cancel all hosts";

  for (auto& iter : prerender_host_by_frame_tree_node_id_) {
    // Asynchronously delete the prerender host.
    ScheduleToDeleteAbandonedHost(
        std::move(iter.second),
        PrerenderCancellationReason(
            PrerenderFinalStatus::kCancelAllHostsForTesting));
  }

  // After we're done scheduling deletion, clear the map and the pending queue.
  prerender_host_by_frame_tree_node_id_.clear();
  pending_prerenders_.clear();
}

void PrerenderHostRegistry::BackNavigationLikely(
    PreloadingPredictor predictor) {
  if (http_cache_query_loader_) {
    return;
  }

  PreloadingDataImpl* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(web_contents());
  preloading_data->SetIsNavigationInDomainCallback(
      predictor,
      base::BindRepeating(IsNavigationInSessionHistoryPredictorDomain));

  WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
  NavigationControllerImpl& controller = contents->GetController();
  const std::optional<int> target_index = controller.GetIndexForGoBack();

  if (!target_index.has_value()) {
    RecordPrerenderBackNavigationEligibility(
        predictor, PrerenderBackNavigationEligibility::kNoBackEntry, nullptr);
    return;
  }

  NavigationEntryImpl* back_entry = controller.GetEntryAtIndex(*target_index);
  CHECK(back_entry);
  const GURL& back_url = back_entry->GetURL();

  if (controller.GetBackForwardCache()
          .GetOrEvictEntry(back_entry->GetUniqueID())
          .has_value()) {
    RecordPrerenderBackNavigationEligibility(
        predictor, PrerenderBackNavigationEligibility::kBfcacheEntryExists,
        nullptr);
    return;
  }

  PreloadingURLMatchCallback same_url_matcher =
      PreloadingData::GetSameURLMatcher(back_url);
  ukm::SourceId triggered_primary_page_source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  preloading_data->AddPreloadingPrediction(predictor, PreloadingConfidence{100},
                                           same_url_matcher,
                                           triggered_primary_page_source_id);
  PreloadingAttempt* attempt = preloading_data->AddPreloadingAttempt(
      predictor, PreloadingType::kPrerender, same_url_matcher,
      /*planned_max_preloading_type=*/std::nullopt,
      triggered_primary_page_source_id);

  if (back_entry->GetMainFrameDocumentSequenceNumber() ==
      controller.GetLastCommittedEntry()
          ->GetMainFrameDocumentSequenceNumber()) {
    RecordPrerenderBackNavigationEligibility(
        predictor, PrerenderBackNavigationEligibility::kTargetIsSameDocument,
        attempt);
    return;
  }

  if (back_entry->root_node()->frame_entry->method() != "GET") {
    RecordPrerenderBackNavigationEligibility(
        predictor, PrerenderBackNavigationEligibility::kMethodNotGet, attempt);
    return;
  }

  if (prerender_navigation_utils::IsDisallowedHttpResponseCode(
          back_entry->GetHttpStatusCode())) {
    RecordPrerenderBackNavigationEligibility(
        predictor,
        PrerenderBackNavigationEligibility::kTargetIsFailedNavigation, attempt);
    return;
  }

  if (!back_url.SchemeIsHTTPOrHTTPS()) {
    RecordPrerenderBackNavigationEligibility(
        predictor, PrerenderBackNavigationEligibility::kTargetIsNonHttp,
        attempt);
    return;
  }

  // While same site back navigations could potentially be prerendered, doing so
  // would involve more significant compat risk. For now, we consider them
  // ineligible. See https://crbug.com/1422266 .
  if (prerender_navigation_utils::IsSameSite(
          back_url,
          contents->GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
    RecordPrerenderBackNavigationEligibility(
        predictor, PrerenderBackNavigationEligibility::kTargetIsSameSite,
        attempt);
    return;
  }

  // Session history prerendering will reuse the back navigation entry's
  // existing SiteInstance. We can't have a prerendered document share a
  // SiteInstance with related active content (i.e. an active document with the
  // same BrowsingInstance) as that would risk having scripting connections to
  // prerendered documents. So this case is not eligible for prerendering.
  SiteInstanceImpl* entry_site_instance = back_entry->site_instance();
  // `entry_site_instance` could be null in cases such as session restore.
  if (entry_site_instance) {
    const bool current_and_target_related =
        contents->GetSiteInstance()->IsRelatedSiteInstance(entry_site_instance);
    const size_t allowable_related_count = current_and_target_related ? 1u : 0u;
    if (entry_site_instance->GetRelatedActiveContentsCount() >
        allowable_related_count) {
      RecordPrerenderBackNavigationEligibility(
          predictor, PrerenderBackNavigationEligibility::kRelatedActiveContents,
          attempt);
      return;
    }
  }

  // To determine whether the resource for the target entry is in the HTTP
  // cache, we send a "fake" ResourceRequest which only loads from the cache.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      contents->GetPrimaryMainFrame()
          ->GetStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  http_cache_query_loader_ = CreateHttpCacheQueryingResourceLoad(back_url);
  http_cache_query_loader_->DownloadHeadersOnly(
      url_loader_factory.get(),
      base::BindOnce(&PrerenderHostRegistry::OnBackResourceCacheResult,
                     base::Unretained(this), predictor, attempt->GetWeakPtr(),
                     back_url));
}

void PrerenderHostRegistry::OnBackResourceCacheResult(
    PreloadingPredictor predictor,
    base::WeakPtr<PreloadingAttempt> attempt,
    GURL back_url,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  // It's safe to delete the SimpleURLLoader while running the callback that was
  // passed to it. We do so once we're done with it in this method.
  std::unique_ptr<network::SimpleURLLoader> http_cache_query_loader =
      std::move(http_cache_query_loader_);

  if (!http_cache_query_loader->LoadedFromCache()) {
    // If not in the cache, then this cache-only request must have failed.
    CHECK_NE(http_cache_query_loader->NetError(), net::OK);

    RecordPrerenderBackNavigationEligibility(
        predictor, PrerenderBackNavigationEligibility::kNoHttpCacheEntry,
        attempt.get());
    return;
  }

  RecordPrerenderBackNavigationEligibility(
      predictor, PrerenderBackNavigationEligibility::kEligible, attempt.get());

  if (attempt) {
    attempt->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
    // At this point, we are only collecting metrics and not actually
    // prerendering anything.
    attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kNoOp);
  }
}

base::WeakPtr<PrerenderHostRegistry> PrerenderHostRegistry::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PrerenderHostRegistry::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPrerender2MainFrameNavigation)) {
    return;
  }

  // DidStartNavigation is used for monitoring the main frame navigation in a
  // prerendered page so do nothing for other navigations.
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  if (!navigation_request->IsInPrerenderedMainFrame() ||
      navigation_request->IsSameDocument()) {
    return;
  }

  // This navigation is running on the main frame in the prerendered page, so
  // its FrameTree::Delegate should be PrerenderHost.
  auto* prerender_host = static_cast<PrerenderHost*>(
      navigation_request->frame_tree_node()->frame_tree().delegate());
  CHECK(prerender_host);

  prerender_host->DidStartNavigation(navigation_handle);
}

void PrerenderHostRegistry::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  // ReadyToCommitNavigation is used for monitoring the main frame navigation in
  // a prerendered page so do nothing for other navigations.
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  if (!navigation_request->IsInPrerenderedMainFrame() ||
      navigation_request->IsSameDocument()) {
    return;
  }

  // This navigation is running on the main frame in the prerendered page, so
  // its FrameTree::Delegate should be PrerenderHost.
  auto* prerender_host = static_cast<PrerenderHost*>(
      navigation_request->frame_tree_node()->frame_tree().delegate());
  CHECK(prerender_host);

  prerender_host->ReadyToCommitNavigation(navigation_handle);
}

void PrerenderHostRegistry::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);

  if (navigation_request->IsSameDocument())
    return;

  FrameTreeNodeId main_frame_host_id = navigation_request->frame_tree_node()
                                           ->frame_tree()
                                           .root()
                                           ->frame_tree_node_id();
  PrerenderHost* prerender_host = FindNonReservedHostById(main_frame_host_id);
  if (!prerender_host)
    return;

  prerender_host->DidFinishNavigation(navigation_handle);

  if (running_prerender_host_id_ == main_frame_host_id) {
    running_prerender_host_id_ = FrameTreeNodeId();
    StartPrerendering(FrameTreeNodeId());
  }
}

void PrerenderHostRegistry::OnVisibilityChanged(Visibility visibility) {
  // Update the timer for prerendering timeout in the background.
  if (IsBackground(visibility)) {
    if (timeout_timer_for_embedder_.IsRunning() ||
        timeout_timer_for_speculation_rules_.IsRunning()) {
      // Keep the timers which started on a previous visibility change.
      return;
    }
    // Keep a prerendered page alive in the background when its visibility
    // state changes to HIDDEN or OCCLUDED.
    timeout_timer_for_embedder_.SetTaskRunner(GetTimerTaskRunner());
    timeout_timer_for_speculation_rules_.SetTaskRunner(GetTimerTaskRunner());

    // Cancel PrerenderHost in the background when it exceeds a certain
    // amount of time. The timeout differs depending on the trigger type.
    timeout_timer_for_embedder_.Start(
        FROM_HERE, kTimeToLiveInBackgroundForEmbedder,
        base::BindOnce(&PrerenderHostRegistry::CancelHostsForTriggers,
                       base::Unretained(this),
                       std::vector({PreloadingTriggerType::kEmbedder}),
                       PrerenderCancellationReason(
                           PrerenderFinalStatus::kTimeoutBackgrounded)));
    timeout_timer_for_speculation_rules_.Start(
        FROM_HERE, kTimeToLiveInBackgroundForSpeculationRules,
        base::BindOnce(
            &PrerenderHostRegistry::CancelHostsForTriggers,
            base::Unretained(this),
            std::vector(
                {PreloadingTriggerType::kSpeculationRule,
                 PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld}),
            PrerenderCancellationReason(
                PrerenderFinalStatus::kTimeoutBackgrounded)));
  } else {
    // Stop the timer when a prerendered page gets visible to users.
    timeout_timer_for_embedder_.Stop();
    timeout_timer_for_speculation_rules_.Stop();

    // Start the next prerender if needed.
    if (running_prerender_host_id_.is_null()) {
      StartPrerendering(FrameTreeNodeId());
    }
  }
}

void PrerenderHostRegistry::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  CancelAllHosts(
      status == base::TERMINATION_STATUS_PROCESS_CRASHED
          ? PrerenderFinalStatus::kPrimaryMainFrameRendererProcessCrashed
          : PrerenderFinalStatus::kPrimaryMainFrameRendererProcessKilled);
}

bool PrerenderHostRegistry::CanNavigationActivateHost(
    NavigationRequest& navigation_request,
    PrerenderHost& host) {
  RenderFrameHostImpl* render_frame_host =
      navigation_request.frame_tree_node()->current_frame_host();
  TRACE_EVENT2("navigation", "PrerenderHostRegistry::CanNavigationActivateHost",
               "navigation_url", navigation_request.GetURL().spec(),
               "render_frame_host", render_frame_host);

  // Disallow activation when other auxiliary browsing contexts (e.g., pop-up
  // windows) exist in the same browsing context group. This is because these
  // browsing contexts should be able to script each other, but prerendered
  // pages are created in new browsing context groups.
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (site_instance->GetRelatedActiveContentsCount() != 1u) {
    CancelHost(host.frame_tree_node_id(),
               PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts);
    return false;
  }

  // TODO(crbug.com/40249964): Remove the restriction after further
  // investigation and discussion. Disallow activation when the navigation
  // happens in the hidden tab.
  if (web_contents()->GetVisibility() == Visibility::HIDDEN &&
      !IsAllowedToActivateInBackgroundForTesting()) {
    CancelHost(host.frame_tree_node_id(),
               PrerenderFinalStatus::kActivatedInBackground);
    return false;
  }

  {
    PrerenderCancellationReason reason = PrerenderCancellationReason::
        CreateCandidateReasonForActivationParameterMismatch();
    // Compare navigation params from activation with the navigation params
    // from the initial prerender navigation. If they don't match, the
    // navigation should not activate the prerendered page.
    if (!host.AreInitialPrerenderNavigationParamsCompatibleWithNavigation(
            navigation_request, reason)) {
      // TODO(lingqi): We'd better cancel all hosts.

      CancelHost(host.frame_tree_node_id(), reason);
      return false;
    }
  }

  if (!host.IsFramePolicyCompatibleWithPrimaryFrameTree()) {
    CancelHost(host.frame_tree_node_id(),
               PrerenderFinalStatus::kActivationFramePolicyNotCompatible);
    return false;
  }

  // Cancel all the other prerender hosts because we no longer need the other
  // hosts after we determine the host to be activated.
  std::vector<FrameTreeNodeId> cancelled_prerenders;
  for (const auto& [host_id, _] : prerender_host_by_frame_tree_node_id_) {
    if (host_id != host.frame_tree_node_id()) {
      cancelled_prerenders.push_back(host_id);
    }
  }
  if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)) {
    for (const auto& [host_id, _] :
         prerender_new_tab_handle_by_frame_tree_node_id_) {
      cancelled_prerenders.push_back(host_id);
    }
  } else {
    CHECK(prerender_new_tab_handle_by_frame_tree_node_id_.empty());
  }
  CancelHosts(cancelled_prerenders,
              PrerenderCancellationReason(
                  PrerenderFinalStatus::kOtherPrerenderedPageActivated));
  pending_prerenders_.clear();

  return true;
}

void PrerenderHostRegistry::ScheduleToDeleteAbandonedHost(
    std::unique_ptr<PrerenderHost> prerender_host,
    const PrerenderCancellationReason& cancellation_reason) {
  prerender_host->RecordFailedFinalStatus(PassKey(), cancellation_reason);

  // Asynchronously delete the prerender host.
  to_be_deleted_hosts_.push_back(std::move(prerender_host));

  // A task has already been scheduled to delete the abandoned hosts.
  if (to_be_deleted_hosts_.size() > 1) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PrerenderHostRegistry::DeleteAbandonedHosts,
                                weak_factory_.GetWeakPtr()));
}

void PrerenderHostRegistry::DeleteAbandonedHosts() {
  // Swap the vector and let it scope out instead of directly destructing the
  // hosts in the vector, for example, by `to_be_deleted_hosts_.clear()`. This
  // avoids potential cases where a host being deleted indirectly modifies
  // `to_be_deleted_hosts_` while the vector is being cleared up. See
  // https://crbug.com/1431744 for contexts.
  std::vector<std::unique_ptr<PrerenderHost>> hosts;
  to_be_deleted_hosts_.swap(hosts);
}

void PrerenderHostRegistry::NotifyTrigger(const GURL& url) {
  for (Observer& obs : observers_) {
    obs.OnTrigger(url);
  }
}

void PrerenderHostRegistry::NotifyCancel(
    FrameTreeNodeId host_frame_tree_node_id,
    const PrerenderCancellationReason& reason) {
  for (Observer& obs : observers_) {
    obs.OnCancel(host_frame_tree_node_id, reason);
  }
}

PreloadingTriggerType PrerenderHostRegistry::GetPrerenderTriggerType(
    FrameTreeNodeId frame_tree_node_id) {
  CHECK(reserved_prerender_host_);
  CHECK_EQ(reserved_prerender_host_->frame_tree_node_id(), frame_tree_node_id);
  return reserved_prerender_host_->trigger_type();
}

const std::string& PrerenderHostRegistry::GetPrerenderEmbedderHistogramSuffix(
    FrameTreeNodeId frame_tree_node_id) {
  CHECK(reserved_prerender_host_);
  CHECK_EQ(reserved_prerender_host_->frame_tree_node_id(), frame_tree_node_id);
  return reserved_prerender_host_->embedder_histogram_suffix();
}

PrerenderHostRegistry::PrerenderLimitGroup
PrerenderHostRegistry::GetPrerenderLimitGroup(
    PreloadingTriggerType trigger_type,
    std::optional<blink::mojom::SpeculationEagerness> eagerness) {
  switch (trigger_type) {
    case PreloadingTriggerType::kSpeculationRule:
    case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
    case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
      CHECK(eagerness.has_value());
      switch (eagerness.value()) {
        // Separate the limits of speculation rules into two categories: eager,
        // which are triggered immediately after adding the rule, and
        // non-eager(moderate, conservative), which wait for a specific user
        // action to trigger, aiming to apply the appropriate corresponding
        // limits for these attributes.
        case blink::mojom::SpeculationEagerness::kEager:
          return PrerenderLimitGroup::kSpeculationRulesEager;
        case blink::mojom::SpeculationEagerness::kModerate:
        case blink::mojom::SpeculationEagerness::kConservative:
          return PrerenderLimitGroup::kSpeculationRulesNonEager;
      }
    case PreloadingTriggerType::kEmbedder:
      return PrerenderLimitGroup::kEmbedder;
  }
}

int PrerenderHostRegistry::GetHostCountByLimitGroup(
    PrerenderLimitGroup limit_group) {
  int host_count = 0;
  for (const auto& [_, host] : prerender_host_by_frame_tree_node_id_) {
    if (GetPrerenderLimitGroup(host->trigger_type(), host->eagerness()) ==
        limit_group) {
      ++host_count;
    }
  }

  if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)) {
    for (const auto& [_, handle] :
         prerender_new_tab_handle_by_frame_tree_node_id_) {
      if (GetPrerenderLimitGroup(handle->trigger_type(), handle->eagerness()) ==
          limit_group) {
        ++host_count;
      }
    }
  }

  return host_count;
}

bool PrerenderHostRegistry::IsAllowedToStartPrerenderingForTrigger(
    PreloadingTriggerType trigger_type,
    std::optional<blink::mojom::SpeculationEagerness> eagerness) {
  PrerenderLimitGroup limit_group =
      GetPrerenderLimitGroup(trigger_type, eagerness);
  int host_count = GetHostCountByLimitGroup(limit_group);

  // Apply the limit of maximum number of running prerenders per
  // PrerenderLimitGroup.
  switch (limit_group) {
    case PrerenderLimitGroup::kSpeculationRulesEager:
      return host_count < base::GetFieldTrialParamByFeatureAsInt(
                              features::kPrerender2NewLimitAndScheduler,
                              kMaxNumOfRunningSpeculationRulesEagerPrerenders,
                              10);
    case PrerenderLimitGroup::kSpeculationRulesNonEager: {
      int limit_non_eager = base::GetFieldTrialParamByFeatureAsInt(
          features::kPrerender2NewLimitAndScheduler,
          kMaxNumOfRunningSpeculationRulesNonEagerPrerenders, 2);

      // When the limit on non-eager speculation rules is reached, cancel the
      // oldest host to allow a newly incoming trigger to start.
      if (host_count >= limit_non_eager) {
        FrameTreeNodeId oldest_prerender_host_id;

        // Find the oldest non-eager prerender that has not been canceled yet.
        do {
          oldest_prerender_host_id =
              non_eager_prerender_host_id_by_arrival_order_.front();
          non_eager_prerender_host_id_by_arrival_order_.pop_front();
        } while (
            base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)
                ? !prerender_host_by_frame_tree_node_id_.contains(
                      oldest_prerender_host_id) &&
                      !prerender_new_tab_handle_by_frame_tree_node_id_.contains(
                          oldest_prerender_host_id)
                : !prerender_host_by_frame_tree_node_id_.contains(
                      oldest_prerender_host_id));

        CHECK(CancelHost(
            oldest_prerender_host_id,
            PrerenderFinalStatus::kMaxNumOfRunningNonEagerPrerendersExceeded));

        CHECK_LT(GetHostCountByLimitGroup(limit_group), limit_non_eager);
      }

      return true;
    }
    case PrerenderLimitGroup::kEmbedder:
      return host_count < base::GetFieldTrialParamByFeatureAsInt(
                              features::kPrerender2NewLimitAndScheduler,
                              kMaxNumOfRunningEmbedderPrerenders, 2);
  }
}

void PrerenderHostRegistry::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // Ignore the memory pressure event if the memory control is disabled.
  if (!base::FeatureList::IsEnabled(
          blink::features::kPrerender2MemoryControls)) {
    return;
  }

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      CancelAllHosts(PrerenderFinalStatus::kMemoryPressureAfterTriggered);
      break;
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
PrerenderHostRegistry::GetTimerTaskRunner() {
  return timer_task_runner_for_testing_
             ? timer_task_runner_for_testing_
             : base::SingleThreadTaskRunner::GetCurrentDefault();
}

void PrerenderHostRegistry::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  timer_task_runner_for_testing_ = std::move(task_runner);
}

bool PrerenderHostRegistry::PrerenderCanBeStartedWhenInitiatorIsInBackground() {
  // Allow at most 1 prerendering to be started if the initiator page is
  // still in the background.

  // There is a running prerender, so no extra prerender is allowed before
  // this one is finished.
  if (running_prerender_host_id_) {
    return false;
  }

  // There are non-pending prerenders, which have finished the initial
  // navigation and been waiting for activation. Don't start a new prerender.
  if (prerender_host_by_frame_tree_node_id_.size() -
          pending_prerenders_.size() >=
      1) {
    return false;
  }

  // One or more than prerenders for new tab finished or are running. Don't
  // start a new prerender.
  if (prerender_new_tab_handle_by_frame_tree_node_id_.size() >= 1) {
    return false;
  }

  return true;
}

}  // namespace content
