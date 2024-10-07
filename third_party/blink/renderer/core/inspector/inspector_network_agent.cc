/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/containers/span_or_size.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/cert/ct_sct_to_string.h"
#include "net/cert/x509_util.h"
#include "net/http/http_status_code.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/websocket.mojom-blink.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/network_resources_data.h"
#include "third_party/blink/renderer/core/inspector/protocol/network.h"
#include "third_party/blink/renderer/core/inspector/request_debug_header_scope.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/render_blocking_behavior.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/service_worker_router_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/inspector_protocol/crdtp/json.h"

using crdtp::SpanFrom;
using crdtp::json::ConvertCBORToJSON;

namespace blink {

using GetRequestPostDataCallback =
    protocol::Network::Backend::GetRequestPostDataCallback;
using GetResponseBodyCallback =
    protocol::Network::Backend::GetResponseBodyCallback;

namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr int kDefaultTotalBufferSize = 10 * 1000 * 1000;    // 10 MB
constexpr int kDefaultResourceBufferSize = 5 * 1000 * 1000;  // 5 MB
#else
constexpr int kDefaultTotalBufferSize = 200 * 1000 * 1000;    // 200 MB
constexpr int kDefaultResourceBufferSize = 20 * 1000 * 1000;  // 20 MB
#endif

// Pattern may contain stars ('*') which match to any (possibly empty) string.
// Stars implicitly assumed at the begin/end of pattern.
bool Matches(const String& url, const String& pattern) {
  Vector<String> parts;
  pattern.Split("*", parts);
  wtf_size_t pos = 0;
  for (const String& part : parts) {
    pos = url.Find(part, pos);
    if (pos == kNotFound)
      return false;
    pos += part.length();
  }
  return true;
}

bool LoadsFromCacheOnly(const ResourceRequest& request) {
  switch (request.GetCacheMode()) {
    case mojom::FetchCacheMode::kDefault:
    case mojom::FetchCacheMode::kNoStore:
    case mojom::FetchCacheMode::kValidateCache:
    case mojom::FetchCacheMode::kBypassCache:
    case mojom::FetchCacheMode::kForceCache:
      return false;
    case mojom::FetchCacheMode::kOnlyIfCached:
    case mojom::FetchCacheMode::kUnspecifiedOnlyIfCachedStrict:
    case mojom::FetchCacheMode::kUnspecifiedForceCacheMiss:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

protocol::Network::CertificateTransparencyCompliance
SerializeCTPolicyCompliance(net::ct::CTPolicyCompliance ct_compliance) {
  switch (ct_compliance) {
    case net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS:
      return protocol::Network::CertificateTransparencyComplianceEnum::
          Compliant;
    case net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS:
    case net::ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS:
      return protocol::Network::CertificateTransparencyComplianceEnum::
          NotCompliant;
    case net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY:
    case net::ct::CTPolicyCompliance::
        CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE:
      return protocol::Network::CertificateTransparencyComplianceEnum::Unknown;
    case net::ct::CTPolicyCompliance::CT_POLICY_COUNT:
      NOTREACHED_IN_MIGRATION();
      // Fallthrough to default.
  }
  NOTREACHED_IN_MIGRATION();
  return protocol::Network::CertificateTransparencyComplianceEnum::Unknown;
}

static std::unique_ptr<protocol::Network::Headers> BuildObjectForHeaders(
    const HTTPHeaderMap& headers) {
  std::unique_ptr<protocol::DictionaryValue> headers_object =
      protocol::DictionaryValue::create();
  for (const auto& header : headers)
    headers_object->setString(header.key.GetString(), header.value);
  protocol::ErrorSupport errors;
  return protocol::Network::Headers::fromValue(headers_object.get(), &errors);
}

class InspectorFileReaderLoaderClient final
    : public GarbageCollected<InspectorFileReaderLoaderClient>,
      public FileReaderClient {
 public:
  InspectorFileReaderLoaderClient(
      scoped_refptr<BlobDataHandle> blob,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceCallback<void(std::optional<SegmentedBuffer>)> callback)
      : blob_(std::move(blob)),
        callback_(std::move(callback)),
        loader_(MakeGarbageCollected<FileReaderLoader>(this,
                                                       std::move(task_runner))),
        keep_alive_(this) {}

  InspectorFileReaderLoaderClient(const InspectorFileReaderLoaderClient&) =
      delete;
  InspectorFileReaderLoaderClient& operator=(
      const InspectorFileReaderLoaderClient&) = delete;

  ~InspectorFileReaderLoaderClient() override = default;

  void Start() {
    loader_->Start(blob_);
  }

  FileErrorCode DidStartLoading(uint64_t) override {
    return FileErrorCode::kOK;
  }

  FileErrorCode DidReceiveData(base::span<const uint8_t> data) override {
    if (!data.empty()) {
      raw_data_.Append(data);
    }
    return FileErrorCode::kOK;
  }

  void DidFinishLoading() override { Done(std::move(raw_data_)); }

  void DidFail(FileErrorCode) override { Done(std::nullopt); }

  void Trace(Visitor* visitor) const override {
    FileReaderClient::Trace(visitor);
    visitor->Trace(loader_);
  }

 private:
  void Done(std::optional<SegmentedBuffer> output) {
    std::move(callback_).Run(std::move(output));
    keep_alive_.Clear();
    loader_ = nullptr;
  }

  scoped_refptr<BlobDataHandle> blob_;
  String mime_type_;
  String text_encoding_name_;
  base::OnceCallback<void(std::optional<SegmentedBuffer>)> callback_;
  Member<FileReaderLoader> loader_;
  SegmentedBuffer raw_data_;
  SelfKeepAlive<InspectorFileReaderLoaderClient> keep_alive_;
};

static void ResponseBodyFileReaderLoaderDone(
    const String& mime_type,
    const String& text_encoding_name,
    std::unique_ptr<GetResponseBodyCallback> callback,
    std::optional<SegmentedBuffer> raw_data) {
  if (!raw_data) {
    callback->sendFailure(
        protocol::Response::ServerError("Couldn't read BLOB"));
    return;
  }
  String result;
  bool base64_encoded;
  if (InspectorPageAgent::SegmentedBufferContent(&*raw_data, mime_type,
                                                 text_encoding_name, &result,
                                                 &base64_encoded)) {
    callback->sendSuccess(result, base64_encoded);
  } else {
    callback->sendFailure(
        protocol::Response::ServerError("Couldn't encode data"));
  }
}

class InspectorPostBodyParser
    : public WTF::RefCounted<InspectorPostBodyParser> {
 public:
  InspectorPostBodyParser(
      std::unique_ptr<GetRequestPostDataCallback> callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : callback_(std::move(callback)),
        task_runner_(std::move(task_runner)),
        error_(false) {}

  InspectorPostBodyParser(const InspectorPostBodyParser&) = delete;
  InspectorPostBodyParser& operator=(const InspectorPostBodyParser&) = delete;

  void Parse(EncodedFormData* request_body) {
    if (!request_body || request_body->IsEmpty())
      return;

    parts_.Grow(request_body->Elements().size());
    for (wtf_size_t i = 0; i < request_body->Elements().size(); i++) {
      const FormDataElement& data = request_body->Elements()[i];
      switch (data.type_) {
        case FormDataElement::kData:
          parts_[i] = String::FromUTF8WithLatin1Fallback(data.data_.data(),
                                                         data.data_.size());
          break;
        case FormDataElement::kEncodedBlob:
          ReadDataBlob(data.optional_blob_data_handle_, &parts_[i]);
          break;
        case FormDataElement::kEncodedFile:
        case FormDataElement::kDataPipe:
          // Do nothing, not supported
          break;
      }
    }
  }

 private:
  friend class WTF::RefCounted<InspectorPostBodyParser>;

  ~InspectorPostBodyParser() {
    if (error_)
      return;
    StringBuilder result;
    for (const auto& part : parts_)
      result.Append(part);
    callback_->sendSuccess(result.ToString());
  }

  void BlobReadCallback(String* destination,
                        std::optional<SegmentedBuffer> raw_data) {
    if (raw_data) {
      Vector<char> flattened_data = std::move(*raw_data).CopyAs<Vector<char>>();
      *destination = String::FromUTF8WithLatin1Fallback(flattened_data.data(),
                                                        flattened_data.size());
    } else {
      error_ = true;
    }
  }

  void ReadDataBlob(scoped_refptr<blink::BlobDataHandle> blob_handle,
                    String* destination) {
    if (!blob_handle)
      return;
    auto* reader = MakeGarbageCollected<InspectorFileReaderLoaderClient>(
        blob_handle, task_runner_,
        WTF::BindOnce(&InspectorPostBodyParser::BlobReadCallback,
                      WTF::RetainedRef(this), WTF::Unretained(destination)));
    reader->Start();
  }

  std::unique_ptr<GetRequestPostDataCallback> callback_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool error_;
  Vector<String> parts_;
};

KURL UrlWithoutFragment(const KURL& url) {
  KURL result = url;
  result.RemoveFragmentIdentifier();
  return result;
}

String MixedContentTypeForContextType(
    mojom::blink::MixedContentContextType context_type) {
  switch (context_type) {
    case mojom::blink::MixedContentContextType::kNotMixedContent:
      return protocol::Security::MixedContentTypeEnum::None;
    case mojom::blink::MixedContentContextType::kBlockable:
      return protocol::Security::MixedContentTypeEnum::Blockable;
    case mojom::blink::MixedContentContextType::kOptionallyBlockable:
    case mojom::blink::MixedContentContextType::kShouldBeBlockable:
      return protocol::Security::MixedContentTypeEnum::OptionallyBlockable;
  }

  return protocol::Security::MixedContentTypeEnum::None;
}

String ResourcePriorityJSON(ResourceLoadPriority priority) {
  switch (priority) {
    case ResourceLoadPriority::kVeryLow:
      return protocol::Network::ResourcePriorityEnum::VeryLow;
    case ResourceLoadPriority::kLow:
      return protocol::Network::ResourcePriorityEnum::Low;
    case ResourceLoadPriority::kMedium:
      return protocol::Network::ResourcePriorityEnum::Medium;
    case ResourceLoadPriority::kHigh:
      return protocol::Network::ResourcePriorityEnum::High;
    case ResourceLoadPriority::kVeryHigh:
      return protocol::Network::ResourcePriorityEnum::VeryHigh;
    case ResourceLoadPriority::kUnresolved:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return protocol::Network::ResourcePriorityEnum::Medium;
}

String BuildBlockedReason(ResourceRequestBlockedReason reason) {
  switch (reason) {
    case ResourceRequestBlockedReason::kCSP:
      return protocol::Network::BlockedReasonEnum::Csp;
    case ResourceRequestBlockedReason::kMixedContent:
      return protocol::Network::BlockedReasonEnum::MixedContent;
    case ResourceRequestBlockedReason::kOrigin:
      return protocol::Network::BlockedReasonEnum::Origin;
    case ResourceRequestBlockedReason::kInspector:
      return protocol::Network::BlockedReasonEnum::Inspector;
    case ResourceRequestBlockedReason::kSubresourceFilter:
      return protocol::Network::BlockedReasonEnum::SubresourceFilter;
    case ResourceRequestBlockedReason::kContentType:
      return protocol::Network::BlockedReasonEnum::ContentType;
    case ResourceRequestBlockedReason::kOther:
      return protocol::Network::BlockedReasonEnum::Other;
    case blink::ResourceRequestBlockedReason::kCoepFrameResourceNeedsCoepHeader:
      return protocol::Network::BlockedReasonEnum::
          CoepFrameResourceNeedsCoepHeader;
    case blink::ResourceRequestBlockedReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage:
      return protocol::Network::BlockedReasonEnum::
          CoopSandboxedIframeCannotNavigateToCoopPage;
    case blink::ResourceRequestBlockedReason::kCorpNotSameOrigin:
      return protocol::Network::BlockedReasonEnum::CorpNotSameOrigin;
    case blink::ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
      return protocol::Network::BlockedReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case blink::ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByDip:
      return protocol::Network::BlockedReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByDip;
    case blink::ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip:
      return protocol::Network::BlockedReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip;
    case blink::ResourceRequestBlockedReason::kCorpNotSameSite:
      return protocol::Network::BlockedReasonEnum::CorpNotSameSite;
    case ResourceRequestBlockedReason::kConversionRequest:
      // This is actually never reached, as the conversion request
      // is marked as successful and no blocking reason is reported.
      NOTREACHED_IN_MIGRATION();
      return protocol::Network::BlockedReasonEnum::Other;
  }
  NOTREACHED_IN_MIGRATION();
  return protocol::Network::BlockedReasonEnum::Other;
}

Maybe<String> BuildBlockedReason(const ResourceError& error) {
  int error_code = error.ErrorCode();
  if (error_code != net::ERR_BLOCKED_BY_CLIENT &&
      error_code != net::ERR_BLOCKED_BY_RESPONSE) {
    return Maybe<String>();
  }

  std::optional<ResourceRequestBlockedReason> resource_request_blocked_reason =
      error.GetResourceRequestBlockedReason();
  if (resource_request_blocked_reason)
    return BuildBlockedReason(*resource_request_blocked_reason);

  // TODO(karandeepb): Embedder would know how to interpret the
  // `error.extended_error_code_` in this case. For now just return Other.
  return {protocol::Network::BlockedReasonEnum::Other};
}

String BuildCorsError(network::mojom::CorsError cors_error) {
  switch (cors_error) {
    case network::mojom::CorsError::kDisallowedByMode:
      return protocol::Network::CorsErrorEnum::DisallowedByMode;

    case network::mojom::CorsError::kInvalidResponse:
      return protocol::Network::CorsErrorEnum::InvalidResponse;

    case network::mojom::CorsError::kWildcardOriginNotAllowed:
      return protocol::Network::CorsErrorEnum::WildcardOriginNotAllowed;

    case network::mojom::CorsError::kMissingAllowOriginHeader:
      return protocol::Network::CorsErrorEnum::MissingAllowOriginHeader;

    case network::mojom::CorsError::kMultipleAllowOriginValues:
      return protocol::Network::CorsErrorEnum::MultipleAllowOriginValues;

    case network::mojom::CorsError::kInvalidAllowOriginValue:
      return protocol::Network::CorsErrorEnum::InvalidAllowOriginValue;

    case network::mojom::CorsError::kAllowOriginMismatch:
      return protocol::Network::CorsErrorEnum::AllowOriginMismatch;

    case network::mojom::CorsError::kInvalidAllowCredentials:
      return protocol::Network::CorsErrorEnum::InvalidAllowCredentials;

    case network::mojom::CorsError::kCorsDisabledScheme:
      return protocol::Network::CorsErrorEnum::CorsDisabledScheme;

    case network::mojom::CorsError::kPreflightInvalidStatus:
      return protocol::Network::CorsErrorEnum::PreflightInvalidStatus;

    case network::mojom::CorsError::kPreflightDisallowedRedirect:
      return protocol::Network::CorsErrorEnum::PreflightDisallowedRedirect;

    case network::mojom::CorsError::kPreflightWildcardOriginNotAllowed:
      return protocol::Network::CorsErrorEnum::
          PreflightWildcardOriginNotAllowed;

    case network::mojom::CorsError::kPreflightMissingAllowOriginHeader:
      return protocol::Network::CorsErrorEnum::
          PreflightMissingAllowOriginHeader;

    case network::mojom::CorsError::kPreflightMultipleAllowOriginValues:
      return protocol::Network::CorsErrorEnum::
          PreflightMultipleAllowOriginValues;

    case network::mojom::CorsError::kPreflightInvalidAllowOriginValue:
      return protocol::Network::CorsErrorEnum::PreflightInvalidAllowOriginValue;

    case network::mojom::CorsError::kPreflightAllowOriginMismatch:
      return protocol::Network::CorsErrorEnum::PreflightAllowOriginMismatch;

    case network::mojom::CorsError::kPreflightInvalidAllowCredentials:
      return protocol::Network::CorsErrorEnum::PreflightInvalidAllowCredentials;

    case network::mojom::CorsError::kPreflightMissingAllowPrivateNetwork:
      return protocol::Network::CorsErrorEnum::
          PreflightMissingAllowPrivateNetwork;

    case network::mojom::CorsError::kPreflightInvalidAllowPrivateNetwork:
      return protocol::Network::CorsErrorEnum::
          PreflightInvalidAllowPrivateNetwork;

    case network::mojom::CorsError::kInvalidAllowMethodsPreflightResponse:
      return protocol::Network::CorsErrorEnum::
          InvalidAllowMethodsPreflightResponse;

    case network::mojom::CorsError::kInvalidAllowHeadersPreflightResponse:
      return protocol::Network::CorsErrorEnum::
          InvalidAllowHeadersPreflightResponse;

    case network::mojom::CorsError::kMethodDisallowedByPreflightResponse:
      return protocol::Network::CorsErrorEnum::
          MethodDisallowedByPreflightResponse;

    case network::mojom::CorsError::kHeaderDisallowedByPreflightResponse:
      return protocol::Network::CorsErrorEnum::
          HeaderDisallowedByPreflightResponse;

    case network::mojom::CorsError::kRedirectContainsCredentials:
      return protocol::Network::CorsErrorEnum::RedirectContainsCredentials;

    case network::mojom::CorsError::kInsecurePrivateNetwork:
      return protocol::Network::CorsErrorEnum::InsecurePrivateNetwork;

    case network::mojom::CorsError::kInvalidPrivateNetworkAccess:
      return protocol::Network::CorsErrorEnum::InvalidPrivateNetworkAccess;

    case network::mojom::CorsError::kUnexpectedPrivateNetworkAccess:
      return protocol::Network::CorsErrorEnum::UnexpectedPrivateNetworkAccess;

    case network::mojom::CorsError::kPreflightMissingPrivateNetworkAccessId:
      return protocol::Network::CorsErrorEnum::
          PreflightMissingPrivateNetworkAccessId;

    case network::mojom::CorsError::kPreflightMissingPrivateNetworkAccessName:
      return protocol::Network::CorsErrorEnum::
          PreflightMissingPrivateNetworkAccessName;

    case network::mojom::CorsError::kPrivateNetworkAccessPermissionUnavailable:
      return protocol::Network::CorsErrorEnum::
          PrivateNetworkAccessPermissionUnavailable;

    case network::mojom::CorsError::kPrivateNetworkAccessPermissionDenied:
      return protocol::Network::CorsErrorEnum::
          PrivateNetworkAccessPermissionDenied;
  }
}

std::unique_ptr<protocol::Network::CorsErrorStatus> BuildCorsErrorStatus(
    const network::CorsErrorStatus& status) {
  return protocol::Network::CorsErrorStatus::create()
      .setCorsError(BuildCorsError(status.cors_error))
      .setFailedParameter(String::FromUTF8(status.failed_parameter))
      .build();
}

String BuildServiceWorkerResponseSource(const ResourceResponse& response) {
  switch (response.GetServiceWorkerResponseSource()) {
    case network::mojom::FetchResponseSource::kCacheStorage:
      return protocol::Network::ServiceWorkerResponseSourceEnum::CacheStorage;
    case network::mojom::FetchResponseSource::kHttpCache:
      return protocol::Network::ServiceWorkerResponseSourceEnum::HttpCache;
    case network::mojom::FetchResponseSource::kNetwork:
      return protocol::Network::ServiceWorkerResponseSourceEnum::Network;
    case network::mojom::FetchResponseSource::kUnspecified:
      return protocol::Network::ServiceWorkerResponseSourceEnum::FallbackCode;
  }
}

String BuildServiceWorkerRouterSourceType(
    const network::mojom::ServiceWorkerRouterSourceType& type) {
  switch (type) {
    case network::mojom::ServiceWorkerRouterSourceType::kNetwork:
      return protocol::Network::ServiceWorkerRouterSourceEnum::Network;
    case network::mojom::ServiceWorkerRouterSourceType::kRace:
      return protocol::Network::ServiceWorkerRouterSourceEnum::
          RaceNetworkAndFetchHandler;
    case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
      return protocol::Network::ServiceWorkerRouterSourceEnum::FetchEvent;
    case network::mojom::ServiceWorkerRouterSourceType::kCache:
      return protocol::Network::ServiceWorkerRouterSourceEnum::Cache;
  }
}

WebConnectionType ToWebConnectionType(const String& connection_type) {
  if (connection_type == protocol::Network::ConnectionTypeEnum::None)
    return kWebConnectionTypeNone;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Cellular2g)
    return kWebConnectionTypeCellular2G;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Cellular3g)
    return kWebConnectionTypeCellular3G;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Cellular4g)
    return kWebConnectionTypeCellular4G;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Bluetooth)
    return kWebConnectionTypeBluetooth;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Ethernet)
    return kWebConnectionTypeEthernet;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Wifi)
    return kWebConnectionTypeWifi;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Wimax)
    return kWebConnectionTypeWimax;
  if (connection_type == protocol::Network::ConnectionTypeEnum::Other)
    return kWebConnectionTypeOther;
  return kWebConnectionTypeUnknown;
}

