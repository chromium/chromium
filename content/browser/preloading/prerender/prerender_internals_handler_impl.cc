// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_internals_handler_impl.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

namespace {

const char* FinalStatusToString(PrerenderHost::FinalStatus final_status) {
  switch (final_status) {
    case PrerenderHost::FinalStatus::kActivated:
      return "Activated";
    case PrerenderHost::FinalStatus::kDestroyed:
      return "Destroyed";
    case PrerenderHost::FinalStatus::kLowEndDevice:
      return "LowEndDevice";
    case PrerenderHost::FinalStatus::kCrossOriginRedirect:
      return "CrossOriginRedirect";
    case PrerenderHost::FinalStatus::kCrossOriginNavigation:
      return "CrossOriginNavigation";
    case PrerenderHost::FinalStatus::kInvalidSchemeRedirect:
      return "InvalidSchemeRedirect";
    case PrerenderHost::FinalStatus::kInvalidSchemeNavigation:
      return "InvalidSchemeNavigation";
    case PrerenderHost::FinalStatus::kInProgressNavigation:
      return "InProgressNavigation";
    case PrerenderHost::FinalStatus::kNavigationRequestBlockedByCsp:
      return "NavigationRequestBlockedByCsp";
    case PrerenderHost::FinalStatus::kMainFrameNavigation:
      return "MainFrameNavigation";
    case PrerenderHost::FinalStatus::kMojoBinderPolicy:
      return "MojoBinderPolicy";
    case PrerenderHost::FinalStatus::kRendererProcessCrashed:
      return "RendererProcessCrashed";
    case PrerenderHost::FinalStatus::kRendererProcessKilled:
      return "RendererProcessKilled";
    case PrerenderHost::FinalStatus::kDownload:
      return "Download";
    case PrerenderHost::FinalStatus::kTriggerDestroyed:
      return "TriggerDestroyed";
    case PrerenderHost::FinalStatus::kNavigationNotCommitted:
      return "NavigationNotCommitted";
    case PrerenderHost::FinalStatus::kNavigationBadHttpStatus:
      return "NavigationBadHttpStatus";
    case PrerenderHost::FinalStatus::kClientCertRequested:
      return "ClientCertRequested";
    case PrerenderHost::FinalStatus::kNavigationRequestNetworkError:
      return "NavigationRequestNetworkError";
    case PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded:
      return "MaxNumOfRunningPrerendersExceeded";
    case PrerenderHost::FinalStatus::kCancelAllHostsForTesting:
      return "CancelAllHostsForTesting";
    case PrerenderHost::FinalStatus::kDidFailLoad:
      return "DidFailLoad";
    case PrerenderHost::FinalStatus::kStop:
      return "Stop";
    case PrerenderHost::FinalStatus::kSslCertificateError:
      return "SslCertificateError";
    case PrerenderHost::FinalStatus::kLoginAuthRequested:
      return "LoginAuthRequested";
    case PrerenderHost::FinalStatus::kUaChangeRequiresReload:
      return "UaChangeRequiresReload";
    case PrerenderHost::FinalStatus::kBlockedByClient:
      return "BlockedByClient";
    case PrerenderHost::FinalStatus::kAudioOutputDeviceRequested:
      return "AudioOutputDeviceRequested";
    case PrerenderHost::FinalStatus::kMixedContent:
      return "MixedContent";
    case PrerenderHost::FinalStatus::kTriggerBackgrounded:
      return "TriggerBackgrounded";
    case PrerenderHost::FinalStatus::kEmbedderTriggeredAndSameOriginRedirected:
      return "EmbedderTriggeredAndSameOriginRedirected";
    case PrerenderHost::FinalStatus::kEmbedderTriggeredAndCrossOriginRedirected:
      return "EmbedderTriggeredAndCrossOriginRedirected";
    case PrerenderHost::FinalStatus::kMemoryLimitExceeded:
      return "MemoryLimitExceeded";
    case PrerenderHost::FinalStatus::kFailToGetMemoryUsage:
      return "FailToGetMemoryUsage";
    case PrerenderHost::FinalStatus::kDataSaverEnabled:
      return "DataSaverEnabled";
    case PrerenderHost::FinalStatus::kHasEffectiveUrl:
      return "HasEffectiveUrl";
    case PrerenderHost::FinalStatus::kActivatedBeforeStarted:
      return "ActivatedBeforeStarted";
  }
  NOTREACHED();
  return "";
}

const char* GetFinalStatus(PrerenderHost& host) {
  absl::optional<PrerenderHost::FinalStatus> final_status = host.final_status();
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
