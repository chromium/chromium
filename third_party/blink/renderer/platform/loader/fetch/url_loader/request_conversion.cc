// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.h"

#include <string_view>

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/filter/source_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom-blink.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
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
namespace {

// TODO(yhirano): Unify these with variables in
// content/public/common/content_constants.h.
constexpr char kCorsExemptPurposeHeaderName[] = "Purpose";
constexpr char kCorsExemptRequestedWithHeaderName[] = "X-Requested-With";

// TODO(yhirano) Dedupe this and the same-name function in
// web_url_request_util.cc.
std::string TrimLWSAndCRLF(const std::string_view& input) {
  std::string_view string = net::HttpUtil::TrimLWS(input);
  const char* begin = string.data();
  const char* end = string.data() + string.size();
  while (begin < end && (end[-1] == '\r' || end[-1] == '\n'))
    --end;
  return std::string(std::string_view(begin, end - begin));
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

    // Json
    case mojom::blink::RequestContextType::JSON:
      return mojom::ResourceType::kJson;

    // Media
    case mojom::blink::RequestContextType::AUDIO:
    case mojom::blink::RequestContextType::VIDEO:
      return mojom::ResourceType::kMedia;

    // Object
    case mojom::blink::RequestContextType::EMBED:
    case mojom::blink::RequestContextType::OBJECT:
      return mojom::ResourceType::kObject;

    // Ping
    case mojom::blink::RequestContextType::ATTRIBUTION_SRC:
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
    case mojom::blink::RequestContextType::SCRIPT:
      return mojom::ResourceType::kScript;

    // Style
    case mojom::blink::RequestContextType::XSLT:
    case mojom::blink::RequestContextType::STYLE:
      return mojom::ResourceType::kStylesheet;

    // Subresource
    case mojom::blink::RequestContextType::DOWNLOAD:
    case mojom::blink::RequestContextType::MANIFEST:
    case mojom::blink::RequestContextType::SPECULATION_RULES:
    case mojom::blink::RequestContextType::SUBRESOURCE:
    case mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE:
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

    // Navigation requests should not go through URLLoader.
    case mojom::blink::RequestContextType::FORM:
    case mojom::blink::RequestContextType::HYPERLINK:
    case mojom::blink::RequestContextType::LOCATION:
    case mojom::blink::RequestContextType::FRAME:
    case mojom::blink::RequestContextType::IFRAME:
      NOTREACHED_IN_MIGRATION();
      return mojom::ResourceType::kSubResource;

    default:
      NOTREACHED_IN_MIGRATION();
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
        mojo::Remote<mojom::blink::Blob> blob_remote(
            element.optional_blob_data_handle_->CloneBlobRemote());
        mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
            data_pipe_getter_remote;
        blob_remote->AsDataPipeGetter(
            data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
        dest->AppendDataPipe(
            ToCrossVariantMojoType(std::move(data_pipe_getter_remote)));
        break;
      }
      case FormDataElement::kDataPipe: {
        mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
            pending_data_pipe_getter;
        element.data_pipe_getter_->GetDataPipeGetter()->Clone(
            pending_data_pipe_getter.InitWithNewPipeAndPassReceiver());
        dest->AppendDataPipe(
            ToCrossVariantMojoType(std::move(pending_data_pipe_getter)));
        break;
      }
    }
  }
}

bool IsBannedCrossSiteAuth(network::ResourceRequest* resource_request,
                           WebURLRequestExtraData* url_request_extra_data) {
  auto& request_url = resource_request->url;
  auto& first_party = resource_request->site_for_cookies;

  bool allow_cross_origin_auth_prompt = false;
  if (url_request_extra_data) {
    allow_cross_origin_auth_prompt =
        url_request_extra_data->allow_cross_origin_auth_prompt();
  }

  if (first_party.IsFirstPartyWithSchemefulMode(
          request_url, /*compute_schemefully=*/false)) {
    // If the first party is secure but the subresource is not, this is
    // mixed-content. Do not allow the image.
    if (!allow_cross_origin_auth_prompt &&
        network::IsUrlPotentiallyTrustworthy(first_party.RepresentativeUrl()) &&
        !network::IsUrlPotentiallyTrustworthy(request_url)) {
      return true;
    }
    return false;
  }

  return !allow_cross_origin_auth_prompt;
}

}  // namespace

