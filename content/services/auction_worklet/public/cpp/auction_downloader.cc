// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/auction_downloader.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_ref.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/services/auction_worklet/public/cpp/auction_worklet_features.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "content/services/auction_worklet/public/mojom/in_progress_auction_download.mojom.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/gurl.h"

namespace auction_worklet {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("auction_downloader", R"(
        semantics {
          sender: "AuctionDownloader"
          description:
            "Requests Protected Audiences script or JSON file for running an "
            "ad auction."
          trigger:
            "Requested when running a Protected Audiences auction."
            "The Protected Audience API allows sites to select content (such "
            "as personalized ads) to display based on cross-site data in a "
            "privacy preserving way."
          data:
            "URL associated with an interest group or seller, and details on "
            "what data specifically is being requested, also provided by the "
            "interest group."
          destination: WEBSITE
          user_data: {
            type: SENSITIVE_URL
          }
          internal {
            contacts {
              email: "privacy-sandbox-dev@chromium.org"
            }
          }
          last_reviewed: "2024-06-08"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable this via Settings > Privacy and Security > Ads "
            "privacy > Site-suggested ads."
          chrome_policy {
            PrivacySandboxSiteEnabledAdsEnabled {
              PrivacySandboxSiteEnabledAdsEnabled: false
            }
          }
        })");

const char kWebAssemblyMime[] = "application/wasm";
const char kAdAuctionTrustedSignalsMime[] =
    "message/ad-auction-trusted-signals-response";

// If `url` is too long to reasonably use as part of an error message, returns a
// truncated copy of it. Otherwise, returns the entire URL as a string.
std::string TruncateUrlIfNeededForError(const GURL& url) {
  if (url.spec().size() <= AuctionDownloader::kMaxErrorUrlLength) {
    // This does duplicate the URL unnecessarily, but since this is only done on
    // error, not a major concern.
    return url.spec();
  }

  return url.spec().substr(0, AuctionDownloader::kMaxErrorUrlLength - 3) +
         "...";
}

// Returns the MIME type string to send for the Accept header for `mime_type`.
// These are the official IANA MIME type strings, though other MIME type strings
// are allows in the response.
std::string_view MimeTypeToString(AuctionDownloader::MimeType mime_type) {
  switch (mime_type) {
    case AuctionDownloader::MimeType::kAdAuctionTrustedSignals:
      return std::string_view(kAdAuctionTrustedSignalsMime);
    case AuctionDownloader::MimeType::kJavascript:
      return std::string_view("application/javascript");
    case AuctionDownloader::MimeType::kJson:
      return std::string_view("application/json");
    case AuctionDownloader::MimeType::kWebAssembly:
      return std::string_view(kWebAssemblyMime);
  }
}

// Checks if `response_info` is consistent with `mime_type`.
bool MimeTypeIsConsistent(
    AuctionDownloader::MimeType mime_type,
    const network::mojom::URLResponseHead& response_info) {
  switch (mime_type) {
    case AuctionDownloader::MimeType::kAdAuctionTrustedSignals:
      return response_info.mime_type == kAdAuctionTrustedSignalsMime;
    case AuctionDownloader::MimeType::kJavascript:
      // ResponseInfo's `mime_type` is always lowercase.
      return blink::IsSupportedJavascriptMimeType(response_info.mime_type);
    case AuctionDownloader::MimeType::kJson:
      // ResponseInfo's `mime_type` is always lowercase.
      return blink::IsJSONMimeType(response_info.mime_type);
    case AuctionDownloader::MimeType::kWebAssembly: {
      // Here we use the headers directly, not the parsed mimetype, since we
      // much check there are no parameters whatsoever. Ref.
      // https://webassembly.github.io/spec/web-api/#streaming-modules
      std::optional<std::string> raw_content_type =
          response_info.headers->GetNormalizedHeader(
              net::HttpRequestHeaders::kContentType);
      if (!raw_content_type) {
        return false;
      }

      return base::ToLowerASCII(*raw_content_type) == kWebAssemblyMime;
    }
  }
}

// Checks if `charset` is a valid charset, in lowercase ASCII. Takes `body` as
// well, to ensure it uses the specified charset.
bool IsAllowedCharset(std::string_view charset, const std::string& body) {
  if (charset == "utf-8" || charset.empty()) {
    return base::IsStringUTF8(body);
  } else if (charset == "us-ascii") {
    return base::IsStringASCII(body);
  }
  // TODO(mmenke): Worth supporting iso-8859-1, or full character set list?
  return false;
}

double CalculateMillisecondDelta(const net::LoadTimingInfo& timing,
                                 base::TimeTicks time) {
  return time.is_null() ? -1 : (time - timing.request_start).InMillisecondsF();
}

void WriteTraceTiming(const net::LoadTimingInfo& timing,
                      perfetto::TracedValue dest) {
  perfetto::TracedDictionary dict = std::move(dest).WriteDictionary();
  dict.Add("requestTime", timing.request_start.since_origin().InSecondsF());
  dict.Add("proxyStart",
           CalculateMillisecondDelta(timing, timing.proxy_resolve_start));
  dict.Add("proxyEnd",
           CalculateMillisecondDelta(timing, timing.proxy_resolve_end));
  dict.Add("dnsStart", CalculateMillisecondDelta(
                           timing, timing.connect_timing.domain_lookup_start));
  dict.Add("dnsEnd", CalculateMillisecondDelta(
                         timing, timing.connect_timing.domain_lookup_end));
  dict.Add("connectStart", CalculateMillisecondDelta(
                               timing, timing.connect_timing.connect_start));
  dict.Add("connectEnd", CalculateMillisecondDelta(
                             timing, timing.connect_timing.connect_end));
  dict.Add("sslStart",
           CalculateMillisecondDelta(timing, timing.connect_timing.ssl_start));
  dict.Add("sslEnd",
           CalculateMillisecondDelta(timing, timing.connect_timing.ssl_end));
  dict.Add("workerStart",
           CalculateMillisecondDelta(timing, timing.service_worker_start_time));
  dict.Add("workerReady",
           CalculateMillisecondDelta(timing, timing.service_worker_ready_time));
  dict.Add("sendStart", CalculateMillisecondDelta(timing, timing.send_start));
  dict.Add("sendEnd", CalculateMillisecondDelta(timing, timing.send_end));
  dict.Add("receiveHeadersEnd",
           CalculateMillisecondDelta(timing, timing.receive_headers_end));
  dict.Add("pushStart", timing.push_start.since_origin().InSecondsF());
  dict.Add("pushEnd", timing.push_end.since_origin().InSecondsF());
}

std::unique_ptr<network::ResourceRequest> MakeResourceRequest(
    const GURL& source_url,
    AuctionDownloader::MimeType mime_type,
    bool post,
    bool allow_stale_response,
    base::optional_ref<const url::Origin> request_initiator,
    std::optional<network::ResourceRequest::TrustedParams> trusted_params,
    std::string_view request_id) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = source_url;
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->enable_load_timing =
      *TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("devtools.timeline");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      MimeTypeToString(mime_type));
  resource_request->trusted_params = std::move(trusted_params);
  if (post) {
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
  }
  if (request_initiator) {
    resource_request->request_initiator = *request_initiator;
    resource_request->mode = network::mojom::RequestMode::kCors;
  }
  // Stale-while-revalidate is not supported for POST in the http cache,
  // so do not try to support it here -- so we do not need to hold onto
  // the post body. The `request_initiator` constructor is, in production,
  // currently only used with POSTs, so no need to support
  // stale-while-revalidate when it is populated, either.
  if (allow_stale_response && !post && !request_initiator) {
    DCHECK(!resource_request->trusted_params);
    resource_request->load_flags |= net::LOAD_SUPPORT_ASYNC_REVALIDATION;
  }
  resource_request->devtools_request_id = request_id;

  return resource_request;
}

}  // namespace