String GetReferrerPolicy(network::mojom::ReferrerPolicy policy) {
  switch (policy) {
    case network::mojom::ReferrerPolicy::kAlways:
      return protocol::Network::Request::ReferrerPolicyEnum::UnsafeUrl;
    case network::mojom::ReferrerPolicy::kDefault:
      return protocol::Network::Request::ReferrerPolicyEnum::
          StrictOriginWhenCrossOrigin;
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return protocol::Network::Request::ReferrerPolicyEnum::
          NoReferrerWhenDowngrade;
    case network::mojom::ReferrerPolicy::kNever:
      return protocol::Network::Request::ReferrerPolicyEnum::NoReferrer;
    case network::mojom::ReferrerPolicy::kOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::Origin;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::
          OriginWhenCrossOrigin;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::SameOrigin;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::StrictOrigin;
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return protocol::Network::Request::ReferrerPolicyEnum::
          StrictOriginWhenCrossOrigin;
  }

  return protocol::Network::Request::ReferrerPolicyEnum::
      NoReferrerWhenDowngrade;
}

std::unique_ptr<protocol::Network::WebSocketFrame> WebSocketMessageToProtocol(
    int op_code,
    bool masked,
    base::span<const char> payload) {
  return protocol::Network::WebSocketFrame::create()
      .setOpcode(op_code)
      .setMask(masked)
      // Only interpret the payload as UTF-8 when it's a text message
      .setPayloadData(op_code == 1 ? String::FromUTF8WithLatin1Fallback(
                                         payload.data(), payload.size())
                                   : Base64Encode(base::as_bytes(payload)))
      .build();
}

String GetTrustTokenOperationType(
    network::mojom::TrustTokenOperationType operation) {
  switch (operation) {
    case network::mojom::TrustTokenOperationType::kIssuance:
      return protocol::Network::TrustTokenOperationTypeEnum::Issuance;
    case network::mojom::TrustTokenOperationType::kRedemption:
      return protocol::Network::TrustTokenOperationTypeEnum::Redemption;
    case network::mojom::TrustTokenOperationType::kSigning:
      return protocol::Network::TrustTokenOperationTypeEnum::Signing;
  }
}

String GetTrustTokenRefreshPolicy(
    network::mojom::TrustTokenRefreshPolicy policy) {
  switch (policy) {
    case network::mojom::TrustTokenRefreshPolicy::kUseCached:
      return protocol::Network::TrustTokenParams::RefreshPolicyEnum::UseCached;
    case network::mojom::TrustTokenRefreshPolicy::kRefresh:
      return protocol::Network::TrustTokenParams::RefreshPolicyEnum::Refresh;
  }
}

