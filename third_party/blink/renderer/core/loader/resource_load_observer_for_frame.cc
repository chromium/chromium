// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource_load_observer_for_frame.h"

#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/alternate_signed_exchange_resource_info.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {

ResourceLoadObserverForFrame::ResourceLoadObserverForFrame(
    DocumentLoader& loader,
    Document& document,
    const ResourceFetcherProperties& fetcher_properties)
    : document_loader_(loader),
      document_(document),
      fetcher_properties_(fetcher_properties) {}
ResourceLoadObserverForFrame::~ResourceLoadObserverForFrame() = default;

void ResourceLoadObserverForFrame::DidStartRequest(
    const FetchParameters& params,
    ResourceType resource_type) {
  // TODO(yhirano): Consider removing ResourceLoadObserver::DidStartRequest
  // completely when we remove V8DOMActivityLogger.
  if (!document_loader_->Archive() && params.Url().IsValid() &&
      !params.IsSpeculativePreload()) {
    V8DOMActivityLogger* activity_logger = nullptr;
    const AtomicString& initiator_name = params.Options().initiator_info.name;
    if (initiator_name == fetch_initiator_type_names::kXmlhttprequest) {
      activity_logger = V8DOMActivityLogger::CurrentActivityLogger();
    } else {
      activity_logger =
          V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld();
    }
    if (activity_logger) {
      Vector<String> argv = {
          Resource::ResourceTypeToString(resource_type, initiator_name),
          params.Url()};
      activity_logger->LogEvent("blinkRequestResource", argv.size(),
                                argv.data());
    }
  }
}

void ResourceLoadObserverForFrame::WillSendRequest(
    uint64_t identifier,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    ResourceType resource_type,
    const FetchInitiatorInfo& initiator_info) {
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  if (redirect_response.IsNull()) {
    // Progress doesn't care about redirects, only notify it when an
    // initial request is sent.
    frame->Loader().Progress().WillStartLoading(identifier, request.Priority());
  }
  probe::WillSendRequest(
      GetProbe(), identifier, document_loader_,
      fetcher_properties_->GetFetchClientSettingsObject().GlobalObjectUrl(),
      request, redirect_response, initiator_info, resource_type);
  if (auto* idleness_detector = frame->GetIdlenessDetector())
    idleness_detector->OnWillSendRequest(document_->Fetcher());
  if (auto* interactive_detector = InteractiveDetector::From(*document_))
    interactive_detector->OnResourceLoadBegin(base::nullopt);
}

void ResourceLoadObserverForFrame::DidChangePriority(
    uint64_t identifier,
    ResourceLoadPriority priority,
    int intra_priority_value) {
  TRACE_EVENT1("devtools.timeline", "ResourceChangePriority", "data",
               inspector_change_resource_priority_event::Data(
                   document_loader_, identifier, priority));
  probe::DidChangeResourcePriority(document_->GetFrame(), document_loader_,
                                   identifier, priority);
}

void ResourceLoadObserverForFrame::DidReceiveResponse(
    uint64_t identifier,
    const ResourceRequest& request,
    const ResourceResponse& response,
    const Resource* resource,
    ResponseSource response_source) {
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  LocalFrameClient* frame_client = frame->Client();
  SubresourceFilter* subresource_filter =
      document_loader_->GetSubresourceFilter();
  if (subresource_filter && resource->GetResourceRequest().IsAdResource())
    subresource_filter->ReportAdRequestId(response.RequestId());

  DCHECK(frame_client);
  if (response.GetCTPolicyCompliance() ==
      ResourceResponse::kCTPolicyDoesNotComply) {
    CountUsage(
        frame->IsMainFrame()
            ? WebFeature::
                  kCertificateTransparencyNonCompliantSubresourceInMainFrame
            : WebFeature::
                  kCertificateTransparencyNonCompliantResourceInSubframe);
  }

  if (response_source == ResponseSource::kFromMemoryCache) {
    ResourceRequest request(resource->GetResourceRequest());

    if (!request.Url().ProtocolIs(url::kDataScheme)) {
      frame_client->DispatchDidLoadResourceFromMemoryCache(request, response);
      frame->GetLocalFrameHostRemote().DidLoadResourceFromMemoryCache(
          request.Url(), String::FromUTF8(request.HttpMethod().Utf8()),
          String::FromUTF8(response.MimeType().Utf8()),
          request.GetRequestDestination());
    }

    // Note: probe::WillSendRequest needs to precede before this probe method.
    probe::MarkResourceAsCached(frame, document_loader_, identifier);
    if (response.IsNull())
      return;
  }

  MixedContentChecker::CheckMixedPrivatePublic(frame,
                                               response.RemoteIPAddress());

  std::unique_ptr<AlternateSignedExchangeResourceInfo> alternate_resource_info;

  // See if this is a prefetch for a SXG.
  if (response.IsSignedExchangeInnerResponse() &&
      resource->GetType() == ResourceType::kLinkPrefetch) {
    CountUsage(WebFeature::kLinkRelPrefetchForSignedExchanges);

    if (RuntimeEnabledFeatures::SignedExchangeSubresourcePrefetchEnabled(
            document_->GetExecutionContext()) &&
        resource->RedirectChainSize() > 0) {
      // See if the outer response (which must be the last response in
      // the redirect chain) had provided alternate links for the prefetch.
      alternate_resource_info =
          AlternateSignedExchangeResourceInfo::CreateIfValid(
              resource->LastResourceResponse().HttpHeaderField(
                  http_names::kLink),
              response.HttpHeaderField(http_names::kLink));
    }
  }

  PreloadHelper::CanLoadResources resource_loading_policy =
      response_source == ResponseSource::kFromMemoryCache
          ? PreloadHelper::kDoNotLoadResources
          : PreloadHelper::kLoadResourcesAndPreconnect;
  PreloadHelper::LoadLinksFromHeader(
      response.HttpHeaderField(http_names::kLink), response.CurrentRequestUrl(),
      *frame, document_, resource_loading_policy, PreloadHelper::kLoadAll,
      nullptr /* viewport_description */, std::move(alternate_resource_info),
      base::OptionalOrNullptr(response.RecursivePrefetchToken()));

  if (response.HasMajorCertificateErrors()) {
    MixedContentChecker::HandleCertificateError(
        response, request.GetRequestContext(),
        MixedContentChecker::DecideCheckModeForPlugin(frame->GetSettings()),
        document_loader_->GetContentSecurityNotifier());
  }

  if (response.IsLegacyTLSVersion()) {
    frame->Loader().ReportLegacyTLSVersion(
        response.CurrentRequestUrl(), true /* is_subresource */,
        resource->GetResourceRequest().IsAdResource());
  }

  frame->Loader().Progress().IncrementProgress(identifier, response);
  probe::DidReceiveResourceResponse(GetProbe(), identifier, document_loader_,
                                    response, resource);
  // It is essential that inspector gets resource response BEFORE console.
  frame->Console().ReportResourceResponseReceived(document_loader_, identifier,
                                                  response);
}