AuctionDownloader::AuctionDownloader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    DownloadMode download_mode,
    MimeType mime_type,
    std::optional<std::string> post_body,
    std::optional<std::string> content_type,
    std::optional<size_t> num_igs_for_trusted_bidding_signals_kvv1,
    ResponseStartedCallback response_started_callback,
    AuctionDownloaderCallback auction_downloader_callback,
    std::unique_ptr<NetworkEventsDelegate> network_events_delegate)
    : AuctionDownloader(url_loader_factory,
                        source_url,
                        download_mode,
                        mime_type,
                        /*url_loader_client_endpoints=*/nullptr,
                        /*request_id=*/std::nullopt,
                        std::move(post_body),
                        std::move(content_type),
                        num_igs_for_trusted_bidding_signals_kvv1,
                        /*request_initiator=*/std::nullopt,
                        /*trusted_params=*/std::nullopt,
                        std::move(response_started_callback),
                        std::move(auction_downloader_callback),
                        std::move(network_events_delegate)) {}

AuctionDownloader::AuctionDownloader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    DownloadMode download_mode,
    MimeType mime_type,
    std::optional<std::string> post_body,
    std::optional<std::string> content_type,
    const url::Origin& request_initiator,
    network::ResourceRequest::TrustedParams trusted_params,
    AuctionDownloaderCallback auction_downloader_callback,
    std::unique_ptr<NetworkEventsDelegate> network_events_delegate)
    : AuctionDownloader(
          url_loader_factory,
          source_url,
          download_mode,
          mime_type,
          /*url_loader_client_endpoints=*/nullptr,
          /*request_id=*/std::nullopt,
          std::move(post_body),
          std::move(content_type),
          /*num_igs_for_trusted_bidding_signals_kvv1=*/std::nullopt,
          request_initiator,
          std::move(trusted_params),
          ResponseStartedCallback(),
          std::move(auction_downloader_callback),
          std::move(network_events_delegate)) {}