std::unique_ptr<protocol::Network::TrustTokenParams> BuildTrustTokenParams(
    const network::mojom::blink::TrustTokenParams& params) {
  auto protocol_params =
      protocol::Network::TrustTokenParams::create()
          .setOperation(GetTrustTokenOperationType(params.operation))
          .setRefreshPolicy(GetTrustTokenRefreshPolicy(params.refresh_policy))
          .build();

  if (!params.issuers.empty()) {
    auto issuers = std::make_unique<protocol::Array<protocol::String>>();
    for (const auto& issuer : params.issuers) {
      issuers->push_back(issuer->ToString());
    }
    protocol_params->setIssuers(std::move(issuers));
  }

  return protocol_params;
}

void SetNetworkStateOverride(bool offline,
                             double latency,
                             double download_throughput,
                             double upload_throughput,
                             WebConnectionType type) {
  // TODO(dgozman): networkStateNotifier is per-process. It would be nice to
  // have per-frame override instead.
  if (offline || latency || download_throughput || upload_throughput) {
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        !offline, type, std::nullopt, latency,
        download_throughput / (1024 * 1024 / 8));
  } else {
    GetNetworkStateNotifier().ClearOverride();
  }
}

String IPAddressToString(const net::IPAddress& address) {
  String unbracketed = String::FromUTF8(address.ToString());
  if (!address.IsIPv6()) {
    return unbracketed;
  }

  return "[" + unbracketed + "]";
}

namespace ContentEncodingEnum = protocol::Network::ContentEncodingEnum;

String AcceptedEncodingFromProtocol(
    const protocol::Network::ContentEncoding& encoding) {
  String result;
  if (ContentEncodingEnum::Gzip == encoding ||
      ContentEncodingEnum::Br == encoding ||
      ContentEncodingEnum::Deflate == encoding ||
      ContentEncodingEnum::Zstd == encoding) {
    result = encoding;
  }
  return result;
}

using SourceTypeEnum = net::SourceStream::SourceType;
SourceTypeEnum SourceTypeFromString(const String& type) {
  if (type == ContentEncodingEnum::Gzip)
    return SourceTypeEnum::TYPE_GZIP;
  if (type == ContentEncodingEnum::Deflate)
    return SourceTypeEnum::TYPE_DEFLATE;
  if (type == ContentEncodingEnum::Br)
    return SourceTypeEnum::TYPE_BROTLI;
  if (type == ContentEncodingEnum::Zstd) {
    return SourceTypeEnum::TYPE_ZSTD;
  }
  NOTREACHED_IN_MIGRATION();
  return SourceTypeEnum::TYPE_UNKNOWN;
}

}  // namespace

void InspectorNetworkAgent::Restore() {
  if (enabled_.Get())
    Enable();
}

static std::unique_ptr<protocol::Network::ResourceTiming> BuildObjectForTiming(
    const ResourceLoadTiming& timing) {
  return protocol::Network::ResourceTiming::create()
      .setRequestTime(timing.RequestTime().since_origin().InSecondsF())
      .setProxyStart(timing.CalculateMillisecondDelta(timing.ProxyStart()))
      .setProxyEnd(timing.CalculateMillisecondDelta(timing.ProxyEnd()))
      .setDnsStart(timing.CalculateMillisecondDelta(timing.DomainLookupStart()))
      .setDnsEnd(timing.CalculateMillisecondDelta(timing.DomainLookupEnd()))
      .setConnectStart(timing.CalculateMillisecondDelta(timing.ConnectStart()))
      .setConnectEnd(timing.CalculateMillisecondDelta(timing.ConnectEnd()))
      .setSslStart(timing.CalculateMillisecondDelta(timing.SslStart()))
      .setSslEnd(timing.CalculateMillisecondDelta(timing.SslEnd()))
      .setWorkerStart(timing.CalculateMillisecondDelta(timing.WorkerStart()))
      .setWorkerReady(timing.CalculateMillisecondDelta(timing.WorkerReady()))
      .setWorkerFetchStart(
          timing.CalculateMillisecondDelta(timing.WorkerFetchStart()))
      .setWorkerRespondWithSettled(
          timing.CalculateMillisecondDelta(timing.WorkerRespondWithSettled()))
      .setSendStart(timing.CalculateMillisecondDelta(timing.SendStart()))
      .setSendEnd(timing.CalculateMillisecondDelta(timing.SendEnd()))
      .setReceiveHeadersStart(
          timing.CalculateMillisecondDelta(timing.ReceiveHeadersStart()))
      .setReceiveHeadersEnd(
          timing.CalculateMillisecondDelta(timing.ReceiveHeadersEnd()))
      .setPushStart(timing.PushStart().since_origin().InSecondsF())
      .setPushEnd(timing.PushEnd().since_origin().InSecondsF())
      .build();
}

static bool FormDataToString(
    scoped_refptr<EncodedFormData> body,
    size_t max_body_size,
    protocol::Array<protocol::Network::PostDataEntry>* data_entries,
    String* content) {
  *content = "";
  if (!body || body->IsEmpty())
    return false;

  // SizeInBytes below doesn't support all element types, so first check if all
  // the body elements are of the right type.
  for (const auto& element : body->Elements()) {
    if (element.type_ != FormDataElement::kData)
      return true;
  }

  if (max_body_size != 0 && body->SizeInBytes() > max_body_size)
    return true;

  for (const auto& element : body->Elements()) {
    auto data_entry = protocol::Network::PostDataEntry::create().build();
    data_entry->setBytes(
        protocol::Binary::fromSpan(base::as_byte_span(element.data_)));
    data_entries->push_back(std::move(data_entry));
  }

  Vector<char> bytes;
  body->Flatten(bytes);
  *content = String::FromUTF8WithLatin1Fallback(bytes.data(), bytes.size());
  return true;
}

static String StringFromASCII(const std::string& str) {
  String ret(str);
  DCHECK(ret.ContainsOnlyASCIIOrEmpty());
  return ret;
}

static std::unique_ptr<protocol::Network::SecurityDetails> BuildSecurityDetails(
    const net::SSLInfo& ssl_info) {
  // This function should be kept in sync with the corresponding function in
  // network_handler.cc in //content.
  if (!ssl_info.cert)
    return nullptr;
  auto signed_certificate_timestamp_list = std::make_unique<
      protocol::Array<protocol::Network::SignedCertificateTimestamp>>();
  for (auto const& sct : ssl_info.signed_certificate_timestamps) {
    std::unique_ptr<protocol::Network::SignedCertificateTimestamp>
        signed_certificate_timestamp =
            protocol::Network::SignedCertificateTimestamp::create()
                .setStatus(StringFromASCII(net::ct::StatusToString(sct.status)))
                .setOrigin(
                    StringFromASCII(net::ct::OriginToString(sct.sct->origin)))
                .setLogDescription(String::FromUTF8(sct.sct->log_description))
                .setLogId(StringFromASCII(base::HexEncode(
                    sct.sct->log_id.c_str(), sct.sct->log_id.length())))
                .setTimestamp(sct.sct->timestamp.InMillisecondsSinceUnixEpoch())
                .setHashAlgorithm(
                    StringFromASCII(net::ct::HashAlgorithmToString(
                        sct.sct->signature.hash_algorithm)))
                .setSignatureAlgorithm(
                    StringFromASCII(net::ct::SignatureAlgorithmToString(
                        sct.sct->signature.signature_algorithm)))
                .setSignatureData(StringFromASCII(base::HexEncode(
                    sct.sct->signature.signature_data.c_str(),
                    sct.sct->signature.signature_data.length())))
                .build();
    signed_certificate_timestamp_list->emplace_back(
        std::move(signed_certificate_timestamp));
  }
  std::vector<std::string> san_dns;
  std::vector<std::string> san_ip;
  ssl_info.cert->GetSubjectAltName(&san_dns, &san_ip);
  auto san_list = std::make_unique<protocol::Array<String>>();
  for (const std::string& san : san_dns) {
    // DNS names in a SAN list are always ASCII.
    san_list->push_back(StringFromASCII(san));
  }
  for (const std::string& san : san_ip) {
    net::IPAddress ip(base::as_byte_span(san));
    san_list->push_back(StringFromASCII(ip.ToString()));
  }

  const char* protocol = "";
  const char* key_exchange = "";
  const char* cipher = "";
  const char* mac = nullptr;
  if (ssl_info.connection_status) {
    net::SSLVersion ssl_version =
        net::SSLConnectionStatusToVersion(ssl_info.connection_status);
    net::SSLVersionToString(&protocol, ssl_version);
    bool is_aead;
    bool is_tls13;
    uint16_t cipher_suite =
        net::SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
    net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                                 &is_tls13, cipher_suite);
    if (key_exchange == nullptr) {
      DCHECK(is_tls13);
      key_exchange = "";
    }
  }

  std::unique_ptr<protocol::Network::SecurityDetails> security_details =
      protocol::Network::SecurityDetails::create()
          .setProtocol(protocol)
          .setKeyExchange(key_exchange)
          .setCipher(cipher)
          .setSubjectName(
              String::FromUTF8(ssl_info.cert->subject().common_name))
          .setSanList(std::move(san_list))
          .setIssuer(String::FromUTF8(ssl_info.cert->issuer().common_name))
          .setValidFrom(ssl_info.cert->valid_start().InSecondsFSinceUnixEpoch())
          .setValidTo(ssl_info.cert->valid_expiry().InSecondsFSinceUnixEpoch())
          .setCertificateId(0)  // Keep this in protocol for compatibility.
          .setSignedCertificateTimestampList(
              std::move(signed_certificate_timestamp_list))
          .setCertificateTransparencyCompliance(
              SerializeCTPolicyCompliance(ssl_info.ct_policy_compliance))
          .setEncryptedClientHello(ssl_info.encrypted_client_hello)
          .build();

  if (ssl_info.key_exchange_group != 0) {
    const char* key_exchange_group =
        SSL_get_curve_name(ssl_info.key_exchange_group);
    if (key_exchange_group)
      security_details->setKeyExchangeGroup(key_exchange_group);
  }
  if (mac)
    security_details->setMac(mac);
  if (ssl_info.peer_signature_algorithm != 0) {
    security_details->setServerSignatureAlgorithm(
        ssl_info.peer_signature_algorithm);
  }

  return security_details;
}

static std::unique_ptr<protocol::Network::Request>
BuildObjectForResourceRequest(const ResourceRequest& request,
                              scoped_refptr<EncodedFormData> post_data,
                              size_t max_body_size) {
  String data_string;
  auto data_entries =
      std::make_unique<protocol::Array<protocol::Network::PostDataEntry>>();
  bool has_post_data = FormDataToString(post_data, max_body_size,
                                        data_entries.get(), &data_string);
  KURL url = request.Url();
  // protocol::Network::Request doesn't have a separate referrer string member
  // like blink::ResourceRequest, so here we add ResourceRequest's referrer
  // string to the protocol request's headers manually.
  auto headers = request.HttpHeaderFields();

  // The request's referrer must be generated at this point.
  DCHECK_NE(request.ReferrerString(), Referrer::ClientReferrerString());
  headers.Set(http_names::kReferer, AtomicString(request.ReferrerString()));

  std::unique_ptr<protocol::Network::Request> result =
      protocol::Network::Request::create()
          .setUrl(UrlWithoutFragment(url).GetString())
          .setMethod(request.HttpMethod())
          .setHeaders(BuildObjectForHeaders(headers))
          .setInitialPriority(ResourcePriorityJSON(request.Priority()))
          .setReferrerPolicy(GetReferrerPolicy(request.GetReferrerPolicy()))
          .build();
  if (url.HasFragmentIdentifier()) {
    result->setUrlFragment("#" + url.FragmentIdentifier().ToString());
  }
  if (!data_string.empty())
    result->setPostData(data_string);
  if (data_entries->size())
    result->setPostDataEntries(std::move(data_entries));
  if (has_post_data)
    result->setHasPostData(true);
  if (request.TrustTokenParams()) {
    result->setTrustTokenParams(
        BuildTrustTokenParams(*request.TrustTokenParams()));
  }
  return result;
}

