// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/preload_handler.h"

#include <algorithm>
#include <utility>

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/prefetch_service_delegate.h"

namespace content::protocol {

Preload::PrerenderFinalStatus PrerenderFinalStatusToProtocol(
    PrerenderFinalStatus feature) {
  switch (feature) {
    case PrerenderFinalStatus::kActivated:
      return Preload::PrerenderFinalStatusEnum::Activated;
    case PrerenderFinalStatus::kAudioOutputDeviceRequested:
      return Preload::PrerenderFinalStatusEnum::AudioOutputDeviceRequested;
    case PrerenderFinalStatus::kBlockedByClient:
      return Preload::PrerenderFinalStatusEnum::BlockedByClient;
    case PrerenderFinalStatus::kCancelAllHostsForTesting:
      return Preload::PrerenderFinalStatusEnum::CancelAllHostsForTesting;
    case PrerenderFinalStatus::kClientCertRequested:
      return Preload::PrerenderFinalStatusEnum::ClientCertRequested;
    case PrerenderFinalStatus::kDataSaverEnabled:
      return Preload::PrerenderFinalStatusEnum::DataSaverEnabled;
    case PrerenderFinalStatus::kDestroyed:
      return Preload::PrerenderFinalStatusEnum::Destroyed;
    case PrerenderFinalStatus::kDidFailLoad:
      return Preload::PrerenderFinalStatusEnum::DidFailLoad;
    case PrerenderFinalStatus::kDownload:
      return Preload::PrerenderFinalStatusEnum::Download;
    case PrerenderFinalStatus::kEmbedderTriggeredAndCrossOriginRedirected:
      return Preload::PrerenderFinalStatusEnum::
          EmbedderTriggeredAndCrossOriginRedirected;
    case PrerenderFinalStatus::kFailToGetMemoryUsage:
      return Preload::PrerenderFinalStatusEnum::FailToGetMemoryUsage;
    case PrerenderFinalStatus::kInProgressNavigation:
      return Preload::PrerenderFinalStatusEnum::InProgressNavigation;
    case PrerenderFinalStatus::kInvalidSchemeNavigation:
      return Preload::PrerenderFinalStatusEnum::InvalidSchemeNavigation;
    case PrerenderFinalStatus::kInvalidSchemeRedirect:
      return Preload::PrerenderFinalStatusEnum::InvalidSchemeRedirect;
    case PrerenderFinalStatus::kLoginAuthRequested:
      return Preload::PrerenderFinalStatusEnum::LoginAuthRequested;
    case PrerenderFinalStatus::kLowEndDevice:
      return Preload::PrerenderFinalStatusEnum::LowEndDevice;
    case PrerenderFinalStatus::kMainFrameNavigation:
      return Preload::PrerenderFinalStatusEnum::MainFrameNavigation;
    case PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded:
      return Preload::PrerenderFinalStatusEnum::
          MaxNumOfRunningPrerendersExceeded;
    case PrerenderFinalStatus::kMemoryLimitExceeded:
      return Preload::PrerenderFinalStatusEnum::MemoryLimitExceeded;
    case PrerenderFinalStatus::kMixedContent:
      return Preload::PrerenderFinalStatusEnum::MixedContent;
    case PrerenderFinalStatus::kMojoBinderPolicy:
      return Preload::PrerenderFinalStatusEnum::MojoBinderPolicy;
    case PrerenderFinalStatus::kNavigationBadHttpStatus:
      return Preload::PrerenderFinalStatusEnum::NavigationBadHttpStatus;
    case PrerenderFinalStatus::kNavigationNotCommitted:
      return Preload::PrerenderFinalStatusEnum::NavigationNotCommitted;
    case PrerenderFinalStatus::kNavigationRequestBlockedByCsp:
      return Preload::PrerenderFinalStatusEnum::NavigationRequestBlockedByCsp;
    case PrerenderFinalStatus::kNavigationRequestNetworkError:
      return Preload::PrerenderFinalStatusEnum::NavigationRequestNetworkError;
    case PrerenderFinalStatus::kRendererProcessCrashed:
      return Preload::PrerenderFinalStatusEnum::RendererProcessCrashed;
    case PrerenderFinalStatus::kRendererProcessKilled:
      return Preload::PrerenderFinalStatusEnum::RendererProcessKilled;
    case PrerenderFinalStatus::kSslCertificateError:
      return Preload::PrerenderFinalStatusEnum::SslCertificateError;
    case PrerenderFinalStatus::kStop:
      return Preload::PrerenderFinalStatusEnum::Stop;
    case PrerenderFinalStatus::kTriggerBackgrounded:
      return Preload::PrerenderFinalStatusEnum::TriggerBackgrounded;
    case PrerenderFinalStatus::kTriggerDestroyed:
      return Preload::PrerenderFinalStatusEnum::TriggerDestroyed;
    case PrerenderFinalStatus::kUaChangeRequiresReload:
      return Preload::PrerenderFinalStatusEnum::UaChangeRequiresReload;
    case PrerenderFinalStatus::kHasEffectiveUrl:
      return Preload::PrerenderFinalStatusEnum::HasEffectiveUrl;
    case PrerenderFinalStatus::kActivatedBeforeStarted:
      return Preload::PrerenderFinalStatusEnum::ActivatedBeforeStarted;
    case PrerenderFinalStatus::kInactivePageRestriction:
      return Preload::PrerenderFinalStatusEnum::InactivePageRestriction;
    case PrerenderFinalStatus::kStartFailed:
      return Preload::PrerenderFinalStatusEnum::StartFailed;
    case PrerenderFinalStatus::kTimeoutBackgrounded:
      return Preload::PrerenderFinalStatusEnum::TimeoutBackgrounded;
    case PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation:
      return Preload::PrerenderFinalStatusEnum::
          CrossSiteRedirectInInitialNavigation;
    case PrerenderFinalStatus::kCrossSiteNavigationInInitialNavigation:
      return Preload::PrerenderFinalStatusEnum::
          CrossSiteNavigationInInitialNavigation;
    case PrerenderFinalStatus::
        kSameSiteCrossOriginRedirectNotOptInInInitialNavigation:
      return Preload::PrerenderFinalStatusEnum::
          SameSiteCrossOriginRedirectNotOptInInInitialNavigation;
    case PrerenderFinalStatus::
        kSameSiteCrossOriginNavigationNotOptInInInitialNavigation:
      return Preload::PrerenderFinalStatusEnum::
          SameSiteCrossOriginNavigationNotOptInInInitialNavigation;
    case PrerenderFinalStatus::kActivationNavigationParameterMismatch:
      return Preload::PrerenderFinalStatusEnum::
          ActivationNavigationParameterMismatch;
    case PrerenderFinalStatus::kActivatedInBackground:
      return Preload::PrerenderFinalStatusEnum::ActivatedInBackground;
    case PrerenderFinalStatus::kEmbedderHostDisallowed:
      return Preload::PrerenderFinalStatusEnum::EmbedderHostDisallowed;
    case PrerenderFinalStatus::kActivationNavigationDestroyedBeforeSuccess:
      return Preload::PrerenderFinalStatusEnum::
          ActivationNavigationDestroyedBeforeSuccess;
    case PrerenderFinalStatus::kTabClosedByUserGesture:
      return Preload::PrerenderFinalStatusEnum::TabClosedByUserGesture;
    case PrerenderFinalStatus::kTabClosedWithoutUserGesture:
      return Preload::PrerenderFinalStatusEnum::TabClosedWithoutUserGesture;
    case PrerenderFinalStatus::kPrimaryMainFrameRendererProcessCrashed:
      return Preload::PrerenderFinalStatusEnum::
          PrimaryMainFrameRendererProcessCrashed;
    case PrerenderFinalStatus::kPrimaryMainFrameRendererProcessKilled:
      return Preload::PrerenderFinalStatusEnum::
          PrimaryMainFrameRendererProcessKilled;
    case PrerenderFinalStatus::kActivationFramePolicyNotCompatible:
      return Preload::PrerenderFinalStatusEnum::
          ActivationFramePolicyNotCompatible;
    case PrerenderFinalStatus::kPreloadingDisabled:
      return Preload::PrerenderFinalStatusEnum::PreloadingDisabled;
    case PrerenderFinalStatus::kBatterySaverEnabled:
      return Preload::PrerenderFinalStatusEnum::BatterySaverEnabled;
    case PrerenderFinalStatus::kActivatedDuringMainFrameNavigation:
      return Preload::PrerenderFinalStatusEnum::
          ActivatedDuringMainFrameNavigation;
    case PrerenderFinalStatus::kPreloadingUnsupportedByWebContents:
      return Preload::PrerenderFinalStatusEnum::
          PreloadingUnsupportedByWebContents;
    case PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation:
      return Preload::PrerenderFinalStatusEnum::
          CrossSiteRedirectInMainFrameNavigation;
    case PrerenderFinalStatus::kCrossSiteNavigationInMainFrameNavigation:
      return Preload::PrerenderFinalStatusEnum::
          CrossSiteNavigationInMainFrameNavigation;
    case PrerenderFinalStatus::
        kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation:
      return Preload::PrerenderFinalStatusEnum::
          SameSiteCrossOriginRedirectNotOptInInMainFrameNavigation;
    case PrerenderFinalStatus::
        kSameSiteCrossOriginNavigationNotOptInInMainFrameNavigation:
      return Preload::PrerenderFinalStatusEnum::
          SameSiteCrossOriginNavigationNotOptInInMainFrameNavigation;
    case PrerenderFinalStatus::kMemoryPressureOnTrigger:
      return Preload::PrerenderFinalStatusEnum::MemoryPressureOnTrigger;
    case PrerenderFinalStatus::kMemoryPressureAfterTriggered:
      return Preload::PrerenderFinalStatusEnum::MemoryPressureAfterTriggered;
  }
}

Preload::PreloadingStatus PreloadingTriggeringOutcomeToProtocol(
    PreloadingTriggeringOutcome feature) {
  switch (feature) {
    case PreloadingTriggeringOutcome::kRunning:
      return Preload::PreloadingStatusEnum::Running;
    case PreloadingTriggeringOutcome::kReady:
      return Preload::PreloadingStatusEnum::Ready;
    case PreloadingTriggeringOutcome::kSuccess:
      return Preload::PreloadingStatusEnum::Success;
    case PreloadingTriggeringOutcome::kFailure:
      return Preload::PreloadingStatusEnum::Failure;
    case PreloadingTriggeringOutcome::kTriggeredButPending:
      return Preload::PreloadingStatusEnum::Pending;
    case PreloadingTriggeringOutcome::kUnspecified:
    case PreloadingTriggeringOutcome::kDuplicate:
    case PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown:
    case PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender:
    case PreloadingTriggeringOutcome::kNoOp:
      return Preload::PreloadingStatusEnum::NotSupported;
  }
}

bool PreloadingTriggeringOutcomeSupportedByPrefetch(
    PreloadingTriggeringOutcome feature) {
  // TODO(crbug/1384419): revisit the unsupported cases call sites to make sure
  // that either they are covered by other CDPs or they are included by the
  // current CDPs in the future.
  switch (feature) {
    case PreloadingTriggeringOutcome::kRunning:
    case PreloadingTriggeringOutcome::kReady:
    case PreloadingTriggeringOutcome::kSuccess:
    case PreloadingTriggeringOutcome::kFailure:
      return true;
    case PreloadingTriggeringOutcome::kTriggeredButPending:
    case PreloadingTriggeringOutcome::kUnspecified:
    case PreloadingTriggeringOutcome::kDuplicate:
    case PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown:
    case PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender:
    case PreloadingTriggeringOutcome::kNoOp:
      return false;
  }
}

bool PreloadingTriggeringOutcomeSupportedByPrerender(
    PreloadingTriggeringOutcome feature) {
  // TODO(crbug/1384419): revisit the unsupported cases call sites to make sure
  // that either they are covered by other CDPs or they are included by the
  // current CDPs in the future.
  switch (feature) {
    case PreloadingTriggeringOutcome::kRunning:
    case PreloadingTriggeringOutcome::kReady:
    case PreloadingTriggeringOutcome::kSuccess:
    case PreloadingTriggeringOutcome::kFailure:
    case PreloadingTriggeringOutcome::kTriggeredButPending:
      return true;
    case PreloadingTriggeringOutcome::kUnspecified:
    case PreloadingTriggeringOutcome::kDuplicate:
    case PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown:
    case PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender:
    case PreloadingTriggeringOutcome::kNoOp:
      return false;
  }
}

PreloadHandler::PreloadHandler()
    : DevToolsDomainHandler(Preload::Metainfo::domainName) {}

PreloadHandler::~PreloadHandler() = default;

// static
std::vector<PreloadHandler*> PreloadHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<PreloadHandler>(Preload::Metainfo::domainName);
}

