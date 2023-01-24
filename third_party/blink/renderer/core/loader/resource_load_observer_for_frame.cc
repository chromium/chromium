// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource_load_observer_for_frame.h"

#include "base/metrics/histogram_macros.h"
#include "base/types/optional_util.h"
#include "components/power_scheduler/power_mode_arbiter.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/cors.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/security/address_space_feature.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {
namespace {

// The list of features which should be reported as deprecated.
constexpr WebFeature kDeprecatedAddressSpaceFeatures[] = {
    WebFeature::kAddressSpacePublicNonSecureContextEmbeddedPrivate,
    WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal,
    WebFeature::kAddressSpacePrivateNonSecureContextEmbeddedLocal,
};

// Returns whether |feature| is deprecated.
bool IsDeprecatedAddressSpaceFeature(WebFeature feature) {
  for (WebFeature entry : kDeprecatedAddressSpaceFeatures) {
    if (feature == entry) {
      return true;
    }
  }
  return false;
}

// Increments the correct kAddressSpace* WebFeature UseCounter corresponding to
// the given |client_frame| performing a subresource fetch |fetch_type| and
// receiving the given |response|.
//
// Does nothing if |client_frame| is nullptr.
void RecordAddressSpaceFeature(LocalFrame* client_frame,
                               const ResourceResponse& response) {
  if (!client_frame) {
    return;
  }

  LocalDOMWindow* window = client_frame->DomWindow();

  if (response.RemoteIPEndpoint().address().IsZero()) {
    UseCounter::Count(window, WebFeature::kPrivateNetworkAccessNullIpAddress);
  }

  absl::optional<WebFeature> feature = AddressSpaceFeature(
      FetchType::kSubresource, response.ClientAddressSpace(),
      window->IsSecureContext(), response.AddressSpace());
  if (!feature.has_value()) {
    return;
  }

  // This WebFeature encompasses all private network requests.
  UseCounter::Count(window,
                    WebFeature::kMixedContentPrivateHostnameInPublicHostname);

  if (IsDeprecatedAddressSpaceFeature(*feature)) {
    Deprecation::CountDeprecation(window, *feature);
  } else {
    UseCounter::Count(window, *feature);
  }
}

}  // namespace

ResourceLoadObserverForFrame::ResourceLoadObserverForFrame(
    DocumentLoader& loader,
    Document& document,
    const ResourceFetcherProperties& fetcher_properties)
    : power_mode_voter_(
          power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
              "PowerModeVoter.ResourceLoads")),
      document_loader_(loader),
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
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    ResourceType resource_type,
    const ResourceLoaderOptions& options,
    RenderBlockingBehavior render_blocking_behavior,
    const Resource* resource) {
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  if (redirect_response.IsNull()) {
    // Progress doesn't care about redirects, only notify it when an
    // initial request is sent.
    frame->Loader().Progress().WillStartLoading(request.InspectorId(),
                                                request.Priority());
  }

  if (!redirect_response.IsNull() &&
      !redirect_response.HttpHeaderField(http_names::kExpectCT).empty()) {
    Deprecation::CountDeprecation(frame->DomWindow(),
                                  mojom::blink::WebFeature::kExpectCTHeader);
  }

  frame->GetAttributionSrcLoader()->MaybeRegisterAttributionHeaders(
      request, redirect_response, resource);

  probe::WillSendRequest(
      GetProbe(), document_loader_,
      fetcher_properties_->GetFetchClientSettingsObject().GlobalObjectUrl(),
      request, redirect_response, options, resource_type,
      render_blocking_behavior, base::TimeTicks::Now());
  if (auto* idleness_detector = frame->GetIdlenessDetector())
    idleness_detector->OnWillSendRequest(document_->Fetcher());
  if (auto* interactive_detector = InteractiveDetector::From(*document_))
    interactive_detector->OnResourceLoadBegin(absl::nullopt);
  UpdatePowerModeVote();
}

void ResourceLoadObserverForFrame::DidChangePriority(
    uint64_t identifier,
    ResourceLoadPriority priority,
    int intra_priority_value) {
  DEVTOOLS_TIMELINE_TRACE_EVENT("ResourceChangePriority",
                                inspector_change_resource_priority_event::Data,
                                document_loader_, identifier, priority);
  probe::DidChangeResourcePriority(document_->GetFrame(), document_loader_,
                                   identifier, priority);
}

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must remain in sync with LinkPrefetchMimeType in
// tools/metrics/histograms/enums.xml.
enum class LinkPrefetchMimeType {
  kUnknown = 0,
  kHtml = 1,
  kScript = 2,
  kStyle = 3,
  kFont = 4,
  kImage = 5,
  kMedia = 6,
  kMaxValue = kMedia,
};