AuctionDownloader::AuctionDownloader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    mojom::InProgressAuctionDownloadPtr in_progress_load,
    DownloadMode download_mode,
    AuctionDownloader::MimeType mime_type,
    std::optional<size_t> num_igs_for_trusted_bidding_signals_kvv1,
    ResponseStartedCallback response_started_callback,
    AuctionDownloaderCallback auction_downloader_callback,
    std::unique_ptr<NetworkEventsDelegate> network_events_delegate)
    : AuctionDownloader(url_loader_factory,
                        std::move(in_progress_load->url),
                        download_mode,
                        mime_type,
                        std::move(in_progress_load->endpoints),
                        std::move(in_progress_load->devtools_request_id),
                        /*post_body=*/std::nullopt,
                        /*content_type=*/std::nullopt,
                        num_igs_for_trusted_bidding_signals_kvv1,
                        /*request_initiator=*/std::nullopt,
                        /*trusted_params=*/std::nullopt,
                        std::move(response_started_callback),
                        std::move(auction_downloader_callback),
                        std::move(network_events_delegate)) {}

AuctionDownloader::NetworkEventsDelegate::~NetworkEventsDelegate() = default;
AuctionDownloader::~AuctionDownloader() = default;

AuctionDownloader::AuctionDownloader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    DownloadMode download_mode,
    MimeType mime_type,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::optional<std::string> request_id,
    std::optional<std::string> post_body,
    std::optional<std::string> content_type,
    std::optional<size_t> num_igs_for_trusted_bidding_signals_kvv1,
    base::optional_ref<const url::Origin> request_initiator,
    std::optional<network::ResourceRequest::TrustedParams> trusted_params,
    ResponseStartedCallback response_started_callback,
    AuctionDownloaderCallback auction_downloader_callback,
    std::unique_ptr<NetworkEventsDelegate> network_events_delegate)
    : url_loader_factory_(*url_loader_factory),
      source_url_(source_url),
      mime_type_(mime_type),
      num_igs_for_trusted_bidding_signals_kvv1_(
          num_igs_for_trusted_bidding_signals_kvv1),
      request_id_(request_id ? std::move(request_id).value()
                             : base::UnguessableToken::Create().ToString()),
      response_started_callback_(std::move(response_started_callback)),
      auction_downloader_callback_(std::move(auction_downloader_callback)),
      network_events_delegate_(std::move(network_events_delegate)) {
  DCHECK(auction_downloader_callback_);

  bool in_progress_load = !url_loader_client_endpoints.is_null();

  if (in_progress_load) {
    simple_url_loader_ = network::SimpleURLLoader::Create(
        source_url_, std::move(url_loader_client_endpoints));
  } else {
    auto resource_request = MakeResourceRequest(
        source_url_, mime_type_, post_body.has_value(),
        /*allow_stale_response=*/
        base::FeatureList::IsEnabled(
            features::kFledgeAuctionDownloaderStaleWhileRevalidate),
        request_initiator, std::move(trusted_params), request_id_);
    if (network_events_delegate_ != nullptr) {
      network_events_delegate_->OnNetworkSendRequest(*resource_request);
    }
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), kTrafficAnnotation);
  }

  if (post_body.has_value()) {
    // We cannot attach an upload to an in-progress load.
    CHECK(!in_progress_load);
    // Although content type header is not required in POST request, but we
    // should have one.
    CHECK(content_type.has_value());
    simple_url_loader_->AttachStringForUpload(std::move(post_body).value(),
                                              std::move(content_type).value());
  }

  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "devtools.timeline", "ResourceSendRequest", TRACE_EVENT_SCOPE_THREAD,
      base::TimeTicks::Now(), "data", [&](perfetto::TracedValue dest) {
        auto dict = std::move(dest).WriteDictionary();
        dict.Add("requestId", request_id_);
        dict.Add("url", source_url_.spec());
        // Value derived from CDP ResourceType enum:
        // https://chromedevtools.github.io/devtools-protocol/tot/Network/#type-ResourceType
        // TODO: Import the enum strings directly
        dict.Add("resourceType", "Other");
        dict.Add("fetchPriorityHint", "auto");
      });

  // Abort on redirects.
  // TODO(mmenke): May want a browser-side proxy to block redirects instead.
  simple_url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &AuctionDownloader::OnRedirect, base::Unretained(this)));

  simple_url_loader_->SetOnResponseStartedCallback(
      base::BindRepeating(&AuctionDownloader::OnResponseStarted,
                          base::Unretained(this), base::Time::Now()));

  simple_url_loader_->SetTimeoutDuration(kRequestTimeout);

  // SimpleURLLoader does not use an URLLoaderFactory when it's created for an
  // in-progress load.
  network::mojom::URLLoaderFactory* factory_for_download =
      in_progress_load ? nullptr : &url_loader_factory_.get();
  // TODO(mmenke): Consider limiting the size of response bodies.
  if (download_mode == DownloadMode::kActualDownload) {
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        factory_for_download, base::BindOnce(&AuctionDownloader::OnBodyReceived,
                                             base::Unretained(this)));
  } else {
    simple_url_loader_->DownloadHeadersOnly(
        factory_for_download,
        base::BindOnce(&AuctionDownloader::OnHeadersOnlyReceived,
                       base::Unretained(this)));
  }
}