void PreloadHandler::DidActivatePrerender(
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const NavigationRequest& nav_request) {
  if (!enabled_) {
    return;
  }
  FrameTreeNode* ftn = nav_request.frame_tree_node();
  std::string initiating_frame_id =
      ftn->current_frame_host()->devtools_frame_token().ToString();
  const GURL& prerendering_url = nav_request.common_params().url;
  last_activated_prerender_initiator_devtools_navigation_token_ =
      initiator_devtools_navigation_token;
  // TODO(crbug/1384419): Handle target_hint.
  auto preloading_attempt_key =
      protocol::Preload::PreloadingAttemptKey::Create()
          .SetLoaderId(initiator_devtools_navigation_token.ToString())
          .SetAction(Preload::SpeculationActionEnum::Prerender)
          .SetUrl(prerendering_url.spec())
          .Build();
  frontend_->PrerenderAttemptCompleted(
      std::move(preloading_attempt_key), initiating_frame_id,
      prerendering_url.spec(), Preload::PrerenderFinalStatusEnum::Activated);
}

void PreloadHandler::DidCancelPrerender(
    const GURL& prerendering_url,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const std::string& initiating_frame_id,
    PrerenderFinalStatus status,
    const std::string& disallowed_api_method) {
  last_activated_prerender_initiator_devtools_navigation_token_.reset();
  if (!enabled_) {
    return;
  }
  DCHECK_NE(status, PrerenderFinalStatus::kActivated);
  Maybe<std::string> opt_disallowed_api_method =
      disallowed_api_method.empty() ? Maybe<std::string>()
                                    : Maybe<std::string>(disallowed_api_method);
  // TODO(crbug/1384419): Handle target_hint.
  auto preloading_attempt_key =
      protocol::Preload::PreloadingAttemptKey::Create()
          .SetLoaderId(initiator_devtools_navigation_token.ToString())
          .SetAction(Preload::SpeculationActionEnum::Prerender)
          .SetUrl(prerendering_url.spec())
          .Build();
  frontend_->PrerenderAttemptCompleted(
      std::move(preloading_attempt_key), initiating_frame_id,
      prerendering_url.spec(), PrerenderFinalStatusToProtocol(status),
      std::move(opt_disallowed_api_method));
}

