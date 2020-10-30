// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.h"

#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/trust_token_params_conversion.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"

namespace blink {

const char* ImageAcceptHeader() {
  static constexpr char kImageAcceptHeaderWithAvif[] =
      "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  static constexpr size_t kOffset = sizeof("image/avif,") - 1;
#if BUILDFLAG(ENABLE_AV1_DECODER)
  static const char* header = base::FeatureList::IsEnabled(features::kAVIF)
                                  ? kImageAcceptHeaderWithAvif
                                  : kImageAcceptHeaderWithAvif + kOffset;
#else
  static const char* header = kImageAcceptHeaderWithAvif + kOffset;
#endif
  return header;
}

namespace {

constexpr char kStylesheetAcceptHeader[] = "text/css,*/*;q=0.1";

// TODO(yhirano): Unify these with variables in
// content/public/common/content_constants.h.
constexpr char kCorsExemptPurposeHeaderName[] = "Purpose";
constexpr char kCorsExemptRequestedWithHeaderName[] = "X-Requested-With";

// This is complementary to ConvertNetPriorityToWebKitPriority, defined in
// service_worker_context_client.cc.
net::RequestPriority ConvertWebKitPriorityToNetPriority(
    WebURLRequest::Priority priority) {
  switch (priority) {
    case WebURLRequest::Priority::kVeryHigh:
      return net::HIGHEST;

    case WebURLRequest::Priority::kHigh:
      return net::MEDIUM;

    case WebURLRequest::Priority::kMedium:
      return net::LOW;

    case WebURLRequest::Priority::kLow:
      return net::LOWEST;

    case WebURLRequest::Priority::kVeryLow:
      return net::IDLE;

    case WebURLRequest::Priority::kUnresolved:
    default:
      NOTREACHED();
      return net::LOW;
  }
}

// TODO(yhirano) Dedupe this and the same-name function in
// web_url_request_util.cc.
std::string TrimLWSAndCRLF(const base::StringPiece& input) {
  base::StringPiece string = net::HttpUtil::TrimLWS(input);
  const char* begin = string.data();
  const char* end = string.data() + string.size();
  while (begin < end && (end[-1] == '\r' || end[-1] == '\n'))
    --end;
  return std::string(base::StringPiece(begin, end - begin));
}

mojom::ResourceType RequestContextToResourceType(
    mojom::blink::RequestContextType request_context) {
  switch (request_context) {
    // CSP report
    case mojom::blink::RequestContextType::CSP_REPORT:
      return mojom::ResourceType::kCspReport;

    // Favicon
    case mojom::blink::RequestContextType::FAVICON:
      return mojom::ResourceType::kFavicon;

    // Font
    case mojom::blink::RequestContextType::FONT:
      return mojom::ResourceType::kFontResource;

    // Image
    case mojom::blink::RequestContextType::IMAGE:
    case mojom::blink::RequestContextType::IMAGE_SET:
      return mojom::ResourceType::kImage;

    // Media
    case mojom::blink::RequestContextType::AUDIO:
    case mojom::blink::RequestContextType::VIDEO:
      return mojom::ResourceType::kMedia;

    // Object
    case mojom::blink::RequestContextType::EMBED:
    case mojom::blink::RequestContextType::OBJECT:
      return mojom::ResourceType::kObject;

    // Ping
    case mojom::blink::RequestContextType::BEACON:
    case mojom::blink::RequestContextType::PING:
      return mojom::ResourceType::kPing;

    // Subresource of plugins
    case mojom::blink::RequestContextType::PLUGIN:
      return mojom::ResourceType::kPluginResource;

    // Prefetch
    case mojom::blink::RequestContextType::PREFETCH:
      return mojom::ResourceType::kPrefetch;

    // Script
    case mojom::blink::RequestContextType::IMPORT:
    case mojom::blink::RequestContextType::SCRIPT:
      return mojom::ResourceType::kScript;

    // Style
    case mojom::blink::RequestContextType::XSLT:
    case mojom::blink::RequestContextType::STYLE:
      return mojom::ResourceType::kStylesheet;

    // Subresource
    case mojom::blink::RequestContextType::DOWNLOAD:
    case mojom::blink::RequestContextType::MANIFEST:
    case mojom::blink::RequestContextType::SUBRESOURCE:
      return mojom::ResourceType::kSubResource;

    // TextTrack
    case mojom::blink::RequestContextType::TRACK:
      return mojom::ResourceType::kMedia;

    // Workers
    case mojom::blink::RequestContextType::SERVICE_WORKER:
      return mojom::ResourceType::kServiceWorker;
    case mojom::blink::RequestContextType::SHARED_WORKER:
      return mojom::ResourceType::kSharedWorker;
    case mojom::blink::RequestContextType::WORKER:
      return mojom::ResourceType::kWorker;

    // Unspecified
    case mojom::blink::RequestContextType::INTERNAL:
    case mojom::blink::RequestContextType::UNSPECIFIED:
      return mojom::ResourceType::kSubResource;

    // XHR
    case mojom::blink::RequestContextType::EVENT_SOURCE:
    case mojom::blink::RequestContextType::FETCH:
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
      return mojom::ResourceType::kXhr;

    // Navigation requests should not go through WebURLLoader.
    case mojom::blink::RequestContextType::FORM:
    case mojom::blink::RequestContextType::HYPERLINK:
    case mojom::blink::RequestContextType::LOCATION:
    case mojom::blink::RequestContextType::FRAME:
    case mojom::blink::RequestContextType::IFRAME:
      NOTREACHED();
      return mojom::ResourceType::kSubResource;

    default:
      NOTREACHED();
      return mojom::ResourceType::kSubResource;
  }
}

void PopulateResourceRequestBody(const EncodedFormData& src,
                                 network::ResourceRequestBody* dest) {
  for (const auto& element : src.Elements()) {
    switch (element.type_) {
      case FormDataElement::kData:
        dest->AppendBytes(element.data_.data(), element.data_.size());
        break;
      case FormDataElement::kEncodedFile:
        if (element.file_length_ == -1) {
          dest->AppendFileRange(
              WebStringToFilePath(element.filename_), 0,
              std::numeric_limits<uint64_t>::max(),
              element.expected_file_modification_time_.value_or(base::Time()));
        } else {
          dest->AppendFileRange(
              WebStringToFilePath(element.filename_),
              static_cast<uint64_t>(element.file_start_),
              static_cast<uint64_t>(element.file_length_),
              element.expected_file_modification_time_.value_or(base::Time()));
        }
        break;
      case FormDataElement::kEncodedBlob: {
        DCHECK(element.optional_blob_data_handle_);
        mojo::Remote<mojom::Blob> blob_remote(mojo::PendingRemote<mojom::Blob>(
            element.optional_blob_data_handle_->CloneBlobRemote().PassPipe(),
            mojom::Blob::Version_));
        mojo::PendingRemote<network::mojom::DataPipeGetter>
            data_pipe_getter_remote;
        blob_remote->AsDataPipeGetter(
            data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
        dest->AppendDataPipe(std::move(data_pipe_getter_remote));
        break;
      }
      case FormDataElement::kDataPipe: {
        // Convert network::mojom::blink::DataPipeGetter to
        // network::mojom::DataPipeGetter through a raw message pipe.
        mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
            pending_data_pipe_getter;
        element.data_pipe_getter_->GetDataPipeGetter()->Clone(
            pending_data_pipe_getter.InitWithNewPipeAndPassReceiver());
        dest->AppendDataPipe(
            mojo::PendingRemote<network::mojom::DataPipeGetter>(
                pending_data_pipe_getter.PassPipe(), 0u));
        break;
      }
    }
  }
}

}  // namespace

void PopulateResourceRequest(const ResourceRequestHead& src,
                             ResourceRequestBody src_body,
                             network::ResourceRequest* dest) {
  dest->method = src.HttpMethod().Latin1();
  dest->url = src.Url();
  dest->site_for_cookies = src.SiteForCookies();
  dest->upgrade_if_insecure = src.UpgradeIfInsecure();
  dest->is_revalidating = src.IsRevalidating();
  if (src.RequestorOrigin()->ToString() == "null") {
    // "file:" origin is treated like an opaque unique origin when
    // allow-file-access-from-files is not specified. Such origin is not opaque
    // (i.e., IsOpaque() returns false) but still serializes to "null". Derive a
    // new opaque origin so that downstream consumers can make use of the
    // origin's precursor.
    dest->request_initiator =
        src.RequestorOrigin()->DeriveNewOpaqueOrigin()->ToUrlOrigin();
  } else {
    dest->request_initiator = src.RequestorOrigin()->ToUrlOrigin();
  }
  if (src.IsolatedWorldOrigin()) {
    dest->isolated_world_origin = src.IsolatedWorldOrigin()->ToUrlOrigin();
  }
  dest->referrer = WebStringToGURL(src.ReferrerString());

  // "default" referrer policy has already been resolved.
  DCHECK_NE(src.GetReferrerPolicy(), network::mojom::ReferrerPolicy::kDefault);
  dest->referrer_policy =
      network::ReferrerPolicyForUrlRequest(src.GetReferrerPolicy());

  for (const auto& item : src.HttpHeaderFields()) {
    const std::string name = item.key.Latin1();
    const std::string value = TrimLWSAndCRLF(item.value.Latin1());
    dest->headers.SetHeader(name, value);
  }
  // Set X-Requested-With header to cors_exempt_headers rather than headers to
  // be exempted from CORS checks.
  if (!src.GetRequestedWithHeader().IsEmpty()) {
    dest->cors_exempt_headers.SetHeader(kCorsExemptRequestedWithHeaderName,
                                        src.GetRequestedWithHeader().Utf8());
  }
  // Set Purpose header to cors_exempt_headers rather than headers to be
  // exempted from CORS checks.
  if (!src.GetPurposeHeader().IsEmpty()) {
    dest->cors_exempt_headers.SetHeader(kCorsExemptPurposeHeaderName,
                                        src.GetPurposeHeader().Utf8());
  }

  // TODO(yhirano): Remove this WrappedResourceRequest.
  dest->load_flags = WrappedResourceRequest(ResourceRequest(src))
                         .GetLoadFlagsForWebUrlRequest();
  dest->recursive_prefetch_token = src.RecursivePrefetchToken();
  dest->priority = ConvertWebKitPriorityToNetPriority(src.Priority());
  dest->should_reset_appcache = src.ShouldResetAppCache();
  dest->is_external_request = src.IsExternalRequest();
  dest->cors_preflight_policy = src.CorsPreflightPolicy();
  dest->skip_service_worker = src.GetSkipServiceWorker();
  dest->mode = src.GetMode();
  dest->destination = src.GetRequestDestination();
  dest->credentials_mode = src.GetCredentialsMode();
  dest->redirect_mode = src.GetRedirectMode();
  dest->fetch_integrity = src.GetFetchIntegrity().Utf8();

  mojom::ResourceType resource_type =
      RequestContextToResourceType(src.GetRequestContext());

  // TODO(kinuko): Deprecate this.
  dest->resource_type = static_cast<int>(resource_type);

  if (resource_type == mojom::ResourceType::kXhr &&
      (dest->url.has_username() || dest->url.has_password())) {
    dest->do_not_prompt_for_login = true;
  }
  if (resource_type == mojom::ResourceType::kPrefetch ||
      resource_type == mojom::ResourceType::kFavicon) {
    dest->do_not_prompt_for_login = true;
  }

  dest->keepalive = src.GetKeepalive();
  dest->has_user_gesture = src.HasUserGesture();
  dest->enable_load_timing = true;
  dest->enable_upload_progress = src.ReportUploadProgress();
  dest->report_raw_headers = src.ReportRawHeaders();
  // TODO(ryansturm): Remove dest->previews_state once it is no
  // longer used in a network delegate. https://crbug.com/842233
  dest->previews_state = static_cast<int>(src.GetPreviewsState());
  dest->throttling_profile_id = src.GetDevToolsToken();
  dest->trust_token_params = ConvertTrustTokenParams(src.TrustTokenParams());

  if (base::UnguessableToken window_id = src.GetFetchWindowId())
    dest->fetch_window_id = base::make_optional(window_id);

  if (src.GetDevToolsId().has_value()) {
    dest->devtools_request_id = src.GetDevToolsId().value().Ascii();
  }

  if (src.GetDevToolsStackId().has_value()) {
    dest->devtools_stack_id = src.GetDevToolsStackId().value().Ascii();
  }

  if (src.IsSignedExchangePrefetchCacheEnabled()) {
    DCHECK_EQ(src.GetRequestContext(),
              mojom::blink::RequestContextType::PREFETCH);
    dest->is_signed_exchange_prefetch_cache_enabled = true;
  }

  dest->is_fetch_like_api = src.IsFetchLikeAPI();

  if (const EncodedFormData* body = src_body.FormBody().get()) {
    DCHECK_NE(dest->method, net::HttpRequestHeaders::kGetMethod);
    DCHECK_NE(dest->method, net::HttpRequestHeaders::kHeadMethod);
    dest->request_body = base::MakeRefCounted<network::ResourceRequestBody>();

    PopulateResourceRequestBody(*body, dest->request_body.get());
  } else if (src_body.StreamBody().is_valid()) {
    DCHECK_NE(dest->method, net::HttpRequestHeaders::kGetMethod);
    DCHECK_NE(dest->method, net::HttpRequestHeaders::kHeadMethod);
    mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
        stream_body = src_body.TakeStreamBody();
    dest->request_body = base::MakeRefCounted<network::ResourceRequestBody>();
    mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>
        network_stream_body(stream_body.PassPipe(), 0u);
    dest->request_body->SetToReadOnceStream(std::move(network_stream_body));
    dest->request_body->SetAllowHTTP1ForStreamingUpload(
        src.AllowHTTP1ForStreamingUpload());
  }

  if (resource_type == mojom::ResourceType::kStylesheet) {
    dest->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                            kStylesheetAcceptHeader);
  } else if (resource_type == mojom::ResourceType::kImage ||
             resource_type == mojom::ResourceType::kFavicon) {
    dest->headers.SetHeaderIfMissing(net::HttpRequestHeaders::kAccept,
                                     ImageAcceptHeader());
  } else {
    // Calling SetHeaderIfMissing() instead of SetHeader() because JS can
    // manually set an accept header on an XHR.
    dest->headers.SetHeaderIfMissing(net::HttpRequestHeaders::kAccept,
                                     network::kDefaultAcceptHeaderValue);
  }
}

}  // namespace blink