void AuctionDownloader::OnHeadersOnlyReceived(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  // Pretend to have a response with empty body on success, so we can share the
  // code with the actually-downloading path.
  // (It will get the headers out of SimpleUrlLoader)
  OnBodyReceived(headers && simple_url_loader_->NetError() == net::OK
                     ? std::make_unique<std::string>()
                     : nullptr);
}

void AuctionDownloader::OnBodyReceived(std::unique_ptr<std::string> body) {
  DCHECK(auction_downloader_callback_);

  auto simple_url_loader = std::move(simple_url_loader_);
  network::URLLoaderCompletionStatus completion_status =
      network::URLLoaderCompletionStatus(simple_url_loader->NetError());

  if (simple_url_loader->CompletionStatus()) {
    completion_status = simple_url_loader->CompletionStatus().value();
  }

  if (!body) {
    FailRequest(std::move(completion_status),
                base::StringPrintf(
                    "Failed to load %s error = %s.",
                    // Avoid large error messages. This is actually needed for a
                    // browser test, since some tests output error messages to
                    // the console, and tests are reported as failing on the
                    // bots when they produce too much output.
                    TruncateUrlIfNeededForError(source_url_).c_str(),
                    net::ErrorToString(simple_url_loader->NetError()).c_str()));
    return;
  }

  if (num_igs_for_trusted_bidding_signals_kvv1_ && response_started_time_) {
    base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - response_started_time_.value();
    base::UmaHistogramTimes(
        "Ads.InterestGroup.Auction.BiddingSignalsResponseDownloadTime",
        elapsed_time);
    base::UmaHistogramTimes(
        "Ads.InterestGroup.Auction.BiddingSignalsResponseDownloadTimePerIG",
        elapsed_time / num_igs_for_trusted_bidding_signals_kvv1_.value());
    base::UmaHistogramTimes(
        "Ads.InterestGroup.Auction."
        "BiddingSignalsResponseDownloadTimeAfterOneDownloadTimePerIG",
        elapsed_time -
            elapsed_time / num_igs_for_trusted_bidding_signals_kvv1_.value());
  }

  if (simple_url_loader->ResponseInfo()->async_revalidation_requested) {
    auto resource_request =
        MakeResourceRequest(source_url_, mime_type_, /*post=*/false,
                            /*allow_stale_response=*/false,
                            /*request_initiator=*/std::nullopt,
                            /*trusted_params=*/std::nullopt, request_id_);
    auto revalidation_url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), kTrafficAnnotation);
    // Pass the URL loader to the callback to prevent it from being destroyed
    // until revalidation is done. If the loader is destroyed, the call to
    // revalidate will not complete.
    revalidation_url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        &url_loader_factory_.get(),
        base::BindOnce(&AuctionDownloader::OnRevalidatedBodyReceived,
                       std::move(revalidation_url_loader)));
  }

  // Everything below is a network success even if it's a semantic failure.

  // OnNetworkRequestComplete in the !body case was handled by FailRequest,
  // when there is a body we handle it here.
  if (network_events_delegate_ != nullptr) {
    network_events_delegate_->OnNetworkRequestComplete(completion_status);
  }

  TraceResult(/*failure=*/false,
              simple_url_loader->CompletionStatus()->completion_time,
              simple_url_loader->ResponseInfo()->encoded_data_length,
              /*decoded_body_length=*/body->size());

  // Most checks happened already in OnResponseStarted, but the charset check
  // is dependent on the body and the mimetype check looks at the same header
  // so it's done at the same time.
  if (!MimeTypeIsConsistent(mime_type_, *simple_url_loader->ResponseInfo())) {
    std::move(auction_downloader_callback_)
        .Run(/*body=*/nullptr, /*headers=*/nullptr,
             base::StringPrintf(
                 "Rejecting load of %s due to unexpected MIME type.",
                 source_url_.spec().c_str()));
  } else if ((mime_type_ != MimeType::kWebAssembly &&
              mime_type_ != MimeType::kAdAuctionTrustedSignals) &&
             !IsAllowedCharset(simple_url_loader->ResponseInfo()->charset,
                               *body)) {
    std::move(auction_downloader_callback_)
        .Run(/*body=*/nullptr, /*headers=*/nullptr,
             base::StringPrintf(
                 "Rejecting load of %s due to unexpected charset.",
                 source_url_.spec().c_str()));
  } else {
    // All OK!
    std::move(auction_downloader_callback_)
        .Run(std::move(body),
             std::move(simple_url_loader->ResponseInfo()->headers),
             /*error_msg=*/std::nullopt);
  }
}