void LogLinkPrefetchMimeTypeHistogram(const AtomicString& mime) {
  // Loosely based on https://mimesniff.spec.whatwg.org/#mime-type-groups.
  // This could be done properly if needed, but this is just to gather
  // approximate data.
  LinkPrefetchMimeType type = LinkPrefetchMimeType::kUnknown;
  if (mime == "text/html" || mime == "application/xhtml+xml") {
    type = LinkPrefetchMimeType::kHtml;
  } else if (mime == "application/javascript" || mime == "text/javascript") {
    type = LinkPrefetchMimeType::kScript;
  } else if (mime == "text/css") {
    type = LinkPrefetchMimeType::kStyle;
  } else if (mime.StartsWith("font/") || mime.StartsWith("application/font-") ||
             mime == "application/vnd.ms-fontobject" ||
             mime == "application/vnd.ms-opentype") {
    type = LinkPrefetchMimeType::kFont;
  } else if (mime.StartsWith("image/")) {
    type = LinkPrefetchMimeType::kImage;
  } else if (mime.StartsWith("audio/") || mime.StartsWith("video/") ||
             mime == "application/ogg") {
    type = LinkPrefetchMimeType::kMedia;
  }
  UMA_HISTOGRAM_ENUMERATION("Blink.Prefetch.LinkPrefetchMimeType", type);
}

}  // namespace

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
  if (response_source == ResponseSource::kFromMemoryCache) {
    ResourceRequest resource_request(resource->GetResourceRequest());

    if (!resource_request.Url().ProtocolIs(url::kDataScheme)) {
      frame_client->DispatchDidLoadResourceFromMemoryCache(resource_request,
                                                           response);
      frame->GetLocalFrameHostRemote().DidLoadResourceFromMemoryCache(
          resource_request.Url(),
          String::FromUTF8(resource_request.HttpMethod().Utf8()),
          String::FromUTF8(response.MimeType().Utf8()),
          resource_request.GetRequestDestination(),
          response.RequestIncludeCredentials());
    }

    // Note: probe::WillSendRequest needs to precede before this probe method.
    probe::MarkResourceAsCached(frame, document_loader_, identifier);
    if (response.IsNull())
      return;
  }

  RecordAddressSpaceFeature(frame, response);

  if (!response.HttpHeaderField(http_names::kExpectCT).empty()) {
    Deprecation::CountDeprecation(frame->DomWindow(),
                                  mojom::blink::WebFeature::kExpectCTHeader);
  }

  document_->Loader()->MaybeRecordServiceWorkerFallbackMainResource(
      response.WasFetchedViaServiceWorker());

  std::unique_ptr<AlternateSignedExchangeResourceInfo> alternate_resource_info;

  // See if this is a prefetch for a SXG.
  if (response.IsSignedExchangeInnerResponse() &&
      resource->GetType() == ResourceType::kLinkPrefetch) {
    CountUsage(WebFeature::kLinkRelPrefetchForSignedExchanges);

    if (resource->RedirectChainSize() > 0) {
      // See if the outer response (which must be the last response in
      // the redirect chain) had provided alternate links for the prefetch.
      alternate_resource_info =
          AlternateSignedExchangeResourceInfo::CreateIfValid(
              resource->LastResourceResponse().HttpHeaderField(
                  http_names::kLink),
              response.HttpHeaderField(http_names::kLink));
    }
  }

  // Count usage of Content-Disposition header in SVGUse resources.
  if (resource->Options().initiator_info.name ==
          fetch_initiator_type_names::kUse &&
      request.Url().ProtocolIsInHTTPFamily() && response.IsAttachment()) {
    CountUsage(WebFeature::kContentDispositionInSvgUse);
  }

  if (resource->GetType() == ResourceType::kLinkPrefetch)
    LogLinkPrefetchMimeTypeHistogram(response.MimeType());

  PreloadHelper::CanLoadResources resource_loading_policy =
      response_source == ResponseSource::kFromMemoryCache
          ? PreloadHelper::kDoNotLoadResources
          : PreloadHelper::kLoadResourcesAndPreconnect;
  PreloadHelper::LoadLinksFromHeader(
      response.HttpHeaderField(http_names::kLink), response.CurrentRequestUrl(),
      *frame, document_, resource_loading_policy, PreloadHelper::kLoadAll,
      nullptr /* viewport_description */, std::move(alternate_resource_info),
      base::OptionalToPtr(response.RecursivePrefetchToken()));

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

  frame->GetAttributionSrcLoader()->MaybeRegisterAttributionHeaders(
      request, response, resource);

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
  UpdatePowerModeVote();
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
    interactive_detector->OnResourceLoadEnd(absl::nullopt);
  }
  if (IdlenessDetector* idleness_detector = frame->GetIdlenessDetector()) {
    idleness_detector->OnDidLoadResource();
  }
  UpdatePowerModeVote();
  document_->CheckCompleted();
}

void ResourceLoadObserverForFrame::DidChangeRenderBlockingBehavior(
    Resource* resource,
    const FetchParameters& params) {
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "devtools.timeline", "PreloadRenderBlockingStatusChange",
      TRACE_EVENT_SCOPE_THREAD, base::TimeTicks::Now(), "data",
      [&](perfetto::TracedValue ctx) {
        inspector_change_render_blocking_behavior_event::Data(
            std::move(ctx), document_->Loader(),
            resource->GetResourceRequest().InspectorId(),
            resource->GetResourceRequest(),
            params.GetResourceRequest().GetRenderBlockingBehavior());
      });
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
  document_loader_->GetUseCounter().Count(feature, document_->GetFrame());
}

void ResourceLoadObserverForFrame::UpdatePowerModeVote() {
  // Vote for loading as long as there are at least three pending requests.
  int request_count = document_->Fetcher()->ActiveRequestCount();
  bool should_vote_loading = request_count > 2;

  if (should_vote_loading == power_mode_vote_is_loading_)
    return;

  if (should_vote_loading) {
    power_mode_voter_->VoteFor(power_scheduler::PowerMode::kLoading);
  } else {
    power_mode_voter_->ResetVoteAfterTimeout(
        power_scheduler::PowerModeVoter::kLoadingTimeout);
  }

  power_mode_vote_is_loading_ = should_vote_loading;
}

}  // namespace blink