void PreloadHandler::DidUpdatePrefetchStatus(
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const std::string& initiating_frame_id,
    const GURL& prefetch_url,
    PreloadingTriggeringOutcome status) {
  if (!enabled_) {
    return;
  }
  // TODO(crbug/1384419): Handle target_hint.
  auto preloading_attempt_key =
      protocol::Preload::PreloadingAttemptKey::Create()
          .SetLoaderId(initiator_devtools_navigation_token.ToString())
          .SetAction(Preload::SpeculationActionEnum::Prefetch)
          .SetUrl(prefetch_url.spec())
          .Build();
  if (PreloadingTriggeringOutcomeSupportedByPrefetch(status)) {
    frontend_->PrefetchStatusUpdated(
        std::move(preloading_attempt_key), initiating_frame_id,
        prefetch_url.spec(), PreloadingTriggeringOutcomeToProtocol(status));
  }
}

void PreloadHandler::DidUpdatePrerenderStatus(
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const std::string& initiating_frame_id,
    const GURL& prerender_url,
    PreloadingTriggeringOutcome status) {
  if (!enabled_) {
    return;
  }
  // TODO(crbug/1384419): Handle target_hint.
  auto preloading_attempt_key =
      protocol::Preload::PreloadingAttemptKey::Create()
          .SetLoaderId(initiator_devtools_navigation_token.ToString())
          .SetAction(Preload::SpeculationActionEnum::Prerender)
          .SetUrl(prerender_url.spec())
          .Build();
  if (PreloadingTriggeringOutcomeSupportedByPrerender(status)) {
    frontend_->PrerenderStatusUpdated(
        std::move(preloading_attempt_key), initiating_frame_id,
        prerender_url.spec(), PreloadingTriggeringOutcomeToProtocol(status));
  }
}