void AuctionDownloader::OnRedirect(
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(auction_downloader_callback_);

  FailRequest(network::URLLoaderCompletionStatus(net::ERR_ABORTED),
              base::StringPrintf("Unexpected redirect on %s.",
                                 source_url_.spec().c_str()));
}

// static
mojom::InProgressAuctionDownloadPtr AuctionDownloader::StartDownload(
    network::mojom::URLLoaderFactory& url_loader_factory,
    const GURL& source_url,
    AuctionDownloader::MimeType mime_type,
    mojom::AuctionNetworkEventsHandler& network_events_handler,
    std::optional<std::string> post_body,
    std::optional<std::string> content_type) {
  // We only use the content_type when we have a post body.
  CHECK_EQ(post_body.has_value(), content_type.has_value());
  auto script_url_loader_endpoints =
      network::mojom::URLLoaderClientEndpoints::New();
  std::string request_id = base::UnguessableToken::Create().ToString();
  auto resource_request = MakeResourceRequest(
      source_url, mime_type,
      /*post=*/post_body.has_value(), /*allow_stale_response=*/
      base::FeatureList::IsEnabled(
          features::kFledgeAuctionDownloaderStaleWhileRevalidate),
      /*request_initiator=*/std::nullopt, /*trusted_params=*/std::nullopt,
      /*request_id=*/request_id);
  if (post_body.has_value() && content_type.has_value()) {
    resource_request->request_body =
        network::ResourceRequestBody::CreateFromCopyOfBytes(
            base::as_byte_span(std::move(post_body.value())));
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                        content_type.value());
  }
  network_events_handler.OnNetworkSendRequest(*resource_request,
                                              base::TimeTicks::Now());
  url_loader_factory.CreateLoaderAndStart(
      script_url_loader_endpoints->url_loader.InitWithNewPipeAndPassReceiver(),
      /*request_id=*/0, network::mojom::kURLLoadOptionNone, *resource_request,
      script_url_loader_endpoints->url_loader_client
          .InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));

  return mojom::InProgressAuctionDownload::New(
      std::move(source_url), std::move(script_url_loader_endpoints),
      std::move(request_id));
}