String AlternateProtocolUsageToString(
    net::AlternateProtocolUsage alternate_protocol_usage) {
  switch (alternate_protocol_usage) {
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_NO_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::
          AlternativeJobWonWithoutRace;
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_WON_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::
          AlternativeJobWonRace;
    case net::AlternateProtocolUsage::
        ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::MainJobWonRace;
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING:
      return protocol::Network::AlternateProtocolUsageEnum::MappingMissing;
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_BROKEN:
      return protocol::Network::AlternateProtocolUsageEnum::Broken;
    case net::AlternateProtocolUsage::
        ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_WITHOUT_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::
          DnsAlpnH3JobWonWithoutRace;
    case net::AlternateProtocolUsage::
        ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::DnsAlpnH3JobWonRace;
    case net::AlternateProtocolUsage::
        ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON:
      return protocol::Network::AlternateProtocolUsageEnum::UnspecifiedReason;
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_MAX:
      return protocol::Network::AlternateProtocolUsageEnum::UnspecifiedReason;
  }
  return protocol::Network::AlternateProtocolUsageEnum::UnspecifiedReason;
}

static std::unique_ptr<protocol::Network::Response>
BuildObjectForResourceResponse(const ResourceResponse& response,
                               const ExecutionContext* context,
                               const Resource* cached_resource = nullptr,
                               bool* is_empty = nullptr) {
  if (response.IsNull())
    return nullptr;

  int status = response.HttpStatusCode();
  String status_text = response.HttpStatusText();
  HTTPHeaderMap headers_map = response.HttpHeaderFields();

  int64_t encoded_data_length = response.EncodedDataLength();

  String security_state = protocol::Security::SecurityStateEnum::Unknown;
  switch (response.GetSecurityStyle()) {
    case SecurityStyle::kUnknown:
      security_state = protocol::Security::SecurityStateEnum::Unknown;
      break;
    case SecurityStyle::kNeutral:
      security_state = protocol::Security::SecurityStateEnum::Neutral;
      break;
    case SecurityStyle::kInsecure:
      security_state = protocol::Security::SecurityStateEnum::Insecure;
      break;
    case SecurityStyle::kSecure:
      security_state = protocol::Security::SecurityStateEnum::Secure;
      break;
    case SecurityStyle::kInsecureBroken:
      security_state = protocol::Security::SecurityStateEnum::InsecureBroken;
      break;
  }

  // Use mime type and charset from cached resource in case the one in response
  // is empty or the response is a 304 Not Modified.
  String mime_type = response.MimeType();
  String charset = response.TextEncodingName();
  if (cached_resource) {
    if (mime_type.empty() || response.HttpStatusCode() == 304) {
      mime_type = cached_resource->GetResponse().MimeType();
    }
    if (charset.empty() || response.HttpStatusCode() == 304) {
      charset = cached_resource->GetResponse().TextEncodingName();
    }
  }

  if (is_empty)
    *is_empty = !status && mime_type.empty() && !headers_map.size();

  std::unique_ptr<protocol::Network::Response> response_object =
      protocol::Network::Response::create()
          .setUrl(UrlWithoutFragment(response.CurrentRequestUrl()).GetString())
          .setStatus(status)
          .setStatusText(status_text)
          .setHeaders(BuildObjectForHeaders(headers_map))
          .setMimeType(mime_type)
          .setCharset(charset)
          .setConnectionReused(response.ConnectionReused())
          .setConnectionId(response.ConnectionID())
          .setEncodedDataLength(encoded_data_length)
          .setSecurityState(security_state)
          .build();

  response_object->setFromDiskCache(response.WasCached());
  response_object->setFromServiceWorker(response.WasFetchedViaServiceWorker());
  if (response.WasFetchedViaServiceWorker()) {
    response_object->setServiceWorkerResponseSource(
        BuildServiceWorkerResponseSource(response));
  }
  if (!response.ResponseTime().is_null()) {
    response_object->setResponseTime(
        response.ResponseTime().InMillisecondsFSinceUnixEpochIgnoringNull());
  }
  if (!response.CacheStorageCacheName().empty()) {
    response_object->setCacheStorageCacheName(response.CacheStorageCacheName());
  }
  if (response.GetServiceWorkerRouterInfo()) {
    auto router_info =
        protocol::Network::ServiceWorkerRouterInfo::create().build();
    if (response.GetServiceWorkerRouterInfo()->RuleIdMatched()) {
      router_info->setRuleIdMatched(
          *response.GetServiceWorkerRouterInfo()->RuleIdMatched());
    }

    if (response.GetServiceWorkerRouterInfo()->MatchedSourceType()) {
      router_info->setMatchedSourceType(BuildServiceWorkerRouterSourceType(
          *response.GetServiceWorkerRouterInfo()->MatchedSourceType()));
    }

    if (response.GetServiceWorkerRouterInfo()->ActualSourceType()) {
      router_info->setActualSourceType(BuildServiceWorkerRouterSourceType(
          *response.GetServiceWorkerRouterInfo()->ActualSourceType()));
    }
    response_object->setServiceWorkerRouterInfo(std::move(router_info));
  }

  response_object->setFromPrefetchCache(response.WasInPrefetchCache());
  if (auto* resource_load_timing = response.GetResourceLoadTiming()) {
    auto load_timing = BuildObjectForTiming(*resource_load_timing);

    if (RuntimeEnabledFeatures::ServiceWorkerStaticRouterTimingInfoEnabled(
            context)) {
      if (!resource_load_timing->WorkerRouterEvaluationStart().is_null()) {
        load_timing->setWorkerRouterEvaluationStart(
            resource_load_timing->CalculateMillisecondDelta(
                resource_load_timing->WorkerRouterEvaluationStart()));
      }

      if (!resource_load_timing->WorkerCacheLokupStart().is_null()) {
        load_timing->setWorkerCacheLookupStart(
            resource_load_timing->CalculateMillisecondDelta(
                resource_load_timing->WorkerCacheLokupStart()));
      }
    }

    response_object->setTiming(std::move(load_timing));
  }

  const net::IPEndPoint& remote_ip_endpoint = response.RemoteIPEndpoint();
  if (remote_ip_endpoint.address().IsValid()) {
    response_object->setRemoteIPAddress(
        IPAddressToString(remote_ip_endpoint.address()));
    response_object->setRemotePort(remote_ip_endpoint.port());
  }

  response_object->setProtocol(
      InspectorNetworkAgent::GetProtocolAsString(response));
  if (response.AlternateProtocolUsage() !=
      net::AlternateProtocolUsage::
          ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON) {
    response_object->setAlternateProtocolUsage(
        AlternateProtocolUsageToString(response.AlternateProtocolUsage()));
  }

  const std::optional<net::SSLInfo>& ssl_info = response.GetSSLInfo();
  if (ssl_info.has_value()) {
    response_object->setSecurityDetails(BuildSecurityDetails(*ssl_info));
  }

  if (cached_resource && cached_resource->IsPreloadedByEarlyHints()) {
    response_object->setFromEarlyHints(true);
  }

  return response_object;
}

InspectorNetworkAgent::~InspectorNetworkAgent() = default;

void InspectorNetworkAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  visitor->Trace(worker_or_worklet_global_scope_);
  visitor->Trace(resources_data_);
  visitor->Trace(replay_xhrs_);
  visitor->Trace(pending_xhr_replay_data_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorNetworkAgent::ShouldBlockRequest(const KURL& url, bool* result) {
  if (blocked_urls_.IsEmpty())
    return;

  String url_string = url.GetString();
  for (const String& blocked : blocked_urls_.Keys()) {
    if (Matches(url_string, blocked)) {
      *result = true;
      return;
    }
  }
}

void InspectorNetworkAgent::ShouldBypassServiceWorker(bool* result) {
  if (bypass_service_worker_.Get())
    *result = true;
}

void InspectorNetworkAgent::DidBlockRequest(
    const ResourceRequest& request,
    DocumentLoader* loader,
    const KURL& fetch_context_url,
    const ResourceLoaderOptions& options,
    ResourceRequestBlockedReason reason,
    ResourceType resource_type) {
  InspectorPageAgent::ResourceType type =
      InspectorPageAgent::ToResourceType(resource_type);

  WillSendRequestInternal(loader, fetch_context_url, request,
                          ResourceResponse(), options, type,
                          base::TimeTicks::Now());

  String request_id = RequestId(loader, request.InspectorId());

  // Conversion Measurement API triggers recording of conversions
  // as redirects to a `/.well-known/register-conversion` url.
  // The redirect request is not actually executed
  // but stored internally and then aborted. As the redirect is blocked using
  // the ResourceRequestBlockedReason::kConversionRequest even when everything
  // worked out fine, we mark the request as successful, as to not confuse devs.
  if (reason == ResourceRequestBlockedReason::kConversionRequest) {
    GetFrontend()->loadingFinished(
        request_id, base::TimeTicks::Now().since_origin().InSecondsF(), 0);
    return;
  }

  String protocol_reason = BuildBlockedReason(reason);
  GetFrontend()->loadingFailed(
      request_id, base::TimeTicks::Now().since_origin().InSecondsF(),
      InspectorPageAgent::ResourceTypeJson(
          resources_data_->GetResourceType(request_id)),
      String(), false, protocol_reason);
}

void InspectorNetworkAgent::DidChangeResourcePriority(
    DocumentLoader* loader,
    uint64_t identifier,
    ResourceLoadPriority load_priority) {
  String request_id = RequestId(loader, identifier);
  GetFrontend()->resourceChangedPriority(
      request_id, ResourcePriorityJSON(load_priority),
      base::TimeTicks::Now().since_origin().InSecondsF());
}

String InspectorNetworkAgent::RequestId(DocumentLoader* loader,
                                        uint64_t identifier) {
  // It's difficult to go from a loader to an execution context, and in the case
  // of iframes the loader that is resolved via |GetTargetExecutionContext()| is
  // not the intended loader
  if (loader)
    return IdentifiersFactory::RequestId(loader, identifier);
  return IdentifiersFactory::RequestId(GetTargetExecutionContext(), identifier);
}

void InspectorNetworkAgent::WillSendRequestInternal(
    DocumentLoader* loader,
    const KURL& fetch_context_url,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const ResourceLoaderOptions& options,
    InspectorPageAgent::ResourceType type,
    base::TimeTicks timestamp) {
  String loader_id = IdentifiersFactory::LoaderId(loader);
  String request_id = RequestId(loader, request.InspectorId());
  NetworkResourcesData::ResourceData const* data =
      resources_data_->Data(request_id);
  // Support for POST request redirect.
  scoped_refptr<EncodedFormData> post_data;
  if (data &&
      (redirect_response.HttpStatusCode() == net::HTTP_TEMPORARY_REDIRECT ||
       redirect_response.HttpStatusCode() == net::HTTP_PERMANENT_REDIRECT)) {
    post_data = data->PostData();
  } else if (request.HttpBody()) {
    post_data = request.HttpBody()->DeepCopy();
  }

  resources_data_->ResourceCreated(request_id, loader_id, request.Url(),
                                   post_data);
  if (options.initiator_info.name ==
      fetch_initiator_type_names::kXmlhttprequest) {
    type = InspectorPageAgent::kXHRResource;
  } else if (options.initiator_info.name ==
             fetch_initiator_type_names::kFetch) {
    type = InspectorPageAgent::kFetchResource;
  } else if (options.initiator_info.name ==
                 fetch_initiator_type_names::kBeacon ||
             options.initiator_info.name == fetch_initiator_type_names::kPing) {
    type = InspectorPageAgent::kPingResource;
  }

  if (pending_request_type_)
    type = *pending_request_type_;
  resources_data_->SetResourceType(request_id, type);

  String frame_id = loader && loader->GetFrame()
                        ? IdentifiersFactory::FrameId(loader->GetFrame())
                        : "";
  std::unique_ptr<protocol::Network::Initiator> initiator_object =
      BuildInitiatorObject(
          loader && loader->GetFrame() ? loader->GetFrame()->GetDocument()
                                       : nullptr,
          options.initiator_info, std::numeric_limits<int>::max());

  std::unique_ptr<protocol::Network::Request> request_info(
      BuildObjectForResourceRequest(request, post_data,
                                    max_post_data_size_.Get()));

  // |loader| is null while inspecting worker.
  // TODO(horo): Refactor MixedContentChecker and set mixed content type even if
  // |loader| is null.
  if (loader) {
    request_info->setMixedContentType(MixedContentTypeForContextType(
        MixedContentChecker::ContextTypeForInspector(loader->GetFrame(),
                                                     request)));
  }

  request_info->setReferrerPolicy(
      GetReferrerPolicy(request.GetReferrerPolicy()));
  if (options.initiator_info.is_link_preload)
    request_info->setIsLinkPreload(true);

  String resource_type = InspectorPageAgent::ResourceTypeJson(type);
  String documentURL = loader
                           ? UrlWithoutFragment(loader->Url()).GetString()
                           : UrlWithoutFragment(fetch_context_url).GetString();
  Maybe<String> maybe_frame_id;
  if (!frame_id.empty())
    maybe_frame_id = frame_id;
  if (loader && loader->GetFrame() && loader->GetFrame()->GetDocument()) {
    request_info->setIsSameSite(
        loader->GetFrame()->GetDocument()->SiteForCookies().IsFirstParty(
            GURL(request.Url())));
  }
  GetFrontend()->requestWillBeSent(
      request_id, loader_id, documentURL, std::move(request_info),
      timestamp.since_origin().InSecondsF(),
      base::Time::Now().InSecondsFSinceUnixEpoch(), std::move(initiator_object),
      redirect_response.EmittedExtraInfo(),
      BuildObjectForResourceResponse(redirect_response,
                                     GetTargetExecutionContext()),
      resource_type, std::move(maybe_frame_id), request.HasUserGesture());
  if (options.synchronous_policy == SynchronousPolicy::kRequestSynchronously)
    GetFrontend()->flush();

  if (pending_xhr_replay_data_) {
    resources_data_->SetXHRReplayData(request_id,
                                      pending_xhr_replay_data_.Get());
    pending_xhr_replay_data_.Clear();
  }
  pending_request_type_ = std::nullopt;
}

void InspectorNetworkAgent::WillSendNavigationRequest(
    uint64_t identifier,
    DocumentLoader* loader,
    const KURL& url,
    const AtomicString& http_method,
    EncodedFormData* http_body) {
  String loader_id = IdentifiersFactory::LoaderId(loader);
  String request_id = loader_id;
  NetworkResourcesData::ResourceData const* data =
      resources_data_->Data(request_id);
  // Support for POST request redirect.
  scoped_refptr<EncodedFormData> post_data;
  if (data)
    post_data = data->PostData();
  else if (http_body)
    post_data = http_body->DeepCopy();
  resources_data_->ResourceCreated(request_id, loader_id, url, post_data);
  resources_data_->SetResourceType(request_id,
                                   InspectorPageAgent::kDocumentResource);
}

// This method was pulled out of PrepareRequest(), because we want to be able
// to create DevTools issues before the PrepareRequest() call. We need these
// IDs to be set, to properly create a DevTools issue.
void InspectorNetworkAgent::SetDevToolsIds(
    ResourceRequest& request,
    const FetchInitiatorInfo& initiator_info) {
  // Network instrumentation ignores the requests initiated internally (these
  // are unexpected to the user and usually do not hit the remote server).
  // Ignore them and do not set the devtools id, so that other systems like
  // network interceptor in the browser do not mistakenly report it.
  if (initiator_info.name == fetch_initiator_type_names::kInternal)
    return;
  request.SetDevToolsToken(devtools_token_);

  // The loader parameter is for generating a browser generated ID for a browser
  // initiated request. We pass it null here because we are reporting a renderer
  // generated ID for a renderer initiated request.
  request.SetDevToolsId(
      IdentifiersFactory::SubresourceRequestId(request.InspectorId()));
}

void InspectorNetworkAgent::PrepareRequest(DocumentLoader* loader,
                                           ResourceRequest& request,
                                           ResourceLoaderOptions& options,
                                           ResourceType resource_type) {
  // Ignore the request initiated internally.
  if (options.initiator_info.name == fetch_initiator_type_names::kInternal)
    return;

  if (!extra_request_headers_.IsEmpty()) {
    for (const WTF::String& key : extra_request_headers_.Keys()) {
      const WTF::String& value = extra_request_headers_.Get(key);
      AtomicString header_name = AtomicString(key);
      // When overriding referrer, also override referrer policy
      // for this request to assure the request will be allowed.
      // TODO: Should we store the referrer header somewhere other than
      // |extra_request_headers_|?
      if (EqualIgnoringASCIICase(header_name, http_names::kReferer)) {
        request.SetReferrerString(value);
        request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kAlways);
      } else {
        request.SetHttpHeaderField(header_name, AtomicString(value));
      }
    }
  }

  if (cache_disabled_.Get()) {
    if (LoadsFromCacheOnly(request) &&
        request.GetRequestContext() !=
            mojom::blink::RequestContextType::INTERNAL) {
      request.SetCacheMode(mojom::FetchCacheMode::kUnspecifiedForceCacheMiss);
    } else {
      request.SetCacheMode(mojom::FetchCacheMode::kBypassCache);
    }
  }
  if (bypass_service_worker_.Get())
    request.SetSkipServiceWorker(true);

  if (attach_debug_stack_enabled_.Get() &&
      // Preserving existing stack id when cloning requests instead of
      // overwriting
      !request.GetDevToolsStackId().has_value()) {
    ExecutionContext* context = nullptr;
    if (worker_or_worklet_global_scope_) {
      context = worker_or_worklet_global_scope_.Get();
    } else if (loader && loader->GetFrame()) {
      context = loader->GetFrame()->GetDocument()->domWindow();
    }
    String stack_id =
        RequestDebugHeaderScope::CaptureStackIdForCurrentLocation(context);
    if (!stack_id.IsNull()) {
      request.SetDevToolsStackId(stack_id);
    }
  }
  if (!accepted_encodings_.IsEmpty()) {
    scoped_refptr<
        base::RefCountedData<base::flat_set<net::SourceStream::SourceType>>>
        accepted_stream_types = request.GetDevToolsAcceptedStreamTypes();
    if (!accepted_stream_types) {
      accepted_stream_types = base::MakeRefCounted<base::RefCountedData<
          base::flat_set<net::SourceStream::SourceType>>>();
    }
    if (!accepted_encodings_.Get("none")) {
      for (auto key : accepted_encodings_.Keys())
        accepted_stream_types->data.insert(SourceTypeFromString(key));
    }
    request.SetDevToolsAcceptedStreamTypes(std::move(accepted_stream_types));
  }
}

void InspectorNetworkAgent::WillSendRequest(
    ExecutionContext*,
    DocumentLoader* loader,
    const KURL& fetch_context_url,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const ResourceLoaderOptions& options,
    ResourceType resource_type,
    RenderBlockingBehavior render_blocking_behavior,
    base::TimeTicks timestamp) {
  // Ignore the request initiated internally.
  if (options.initiator_info.name == fetch_initiator_type_names::kInternal)
    return;

  InspectorPageAgent::ResourceType type =
      InspectorPageAgent::ToResourceType(resource_type);

  WillSendRequestInternal(loader, fetch_context_url, request, redirect_response,
                          options, type, timestamp);
}

void InspectorNetworkAgent::MarkResourceAsCached(DocumentLoader* loader,
                                                 uint64_t identifier) {
  GetFrontend()->requestServedFromCache(RequestId(loader, identifier));
}

void InspectorNetworkAgent::DidReceiveResourceResponse(
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceResponse& response,
    const Resource* cached_resource) {
  String request_id = RequestId(loader, identifier);
  bool is_not_modified = response.HttpStatusCode() == 304;

  bool resource_is_empty = true;
  std::unique_ptr<protocol::Network::Response> resource_response =
      BuildObjectForResourceResponse(response, GetTargetExecutionContext(),
                                     cached_resource, &resource_is_empty);

  InspectorPageAgent::ResourceType type =
      cached_resource
          ? InspectorPageAgent::ToResourceType(cached_resource->GetType())
          : InspectorPageAgent::kOtherResource;
  // Override with already discovered resource type.
  InspectorPageAgent::ResourceType saved_type =
      resources_data_->GetResourceType(request_id);
  if (saved_type == InspectorPageAgent::kScriptResource ||
      saved_type == InspectorPageAgent::kXHRResource ||
      saved_type == InspectorPageAgent::kDocumentResource ||
      saved_type == InspectorPageAgent::kFetchResource ||
      saved_type == InspectorPageAgent::kEventSourceResource ||
      saved_type == InspectorPageAgent::kPingResource) {
    type = saved_type;
  }

  // Main Worker requests are initiated in the browser, so saved_type will not
  // be found. We therefore must explicitly set it.
  if (worker_or_worklet_global_scope_ &&
      worker_or_worklet_global_scope_->IsWorkerGlobalScope()) {
    WorkerGlobalScope* worker_global_scope =
        To<WorkerGlobalScope>(worker_or_worklet_global_scope_.Get());
    auto main_resource_identifier =
        worker_global_scope->MainResourceIdentifier();

    if (main_resource_identifier == identifier) {
      DCHECK(saved_type == InspectorPageAgent::kOtherResource);
      type = InspectorPageAgent::kScriptResource;
    }
  }

  // Resources are added to NetworkResourcesData as a WeakMember here and
  // removed in willDestroyResource() called in the prefinalizer of Resource.
  // Because NetworkResourceData retains weak references only, it
  // doesn't affect Resource lifetime.
  if (cached_resource)
    resources_data_->AddResource(request_id, cached_resource);
  String frame_id = loader && loader->GetFrame()
                        ? IdentifiersFactory::FrameId(loader->GetFrame())
                        : "";
  String loader_id = IdentifiersFactory::LoaderId(loader);
  resources_data_->SetResourceType(request_id, type);
  resources_data_->ResponseReceived(request_id, frame_id, response);

  const std::optional<net::SSLInfo>& ssl_info = response.GetSSLInfo();
  if (ssl_info.has_value() && ssl_info->cert) {
    resources_data_->SetCertificate(request_id, ssl_info->cert);
  }

  if (IsNavigation(loader, identifier))
    return;
  if (resource_response && !resource_is_empty) {
    Maybe<String> maybe_frame_id;
    if (!frame_id.empty())
      maybe_frame_id = frame_id;
    GetFrontend()->responseReceived(
        request_id, loader_id,
        base::TimeTicks::Now().since_origin().InSecondsF(),
        InspectorPageAgent::ResourceTypeJson(type),
        std::move(resource_response), response.EmittedExtraInfo(),
        std::move(maybe_frame_id));
  }
  // If we revalidated the resource and got Not modified, send content length
  // following didReceiveResponse as there will be no calls to didReceiveData
  // from the network stack.
  if (is_not_modified && cached_resource && cached_resource->EncodedSize()) {
    DidReceiveData(
        identifier, loader,
        base::SpanOrSize<const char>(cached_resource->EncodedSize()));
  }
}

static bool IsErrorStatusCode(int status_code) {
  return status_code >= 400;
}

protocol::Response InspectorNetworkAgent::streamResourceContent(
    const String& request_id,
    protocol::Binary* buffered_data) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);

  if (!resource_data) {
    return protocol::Response::InvalidParams(
        "Request with the provided ID does not exists");
  }

  if (resource_data->HasContent()) {
    return protocol::Response::InvalidParams(
        "Request with the provided ID has already finished loading");
  }

  streaming_request_ids_.insert(request_id);

  const std::optional<SegmentedBuffer>& data = resource_data->Data();
  if (data) {
    *buffered_data =
        protocol::Binary::fromVector(data->CopyAs<Vector<uint8_t>>());
  }
  return protocol::Response::Success();
}

