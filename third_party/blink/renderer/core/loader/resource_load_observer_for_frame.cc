// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource_load_observer_for_frame.h"

#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/alternate_signed_exchange_resource_info.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"
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
    const FrameOrImportedDocument& frame_or_imported_document,
    const ResourceFetcherProperties& fetcher_properties)
    : frame_or_imported_document_(frame_or_imported_document),
      fetcher_properties_(fetcher_properties) {}
ResourceLoadObserverForFrame::~ResourceLoadObserverForFrame() = default;

void ResourceLoadObserverForFrame::DidStartRequest(
    const FetchParameters& params,
    ResourceType resource_type) {
  // TODO(yhirano): Consider removing ResourceLoadObserver::DidStartRequest
  // completely when we remove V8DOMActivityLogger.
  DocumentLoader* document_loader =
      frame_or_imported_document_->GetDocumentLoader();
  if (document_loader && !document_loader->Archive() &&
      params.Url().IsValid() && !params.IsSpeculativePreload()) {
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
  LocalFrame& frame = frame_or_imported_document_->GetFrame();
  if (redirect_response.IsNull()) {
    // Progress doesn't care about redirects, only notify it when an
    // initial request is sent.
    frame.Loader().Progress().WillStartLoading(identifier, request.Priority());
  }
  DocumentLoader& document_loader =
      frame_or_imported_document_->GetMasterDocumentLoader();
  Document& document = frame_or_imported_document_->GetDocument();
  probe::WillSendRequest(
      GetProbe(), identifier, &document_loader,
      fetcher_properties_->GetFetchClientSettingsObject().GlobalObjectUrl(),
      request, redirect_response, initiator_info, resource_type);
  if (auto* idleness_detector = frame.GetIdlenessDetector())
    idleness_detector->OnWillSendRequest(document.Fetcher());
  if (auto* interactive_detector = InteractiveDetector::From(document))
    interactive_detector->OnResourceLoadBegin(base::nullopt);
}

void ResourceLoadObserverForFrame::DidChangePriority(
    uint64_t identifier,
    ResourceLoadPriority priority,
    int intra_priority_value) {
  DocumentLoader& document_loader =
      frame_or_imported_document_->GetMasterDocumentLoader();
  TRACE_EVENT1("devtools.timeline", "ResourceChangePriority", "data",
               inspector_change_resource_priority_event::Data(
                   &document_loader, identifier, priority));
  probe::DidChangeResourcePriority(&frame_or_imported_document_->GetFrame(),
                                   &document_loader, identifier, priority);
}

void ResourceLoadObserverForFrame::DidReceiveResponse(
    uint64_t identifier,
    const ResourceRequest& request,
    const ResourceResponse& response,
    const Resource* resource,
    ResponseSource response_source) {
  LocalFrame& frame = frame_or_imported_document_->GetFrame();
  DocumentLoader& document_loader =
      frame_or_imported_document_->GetMasterDocumentLoader();
  LocalFrameClient* frame_client = frame.Client();
  SubresourceFilter* subresource_filter =
      document_loader.GetSubresourceFilter();
  if (subresource_filter && resource->GetResourceRequest().IsAdResource())
    subresource_filter->ReportAdRequestId(response.RequestId());

  DCHECK(frame_client);
  if (response.GetCTPolicyCompliance() ==
      ResourceResponse::kCTPolicyDoesNotComply) {
    CountUsage(
        frame.IsMainFrame()
            ? WebFeature::
                  kCertificateTransparencyNonCompliantSubresourceInMainFrame
            : WebFeature::
                  kCertificateTransparencyNonCompliantResourceInSubframe);
  }

  if (response_source == ResponseSource::kFromMemoryCache) {
    frame_client->DispatchDidLoadResourceFromMemoryCache(
        resource->GetResourceRequest(), response);

    // Note: probe::WillSendRequest needs to precede before this probe method.
    probe::MarkResourceAsCached(&frame, &document_loader, identifier);
    if (response.IsNull())
      return;
  }

  MixedContentChecker::CheckMixedPrivatePublic(&frame,
                                               response.RemoteIPAddress());

  std::unique_ptr<AlternateSignedExchangeResourceInfo> alternate_resource_info;
  if (RuntimeEnabledFeatures::SignedExchangeSubresourcePrefetchEnabled(
          &frame_or_imported_document_->GetDocument()) &&
      response.IsSignedExchangeInnerResponse() &&
      resource->GetType() == ResourceType::kLinkPrefetch &&
      resource->LastResourceResponse()) {
    // If this is a prefetch for a SXG, see if the outer response (which must be
    // the last response in the redirect chain) had provided alternate links for
    // the prefetch.
    alternate_resource_info =
        AlternateSignedExchangeResourceInfo::CreateIfValid(
            resource->LastResourceResponse()->HttpHeaderField(
                http_names::kLink),
            response.HttpHeaderField(http_names::kLink));
  }

  PreloadHelper::CanLoadResources resource_loading_policy =
      response_source == ResponseSource::kFromMemoryCache
          ? PreloadHelper::kDoNotLoadResources
          : PreloadHelper::kLoadResourcesAndPreconnect;
  PreloadHelper::LoadLinksFromHeader(
      response.HttpHeaderField(http_names::kLink), response.CurrentRequestUrl(),
      frame, &frame_or_imported_document_->GetDocument(),
      resource_loading_policy, PreloadHelper::kLoadAll,
      base::nullopt /* viewport_description */,
      std::move(alternate_resource_info), response.RecursivePrefetchToken());

  if (response.HasMajorCertificateErrors()) {
    MixedContentChecker::HandleCertificateError(&frame, response,
                                                request.GetRequestContext());
  }

  if (response.IsLegacyTLSVersion()) {
    frame.Loader().ReportLegacyTLSVersion(
        response.CurrentRequestUrl(), true /* is_subresource */,
        resource->GetResourceRequest().IsAdResource());
  }

  frame.Loader().Progress().IncrementProgress(identifier, response);
  probe::DidReceiveResourceResponse(GetProbe(), identifier, &document_loader,
                                    response, resource);
  // It is essential that inspector gets resource response BEFORE console.
  frame.Console().ReportResourceResponseReceived(&document_loader, identifier,
                                                 response);
}

void ResourceLoadObserverForFrame::DidReceiveData(
    uint64_t identifier,
    base::span<const char> chunk) {
  LocalFrame& frame = frame_or_imported_document_->GetFrame();
  DocumentLoader& document_loader =
      frame_or_imported_document_->GetMasterDocumentLoader();
  frame.Loader().Progress().IncrementProgress(identifier, chunk.size());
  probe::DidReceiveData(GetProbe(), identifier, &document_loader, chunk.data(),
                        chunk.size());
}

void ResourceLoadObserverForFrame::DidReceiveTransferSizeUpdate(
    uint64_t identifier,
    int transfer_size_diff) {
  DCHECK_GT(transfer_size_diff, 0);
  DocumentLoader& document_loader =
      frame_or_imported_document_->GetMasterDocumentLoader();
  probe::DidReceiveEncodedDataLength(GetProbe(), &document_loader, identifier,
                                     transfer_size_diff);
}

void ResourceLoadObserverForFrame::DidDownloadToBlob(uint64_t identifier,
                                                     BlobDataHandle* blob) {
  if (blob) {
    probe::DidReceiveBlob(
        GetProbe(), identifier,
        &frame_or_imported_document_->GetMasterDocumentLoader(), blob);
  }
}

void ResourceLoadObserverForFrame::DidFinishLoading(
    uint64_t identifier,
    base::TimeTicks finish_time,
    int64_t encoded_data_length,
    int64_t decoded_body_length,
    bool should_report_corb_blocking) {
  LocalFrame& frame = frame_or_imported_document_->GetFrame();
  DocumentLoader& document_loader =
      frame_or_imported_document_->GetMasterDocumentLoader();
  frame.Loader().Progress().CompleteProgress(identifier);
  probe::DidFinishLoading(GetProbe(), identifier, &document_loader, finish_time,
                          encoded_data_length, decoded_body_length,
                          should_report_corb_blocking);

  Document& document = frame_or_imported_document_->GetDocument();
  if (auto* interactive_detector = InteractiveDetector::From(document)) {
    interactive_detector->OnResourceLoadEnd(finish_time);
  }
  if (LocalFrame* frame = document.GetFrame()) {
    if (IdlenessDetector* idleness_detector = frame->GetIdlenessDetector()) {
      idleness_detector->OnDidLoadResource();
    }
  }
  document.CheckCompleted();
}

void ResourceLoadObserverForFrame::DidFailLoading(
    const KURL&,
    uint64_t identifier,
    const ResourceError& error,
    int64_t,
    IsInternalRequest is_internal_request) {
  LocalFrame& frame = frame_or_imported_document_->GetFrame();
  DocumentLoader& document_loader =
      frame_or_imported_document_->GetMasterDocumentLoader();
  frame.Loader().Progress().CompleteProgress(identifier);
  probe::DidFailLoading(GetProbe(), identifier, &document_loader, error);

  // Notification to FrameConsole should come AFTER InspectorInstrumentation
  // call, DevTools front-end relies on this.
  if (!is_internal_request) {
    frame.Console().DidFailLoading(&document_loader, identifier, error);
  }
  Document& document = frame_or_imported_document_->GetDocument();
  if (auto* interactive_detector = InteractiveDetector::From(document)) {
    // We have not yet recorded load_finish_time. Pass nullopt here; we will
    // call base::TimeTicks::Now() lazily when we need it.
    interactive_detector->OnResourceLoadEnd(base::nullopt);
  }
  if (LocalFrame* frame = document.GetFrame()) {
    if (IdlenessDetector* idleness_detector = frame->GetIdlenessDetector()) {
      idleness_detector->OnDidLoadResource();
    }
  }
  document.CheckCompleted();
}

void ResourceLoadObserverForFrame::Trace(Visitor* visitor) {
  visitor->Trace(frame_or_imported_document_);
  visitor->Trace(fetcher_properties_);
  ResourceLoadObserver::Trace(visitor);
}

CoreProbeSink* ResourceLoadObserverForFrame::GetProbe() {
  return probe::ToCoreProbeSink(
      frame_or_imported_document_->GetFrame().GetDocument());
}

void ResourceLoadObserverForFrame::CountUsage(WebFeature feature) {
  frame_or_imported_document_->GetMasterDocumentLoader()
      .GetUseCounterHelper()
      .Count(feature, &frame_or_imported_document_->GetFrame());
}

}  // namespace blink