Response PreloadHandler::Enable() {
  enabled_ = true;
  RetrievePrerenderActivationFromWebContents();
  SendInitialPreloadEnabledState();
  return Response::FallThrough();
}

Response PreloadHandler::Disable() {
  enabled_ = false;
  return Response::FallThrough();
}

void PreloadHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Preload::Frontend>(dispatcher->channel());
  Preload::Dispatcher::wire(dispatcher, this);
}

void PreloadHandler::SetRenderer(int process_host_id,
                                 RenderFrameHostImpl* frame_host) {
  host_ = frame_host;
}

void PreloadHandler::RetrievePrerenderActivationFromWebContents() {
  if (!host_) {
    return;
  }
  WebContentsImpl* web_contents =
      WebContentsImpl::FromRenderFrameHostImpl(host_);
  if (web_contents->last_navigation_was_prerender_activation_for_devtools() &&
      last_activated_prerender_initiator_devtools_navigation_token_
          .has_value()) {
    std::string frame_token = host_->devtools_frame_token().ToString();
    // TODO(crbug/1384419): Handle target_hint.
    auto preloading_attempt_key =
        protocol::Preload::PreloadingAttemptKey::Create()
            .SetLoaderId(
                last_activated_prerender_initiator_devtools_navigation_token_
                    .value()
                    .ToString())
            .SetAction(Preload::SpeculationActionEnum::Prerender)
            .SetUrl(host_->GetLastCommittedURL().spec())
            .Build();
    frontend_->PrerenderAttemptCompleted(
        std::move(preloading_attempt_key), frame_token,
        host_->GetLastCommittedURL().spec(),
        Preload::PrerenderFinalStatusEnum::Activated);
    last_activated_prerender_initiator_devtools_navigation_token_.reset();
  }
}