void ResourceLoadObserverForFrame::DidReceiveData(
    uint64_t identifier,
    base::span<const char> chunk) {
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  frame->Loader().Progress().IncrementProgress(identifier, chunk.size());
  probe::DidReceiveData(GetProbe(), identifier, document_loader_, chunk.data(),
                        chunk.size());
}

void ResourceLoadObserverForFrame::DidReceiveTransferSizeUpdate(
    uint64_t identifier,
    int transfer_size_diff) {
  DCHECK_GT(transfer_size_diff, 0);
  probe::DidReceiveEncodedDataLength(GetProbe(), document_loader_, identifier,
                                     transfer_size_diff);
}

void ResourceLoadObserverForFrame::DidDownloadToBlob(uint64_t identifier,
                                                     BlobDataHandle* blob) {
  if (blob) {
    probe::DidReceiveBlob(GetProbe(), identifier, document_loader_, blob);
  }
}

void ResourceLoadObserverForFrame::DidFinishLoading(
    uint64_t identifier,
    base::TimeTicks finish_time,
    int64_t encoded_data_length,
    int64_t decoded_body_length,
    bool should_report_corb_blocking) {
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  frame->Loader().Progress().CompleteProgress(identifier);
  probe::DidFinishLoading(GetProbe(), identifier, document_loader_, finish_time,
                          encoded_data_length, decoded_body_length,
                          should_report_corb_blocking);

  if (auto* interactive_detector = InteractiveDetector::From(*document_)) {
    interactive_detector->OnResourceLoadEnd(finish_time);
  }
  if (IdlenessDetector* idleness_detector = frame->GetIdlenessDetector()) {
    idleness_detector->OnDidLoadResource();
  }
  document_->CheckCompleted();
}

void ResourceLoadObserverForFrame::DidFailLoading(
    const KURL&,
    uint64_t identifier,
    const ResourceError& error,
    int64_t,
    IsInternalRequest is_internal_request) {
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  frame->Loader().Progress().CompleteProgress(identifier);

  probe::DidFailLoading(GetProbe(), identifier, document_loader_, error,
                        frame->GetDevToolsFrameToken());

  // Notification to FrameConsole should come AFTER InspectorInstrumentation
  // call, DevTools front-end relies on this.
  if (!is_internal_request) {
    frame->Console().DidFailLoading(document_loader_, identifier, error);
  }
  if (auto* interactive_detector = InteractiveDetector::From(*document_)) {
    // We have not yet recorded load_finish_time. Pass nullopt here; we will
    // call base::TimeTicks::Now() lazily when we need it.
    interactive_detector->OnResourceLoadEnd(base::nullopt);
  }
  if (IdlenessDetector* idleness_detector = frame->GetIdlenessDetector()) {
    idleness_detector->OnDidLoadResource();
  }
  document_->CheckCompleted();
}

void ResourceLoadObserverForFrame::Trace(Visitor* visitor) const {
  visitor->Trace(document_loader_);
  visitor->Trace(document_);
  visitor->Trace(fetcher_properties_);
  ResourceLoadObserver::Trace(visitor);
}

CoreProbeSink* ResourceLoadObserverForFrame::GetProbe() {
  return probe::ToCoreProbeSink(*document_);
}

void ResourceLoadObserverForFrame::CountUsage(WebFeature feature) {
  document_loader_->GetUseCounterHelper().Count(feature, document_->GetFrame());
}

}  // namespace blink