// static
std::optional<std::string> AuctionDownloader::CheckResponseAllowed(
    const GURL& url,
    const network::mojom::URLResponseHead& response_head,
    network::URLLoaderCompletionStatus& status_out) {
  if (response_head.headers &&
      response_head.headers->response_code() / 100 != 2) {
    status_out =
        network::URLLoaderCompletionStatus(net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    return base::StringPrintf("Failed to load %s HTTP status = %d %s.",
                              url.spec().c_str(),
                              response_head.headers->response_code(),
                              response_head.headers->GetStatusText().c_str());
  }

  bool allow_fledge_header_found = false;
  if (response_head.headers) {
    for (const std::string_view header :
         {"Ad-Auction-Allowed", "X-Allow-FLEDGE"}) {
      std::optional<std::string> allow_fledge =
          response_head.headers->GetNormalizedHeader(header);
      if (allow_fledge &&
          base::EqualsCaseInsensitiveASCII(*allow_fledge, "true")) {
        allow_fledge_header_found = true;
        break;
      }
    }
  }
  if (!allow_fledge_header_found) {
    status_out = network::URLLoaderCompletionStatus(net::ERR_ABORTED);
    return base::StringPrintf(
        "Rejecting load of %s due to lack of Ad-Auction-Allowed: true "
        "(or the deprecated X-Allow-FLEDGE: true).",
        url.spec().c_str());
  }
  return std::nullopt;
}

// static
std::string_view AuctionDownloader::MimeTypeToStringForTesting(
    AuctionDownloader::MimeType mime_type) {
  return MimeTypeToString(mime_type);
}

void AuctionDownloader::OnResponseStarted(
    base::Time request_time,
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  if (network_events_delegate_ != nullptr) {
    network_events_delegate_->OnNetworkResponseReceived(final_url,
                                                        response_head);
  }
  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "ResourceReceiveResponse", TRACE_EVENT_SCOPE_THREAD,
      "data", [&](perfetto::TracedValue dest) {
        perfetto::TracedDictionary dict = std::move(dest).WriteDictionary();
        dict.Add("requestId", request_id_);
        if (response_head.headers) {
          dict.Add("statusCode", response_head.headers->response_code());
          auto header_array = dict.AddArray("headers");

          size_t header_iterator = 0;
          std::string header_name;
          std::string header_value;
          while (response_head.headers->EnumerateHeaderLines(
              &header_iterator, &header_name, &header_value)) {
            auto item_dict = header_array.AppendDictionary();
            item_dict.Add("name", header_name);
            item_dict.Add("value", header_value);
          }
        }
        dict.Add("mimeType", response_head.mime_type);
        dict.Add("encodedDataLength", response_head.encoded_data_length);

        // ref.  WebURLResponse::Create
        dict.Add("fromCache",
                 (!response_head.load_timing.request_start_time.is_null() &&
                  response_head.response_time <
                      response_head.load_timing.request_start_time));
        dict.Add("fromServiceWorker",
                 response_head.was_fetched_via_service_worker);

        if (response_head.was_fetched_via_service_worker) {
          switch (response_head.service_worker_response_source) {
            case network::mojom::FetchResponseSource::kCacheStorage:
              dict.Add("serviceWorkerResponseSource", "cacheStorage");
              break;
            case network::mojom::FetchResponseSource::kHttpCache:
              dict.Add("serviceWorkerResponseSource", "httpCache");
              break;
            case network::mojom::FetchResponseSource::kNetwork:
              dict.Add("serviceWorkerResponseSource", "network");
              break;
            case network::mojom::FetchResponseSource::kUnspecified:
              dict.Add("serviceWorkerResponseSource", "fallbackCode");
          }
        }

        if (!response_head.response_time.is_null()) {
          dict.Add("responseTime",
                   response_head.response_time.InMillisecondsFSinceUnixEpoch());
        }

        // Only send load timing if it exists, e.g. not a cache hit.
        if (!response_head.load_timing.receive_headers_end.is_null()) {
          WriteTraceTiming(response_head.load_timing, dict.AddItem("timing"));
        }
      });

  network::URLLoaderCompletionStatus status;
  std::optional<std::string> error =
      CheckResponseAllowed(source_url_, response_head, status);
  if (error) {
    FailRequest(status, *error);
    return;
  }

  // Record the cached response's age if there was an entry in the cache.
  if (num_igs_for_trusted_bidding_signals_kvv1_ &&
      response_head.was_fetched_via_cache &&
      response_head.original_response_time < request_time) {
    base::UmaHistogramTimes(
        "Ads.InterestGroup.Auction.HttpCachedTrustedBiddingSignalsAge2",
        request_time - response_head.original_response_time);
  }

  response_started_time_ = base::TimeTicks::Now();

  if (response_started_callback_) {
    std::move(response_started_callback_).Run(response_head);
  }
}