scoped_refptr<network::ResourceRequestBody> NetworkResourceRequestBodyFor(
    ResourceRequestBody src_body) {
  scoped_refptr<network::ResourceRequestBody> dest_body;
  if (const EncodedFormData* form_body = src_body.FormBody().get()) {
    dest_body = base::MakeRefCounted<network::ResourceRequestBody>();

    PopulateResourceRequestBody(*form_body, dest_body.get());
  } else if (src_body.StreamBody().is_valid()) {
    mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
        stream_body = src_body.TakeStreamBody();
    dest_body = base::MakeRefCounted<network::ResourceRequestBody>();
    dest_body->SetToChunkedDataPipe(
        ToCrossVariantMojoType(std::move(stream_body)),
        network::ResourceRequestBody::ReadOnlyOnce(true));
  }
  return dest_body;
}

void PopulateResourceRequest(const ResourceRequestHead& src,
                             ResourceRequestBody src_body,
                             network::ResourceRequest* dest) {
  dest->method = src.HttpMethod().Latin1();
  dest->url = GURL(src.Url());
  dest->site_for_cookies = src.SiteForCookies();
  dest->upgrade_if_insecure = src.UpgradeIfInsecure();
  dest->is_revalidating = src.IsRevalidating();
  if (src.GetDevToolsAcceptedStreamTypes()) {
    dest->devtools_accepted_stream_types =
        std::vector<net::SourceStream::SourceType>(
            src.GetDevToolsAcceptedStreamTypes()->data.begin(),
            src.GetDevToolsAcceptedStreamTypes()->data.end());
  }
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

  DCHECK(dest->navigation_redirect_chain.empty());
  dest->navigation_redirect_chain.reserve(src.NavigationRedirectChain().size());
  for (const KURL& url : src.NavigationRedirectChain()) {
    dest->navigation_redirect_chain.push_back(GURL(url));
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
  if (!src.GetRequestedWithHeader().empty()) {
    dest->cors_exempt_headers.SetHeader(kCorsExemptRequestedWithHeaderName,
                                        src.GetRequestedWithHeader().Utf8());
  }
  // Set Purpose header to cors_exempt_headers rather than headers to be
  // exempted from CORS checks.
  if (!src.GetPurposeHeader().empty()) {
    dest->cors_exempt_headers.SetHeader(kCorsExemptPurposeHeaderName,
                                        src.GetPurposeHeader().Utf8());
  }

  // TODO(yhirano): Remove this WrappedResourceRequest.
  dest->load_flags = WrappedResourceRequest(ResourceRequest(src))
                         .GetLoadFlagsForWebUrlRequest();
  dest->recursive_prefetch_token = src.RecursivePrefetchToken();
  dest->priority = WebURLRequest::ConvertToNetPriority(src.Priority());
  dest->priority_incremental = src.PriorityIncremental();
  dest->cors_preflight_policy = src.CorsPreflightPolicy();
  dest->skip_service_worker = src.GetSkipServiceWorker();
  dest->mode = src.GetMode();
  dest->destination = src.GetRequestDestination();
  dest->credentials_mode = src.GetCredentialsMode();
  dest->redirect_mode = src.GetRedirectMode();
  dest->fetch_integrity = src.GetFetchIntegrity().Utf8();
  if (src.GetWebBundleTokenParams().has_value()) {
    dest->web_bundle_token_params =
        std::make_optional(network::ResourceRequest::WebBundleTokenParams(
            GURL(src.GetWebBundleTokenParams()->bundle_url),
            src.GetWebBundleTokenParams()->token,
            ToCrossVariantMojoType(
                src.GetWebBundleTokenParams()->CloneHandle())));
  }

  // TODO(kinuko): Deprecate this.
  dest->resource_type =
      static_cast<int>(RequestContextToResourceType(src.GetRequestContext()));

  if (src.IsFetchLikeAPI() &&
      (dest->url.has_username() || dest->url.has_password())) {
    dest->do_not_prompt_for_login = true;
  }
  if (src.GetRequestContext() == mojom::blink::RequestContextType::PREFETCH ||
      src.IsFavicon()) {
    dest->do_not_prompt_for_login = true;
  }

  dest->keepalive = src.GetKeepalive();
  dest->browsing_topics = src.GetBrowsingTopics();
  dest->ad_auction_headers = src.GetAdAuctionHeaders();
  dest->shared_storage_writable_eligible =
      src.GetSharedStorageWritableEligible();
  dest->has_user_gesture = src.HasUserGesture();
  dest->enable_load_timing = true;
  dest->enable_upload_progress = src.ReportUploadProgress();
  dest->throttling_profile_id = src.GetDevToolsToken();
  dest->trust_token_params = ConvertTrustTokenParams(src.TrustTokenParams());
  dest->required_ip_address_space = src.GetTargetAddressSpace();

  if (base::UnguessableToken window_id = src.GetFetchWindowId())
    dest->fetch_window_id = std::make_optional(window_id);

  if (!src.GetDevToolsId().IsNull()) {
    dest->devtools_request_id = src.GetDevToolsId().Ascii();
  }

  if (src.GetDevToolsStackId().has_value()) {
    dest->devtools_stack_id = src.GetDevToolsStackId().value().Ascii();
  }

  dest->is_fetch_like_api = src.IsFetchLikeAPI();

  dest->is_fetch_later_api = src.IsFetchLaterAPI();

  dest->is_favicon = src.IsFavicon();

  dest->request_body = NetworkResourceRequestBodyFor(std::move(src_body));
  if (dest->request_body) {
    DCHECK_NE(dest->method, net::HttpRequestHeaders::kGetMethod);
    DCHECK_NE(dest->method, net::HttpRequestHeaders::kHeadMethod);
  }

  network::mojom::RequestDestination request_destination =
      src.GetRequestDestination();
  network_utils::SetAcceptHeader(dest->headers, request_destination);

  dest->original_destination = src.GetOriginalDestination();

  if (src.GetURLRequestExtraData()) {
    src.GetURLRequestExtraData()->CopyToResourceRequest(dest);
  }

  if (!dest->is_favicon &&
      request_destination == network::mojom::RequestDestination::kImage &&
      IsBannedCrossSiteAuth(dest, src.GetURLRequestExtraData().get())) {
    // Prevent third-party image content from prompting for login, as this
    // is often a scam to extract credentials for another domain from the
    // user. Only block image loads, as the attack applies largely to the
    // "src" property of the <img> tag. It is common for web properties to
    // allow untrusted values for <img src>; this is considered a fair thing
    // for an HTML sanitizer to do. Conversely, any HTML sanitizer that didn't
    // filter sources for <script>, <link>, <embed>, <object>, <iframe> tags
    // would be considered vulnerable in and of itself.
    dest->do_not_prompt_for_login = true;
    dest->load_flags |= net::LOAD_DO_NOT_USE_EMBEDDED_IDENTITY;
  }

  dest->storage_access_api_status = src.GetStorageAccessApiStatus();

  dest->attribution_reporting_support = src.GetAttributionReportingSupport();

  dest->attribution_reporting_eligibility =
      src.GetAttributionReportingEligibility();

  dest->attribution_reporting_src_token = src.GetAttributionSrcToken();

  dest->shared_dictionary_writer_enabled = src.SharedDictionaryWriterEnabled();

  dest->is_ad_tagged = src.IsAdResource();
}

}  // namespace blink