void PreloadHandler::SendInitialPreloadEnabledState() {
  if (!host_) {
    return;
  }

  WebContentsImpl* web_contents =
      WebContentsImpl::FromRenderFrameHostImpl(host_);
  PrefetchService* prefetch_service = PrefetchService::GetFromFrameTreeNodeId(
      web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId());

  Preload::PreloadEnabledState state =
      Preload::PreloadEnabledStateEnum::NotSupported;
  if (prefetch_service && prefetch_service->GetPrefetchServiceDelegate()) {
    // TODO(https://crbug.com/1384419): Add more grainularity to
    // PreloadingEligibility to distinguish PreloadHoldback and
    // DisabledByPreference for PreloadingEligibility::kPreloadingDisabled.
    // Use more general method to check status of Preloading instead of
    // relying on PrefetchService.
    switch (prefetch_service->GetPrefetchServiceDelegate()
                ->IsSomePreloadingEnabled()) {
      case PreloadingEligibility::kDataSaverEnabled:
        state = Preload::PreloadEnabledStateEnum::DisabledByDataSaver;
        break;
      case PreloadingEligibility::kBatterySaverEnabled:
        state = Preload::PreloadEnabledStateEnum::DisabledByBatterySaver;
        break;
      case PreloadingEligibility::kPreloadingDisabled:
        state = Preload::PreloadEnabledStateEnum::DisabledByPreference;
        break;
      default:
        state = Preload::PreloadEnabledStateEnum::Enabled;
        break;
    }

    frontend_->PreloadEnabledStateUpdated(state);
  }
}

}  // namespace content::protocol