void InspectorNetworkAgent::DidReceiveData(uint64_t identifier,
                                           DocumentLoader* loader,
                                           base::SpanOrSize<const char> data) {
  String request_id = RequestId(loader, identifier);
  Maybe<protocol::Binary> binary_data;

  if (auto data_span = data.span(); data_span) {
    NetworkResourcesData::ResourceData const* resource_data =
        resources_data_->Data(request_id);
    if (resource_data && !resource_data->HasContent() &&
        (!resource_data->CachedResource() ||
         resource_data->CachedResource()->GetDataBufferingPolicy() ==
             kDoNotBufferData ||
         IsErrorStatusCode(resource_data->HttpStatusCode()))) {
      resources_data_->MaybeAddResourceData(request_id, *data_span);
    }

    if (streaming_request_ids_.Contains(request_id)) {
      binary_data = protocol::Binary::fromSpan(base::as_bytes(*data_span));
    }
  }

  GetFrontend()->dataReceived(
      request_id, base::TimeTicks::Now().since_origin().InSecondsF(),
      static_cast<int>(data.size()),
      static_cast<int>(
          resources_data_->GetAndClearPendingEncodedDataLength(request_id)),
      std::move(binary_data));
}

void InspectorNetworkAgent::DidReceiveBlob(uint64_t identifier,
                                           DocumentLoader* loader,
                                           scoped_refptr<BlobDataHandle> blob) {
  String request_id = RequestId(loader, identifier);
  resources_data_->BlobReceived(request_id, std::move(blob));
}

void InspectorNetworkAgent::DidReceiveEncodedDataLength(
    DocumentLoader* loader,
    uint64_t identifier,
    size_t encoded_data_length) {
  String request_id = RequestId(loader, identifier);
  resources_data_->AddPendingEncodedDataLength(request_id, encoded_data_length);
}

void InspectorNetworkAgent::DidFinishLoading(
    uint64_t identifier,
    DocumentLoader* loader,
    base::TimeTicks monotonic_finish_time,
    int64_t encoded_data_length,
    int64_t decoded_body_length) {
  String request_id = RequestId(loader, identifier);
  streaming_request_ids_.erase(request_id);

  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);

  int pending_encoded_data_length = static_cast<int>(
      resources_data_->GetAndClearPendingEncodedDataLength(request_id));
  if (pending_encoded_data_length > 0) {
    GetFrontend()->dataReceived(
        request_id, base::TimeTicks::Now().since_origin().InSecondsF(), 0,
        pending_encoded_data_length);
  }

  if (resource_data && !resource_data->HasContent() &&
      (!resource_data->CachedResource() ||
       resource_data->CachedResource()->GetDataBufferingPolicy() ==
           kDoNotBufferData ||
       IsErrorStatusCode(resource_data->HttpStatusCode()))) {
    resources_data_->MaybeAddResourceData(request_id,
                                          base::span_from_cstring(""));
  }

  resources_data_->MaybeDecodeDataToContent(request_id);
  if (monotonic_finish_time.is_null())
    monotonic_finish_time = base::TimeTicks::Now();

  // TODO(npm): Use base::TimeTicks in Network.h.
  GetFrontend()->loadingFinished(
      request_id, monotonic_finish_time.since_origin().InSecondsF(),
      encoded_data_length);
}

void InspectorNetworkAgent::DidReceiveCorsRedirectResponse(
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceResponse& response,
    Resource* resource) {
  // Update the response and finish loading
  DidReceiveResourceResponse(identifier, loader, response, resource);
  DidFinishLoading(identifier, loader, base::TimeTicks(),
                   URLLoaderClient::kUnknownEncodedDataLength, 0);
}

void InspectorNetworkAgent::DidFailLoading(
    CoreProbeSink* sink,
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceError& error,
    const base::UnguessableToken& devtools_frame_or_worker_token) {
  String request_id = RequestId(loader, identifier);
  streaming_request_ids_.erase(request_id);

  // A Trust Token redemption can be served from cache if a valid
  // Signed-Redemption-Record is present. In this case the request is aborted
  // with a special error code. Sementically, the request did succeed, so that
  // is what we report to the frontend.
  if (error.IsTrustTokenCacheHit()) {
    GetFrontend()->requestServedFromCache(request_id);
    GetFrontend()->loadingFinished(
        request_id, base::TimeTicks::Now().since_origin().InSecondsF(), 0);
    return;
  }

  bool canceled = error.IsCancellation();

  protocol::Maybe<String> blocked_reason = BuildBlockedReason(error);
  auto cors_error_status = error.CorsErrorStatus();
  protocol::Maybe<protocol::Network::CorsErrorStatus>
      protocol_cors_error_status;
  if (cors_error_status) {
    protocol_cors_error_status = BuildCorsErrorStatus(*cors_error_status);
  }
  GetFrontend()->loadingFailed(
      request_id, base::TimeTicks::Now().since_origin().InSecondsF(),
      InspectorPageAgent::ResourceTypeJson(
          resources_data_->GetResourceType(request_id)),
      error.LocalizedDescription(), canceled, std::move(blocked_reason),
      std::move(protocol_cors_error_status));
}

void InspectorNetworkAgent::ScriptImported(uint64_t identifier,
                                           const String& source_string) {
  resources_data_->SetResourceContent(
      IdentifiersFactory::SubresourceRequestId(identifier), source_string);
}

void InspectorNetworkAgent::DidReceiveScriptResponse(uint64_t identifier) {
  resources_data_->SetResourceType(
      IdentifiersFactory::SubresourceRequestId(identifier),
      InspectorPageAgent::kScriptResource);
}

// static
bool InspectorNetworkAgent::IsNavigation(DocumentLoader* loader,
                                         uint64_t identifier) {
  return loader && loader->MainResourceIdentifier() == identifier;
}

void InspectorNetworkAgent::WillLoadXHR(ExecutionContext* execution_context,
                                        const AtomicString& method,
                                        const KURL& url,
                                        bool async,
                                        const HTTPHeaderMap& headers,
                                        bool include_credentials) {
  DCHECK(!pending_request_type_);
  pending_xhr_replay_data_ = MakeGarbageCollected<XHRReplayData>(
      execution_context, method, UrlWithoutFragment(url), async,
      include_credentials);
  for (const auto& header : headers)
    pending_xhr_replay_data_->AddHeader(header.key, header.value);
}

void InspectorNetworkAgent::DidFinishXHR(XMLHttpRequest* xhr) {
  replay_xhrs_.erase(xhr);
}

void InspectorNetworkAgent::WillSendEventSourceRequest() {
  DCHECK(!pending_request_type_);
  pending_request_type_ = InspectorPageAgent::kEventSourceResource;
}

void InspectorNetworkAgent::WillDispatchEventSourceEvent(
    uint64_t identifier,
    const AtomicString& event_name,
    const AtomicString& event_id,
    const String& data) {
  GetFrontend()->eventSourceMessageReceived(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      event_name.GetString(), event_id.GetString(), data);
}

std::unique_ptr<protocol::Network::Initiator>
InspectorNetworkAgent::BuildInitiatorObject(
    Document* document,
    const FetchInitiatorInfo& initiator_info,
    int max_async_depth) {
  if (initiator_info.is_imported_module && !initiator_info.referrer.empty()) {
    std::unique_ptr<protocol::Network::Initiator> initiator_object =
        protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Script)
            .build();
    initiator_object->setUrl(initiator_info.referrer);
    initiator_object->setLineNumber(
        initiator_info.position.line_.ZeroBasedInt());
    initiator_object->setColumnNumber(
        initiator_info.position.column_.ZeroBasedInt());
    return initiator_object;
  }

  bool was_requested_by_stylesheet =
      initiator_info.name == fetch_initiator_type_names::kCSS ||
      initiator_info.name == fetch_initiator_type_names::kUacss;
  if (was_requested_by_stylesheet && !initiator_info.referrer.empty()) {
    std::unique_ptr<protocol::Network::Initiator> initiator_object =
        protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Parser)
            .build();
    if (initiator_info.position != TextPosition::BelowRangePosition()) {
      initiator_object->setLineNumber(
          initiator_info.position.line_.ZeroBasedInt());
      initiator_object->setColumnNumber(
          initiator_info.position.column_.ZeroBasedInt());
    }
    initiator_object->setUrl(initiator_info.referrer);
    return initiator_object;
  }

  // We skip stack checking for stylesheet-initiated requests as it may
  // represent the cause of a style recalculation rather than the actual
  // resources themselves. See crbug.com/918196.
  if (!was_requested_by_stylesheet) {
    std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
        current_stack_trace =
            CaptureSourceLocation(document ? document->GetExecutionContext()
                                           : nullptr)
                ->BuildInspectorObject(max_async_depth);
    if (current_stack_trace) {
      std::unique_ptr<protocol::Network::Initiator> initiator_object =
          protocol::Network::Initiator::create()
              .setType(protocol::Network::Initiator::TypeEnum::Script)
              .build();
      if (initiator_info.position != TextPosition::BelowRangePosition()) {
        initiator_object->setLineNumber(
            initiator_info.position.line_.ZeroBasedInt());
        initiator_object->setColumnNumber(
            initiator_info.position.column_.ZeroBasedInt());
      }
      initiator_object->setStack(std::move(current_stack_trace));
      return initiator_object;
    }
  }

  while (document && !document->GetScriptableDocumentParser())
    document = document->LocalOwner() ? document->LocalOwner()->ownerDocument()
                                      : nullptr;
  if (document && document->GetScriptableDocumentParser()) {
    std::unique_ptr<protocol::Network::Initiator> initiator_object =
        protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Parser)
            .build();
    initiator_object->setUrl(UrlWithoutFragment(document->Url()).GetString());
    if (TextPosition::BelowRangePosition() != initiator_info.position) {
      initiator_object->setLineNumber(
          initiator_info.position.line_.ZeroBasedInt());
      initiator_object->setColumnNumber(
          initiator_info.position.column_.ZeroBasedInt());
    } else {
      initiator_object->setLineNumber(document->GetScriptableDocumentParser()
                                          ->GetTextPosition()
                                          .line_.ZeroBasedInt());
      initiator_object->setColumnNumber(document->GetScriptableDocumentParser()
                                            ->GetTextPosition()
                                            .column_.ZeroBasedInt());
    }
    return initiator_object;
  }

  return protocol::Network::Initiator::create()
      .setType(protocol::Network::Initiator::TypeEnum::Other)
      .build();
}

String InspectorNetworkAgent::GetProtocolAsString(
    const ResourceResponse& response) {
  String protocol = response.AlpnNegotiatedProtocol();
  if (protocol.empty() || protocol == "unknown") {
    if (response.WasFetchedViaSPDY()) {
      protocol = "h2";
    } else if (response.IsHTTP()) {
      protocol = "http";
      if (response.HttpVersion() ==
          ResourceResponse::HTTPVersion::kHTTPVersion_0_9) {
        protocol = "http/0.9";
      } else if (response.HttpVersion() ==
                 ResourceResponse::HTTPVersion::kHTTPVersion_1_0) {
        protocol = "http/1.0";
      } else if (response.HttpVersion() ==
                 ResourceResponse::HTTPVersion::kHTTPVersion_1_1) {
        protocol = "http/1.1";
      }
    } else {
      protocol = response.CurrentRequestUrl().Protocol();
    }
  }
  return protocol;
}

void InspectorNetworkAgent::WillCreateP2PSocketUdp(
    std::optional<base::UnguessableToken>* devtools_token) {
  *devtools_token = devtools_token_;
}

