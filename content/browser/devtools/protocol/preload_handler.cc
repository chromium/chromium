// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/preload_handler.h"

#include <memory>
#include <utility>

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_preload_storage.h"
#include "content/browser/devtools/protocol/preload.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_config.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/prefetch_service_delegate.h"

namespace content::protocol {

namespace {

Preload::PrerenderFinalStatus PrerenderFinalStatusToProtocol(
    PrerenderFinalStatus feature) {
  switch (feature) {
    case PrerenderFinalStatus::kActivated:
      return Preload::PrerenderFinalStatusEnum::Activated;
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
    case PrerenderFinalStatus::kTriggerUrlHasEffectiveUrl:
      return Preload::PrerenderFinalStatusEnum::TriggerUrlHasEffectiveUrl;
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
    case PrerenderFinalStatus::kPrerenderingDisabledByDevTools:
      return Preload::PrerenderFinalStatusEnum::PrerenderingDisabledByDevTools;
    case PrerenderFinalStatus::kSpeculationRuleRemoved:
      return Preload::PrerenderFinalStatusEnum::SpeculationRuleRemoved;
    case PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts:
      return Preload::PrerenderFinalStatusEnum::
          ActivatedWithAuxiliaryBrowsingContexts;
    case PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded:
      return Preload::PrerenderFinalStatusEnum::
          MaxNumOfRunningEagerPrerendersExceeded;
    case PrerenderFinalStatus::kMaxNumOfRunningNonEagerPrerendersExceeded:
      return Preload::PrerenderFinalStatusEnum::
          MaxNumOfRunningNonEagerPrerendersExceeded;
    case PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded:
      return Preload::PrerenderFinalStatusEnum::
          MaxNumOfRunningEmbedderPrerendersExceeded;
    case PrerenderFinalStatus::kPrerenderingUrlHasEffectiveUrl:
      return Preload::PrerenderFinalStatusEnum::PrerenderingUrlHasEffectiveUrl;
    case PrerenderFinalStatus::kRedirectedPrerenderingUrlHasEffectiveUrl:
      return Preload::PrerenderFinalStatusEnum::
          RedirectedPrerenderingUrlHasEffectiveUrl;
    case PrerenderFinalStatus::kActivationUrlHasEffectiveUrl:
      return Preload::PrerenderFinalStatusEnum::ActivationUrlHasEffectiveUrl;
    case PrerenderFinalStatus::kJavaScriptInterfaceAdded:
      return Preload::PrerenderFinalStatusEnum::JavaScriptInterfaceAdded;
    case PrerenderFinalStatus::kJavaScriptInterfaceRemoved:
      return Preload::PrerenderFinalStatusEnum::JavaScriptInterfaceRemoved;
    case PrerenderFinalStatus::kAllPrerenderingCanceled:
      return Preload::PrerenderFinalStatusEnum::AllPrerenderingCanceled;
    case PrerenderFinalStatus::kWindowClosed:
      return Preload::PrerenderFinalStatusEnum::WindowClosed;
    case PrerenderFinalStatus::kSlowNetwork:
      return Preload::PrerenderFinalStatusEnum::SlowNetwork;
    case PrerenderFinalStatus::kOtherPrerenderedPageActivated:
      return Preload::PrerenderFinalStatusEnum::OtherPrerenderedPageActivated;
    case PrerenderFinalStatus::kV8OptimizerDisabled:
      return Preload::PrerenderFinalStatusEnum::V8OptimizerDisabled;
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

Preload::PrefetchStatus PrefetchStatusToProtocol(PrefetchStatus status) {
  switch (status) {
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
      return Preload::PrefetchStatusEnum::PrefetchNotUsedProbeFailed;
    case PrefetchStatus::kPrefetchNotStarted:
      return Preload::PrefetchStatusEnum::PrefetchNotStarted;
    case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
      return Preload::PrefetchStatusEnum::PrefetchNotEligibleUserHasCookies;
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
      return Preload::PrefetchStatusEnum::
          PrefetchNotEligibleUserHasServiceWorker;
    case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
      return Preload::PrefetchStatusEnum::PrefetchNotEligibleSchemeIsNotHttps;
    case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
      return Preload::PrefetchStatusEnum::
          PrefetchNotEligibleNonDefaultStoragePartition;
    case PrefetchStatus::kPrefetchNotFinishedInTime:
      return Preload::PrefetchStatusEnum::PrefetchNotFinishedInTime;
    case PrefetchStatus::kPrefetchFailedNetError:
      return Preload::PrefetchStatusEnum::PrefetchFailedNetError;
    case PrefetchStatus::kPrefetchFailedNon2XX:
      return Preload::PrefetchStatusEnum::PrefetchFailedNon2XX;
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
      return Preload::PrefetchStatusEnum::PrefetchFailedMIMENotSupported;
    case PrefetchStatus::kPrefetchSuccessful:
      return Preload::PrefetchStatusEnum::PrefetchSuccessfulButNotUsed;
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
      return Preload::PrefetchStatusEnum::PrefetchIneligibleRetryAfter;
    case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
      return Preload::PrefetchStatusEnum::PrefetchProxyNotAvailable;
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
      return Preload::PrefetchStatusEnum::PrefetchIsPrivacyDecoy;
    case PrefetchStatus::kPrefetchIsStale:
      return Preload::PrefetchStatusEnum::PrefetchIsStale;
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
      return Preload::PrefetchStatusEnum::PrefetchNotUsedCookiesChanged;
    case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
      return Preload::PrefetchStatusEnum::PrefetchNotEligibleHostIsNonUnique;
    case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
      return Preload::PrefetchStatusEnum::PrefetchNotEligibleDataSaverEnabled;
    case PrefetchStatus::kPrefetchIneligibleExistingProxy:
      return Preload::PrefetchStatusEnum::PrefetchNotEligibleExistingProxy;
    case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
      return Preload::PrefetchStatusEnum::PrefetchNotEligiblePreloadingDisabled;
    case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
      return Preload::PrefetchStatusEnum::
          PrefetchNotEligibleBatterySaverEnabled;
    case PrefetchStatus::kPrefetchHeldback:
      return Preload::PrefetchStatusEnum::PrefetchHeldback;
    case PrefetchStatus::kPrefetchAllowed:
      return Preload::PrefetchStatusEnum::PrefetchAllowed;
    case PrefetchStatus::kPrefetchResponseUsed:
      return Preload::PrefetchStatusEnum::PrefetchResponseUsed;
    case PrefetchStatus::kPrefetchFailedInvalidRedirect:
      return Preload::PrefetchStatusEnum::PrefetchFailedInvalidRedirect;
    case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
      return Preload::PrefetchStatusEnum::PrefetchFailedIneligibleRedirect;
    case PrefetchStatus::
        kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
      return Preload::PrefetchStatusEnum::
          PrefetchNotEligibleSameSiteCrossOriginPrefetchRequiredProxy;
    case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
      return Preload::PrefetchStatusEnum::PrefetchEvictedAfterCandidateRemoved;
    case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
      return Preload::PrefetchStatusEnum::PrefetchEvictedForNewerPrefetch;
  }
}

bool PreloadingTriggeringOutcomeSupportedByPrefetch(
    PreloadingTriggeringOutcome feature) {
  // TODO(crbug.com/40246462): revisit the unsupported cases call sites to make
  // sure that either they are covered by other CDPs or they are included by the
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
  // TODO(crbug.com/40246462): revisit the unsupported cases call sites to make
  // sure that either they are covered by other CDPs or they are included by the
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

std::optional<protocol::Preload::SpeculationTargetHint>
GetProtocolSpeculationTargetHint(
    std::optional<blink::mojom::SpeculationTargetHint> target_hint) {
  if (!target_hint.has_value()) {
    return std::nullopt;
  }
  switch (target_hint.value()) {
    case blink::mojom::SpeculationTargetHint::kNoHint:
      return std::nullopt;
    case blink::mojom::SpeculationTargetHint::kBlank:
      return protocol::Preload::SpeculationTargetHintEnum::Blank;
    case blink::mojom::SpeculationTargetHint::kSelf:
      return protocol::Preload::SpeculationTargetHintEnum::Self;
  }
}

}  // namespace

PreloadHandler::PreloadHandler()
    : DevToolsDomainHandler(Preload::Metainfo::domainName) {}

PreloadHandler::~PreloadHandler() = default;

// static
std::vector<PreloadHandler*> PreloadHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<PreloadHandler>(Preload::Metainfo::domainName);
}

void PreloadHandler::DidUpdatePrefetchStatus(
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const std::string& initiating_frame_id,
    const GURL& prefetch_url,
    PreloadingTriggeringOutcome status,
    PrefetchStatus prefetch_status,
    const std::string& request_id) {
  if (!enabled_) {
    return;
  }

  auto preloading_attempt_key =
      protocol::Preload::PreloadingAttemptKey::Create()
          .SetLoaderId(initiator_devtools_navigation_token.ToString())
          .SetAction(Preload::SpeculationActionEnum::Prefetch)
          .SetUrl(prefetch_url.spec())
          .Build();
  if (PreloadingTriggeringOutcomeSupportedByPrefetch(status)) {
    frontend_->PrefetchStatusUpdated(
        std::move(preloading_attempt_key), initiating_frame_id,
        prefetch_url.spec(), PreloadingTriggeringOutcomeToProtocol(status),
        PrefetchStatusToProtocol(prefetch_status), request_id);
  }
}

void PreloadHandler::DidUpdatePrerenderStatus(
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const GURL& prerender_url,
    std::optional<blink::mojom::SpeculationTargetHint> target_hint,
    PreloadingTriggeringOutcome status,
    std::optional<PrerenderFinalStatus> prerender_status,
    std::optional<std::string> disallowed_mojo_interface,
    const std::vector<PrerenderMismatchedHeaders>* mismatched_headers) {
  if (!enabled_) {
    return;
  }

  auto preloading_attempt_key =
      protocol::Preload::PreloadingAttemptKey::Create()
          .SetLoaderId(initiator_devtools_navigation_token.ToString())
          .SetAction(Preload::SpeculationActionEnum::Prerender)
          .SetUrl(prerender_url.spec())
          .Build();
  std::optional<protocol::Preload::SpeculationTargetHint> protocol_target_hint =
      GetProtocolSpeculationTargetHint(target_hint);
  if (protocol_target_hint.has_value()) {
    preloading_attempt_key->SetTargetHint(protocol_target_hint.value());
  }
  Maybe<Preload::PrerenderFinalStatus> protocol_prerender_status =
      prerender_status.has_value()
          ? PrerenderFinalStatusToProtocol(prerender_status.value())
          : Maybe<Preload::PrerenderFinalStatus>();
  Maybe<std::string> protocol_disallowed_mojo_interface =
      disallowed_mojo_interface.has_value()
          ? Maybe<std::string>(disallowed_mojo_interface.value())
          : Maybe<std::string>();
  Maybe<protocol::Array<protocol::Preload::PrerenderMismatchedHeaders>>
      maybe_mismatched_headers;
  if (mismatched_headers) {
    auto mismatched_headers_internal = std::make_unique<
        protocol::Array<protocol::Preload::PrerenderMismatchedHeaders>>();

    for (const auto& mismatched_headers_it : *mismatched_headers) {
      auto protocol_mismatched_headers =
          protocol::Preload::PrerenderMismatchedHeaders::Create()
              .SetHeaderName(mismatched_headers_it.header_name)
              .Build();
      if (mismatched_headers_it.initial_value) {
        protocol_mismatched_headers->SetInitialValue(
            mismatched_headers_it.initial_value.value());
      }
      if (mismatched_headers_it.activation_value) {
        protocol_mismatched_headers->SetActivationValue(
            mismatched_headers_it.activation_value.value());
      }
      mismatched_headers_internal->push_back(
          std::move(protocol_mismatched_headers));
    }
    maybe_mismatched_headers = std::move(mismatched_headers_internal);
  }

  if (PreloadingTriggeringOutcomeSupportedByPrerender(status)) {
    frontend_->PrerenderStatusUpdated(
        std::move(preloading_attempt_key),
        PreloadingTriggeringOutcomeToProtocol(status),
        std::move(protocol_prerender_status),
        std::move(protocol_disallowed_mojo_interface),
        std::move(maybe_mismatched_headers));
  }
}

Response PreloadHandler::Enable() {
  enabled_ = true;
  SendInitialPreloadEnabledState();
  SendCurrentPreloadStatus();
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

void PreloadHandler::SendInitialPreloadEnabledState() {
  if (!host_) {
    return;
  }

  WebContentsImpl* web_contents =
      WebContentsImpl::FromRenderFrameHostImpl(host_);
  PrefetchService* prefetch_service = PrefetchService::GetFromFrameTreeNodeId(
      web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId());

  if (!prefetch_service || !prefetch_service->GetPrefetchServiceDelegate()) {
    return;
  }

  auto* delegate = prefetch_service->GetPrefetchServiceDelegate();
  auto& config = PreloadingConfig::GetInstance();

  // TODO(crbug.com/40246462): Add more grainularity to
  // PreloadingEligibility to distinguish PreloadHoldback and
  // DisabledByPreference for PreloadingEligibility::kPreloadingDisabled.
  // Use more general method to check status of Preloading instead of
  // relying on PrefetchService.
  frontend_->PreloadEnabledStateUpdated(
      !delegate->IsPreloadingPrefEnabled(), delegate->IsDataSaverEnabled(),
      delegate->IsBatterySaverEnabled(),
      // Keep them in alphabetical order.
      config.ShouldHoldback(
          PreloadingType::kPrefetch,
          content::content_preloading_predictor::kSpeculationRules),
      config.ShouldHoldback(
          PreloadingType::kPrerender,
          content::content_preloading_predictor::kSpeculationRules));
}

void PreloadHandler::SendCurrentPreloadStatus() {
  if (!host_) {
    return;
  }

  std::vector<RenderFrameHostImpl*> documents_in_local_subtree;
  RenderFrameHostImpl* root = host_;
  host_->ForEachRenderFrameHostWithAction(
      [&documents_in_local_subtree, root](
          RenderFrameHostImpl* rfh) -> RenderFrameHost::FrameIterationAction {
        if (rfh != root &&
            RenderFrameDevToolsAgentHost::ShouldCreateDevToolsForHost(rfh)) {
          return RenderFrameHost::FrameIterationAction::kSkipChildren;
        }
        documents_in_local_subtree.push_back(rfh);
        return RenderFrameHost::FrameIterationAction::kContinue;
      });

  for (RenderFrameHostImpl* document : documents_in_local_subtree) {
    auto* preload_storage =
        DevToolsPreloadStorage::GetForCurrentDocument(document);
    if (!preload_storage) {
      continue;
    }

    const base::UnguessableToken initiator_devtools_navigation_token =
        document->GetDevToolsNavigationToken().value();
    const std::string initiating_frame_id =
        document->GetDevToolsFrameToken().ToString();
    for (const auto& [key, data] : preload_storage->prefetch_data_map()) {
      DidUpdatePrefetchStatus(
          initiator_devtools_navigation_token, initiating_frame_id,
          /*prefetch_url=*/key, data.outcome, data.status, data.request_id);
    }
    for (const auto& [key, data] : preload_storage->prerender_data_map()) {
      DidUpdatePrerenderStatus(
          initiator_devtools_navigation_token, /*prerender_url=*/key.first,
          /*target_hint=*/key.second, data.outcome, data.status,
          data.disallowed_mojo_interface,
          data.mismatched_headers.empty() ? nullptr : &data.mismatched_headers);
    }
  }
}

}  // namespace content::protocol
