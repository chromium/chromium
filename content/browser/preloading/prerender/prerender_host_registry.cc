// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_host_registry.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/preloading/prerender/devtools_prerender_attempt.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/preloading/prerender/prerender_navigation_utils.h"
#include "content/browser/preloading/prerender/prerender_new_tab_handle.h"
#include "content/browser/preloading/prerender/prerender_trigger_type_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
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

bool DeviceHasEnoughMemoryForPrerender() {
  // This method disallows prerendering on low-end devices if the
  // kPrerender2MemoryControls feature is enabled.
  if (!base::FeatureList::IsEnabled(blink::features::kPrerender2MemoryControls))
    return true;

  // Use the same default threshold as the back/forward cache. See comments in
  // DeviceHasEnoughMemoryForBackForwardCache().
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

bool IsDevToolsOpen(WebContents& web_contents) {
  return DevToolsAgentHost::HasFor(&web_contents);
}

PreloadingEligibility ToEligibility(PrerenderFinalStatus status) {
  switch (status) {
    case PrerenderFinalStatus::kActivated:
    case PrerenderFinalStatus::kDestroyed:
      NOTREACHED_NORETURN();
    case PrerenderFinalStatus::kLowEndDevice:
      return PreloadingEligibility::kLowMemory;
    case PrerenderFinalStatus::kInvalidSchemeRedirect:
    case PrerenderFinalStatus::kInvalidSchemeNavigation:
    case PrerenderFinalStatus::kInProgressNavigation:
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
    case PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded:
    case PrerenderFinalStatus::kCancelAllHostsForTesting:
    case PrerenderFinalStatus::kDidFailLoad:
    case PrerenderFinalStatus::kStop:
    case PrerenderFinalStatus::kSslCertificateError:
    case PrerenderFinalStatus::kLoginAuthRequested:
    case PrerenderFinalStatus::kUaChangeRequiresReload:
    case PrerenderFinalStatus::kBlockedByClient:
    case PrerenderFinalStatus::kMixedContent:
      NOTREACHED_NORETURN();
    case PrerenderFinalStatus::kTriggerBackgrounded:
      return PreloadingEligibility::kHidden;
    case PrerenderFinalStatus::kMemoryLimitExceeded:
    case PrerenderFinalStatus::kFailToGetMemoryUsage:
      NOTREACHED_NORETURN();
    case PrerenderFinalStatus::kDataSaverEnabled:
      return PreloadingEligibility::kDataSaverEnabled;
    case PrerenderFinalStatus::kHasEffectiveUrl:
      return PreloadingEligibility::kHasEffectiveUrl;
    case PrerenderFinalStatus::kActivatedBeforeStarted:
    case PrerenderFinalStatus::kInactivePageRestriction:
    case PrerenderFinalStatus::kStartFailed:
    case PrerenderFinalStatus::kTimeoutBackgrounded:
    case PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation:
      NOTREACHED_NORETURN();
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
      NOTREACHED_NORETURN();
    case PrerenderFinalStatus::kPreloadingDisabled:
      return PreloadingEligibility::kPreloadingDisabled;
    case PrerenderFinalStatus::kBatterySaverEnabled:
      return PreloadingEligibility::kBatterySaverEnabled;
    case PrerenderFinalStatus::kActivatedDuringMainFrameNavigation:
      NOTREACHED_NORETURN();
    case PrerenderFinalStatus::kPreloadingUnsupportedByWebContents:
      return PreloadingEligibility::kPreloadingUnsupportedByWebContents;
    case PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation:
    case PrerenderFinalStatus::kCrossSiteNavigationInMainFrameNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginNavigationNotOptInInMainFrameNavigation:
      NOTREACHED_NORETURN();
    case PrerenderFinalStatus::kMemoryPressureOnTrigger:
      return PreloadingEligibility::kMemoryPressure;
    case PrerenderFinalStatus::kMemoryPressureAfterTriggered:
      NOTREACHED_NORETURN();
    case PrerenderFinalStatus::kPrerenderingDisabledByDevTools:
      return PreloadingEligibility::kPreloadingDisabledByDevTools;
    case PrerenderFinalStatus::kResourceLoadBlockedByClient:
      return PreloadingEligibility::kPreloadingDisabledByDevTools;
    case PrerenderFinalStatus::kSpeculationRuleRemoved:
    case PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts:
      NOTREACHED_NORETURN();
  }

  NOTREACHED_NORETURN();
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
  // TODO(https://crbug.com/1435376): Make this private again.
  void Drop();
  bool IsDropped();

 private:
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

bool PrerenderHostBuilder::IsDropped() {
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

}  // namespace

// Kill-switch controlled by the field trial. When this feature is enabled,
// PrerenderHostRegistry doesn't cancel prerendering even if query about the
// current memory footprint fails. Now this is enabled by default as the query
// frequently fails. Without the memory footprint check, the limit on the number
// of ongoing prerendering requests and memory pressure events should prevent
// excessive memory usage. See https://crbug.com/1444521 for details.
BASE_FEATURE(kPrerender2IgnoreFailureOnMemoryFootprintQuery,
             "Prerender2IgnoreFailureOnMemoryFootprintQuery",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill-switch controlled by the field trial. When this feature is enabled,
// PrerenderHostRegistry doesn't query about the current memory footprint and
// bypasses the memory limit check, while it still checks the limit on the
// number of ongoing prerendering requests and memory pressure events to prevent
// excessive memory usage. See https://crbug.com/1382697 for details.
BASE_FEATURE(kPrerender2BypassMemoryLimitCheck,
             "Prerender2BypassMemoryLimitCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables to introduce the new limit and scheduler for prerender triggers.
// See crbug.com/1464021 for more details.
BASE_FEATURE(kPrerender2NewLimitAndScheduler,
             "Prerender2NewLimitAndScheduler",
             base::FEATURE_DISABLED_BY_DEFAULT);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PrerenderHostRegistry::~PrerenderHostRegistry() {
  // This function is called by WebContentsImpl's dtor, so web_contents() should
  // not be a null ptr at this moment.
  CHECK(web_contents());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

int PrerenderHostRegistry::CreateAndStartHost(
    const PrerenderAttributes& attributes,
    PreloadingAttempt* attempt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  int frame_tree_node_id = RenderFrameHost::kNoFrameTreeNodeId;

  {
    RenderFrameHostImpl* initiator_rfh =
        attributes.IsBrowserInitiated()
            ? nullptr
            : RenderFrameHostImpl::FromFrameToken(
                  attributes.initiator_process_id,
                  attributes.initiator_frame_token.value());

    // Ensure observers are notified that a trigger occurred.
    base::ScopedClosureRunner notify_trigger(
        base::BindOnce(&PrerenderHostRegistry::NotifyTrigger,
                       base::Unretained(this), attributes.prerendering_url));

    auto builder = PrerenderHostBuilder(attempt);

    // We don't know the root cause, but there is a case this is null.
    //
    // TODO(https://crbug.com/1435376): Continue investigation and fix the root
    // cause.
    if (initiator_web_contents.GetDelegate() == nullptr) {
      // Note that return without consuming `builder` is exceptional.
      builder.Drop();
      return RenderFrameHost::kNoFrameTreeNodeId;
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
        return RenderFrameHost::kNoFrameTreeNodeId;
      case PreloadingEligibility::kDataSaverEnabled:
        builder.RejectAsNotEligible(attributes,
                                    PrerenderFinalStatus::kDataSaverEnabled);
        return RenderFrameHost::kNoFrameTreeNodeId;
      case PreloadingEligibility::kBatterySaverEnabled:
        builder.RejectAsNotEligible(attributes,
                                    PrerenderFinalStatus::kBatterySaverEnabled);
        return RenderFrameHost::kNoFrameTreeNodeId;
      case PreloadingEligibility::kPreloadingUnsupportedByWebContents:
        builder.RejectAsNotEligible(
            attributes,
            PrerenderFinalStatus::kPreloadingUnsupportedByWebContents);
        return RenderFrameHost::kNoFrameTreeNodeId;
      default:
        NOTREACHED_NORETURN();
    }

    // Don't prerender when the initiator is in the background and its type is
    // `kEmbedder`, as current implementation doesn't use `pending_prerenders_`
    // when kEmbedder.
    // If the trigger type is speculation rules, nothing should be done here and
    // then prerender host will be created and its id will be enqueued to
    // `pending_prerenders_`. The visibility of the initiator will be considered
    // when trying to pop from `pending_prerenders_` on `StartPrerendering()`.
    if (attributes.trigger_type == PrerenderTriggerType::kEmbedder &&
        initiator_web_contents.GetVisibility() == Visibility::HIDDEN) {
      builder.RejectAsNotEligible(attributes,
                                  PrerenderFinalStatus::kTriggerBackgrounded);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // Don't prerender on low-end devices.
    if (!DeviceHasEnoughMemoryForPrerender()) {
      builder.RejectAsNotEligible(attributes,
                                  PrerenderFinalStatus::kLowEndDevice);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // Don't prerender under critical memory pressure.
    switch (GetCurrentMemoryPressureLevel()) {
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
        break;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
        builder.RejectAsNotEligible(
            attributes, PrerenderFinalStatus::kMemoryPressureOnTrigger);
        return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // Allow prerendering only for same-site. The initiator origin is nullopt
    // when prerendering is initiated by the browser (not by a renderer using
    // Speculation Rules API). In that case, skip this same-site check.
    // TODO(crbug.com/1176054): Support cross-site prerendering.
    if (!attributes.IsBrowserInitiated() &&
        !prerender_navigation_utils::IsSameSite(
            attributes.prerendering_url, attributes.initiator_origin.value())) {
      builder.RejectAsNotEligible(
          attributes,
          PrerenderFinalStatus::kCrossSiteNavigationInInitialNavigation);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // Disallow all pages that have an effective URL like hosted apps and NTP.
    if (SiteInstanceImpl::HasEffectiveURL(
            prerender_web_contents.GetBrowserContext(),
            prerender_web_contents.GetURL())) {
      builder.RejectAsNotEligible(attributes,
                                  PrerenderFinalStatus::kHasEffectiveUrl);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    if (initiator_rfh && initiator_rfh->frame_tree() &&
        !devtools_instrumentation::IsPrerenderAllowed(
            *initiator_rfh->frame_tree())) {
      builder.RejectAsNotEligible(
          attributes, PrerenderFinalStatus::kPrerenderingDisabledByDevTools);
      return RenderFrameHost::kNoFrameTreeNodeId;
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
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // Ignore prerendering requests for the same URL.
    for (auto& iter : prerender_host_by_frame_tree_node_id_) {
      if (iter.second->GetInitialUrl() == attributes.prerendering_url) {
        builder.RejectAsDuplicate();
        return RenderFrameHost::kNoFrameTreeNodeId;
      }
    }

    // TODO(crbug.com/1355151): Enqueue the request exceeding the number limit
    // until the forerunners are cancelled, and suspend starting a new prerender
    // when the number reaches the limit.
    if (!IsAllowedToStartPrerenderingForTrigger(attributes.trigger_type,
                                                attributes.eagerness)) {
      // The reason we don't consider limit exceeded as an ineligibility
      // reason is because we can't replicate the behavior in our other
      // experiment groups for analysis. To prevent this we set
      // TriggeringOutcome to kFailure and look into the failure reason to
      // learn more.
      builder.RejectAsFailure(
          attributes, PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    auto prerender_host = builder.Build(attributes, prerender_web_contents);
    frame_tree_node_id = prerender_host->frame_tree_node_id();

    CHECK(!base::Contains(prerender_host_by_frame_tree_node_id_,
                          frame_tree_node_id));
    prerender_host_by_frame_tree_node_id_[frame_tree_node_id] =
        std::move(prerender_host);
  }

  switch (attributes.trigger_type) {
    case PrerenderTriggerType::kSpeculationRule:
    case PrerenderTriggerType::kSpeculationRuleFromIsolatedWorld:
      pending_prerenders_.push_back(frame_tree_node_id);
      // Start the initial prerendering navigation of the pending request in
      // the head of the queue if there's no running prerender and the initiator
      // is in the foreground.
      if (running_prerender_host_id_ == RenderFrameHost::kNoFrameTreeNodeId) {
        // No running prerender means that either no other prerenders are in the
        // pending queue or the initiator continues to be in the background.
        // Skip starting prerendering in the latter case.
        if (IsBackground(initiator_web_contents.GetVisibility())) {
          break;
        }
        CHECK_EQ(pending_prerenders_.size(), 1u);
        int started_frame_tree_node_id =
            StartPrerendering(RenderFrameHost::kNoFrameTreeNodeId);
        CHECK(started_frame_tree_node_id == frame_tree_node_id ||
              started_frame_tree_node_id ==
                  RenderFrameHost::kNoFrameTreeNodeId);
        frame_tree_node_id = started_frame_tree_node_id;
      }
      break;
    case PrerenderTriggerType::kEmbedder:
      // The prerendering request from embedder should have high-priority
      // because embedder prediction is more likely for the user to visit.
      // Hold the return value of `StartPrerendering` because the requested
      // prerender might be cancelled due to some restrictions and
      // `kNoFrameTreeNodeId` should be returned in that case.
      frame_tree_node_id = StartPrerendering(frame_tree_node_id);
      break;
  }

  return frame_tree_node_id;
}

int PrerenderHostRegistry::CreateAndStartHostForNewTab(
    const PrerenderAttributes& attributes) {
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
  int prerender_host_id = handle->StartPrerendering();
  if (prerender_host_id == RenderFrameHost::kNoFrameTreeNodeId)
    return RenderFrameHost::kNoFrameTreeNodeId;
  prerender_new_tab_handle_by_frame_tree_node_id_[prerender_host_id] =
      std::move(handle);
  return prerender_host_id;
}

int PrerenderHostRegistry::StartPrerendering(int frame_tree_node_id) {
  // TODO(https://crbug.com/1424425): Don't start prerendering if the current
  // memory pressure level is critical, and then retry prerendering when the
  // memory pressure level goes down.

  if (frame_tree_node_id == RenderFrameHost::kNoFrameTreeNodeId) {
    CHECK_EQ(running_prerender_host_id_, RenderFrameHost::kNoFrameTreeNodeId);

    for (auto iter = pending_prerenders_.begin();
         iter != pending_prerenders_.end();) {
      int host_id = *iter;

      // Skip a cancelled request.
      auto found = prerender_host_by_frame_tree_node_id_.find(host_id);
      if (found == prerender_host_by_frame_tree_node_id_.end()) {
        // Remove the cancelled request from the pending queue.
        iter = pending_prerenders_.erase(iter);
        continue;
      }
      PrerenderHost* prerender_host = found->second.get();

      // The initiator WebContents should be alive as it cancels all the
      // prerendering requests during destruction.
      CHECK(prerender_host->initiator_web_contents());

      // Don't start the pending prerender triggered by the background tab.
      if (IsBackground(
              prerender_host->initiator_web_contents()->GetVisibility())) {
        return RenderFrameHost::kNoFrameTreeNodeId;
      }

      // Found the request to run.
      pending_prerenders_.erase(iter);
      frame_tree_node_id = host_id;
      break;
    }

    if (frame_tree_node_id == RenderFrameHost::kNoFrameTreeNodeId) {
      return RenderFrameHost::kNoFrameTreeNodeId;
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
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  switch (prerender_host_by_frame_tree_node_id_[frame_tree_node_id]
              ->trigger_type()) {
    case PrerenderTriggerType::kSpeculationRule:
    case PrerenderTriggerType::kSpeculationRuleFromIsolatedWorld:
      // Check the current memory usage and destroy a prerendering if the entire
      // browser uses excessive memory. This occurs asynchronously.
      DestroyWhenUsingExcessiveMemory(frame_tree_node_id);

      // Update the `running_prerender_host_id` to the starting prerender's id.
      running_prerender_host_id_ = frame_tree_node_id;
      break;
    case PrerenderTriggerType::kEmbedder:
      // We don't check the memory usage for embedder triggered prerenderings
      // for now.

      // `running_prerender_host_id` only tracks the id for speculation rules
      // trigger, so we also don't update it in the case of embedder.
      break;
  }

  RecordPrerenderTriggered(
      prerender_host_by_frame_tree_node_id_[frame_tree_node_id]
          ->initiator_ukm_id());
  return frame_tree_node_id;
}

std::set<int> PrerenderHostRegistry::CancelHosts(
    const std::vector<int>& frame_tree_node_ids,
    const PrerenderCancellationReason& reason) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::CancelHosts",
               "frame_tree_node_ids", frame_tree_node_ids);

  // Cancel must not be requested during activation.
  CHECK(!reserved_prerender_host_);

  std::set<int> cancelled_ids;

  for (int host_id : frame_tree_node_ids) {
    // Look up the id in the non-reserved host map.
    if (auto iter = prerender_host_by_frame_tree_node_id_.find(host_id);
        iter != prerender_host_by_frame_tree_node_id_.end()) {
      if (running_prerender_host_id_ == host_id)
        running_prerender_host_id_ = RenderFrameHost::kNoFrameTreeNodeId;

      // Remove the prerender host from the host map so that it's not used for
      // activation during asynchronous deletion.
      std::unique_ptr<PrerenderHost> prerender_host = std::move(iter->second);
      prerender_host_by_frame_tree_node_id_.erase(iter);

      reason.ReportMetrics(prerender_host->trigger_type(),
                           prerender_host->embedder_histogram_suffix());

      // Asynchronously delete the prerender host.
      ScheduleToDeleteAbandonedHost(std::move(prerender_host), reason);
      cancelled_ids.insert(host_id);
    }

    if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)) {
      // Look up the id in the prerender-in-new-tab handle map.
      if (auto iter =
              prerender_new_tab_handle_by_frame_tree_node_id_.find(host_id);
          iter != prerender_new_tab_handle_by_frame_tree_node_id_.end()) {
        // The host should be driven by PrerenderHostRegistry associated with
        // the new tab.
        CHECK_NE(running_prerender_host_id_, host_id);

        std::unique_ptr<PrerenderNewTabHandle> handle = std::move(iter->second);
        prerender_new_tab_handle_by_frame_tree_node_id_.erase(iter);
        handle->CancelPrerendering(reason);
        cancelled_ids.insert(host_id);
      }
    } else {
      CHECK(prerender_new_tab_handle_by_frame_tree_node_id_.empty());
    }
  }

  // Start another prerender if the running prerender is cancelled.
  if (running_prerender_host_id_ == RenderFrameHost::kNoFrameTreeNodeId) {
    StartPrerendering(RenderFrameHost::kNoFrameTreeNodeId);
  }

  return cancelled_ids;
}

bool PrerenderHostRegistry::CancelHost(int frame_tree_node_id,
                                       PrerenderFinalStatus final_status) {
  return CancelHost(frame_tree_node_id,
                    PrerenderCancellationReason(final_status));
}

bool PrerenderHostRegistry::CancelHost(
    int frame_tree_node_id,
    const PrerenderCancellationReason& reason) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::CancelHost",
               "frame_tree_node_id", frame_tree_node_id);
  std::set<int> cancelled_ids = CancelHosts({frame_tree_node_id}, reason);
  return !cancelled_ids.empty();
}

void PrerenderHostRegistry::CancelHostsForTriggers(
    std::vector<PrerenderTriggerType> trigger_types,
    const PrerenderCancellationReason& reason) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::CancelHostsForTrigger",
               "trigger_type", trigger_types[0]);

  std::vector<int> ids_to_be_deleted;

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

  auto prerender_host_map = std::move(prerender_host_by_frame_tree_node_id_);
  for (auto& iter : prerender_host_map) {
    std::unique_ptr<PrerenderHost> prerender_host = std::move(iter.second);
    ScheduleToDeleteAbandonedHost(std::move(prerender_host), reason);
  }

  if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)) {
    auto prerender_new_tab_handle_map =
        std::move(prerender_new_tab_handle_by_frame_tree_node_id_);
    for (auto& iter : prerender_new_tab_handle_map)
      iter.second->CancelPrerendering(reason);
  } else {
    CHECK(prerender_new_tab_handle_by_frame_tree_node_id_.empty());
  }

  pending_prerenders_.clear();
}

int PrerenderHostRegistry::FindPotentialHostToActivate(
    NavigationRequest& navigation_request) {
  TRACE_EVENT2(
      "navigation", "PrerenderHostRegistry::FindPotentialHostToActivate",
      "navigation_url", navigation_request.GetURL().spec(), "render_frame_host",
      navigation_request.frame_tree_node()->current_frame_host());
  return FindHostToActivateInternal(navigation_request);
}

int PrerenderHostRegistry::ReserveHostToActivate(
    NavigationRequest& navigation_request,
    int expected_host_id) {
  RenderFrameHostImpl* render_frame_host =
      navigation_request.frame_tree_node()->current_frame_host();
  TRACE_EVENT2("navigation", "PrerenderHostRegistry::ReserveHostToActivate",
               "navigation_url", navigation_request.GetURL().spec(),
               "render_frame_host", render_frame_host);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Find an available host for the navigation request.
  int host_id = FindHostToActivateInternal(navigation_request);
  if (host_id == RenderFrameHost::kNoFrameTreeNodeId)
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Check if the host is what the NavigationRequest expects. The host can be
  // different when a trigger page removes the existing prerender and then
  // re-adds a new prerender for the same URL.
  //
  // NavigationRequest makes sure that the prerender is ready for activation by
  // waiting for PrerenderCommitDeferringCondition before this point. Without
  // this check, if the prerender is changed during the period,
  // NavigationRequest may attempt to activate the new prerender that is not
  // ready.
  if (host_id != expected_host_id)
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Disallow activation when ongoing navigations exist. It can happen when the
  // main frame navigation starts after PrerenderCommitDeferringCondition posts
  // a task to resume activation and before the activation is completed.
  auto& prerender_frame_tree = prerender_host_by_frame_tree_node_id_[host_id]
                                   .get()
                                   ->GetPrerenderFrameTree();
  if (prerender_frame_tree.root()->HasNavigation()) {
    CancelHost(host_id,
               PrerenderFinalStatus::kActivatedDuringMainFrameNavigation);
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  // Remove the host from the map of non-reserved hosts.
  std::unique_ptr<PrerenderHost> host =
      std::move(prerender_host_by_frame_tree_node_id_[host_id]);
  prerender_host_by_frame_tree_node_id_.erase(host_id);
  CHECK_EQ(host_id, host->frame_tree_node_id());

  // Reserve the host for activation.
  CHECK(!reserved_prerender_host_);
  reserved_prerender_host_ = std::move(host);

  return host_id;
}

RenderFrameHostImpl* PrerenderHostRegistry::GetRenderFrameHostForReservedHost(
    int frame_tree_node_id) {
  if (!reserved_prerender_host_)
    return nullptr;

  CHECK_EQ(frame_tree_node_id, reserved_prerender_host_->frame_tree_node_id());

  return reserved_prerender_host_->GetPrerenderedMainFrameHost();
}

std::unique_ptr<StoredPage> PrerenderHostRegistry::ActivateReservedHost(
    int frame_tree_node_id,
    NavigationRequest& navigation_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(reserved_prerender_host_);
  CHECK_EQ(frame_tree_node_id, reserved_prerender_host_->frame_tree_node_id());

  std::unique_ptr<PrerenderHost> prerender_host =
      std::move(reserved_prerender_host_);
  return prerender_host->Activate(navigation_request);
}

void PrerenderHostRegistry::OnActivationFinished(int frame_tree_node_id) {
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

  // TODO(https://crbug.com/1378151): Monitor the final status metric and see
  // whether it could be possible.
  ScheduleToDeleteAbandonedHost(
      std::move(reserved_prerender_host_),
      PrerenderCancellationReason(
          PrerenderFinalStatus::kActivationNavigationDestroyedBeforeSuccess));
}

PrerenderHost* PrerenderHostRegistry::FindNonReservedHostById(
    int frame_tree_node_id) {
  auto id_iter = prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (id_iter == prerender_host_by_frame_tree_node_id_.end())
    return nullptr;
  return id_iter->second.get();
}

PrerenderHost* PrerenderHostRegistry::FindReservedHostById(
    int frame_tree_node_id) {
  if (!reserved_prerender_host_)
    return nullptr;

  if (frame_tree_node_id != reserved_prerender_host_->frame_tree_node_id())
    return nullptr;

  return reserved_prerender_host_.get();
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
    if (iter.second->GetInitialUrl() == prerendering_url)
      return iter.second.get();
  }
  for (auto& iter : prerender_new_tab_handle_by_frame_tree_node_id_) {
    PrerenderHost* host = iter.second->GetPrerenderHostForTesting();  // IN-TEST
    if (host && host->GetInitialUrl() == prerendering_url)
      return host;
  }
  return nullptr;
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

  PreloadingData* preloading_data =
      PreloadingData::GetOrCreateForWebContents(web_contents());
  preloading_data->SetIsNavigationInDomainCallback(
      predictor,
      base::BindRepeating(IsNavigationInSessionHistoryPredictorDomain));

  WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
  NavigationControllerImpl& controller = contents->GetController();
  const absl::optional<int> target_index = controller.GetIndexForGoBack();

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
  preloading_data->AddPreloadingPrediction(predictor, /*confidence=*/100,
                                           same_url_matcher);
  PreloadingAttempt* attempt = preloading_data->AddPreloadingAttempt(
      predictor, PreloadingType::kPrerender, same_url_matcher);

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

void PrerenderHostRegistry::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);

  if (navigation_request->IsSameDocument())
    return;

  int main_frame_host_id = navigation_request->frame_tree_node()
                               ->frame_tree()
                               .root()
                               ->frame_tree_node_id();
  PrerenderHost* prerender_host = FindNonReservedHostById(main_frame_host_id);
  if (!prerender_host)
    return;

  prerender_host->DidFinishNavigation(navigation_handle);

  if (running_prerender_host_id_ == main_frame_host_id) {
    running_prerender_host_id_ = RenderFrameHost::kNoFrameTreeNodeId;
    StartPrerendering(RenderFrameHost::kNoFrameTreeNodeId);
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
                       std::vector({PrerenderTriggerType::kEmbedder}),
                       PrerenderCancellationReason(
                           PrerenderFinalStatus::kTimeoutBackgrounded)));
    timeout_timer_for_speculation_rules_.Start(
        FROM_HERE, kTimeToLiveInBackgroundForSpeculationRules,
        base::BindOnce(
            &PrerenderHostRegistry::CancelHostsForTriggers,
            base::Unretained(this),
            std::vector(
                {PrerenderTriggerType::kSpeculationRule,
                 PrerenderTriggerType::kSpeculationRuleFromIsolatedWorld}),
            PrerenderCancellationReason(
                PrerenderFinalStatus::kTimeoutBackgrounded)));
  } else {
    // Stop the timer when a prerendered page gets visible to users.
    timeout_timer_for_embedder_.Stop();
    timeout_timer_for_speculation_rules_.Stop();

    // Start the next prerender if needed.
    if (running_prerender_host_id_ == RenderFrameHost::kNoFrameTreeNodeId) {
      StartPrerendering(RenderFrameHost::kNoFrameTreeNodeId);
    }
  }
}

void PrerenderHostRegistry::ResourceLoadComplete(
    RenderFrameHost* render_frame_host,
    const GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  CHECK(render_frame_host);

  if (render_frame_host->GetLifecycleState() !=
      RenderFrameHost::LifecycleState::kPrerendering) {
    return;
  }

  // This function only handles ERR_BLOCKED_BY_CLIENT error for now.
  if (resource_load_info.net_error != net::Error::ERR_BLOCKED_BY_CLIENT) {
    return;
  }

  // Cancel the corresponding prerender if the resource load is blocked.
  for (auto& [host_id, host] : prerender_host_by_frame_tree_node_id_) {
    if (&render_frame_host->GetPage() !=
        &host->GetPrerenderedMainFrameHost()->GetPage()) {
      continue;
    }
    RecordBlockedByClientResourceType(resource_load_info.request_destination,
                                      host->trigger_type(),
                                      host->embedder_histogram_suffix());
    CancelHost(host_id, PrerenderFinalStatus::kResourceLoadBlockedByClient);
    break;
  }
}

void PrerenderHostRegistry::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  CancelAllHosts(
      status == base::TERMINATION_STATUS_PROCESS_CRASHED
          ? PrerenderFinalStatus::kPrimaryMainFrameRendererProcessCrashed
          : PrerenderFinalStatus::kPrimaryMainFrameRendererProcessKilled);
}

int PrerenderHostRegistry::FindHostToActivateInternal(
    NavigationRequest& navigation_request) {
  RenderFrameHostImpl* render_frame_host =
      navigation_request.frame_tree_node()->current_frame_host();
  TRACE_EVENT2("navigation",
               "PrerenderHostRegistry::FindHostToActivateInternal",
               "navigation_url", navigation_request.GetURL().spec(),
               "render_frame_host", render_frame_host);

  // Disallow activation when the navigation is for a nested browsing context
  // (e.g., iframes, fenced frames). This is because nested browsing contexts
  // such as iframes are supposed to be created in the parent's browsing context
  // group and can script with the parent, but prerendered pages are created in
  // new browsing context groups. And also, we disallow activation when the
  // navigation is for a fenced frame to prevent the communication path from the
  // embedding page to the fenced frame.
  if (!navigation_request.IsInPrimaryMainFrame())
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Disallow activation when the navigation happens in the prerendering frame
  // tree.
  if (navigation_request.IsInPrerenderedMainFrame())
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Find an available host for the navigation URL.
  PrerenderHost* host = nullptr;
  for (const auto& [host_id, it_prerender_host] :
       prerender_host_by_frame_tree_node_id_) {
    if (it_prerender_host->IsUrlMatch(navigation_request.GetURL())) {
      host = it_prerender_host.get();
      break;
    }
  }
  if (!host)
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Disallow activation when other auxiliary browsing contexts (e.g., pop-up
  // windows) exist in the same browsing context group. This is because these
  // browsing contexts should be able to script each other, but prerendered
  // pages are created in new browsing context groups.
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (site_instance->GetRelatedActiveContentsCount() != 1u) {
    CancelHost(host->frame_tree_node_id(),
               PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts);
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  // TODO(crbug.com/1399709): Remove the restriction after further investigation
  // and discussion.
  // Disallow activation when the navigation happens in the hidden tab.
  if (web_contents()->GetVisibility() == Visibility::HIDDEN) {
    CancelHost(host->frame_tree_node_id(),
               PrerenderFinalStatus::kActivatedInBackground);
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  if (!host->GetInitialNavigationId().has_value()) {
    CancelHost(host->frame_tree_node_id(),
               PrerenderFinalStatus::kActivatedBeforeStarted);
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  // Compare navigation params from activation with the navigation params
  // from the initial prerender navigation. If they don't match, the navigation
  // should not activate the prerendered page.
  if (!host->AreInitialPrerenderNavigationParamsCompatibleWithNavigation(
          navigation_request)) {
    // TODO(https://crbug.com/1328365): Report a detailed reason to devtools.
    // Currently users have to check
    // Prerender.Experimental.ActivationNavigationParamsMatch.
    // TODO(lingqi): We'd better cancel all hosts.
    CancelHost(host->frame_tree_node_id(),
               PrerenderFinalStatus::kActivationNavigationParameterMismatch);
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  if (!host->IsFramePolicyCompatibleWithPrimaryFrameTree()) {
    CancelHost(host->frame_tree_node_id(),
               PrerenderFinalStatus::kActivationFramePolicyNotCompatible);
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  // Cancel all the other prerender hosts because we no longer need the other
  // hosts after we determine the host to be activated.
  std::vector<int> cancelled_prerenders;
  for (const auto& [host_id, _] : prerender_host_by_frame_tree_node_id_) {
    if (host_id != host->frame_tree_node_id()) {
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
  CancelHosts(
      cancelled_prerenders,
      PrerenderCancellationReason(PrerenderFinalStatus::kTriggerDestroyed));
  pending_prerenders_.clear();

  return host->frame_tree_node_id();
}

void PrerenderHostRegistry::ScheduleToDeleteAbandonedHost(
    std::unique_ptr<PrerenderHost> prerender_host,
    const PrerenderCancellationReason& cancellation_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
      << "Post tasks to destroy PrerenderHosts on non-ui threads "
         "with reason of "
      << static_cast<int>(cancellation_reason.final_status());
  prerender_host->RecordFailedFinalStatus(PassKey(), cancellation_reason);

  // Asynchronously delete the prerender host.
  to_be_deleted_hosts_.push_back(std::move(prerender_host));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PrerenderHostRegistry::DeleteAbandonedHosts,
                                weak_factory_.GetWeakPtr()));
}

void PrerenderHostRegistry::DeleteAbandonedHosts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Swap the vector and let it scope out instead of directly destructing the
  // hosts in the vector, for example, by `to_be_deleted_hosts_.clear()`. This
  // avoids potential cases where a host being deleted indirectly modifies
  // `to_be_deleted_hosts_` while the vector is being cleared up. See
  // https://crbug.com/1431744 for contexts.
  std::vector<std::unique_ptr<PrerenderHost>> hosts;
  to_be_deleted_hosts_.swap(hosts);
}

void PrerenderHostRegistry::NotifyTrigger(const GURL& url) {
  for (Observer& obs : observers_)
    obs.OnTrigger(url);
}

PrerenderTriggerType PrerenderHostRegistry::GetPrerenderTriggerType(
    int frame_tree_node_id) {
  PrerenderHost* prerender_host = FindReservedHostById(frame_tree_node_id);
  CHECK(prerender_host);

  return prerender_host->trigger_type();
}

const std::string& PrerenderHostRegistry::GetPrerenderEmbedderHistogramSuffix(
    int frame_tree_node_id) {
  PrerenderHost* prerender_host = FindReservedHostById(frame_tree_node_id);
  CHECK(prerender_host);

  return prerender_host->embedder_histogram_suffix();
}

PrerenderHostRegistry::PrerenderLimitGroup
PrerenderHostRegistry::GetPrerenderLimitGroup(
    PrerenderTriggerType trigger_type,
    absl::optional<blink::mojom::SpeculationEagerness> eagerness) {
  CHECK(base::FeatureList::IsEnabled(kPrerender2NewLimitAndScheduler));

  switch (trigger_type) {
    case PrerenderTriggerType::kSpeculationRule:
    case PrerenderTriggerType::kSpeculationRuleFromIsolatedWorld:
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
    case PrerenderTriggerType::kEmbedder:
      return PrerenderLimitGroup::kEmbedder;
  }
}

bool PrerenderHostRegistry::IsAllowedToStartPrerenderingForTrigger(
    PrerenderTriggerType trigger_type,
    absl::optional<blink::mojom::SpeculationEagerness> eagerness) {
  int trigger_type_count = 0;

  PrerenderLimitGroup limit_group;
  if (base::FeatureList::IsEnabled(kPrerender2NewLimitAndScheduler)) {
    limit_group = GetPrerenderLimitGroup(trigger_type, eagerness);
  }

  for (const auto& [_, host] : prerender_host_by_frame_tree_node_id_) {
    if (base::FeatureList::IsEnabled(kPrerender2NewLimitAndScheduler)) {
      if (GetPrerenderLimitGroup(host->trigger_type(), host->eagerness()) ==
          limit_group) {
        ++trigger_type_count;
      }
    } else {
      if (host->trigger_type() == trigger_type) {
        ++trigger_type_count;
      }
    }
  }
  // TODO(crbug.com/1350676): Make this function care about
  // `prerender_new_tab_handle_by_frame_tree_node_id_` as well.

  if (base::FeatureList::IsEnabled(kPrerender2NewLimitAndScheduler)) {
    // Apply the limit of maximum number of running prerenders per
    // PrerenderLimitGroup.
    switch (limit_group) {
      case PrerenderLimitGroup::kSpeculationRulesEager:
        return trigger_type_count <
               base::GetFieldTrialParamByFeatureAsInt(
                   kPrerender2NewLimitAndScheduler,
                   kMaxNumOfRunningSpeculationRulesEagerPrerenders, 10);
      case PrerenderLimitGroup::kSpeculationRulesNonEager:
        return trigger_type_count <
               base::GetFieldTrialParamByFeatureAsInt(
                   kPrerender2NewLimitAndScheduler,
                   kMaxNumOfRunningSpeculationRulesNonEagerPrerenders, 2);
      case PrerenderLimitGroup::kEmbedder:
        return trigger_type_count < base::GetFieldTrialParamByFeatureAsInt(
                                        kPrerender2NewLimitAndScheduler,
                                        kMaxNumOfRunningEmbedderPrerenders, 2);
    }
  }
  switch (trigger_type) {
    case PrerenderTriggerType::kSpeculationRule:
    case PrerenderTriggerType::kSpeculationRuleFromIsolatedWorld:
      // The number of prerenders triggered by speculation rules is limited to
      // a Finch config param.
      return trigger_type_count <
             base::GetFieldTrialParamByFeatureAsInt(
                 blink::features::kPrerender2,
                 blink::features::kPrerender2MaxNumOfRunningSpeculationRules,
                 10);
    case PrerenderTriggerType::kEmbedder:
      // Currently the number of prerenders triggered by an embedder is
      // limited to two.
      return trigger_type_count < 2;
  }
}

void PrerenderHostRegistry::DestroyWhenUsingExcessiveMemory(
    int frame_tree_node_id) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPrerender2MemoryControls)) {
    return;
  }

  if (base::FeatureList::IsEnabled(kPrerender2BypassMemoryLimitCheck)) {
    return;
  }

  // Override the memory restriction when the DevTools is open.
  if (IsDevToolsOpen(*web_contents())) {
    return;
  }

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(
          base::kNullProcessId,
          base::BindOnce(&PrerenderHostRegistry::DidReceiveMemoryDump,
                         weak_factory_.GetWeakPtr(), frame_tree_node_id));
}

void PrerenderHostRegistry::DidReceiveMemoryDump(
    int frame_tree_node_id,
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
  CHECK(
      base::FeatureList::IsEnabled(blink::features::kPrerender2MemoryControls));

  // Override the memory restriction when the DevTools is open.
  if (IsDevToolsOpen(*web_contents())) {
    return;
  }

  // Stop a prerendering or give up checking the memory consumption depending on
  // the feature flag when we can't get the current memory usage.
  if (!success) {
    if (base::FeatureList::IsEnabled(
            kPrerender2IgnoreFailureOnMemoryFootprintQuery)) {
      return;
    }
    CancelHost(frame_tree_node_id, PrerenderFinalStatus::kFailToGetMemoryUsage);
    return;
  }

  int64_t private_footprint_total_kb = 0;
  for (const auto& pmd : dump->process_dumps()) {
    private_footprint_total_kb += pmd.os_dump().private_footprint_kb;
  }

  // The default acceptable percent is 60% of the system memory.
  int acceptable_percent_of_system_memory =
      base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kPrerender2MemoryControls,
          blink::features::
              kPrerender2MemoryAcceptablePercentOfSystemMemoryParamName,
          60);

  // When the current memory usage is higher than
  // `acceptable_percent_of_system_memory` % of the system memory, cancel a
  // prerendering with `frame_tree_node_id`.
  if (private_footprint_total_kb * 1024 >=
      acceptable_percent_of_system_memory * 0.01 *
          base::SysInfo::AmountOfPhysicalMemory()) {
    CancelHost(frame_tree_node_id, PrerenderFinalStatus::kMemoryLimitExceeded);
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

}  // namespace content