void InspectorNetworkAgent::WillCreateWebSocket(
    ExecutionContext* execution_context,
    uint64_t identifier,
    const KURL& request_url,
    const String&,
    std::optional<base::UnguessableToken>* devtools_token) {
  *devtools_token = devtools_token_;
  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
      current_stack_trace =
          CaptureSourceLocation(execution_context)->BuildInspectorObject();
  if (!current_stack_trace) {
    GetFrontend()->webSocketCreated(
        IdentifiersFactory::SubresourceRequestId(identifier),
        UrlWithoutFragment(request_url).GetString());
    return;
  }

  std::unique_ptr<protocol::Network::Initiator> initiator_object =
      protocol::Network::Initiator::create()
          .setType(protocol::Network::Initiator::TypeEnum::Script)
          .build();
  initiator_object->setStack(std::move(current_stack_trace));
  GetFrontend()->webSocketCreated(
      IdentifiersFactory::SubresourceRequestId(identifier),
      UrlWithoutFragment(request_url).GetString(), std::move(initiator_object));
}

void InspectorNetworkAgent::WillSendWebSocketHandshakeRequest(
    ExecutionContext*,
    uint64_t identifier,
    network::mojom::blink::WebSocketHandshakeRequest* request) {
  DCHECK(request);
  HTTPHeaderMap headers;
  for (auto& header : request->headers)
    headers.Add(AtomicString(header->name), AtomicString(header->value));
  std::unique_ptr<protocol::Network::WebSocketRequest> request_object =
      protocol::Network::WebSocketRequest::create()
          .setHeaders(BuildObjectForHeaders(headers))
          .build();
  GetFrontend()->webSocketWillSendHandshakeRequest(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      base::Time::Now().InSecondsFSinceUnixEpoch(), std::move(request_object));
}

void InspectorNetworkAgent::DidReceiveWebSocketHandshakeResponse(
    ExecutionContext*,
    uint64_t identifier,
    network::mojom::blink::WebSocketHandshakeRequest* request,
    network::mojom::blink::WebSocketHandshakeResponse* response) {
  DCHECK(response);

  HTTPHeaderMap response_headers;
  for (auto& header : response->headers) {
    HTTPHeaderMap::AddResult add_result = response_headers.Add(
        AtomicString(header->name), AtomicString(header->value));
    if (!add_result.is_new_entry) {
      // Protocol expects the "\n" separated format.
      add_result.stored_value->value =
          add_result.stored_value->value + "\n" + header->value;
    }
  }

  std::unique_ptr<protocol::Network::WebSocketResponse> response_object =
      protocol::Network::WebSocketResponse::create()
          .setStatus(response->status_code)
          .setStatusText(response->status_text)
          .setHeaders(BuildObjectForHeaders(response_headers))
          .build();
  if (!response->headers_text.empty())
    response_object->setHeadersText(response->headers_text);

  if (request) {
    HTTPHeaderMap request_headers;
    for (auto& header : request->headers) {
      request_headers.Add(AtomicString(header->name),
                          AtomicString(header->value));
    }
    response_object->setRequestHeaders(BuildObjectForHeaders(request_headers));
    if (!request->headers_text.empty())
      response_object->setRequestHeadersText(request->headers_text);
  }

  GetFrontend()->webSocketHandshakeResponseReceived(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      std::move(response_object));
}

void InspectorNetworkAgent::DidCloseWebSocket(ExecutionContext*,
                                              uint64_t identifier) {
  GetFrontend()->webSocketClosed(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF());
}

void InspectorNetworkAgent::DidReceiveWebSocketMessage(
    uint64_t identifier,
    int op_code,
    bool masked,
    const Vector<base::span<const char>>& data) {
  size_t size = 0;
  for (const auto& span : data) {
    size += span.size();
  }
  Vector<char> flatten;
  flatten.reserve(base::checked_cast<wtf_size_t>(size));
  for (const auto& span : data) {
    flatten.AppendSpan(span);
  }
  GetFrontend()->webSocketFrameReceived(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      WebSocketMessageToProtocol(op_code, masked, flatten));
}

void InspectorNetworkAgent::DidSendWebSocketMessage(
    uint64_t identifier,
    int op_code,
    bool masked,
    base::span<const char> payload) {
  GetFrontend()->webSocketFrameSent(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      WebSocketMessageToProtocol(op_code, masked, payload));
}

void InspectorNetworkAgent::DidReceiveWebSocketMessageError(
    uint64_t identifier,
    const String& error_message) {
  GetFrontend()->webSocketFrameError(
      IdentifiersFactory::SubresourceRequestId(identifier),
      base::TimeTicks::Now().since_origin().InSecondsF(), error_message);
}

void InspectorNetworkAgent::WebTransportCreated(
    ExecutionContext* execution_context,
    uint64_t transport_id,
    const KURL& request_url) {
  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
      current_stack_trace =
          CaptureSourceLocation(execution_context)->BuildInspectorObject();
  if (!current_stack_trace) {
    GetFrontend()->webTransportCreated(
        IdentifiersFactory::SubresourceRequestId(transport_id),
        UrlWithoutFragment(request_url).GetString(),
        base::TimeTicks::Now().since_origin().InSecondsF());
    return;
  }

  std::unique_ptr<protocol::Network::Initiator> initiator_object =
      protocol::Network::Initiator::create()
          .setType(protocol::Network::Initiator::TypeEnum::Script)
          .build();
  initiator_object->setStack(std::move(current_stack_trace));
  GetFrontend()->webTransportCreated(
      IdentifiersFactory::SubresourceRequestId(transport_id),
      UrlWithoutFragment(request_url).GetString(),
      base::TimeTicks::Now().since_origin().InSecondsF(),
      std::move(initiator_object));
}

void InspectorNetworkAgent::WebTransportConnectionEstablished(
    uint64_t transport_id) {
  GetFrontend()->webTransportConnectionEstablished(
      IdentifiersFactory::SubresourceRequestId(transport_id),
      base::TimeTicks::Now().since_origin().InSecondsF());
}

void InspectorNetworkAgent::WebTransportClosed(uint64_t transport_id) {
  GetFrontend()->webTransportClosed(
      IdentifiersFactory::SubresourceRequestId(transport_id),
      base::TimeTicks::Now().since_origin().InSecondsF());
}

protocol::Response InspectorNetworkAgent::enable(
    Maybe<int> total_buffer_size,
    Maybe<int> resource_buffer_size,
    Maybe<int> max_post_data_size) {
  total_buffer_size_.Set(total_buffer_size.value_or(kDefaultTotalBufferSize));
  resource_buffer_size_.Set(
      resource_buffer_size.value_or(kDefaultResourceBufferSize));
  max_post_data_size_.Set(max_post_data_size.value_or(0));
  Enable();
  return protocol::Response::Success();
}

void InspectorNetworkAgent::Enable() {
  if (!GetFrontend())
    return;
  enabled_.Set(true);
  resources_data_->SetResourcesDataSizeLimits(total_buffer_size_.Get(),
                                              resource_buffer_size_.Get());
  instrumenting_agents_->AddInspectorNetworkAgent(this);
}

protocol::Response InspectorNetworkAgent::disable() {
  DCHECK(!pending_request_type_);
  if (IsMainThread())
    GetNetworkStateNotifier().ClearOverride();
  instrumenting_agents_->RemoveInspectorNetworkAgent(this);
  agent_state_.ClearAllFields();
  resources_data_->Clear();
  streaming_request_ids_.clear();
  clearAcceptedEncodingsOverride();
  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::setExtraHTTPHeaders(
    std::unique_ptr<protocol::Network::Headers> headers) {
  extra_request_headers_.Clear();
  std::unique_ptr<protocol::DictionaryValue> in = headers->toValue();
  for (size_t i = 0; i < in->size(); ++i) {
    const auto& entry = in->at(i);
    String value;
    if (entry.second && entry.second->asString(&value))
      extra_request_headers_.Set(entry.first, value);
  }
  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::setAttachDebugStack(bool enabled) {
  if (enabled && !enabled_.Get())
    return protocol::Response::InvalidParams("Domain must be enabled");
  attach_debug_stack_enabled_.Set(enabled);
  return protocol::Response::Success();
}

bool InspectorNetworkAgent::CanGetResponseBodyBlob(const String& request_id) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);
  BlobDataHandle* blob =
      resource_data ? resource_data->DownloadedFileBlob() : nullptr;
  if (!blob)
    return false;
  if (worker_or_worklet_global_scope_) {
    return true;
  }
  LocalFrame* frame = IdentifiersFactory::FrameById(inspected_frames_,
                                                    resource_data->FrameId());
  return frame && frame->GetDocument();
}

void InspectorNetworkAgent::GetResponseBodyBlob(
    const String& request_id,
    std::unique_ptr<GetResponseBodyCallback> callback) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);
  BlobDataHandle* blob = resource_data->DownloadedFileBlob();
  ExecutionContext* context = GetTargetExecutionContext();
  if (!context) {
    callback->sendFailure(protocol::Response::InternalError());
    return;
  }
  InspectorFileReaderLoaderClient* client =
      MakeGarbageCollected<InspectorFileReaderLoaderClient>(
          blob, context->GetTaskRunner(TaskType::kFileReading),
          WTF::BindOnce(
              ResponseBodyFileReaderLoaderDone, resource_data->MimeType(),
              resource_data->TextEncodingName(), std::move(callback)));
  client->Start();
}

void InspectorNetworkAgent::getResponseBody(
    const String& request_id,
    std::unique_ptr<GetResponseBodyCallback> callback) {
  if (CanGetResponseBodyBlob(request_id)) {
    GetResponseBodyBlob(request_id, std::move(callback));
    return;
  }

  String content;
  bool base64_encoded;
  protocol::Response response =
      GetResponseBody(request_id, &content, &base64_encoded);
  if (response.IsSuccess()) {
    callback->sendSuccess(content, base64_encoded);
  } else {
    callback->sendFailure(response);
  }
}