void AuctionDownloader::FailRequest(network::URLLoaderCompletionStatus status,
                                    std::string error_string) {
  simple_url_loader_.reset();
  TraceResult(/*failure=*/true, /*completion_time=*/base::TimeTicks(),
              /*encoded_data_length=*/0,
              /*decoded_body_length=*/0);
  if (network_events_delegate_ != nullptr) {
    network_events_delegate_->OnNetworkRequestComplete(std::move(status));
  }
  std::move(auction_downloader_callback_)
      .Run(/*body=*/nullptr, /*headers=*/nullptr, std::move(error_string));
}

void AuctionDownloader::TraceResult(bool failure,
                                    base::TimeTicks completion_time,
                                    int64_t encoded_data_length,
                                    int64_t decoded_body_length) {
  if (!completion_time.is_null()) {
    base::UmaHistogramTimes("Ads.InterestGroup.Auction.DownloadThreadDelay",
                            base::TimeTicks::Now() - completion_time);
  }
  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data",
      [&](perfetto::TracedValue dest) {
        perfetto::TracedDictionary dict = std::move(dest).WriteDictionary();
        dict.Add("requestId", request_id_);
        dict.Add("didFail", failure);
        dict.Add("encodedDataLength", encoded_data_length);
        dict.Add("decodedBodyLength", decoded_body_length);
        if (!completion_time.is_null()) {
          dict.Add("finishTime", completion_time.since_origin().InSecondsF());
        }
      });
}

}  // namespace auction_worklet
