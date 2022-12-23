// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_internals_handler_impl.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

namespace {

const char* FinalStatusToString(PrerenderFinalStatus final_status) {
  switch (final_status) {
    case PrerenderFinalStatus::kActivated:
      return "Activated";
    case PrerenderFinalStatus::kDestroyed:
      return "Destroyed";
    case PrerenderFinalStatus::kLowEndDevice:
      return "LowEndDevice";
    case PrerenderFinalStatus::kInvalidSchemeRedirect:
      return "InvalidSchemeRedirect";
    case PrerenderFinalStatus::kInvalidSchemeNavigation:
      return "InvalidSchemeNavigation";
    case PrerenderFinalStatus::kInProgressNavigation:
      return "InProgressNavigation";
    case PrerenderFinalStatus::kNavigationRequestBlockedByCsp:
      return "NavigationRequestBlockedByCsp";
    case PrerenderFinalStatus::kMainFrameNavigation:
      return "MainFrameNavigation";
    case PrerenderFinalStatus::kMojoBinderPolicy:
      return "MojoBinderPolicy";
    case PrerenderFinalStatus::kRendererProcessCrashed:
      return "RendererProcessCrashed";
    case PrerenderFinalStatus::kRendererProcessKilled:
      return "RendererProcessKilled";
    case PrerenderFinalStatus::kDownload:
      return "Download";
    case PrerenderFinalStatus::kTriggerDestroyed:
      return "TriggerDestroyed";
    case PrerenderFinalStatus::kNavigationNotCommitted:
      return "NavigationNotCommitted";
    case PrerenderFinalStatus::kNavigationBadHttpStatus:
      return "NavigationBadHttpStatus";
    case PrerenderFinalStatus::kClientCertRequested:
      return "ClientCertRequested";
    case PrerenderFinalStatus::kNavigationRequestNetworkError:
      return "NavigationRequestNetworkError";
    case PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded:
      return "MaxNumOfRunningPrerendersExceeded";
    case PrerenderFinalStatus::kCancelAllHostsForTesting:
      return "CancelAllHostsForTesting";
    case PrerenderFinalStatus::kDidFailLoad:
      return "DidFailLoad";
    case PrerenderFinalStatus::kStop:
      return "Stop";
    case PrerenderFinalStatus::kSslCertificateError:
      return "SslCertificateError";
    case PrerenderFinalStatus::kLoginAuthRequested:
      return "LoginAuthRequested";
    case PrerenderFinalStatus::kUaChangeRequiresReload:
      return "UaChangeRequiresReload";
    case PrerenderFinalStatus::kBlockedByClient:
      return "BlockedByClient";
    case PrerenderFinalStatus::kAudioOutputDeviceRequested:
      return "AudioOutputDeviceRequested";
    case PrerenderFinalStatus::kMixedContent:
      return "MixedContent";
    case PrerenderFinalStatus::kTriggerBackgrounded:
      return "TriggerBackgrounded";
    case PrerenderFinalStatus::kEmbedderTriggeredAndCrossOriginRedirected:
      return "EmbedderTriggeredAndCrossOriginRedirected";
    case PrerenderFinalStatus::kMemoryLimitExceeded:
      return "MemoryLimitExceeded";
    case PrerenderFinalStatus::kFailToGetMemoryUsage:
      return "FailToGetMemoryUsage";
    case PrerenderFinalStatus::kDataSaverEnabled:
      return "DataSaverEnabled";
    case PrerenderFinalStatus::kHasEffectiveUrl:
      return "HasEffectiveUrl";
    case PrerenderFinalStatus::kActivatedBeforeStarted:
      return "ActivatedBeforeStarted";
    case PrerenderFinalStatus::kInactivePageRestriction:
      return "InactivePageRestriction";
    case PrerenderFinalStatus::kStartFailed:
      return "StartFailed";
    case PrerenderFinalStatus::kTimeoutBackgrounded:
      return "TimeoutBackgrounded";
    case PrerenderFinalStatus::kCrossSiteRedirect:
      return "CrossSiteRedirect";
    case PrerenderFinalStatus::kCrossSiteNavigation:
      return "CrossSiteNavigation";
    case PrerenderFinalStatus::kSameSiteCrossOriginRedirect:
      return "SameSiteCrossOriginRedirect";
    case PrerenderFinalStatus::kSameSiteCrossOriginNavigation:
      return "SameSiteCrossOriginNavigation";
    case PrerenderFinalStatus::kSameSiteCrossOriginRedirectNotOptIn:
      return "SameSiteCrossOriginRedirectNotOptIn";
    case PrerenderFinalStatus::kSameSiteCrossOriginNavigationNotOptIn:
      return "SameSiteCrossOriginNavigationNotOptIn";
    case PrerenderFinalStatus::kActivationNavigationParameterMismatch:
      return "ActivationNavigationParameterMismatch";
    case PrerenderFinalStatus::kEmbedderHostDisallowed:
      return "EmbedderHostDisallowed";
  }
  NOTREACHED();
  return "";
}

const char* GetFinalStatus(PrerenderHost& host) {
  absl::optional<PrerenderFinalStatus> final_status = host.final_status();
  if (final_status) {
    return FinalStatusToString(final_status.value());
  } else {
    return "FinalStatus is not set";
  }
}

}  // namespace

PrerenderInternalsHandlerImpl::PrerenderInternalsHandlerImpl(
    mojo::PendingReceiver<mojom::PrerenderInternalsHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

PrerenderInternalsHandlerImpl::~PrerenderInternalsHandlerImpl() = default;

void PrerenderInternalsHandlerImpl::GetPrerenderInfo(
    GetPrerenderInfoCallback callback) {
  if (!blink::features::IsPrerender2Enabled()) {
    std::move(callback).Run(std::vector<mojom::PrerenderInfoPtr>());
    return;
  }

  std::vector<mojom::PrerenderInfoPtr> infos;
  std::vector<WebContentsImpl*> all_contents =
      WebContentsImpl::GetAllWebContents();

  for (WebContentsImpl* web_contents : all_contents) {
    mojom::PrerenderInfoPtr info = mojom::PrerenderInfo::New();

    PrerenderHostRegistry* prerender_host_registry =
        web_contents->GetPrerenderHostRegistry();

    prerender_host_registry->ForEachPrerenderHost(base::BindRepeating(
        [](mojom::PrerenderInfo* info, PrerenderHost& host) {
          mojom::PrerenderedPageInfoPtr prerendered_page_info =
              mojom::PrerenderedPageInfo::New();
          RenderFrameHostImpl* render_frame_host =
              host.GetPrerenderedMainFrameHost();
          prerendered_page_info->url = render_frame_host->GetLastCommittedURL();
          prerendered_page_info->trigger_page_url = host.initiator_url();
          prerendered_page_info->final_status = GetFinalStatus(host);

          info->prerendered_page_infos.push_back(
              std::move(prerendered_page_info));
        },
        info.get()));

    if (info->prerendered_page_infos.empty()) {
      continue;
    }

    infos.push_back(std::move(info));
  }

  std::move(callback).Run(std::move(infos));
}

}  // namespace content