protocol::Response InspectorNetworkAgent::setBlockedURLs(
    std::unique_ptr<protocol::Array<String>> urls) {
  blocked_urls_.Clear();
  for (const String& url : *urls)
    blocked_urls_.Set(url, true);
  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::replayXHR(const String& request_id) {
  String actual_request_id = request_id;

  XHRReplayData* xhr_replay_data = resources_data_->XhrReplayData(request_id);
  auto* data = resources_data_->Data(request_id);
  if (!xhr_replay_data || !data) {
    return protocol::Response::ServerError(
        "Given id does not correspond to XHR");
  }

  ExecutionContext* execution_context = xhr_replay_data->GetExecutionContext();
  if (!execution_context || execution_context->IsContextDestroyed()) {
    resources_data_->SetXHRReplayData(request_id, nullptr);
    return protocol::Response::ServerError("Document is already detached");
  }

  XMLHttpRequest* xhr = XMLHttpRequest::Create(execution_context);

  execution_context->RemoveURLFromMemoryCache(xhr_replay_data->Url());

  xhr->open(xhr_replay_data->Method(), xhr_replay_data->Url(),
            xhr_replay_data->Async(), IGNORE_EXCEPTION_FOR_TESTING);
  if (xhr_replay_data->IncludeCredentials())
    xhr->setWithCredentials(true, IGNORE_EXCEPTION_FOR_TESTING);
  for (const auto& header : xhr_replay_data->Headers()) {
    xhr->setRequestHeader(header.key, header.value,
                          IGNORE_EXCEPTION_FOR_TESTING);
  }
  xhr->SendForInspectorXHRReplay(data ? data->PostData() : nullptr,
                                 IGNORE_EXCEPTION_FOR_TESTING);

  replay_xhrs_.insert(xhr);
  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::canClearBrowserCache(bool* result) {
  *result = true;
  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::canClearBrowserCookies(bool* result) {
  *result = true;
  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::setAcceptedEncodings(
    std::unique_ptr<protocol::Array<protocol::Network::ContentEncoding>>
        encodings) {
  HashSet<String> accepted_encodings;
  for (const protocol::Network::ContentEncoding& encoding : *encodings) {
    String value = AcceptedEncodingFromProtocol(encoding);
    if (value.IsNull()) {
      return protocol::Response::InvalidParams("Unknown encoding type: " +
                                               encoding.Utf8());
    }
    accepted_encodings.insert(value);
  }
  // If invoked with an empty list, it means none of the encodings should be
  // accepted. See InspectorNetworkAgent::PrepareRequest.
  if (accepted_encodings.empty())
    accepted_encodings.insert("none");

  // Set the inspector state.
  accepted_encodings_.Clear();
  for (auto encoding : accepted_encodings)
    accepted_encodings_.Set(encoding, true);

  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::clearAcceptedEncodingsOverride() {
  accepted_encodings_.Clear();
  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::emulateNetworkConditions(
    bool offline,
    double latency,
    double download_throughput,
    double upload_throughput,
    Maybe<String> connection_type,
    Maybe<double> packet_loss,
    Maybe<int> packet_queue_length,
    Maybe<bool> packet_reordering) {
  WebConnectionType type = kWebConnectionTypeUnknown;
  if (connection_type.has_value()) {
    type = ToWebConnectionType(connection_type.value());
    if (type == kWebConnectionTypeUnknown)
      return protocol::Response::ServerError("Unknown connection type");
  }

  if (worker_or_worklet_global_scope_) {
    if (worker_or_worklet_global_scope_->IsServiceWorkerGlobalScope() ||
        worker_or_worklet_global_scope_->IsSharedWorkerGlobalScope()) {
      // In service workers and shared workers, we don't inspect the main thread
      // so we must post a task there to make it possible to use
      // NetworkStateNotifier.
      PostCrossThreadTask(
          *Thread::MainThread()->GetTaskRunner(
              MainThreadTaskRunnerRestricted()),
          FROM_HERE,
          CrossThreadBindOnce(SetNetworkStateOverride, offline, latency,
                              download_throughput, upload_throughput, type));
      return protocol::Response::Success();
    }
    return protocol::Response::ServerError("Not supported");
  }

  SetNetworkStateOverride(offline, latency, download_throughput,
                          upload_throughput, type);

  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::setCacheDisabled(
    bool cache_disabled) {
  // TODO(ananta)
  // We should extract network cache state into a global entity which can be
  // queried from FrameLoader and other places.
  cache_disabled_.Set(cache_disabled);
  if (cache_disabled && IsMainThread())
    MemoryCache::Get()->EvictResources();
  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::setBypassServiceWorker(bool bypass) {
  bypass_service_worker_.Set(bypass);
  return protocol::Response::Success();
}

protocol::Response InspectorNetworkAgent::getCertificate(
    const String& origin,
    std::unique_ptr<protocol::Array<String>>* certificate) {
  *certificate = std::make_unique<protocol::Array<String>>();
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::CreateFromString(origin);
  for (auto& resource : resources_data_->Resources()) {
    scoped_refptr<const SecurityOrigin> resource_origin =
        SecurityOrigin::Create(resource->RequestedURL());
    net::X509Certificate* cert = resource->Certificate();
    if (resource_origin->IsSameOriginWith(security_origin.get()) && cert) {
      (*certificate)
          ->push_back(Base64Encode(
              net::x509_util::CryptoBufferAsSpan(cert->cert_buffer())));
      for (const auto& buf : cert->intermediate_buffers()) {
        (*certificate)
            ->push_back(
                Base64Encode(net::x509_util::CryptoBufferAsSpan(buf.get())));
      }
      return protocol::Response::Success();
    }
  }
  return protocol::Response::Success();
}

void InspectorNetworkAgent::DidCommitLoad(LocalFrame* frame,
                                          DocumentLoader* loader) {
  DCHECK(IsMainThread());
  if (loader->GetFrame() != inspected_frames_->Root())
    return;

  if (cache_disabled_.Get())
    MemoryCache::Get()->EvictResources();

  resources_data_->Clear(IdentifiersFactory::LoaderId(loader));
}

void InspectorNetworkAgent::FrameScheduledNavigation(LocalFrame* frame,
                                                     const KURL&,
                                                     base::TimeDelta,
                                                     ClientNavigationReason) {
  // For navigations, we limit async stack trace to depth 1 to avoid the
  // base::Value depth limits with Mojo serialization / parsing.
  // See http://crbug.com/809996.
  frame_navigation_initiator_map_.Set(
      IdentifiersFactory::FrameId(frame),
      BuildInitiatorObject(frame->GetDocument(), FetchInitiatorInfo(),
                           /*max_async_depth=*/1));
}

void InspectorNetworkAgent::FrameClearedScheduledNavigation(LocalFrame* frame) {
  frame_navigation_initiator_map_.erase(IdentifiersFactory::FrameId(frame));
}

protocol::Response InspectorNetworkAgent::GetResponseBody(
    const String& request_id,
    String* content,
    bool* base64_encoded) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);
  if (!resource_data) {
    return protocol::Response::ServerError(
        "No resource with given identifier found");
  }

  if (resource_data->HasContent()) {
    *content = resource_data->Content();
    *base64_encoded = resource_data->Base64Encoded();
    return protocol::Response::Success();
  }

  if (resource_data->IsContentEvicted()) {
    return protocol::Response::ServerError(
        "Request content was evicted from inspector cache");
  }

  if (resource_data->CachedResource() &&
      InspectorPageAgent::CachedResourceContent(resource_data->CachedResource(),
                                                content, base64_encoded)) {
    return protocol::Response::Success();
  }

  return protocol::Response::ServerError(
      "No data found for resource with given identifier");
}

protocol::Response InspectorNetworkAgent::searchInResponseBody(
    const String& request_id,
    const String& query,
    Maybe<bool> case_sensitive,
    Maybe<bool> is_regex,
    std::unique_ptr<
        protocol::Array<v8_inspector::protocol::Debugger::API::SearchMatch>>*
        matches) {
  String content;
  bool base64_encoded;
  protocol::Response response =
      GetResponseBody(request_id, &content, &base64_encoded);
  if (!response.IsSuccess())
    return response;

  auto results = v8_session_->searchInTextByLines(
      ToV8InspectorStringView(content), ToV8InspectorStringView(query),
      case_sensitive.value_or(false), is_regex.value_or(false));
  *matches = std::make_unique<
      protocol::Array<v8_inspector::protocol::Debugger::API::SearchMatch>>(
      std::move(results));
  return protocol::Response::Success();
}

bool InspectorNetworkAgent::FetchResourceContent(Document* document,
                                                 const KURL& url,
                                                 String* content,
                                                 bool* base64_encoded,
                                                 bool* loadingFailed) {
  DCHECK(document);
  DCHECK(IsMainThread());
  // First try to fetch content from the cached resource.
  Resource* cached_resource = document->Fetcher()->CachedResource(url);
  if (!cached_resource) {
    cached_resource = MemoryCache::Get()->ResourceForURL(
        url, document->Fetcher()->GetCacheIdentifier(url));
  }
  if (cached_resource && InspectorPageAgent::CachedResourceContent(
                             cached_resource, content, base64_encoded)) {
    *loadingFailed = cached_resource->ErrorOccurred();
    return true;
  }

  // Then fall back to resource data.
  for (auto& resource : resources_data_->Resources()) {
    if (resource->RequestedURL() == url) {
      *content = resource->Content();
      *base64_encoded = resource->Base64Encoded();
      *loadingFailed = IsErrorStatusCode(resource->HttpStatusCode());

      return true;
    }
  }
  return false;
}

String InspectorNetworkAgent::NavigationInitiatorInfo(LocalFrame* frame) {
  if (!enabled_.Get())
    return String();
  auto it =
      frame_navigation_initiator_map_.find(IdentifiersFactory::FrameId(frame));
  std::vector<uint8_t> cbor;
  if (it != frame_navigation_initiator_map_.end()) {
    it->value->AppendSerialized(&cbor);
  } else {
    // For navigations, we limit async stack trace to depth 1 to avoid the
    // base::Value depth limits with Mojo serialization / parsing.
    // See http://crbug.com/809996.
    BuildInitiatorObject(frame->GetDocument(), FetchInitiatorInfo(),
                         /*max_async_depth=*/1)
        ->AppendSerialized(&cbor);
  }
  std::vector<uint8_t> json;
  ConvertCBORToJSON(SpanFrom(cbor), &json);
  return String(reinterpret_cast<const char*>(json.data()), json.size());
}

InspectorNetworkAgent::InspectorNetworkAgent(
    InspectedFrames* inspected_frames,
    WorkerOrWorkletGlobalScope* worker_or_worklet_global_scope,
    v8_inspector::V8InspectorSession* v8_session)
    : inspected_frames_(inspected_frames),
      worker_or_worklet_global_scope_(worker_or_worklet_global_scope),
      v8_session_(v8_session),
      resources_data_(MakeGarbageCollected<NetworkResourcesData>(
          kDefaultTotalBufferSize,
          kDefaultResourceBufferSize)),
      devtools_token_(
          worker_or_worklet_global_scope_
              ? worker_or_worklet_global_scope_->GetParentDevToolsToken()
              : inspected_frames->Root()->GetDevToolsFrameToken()),
      enabled_(&agent_state_, /*default_value=*/false),
      cache_disabled_(&agent_state_, /*default_value=*/false),
      bypass_service_worker_(&agent_state_, /*default_value=*/false),
      blocked_urls_(&agent_state_, /*default_value=*/false),
      extra_request_headers_(&agent_state_, /*default_value=*/WTF::String()),
      attach_debug_stack_enabled_(&agent_state_, /*default_value=*/false),
      total_buffer_size_(&agent_state_,
                         /*default_value=*/kDefaultTotalBufferSize),
      resource_buffer_size_(&agent_state_,
                            /*default_value=*/kDefaultResourceBufferSize),
      max_post_data_size_(&agent_state_, /*default_value=*/0),
      accepted_encodings_(&agent_state_,
                          /*default_value=*/false) {
  DCHECK((IsMainThread() &&
          (!worker_or_worklet_global_scope_ ||
           worker_or_worklet_global_scope_->IsWorkletGlobalScope())) ||
         (!IsMainThread() && worker_or_worklet_global_scope_));
  DCHECK(worker_or_worklet_global_scope_ || inspected_frames_);
}

void InspectorNetworkAgent::ShouldForceCorsPreflight(bool* result) {
  if (cache_disabled_.Get())
    *result = true;
}

void InspectorNetworkAgent::getRequestPostData(
    const String& request_id,
    std::unique_ptr<GetRequestPostDataCallback> callback) {
  NetworkResourcesData::ResourceData const* resource_data =
      resources_data_->Data(request_id);
  if (!resource_data) {
    callback->sendFailure(
        protocol::Response::ServerError("No resource with given id was found"));
    return;
  }
  scoped_refptr<EncodedFormData> post_data = resource_data->PostData();
  if (!post_data || post_data->IsEmpty()) {
    callback->sendFailure(protocol::Response::ServerError(
        "No post data available for the request"));
    return;
  }
  ExecutionContext* context = GetTargetExecutionContext();
  if (!context) {
    callback->sendFailure(protocol::Response::InternalError());
    return;
  }
  scoped_refptr<InspectorPostBodyParser> parser =
      base::MakeRefCounted<InspectorPostBodyParser>(
          std::move(callback), context->GetTaskRunner(TaskType::kFileReading));
  // TODO(crbug.com/810554): Extend protocol to fetch body parts separately
  parser->Parse(post_data.get());
}

ExecutionContext* InspectorNetworkAgent::GetTargetExecutionContext() const {
  if (worker_or_worklet_global_scope_) {
    return worker_or_worklet_global_scope_.Get();
  }
  DCHECK(inspected_frames_);
  return inspected_frames_->Root()->DomWindow();
}

void InspectorNetworkAgent::IsCacheDisabled(bool* is_cache_disabled) const {
  if (cache_disabled_.Get())
    *is_cache_disabled = true;
}

}  // namespace blink
