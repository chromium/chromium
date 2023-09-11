// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/auction_downloader.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "base/values.h"
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
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
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
            "Requests FLEDGE script or JSON file for running an ad auction."
          trigger:
            "Requested when running a FLEDGE auction."
          data: "URL associated with an interest group or seller."
          destination: WEBSITE
          user_data: {
            type: SENSITIVE_URL
          }
          internal {
            contacts {
              email: "privacy-sandbox-dev@chromium.org"
            }
          }
          last_reviewed: "2023-06-12"
        }
        policy {
          cookies_allowed: NO
          setting:
            "These requests cannot be disabled."
          policy_exception_justification:
            "These requests triggered by a website."
        })");

const char kWebAssemblyMime[] = "application/wasm";

// Returns the MIME type string to send for the Accept header for `mime_type`.
// These are the official IANA MIME type strings, though other MIME type strings
// are allows in the response.
base::StringPiece MimeTypeToString(AuctionDownloader::MimeType mime_type) {
  switch (mime_type) {
    case AuctionDownloader::MimeType::kJavascript:
      return base::StringPiece("application/javascript");
    case AuctionDownloader::MimeType::kJson:
      return base::StringPiece("application/json");
    case AuctionDownloader::MimeType::kWebAssembly:
      return base::StringPiece(kWebAssemblyMime);
  }
}

// Checks if `response_info` is consistent with `mime_type`.
bool MimeTypeIsConsistent(
    AuctionDownloader::MimeType mime_type,
    const network::mojom::URLResponseHead* response_info) {
  switch (mime_type) {
    case AuctionDownloader::MimeType::kJavascript:
      // ResponseInfo's `mime_type` is always lowercase.
      return blink::IsSupportedJavascriptMimeType(response_info->mime_type);
    case AuctionDownloader::MimeType::kJson:
      // ResponseInfo's `mime_type` is always lowercase.
      return blink::IsJSONMimeType(response_info->mime_type);
    case AuctionDownloader::MimeType::kWebAssembly: {
      std::string raw_content_type;
      // Here we use the headers directly, not the parsed mimetype, since we
      // much check there are no parameters whatsoever. Ref.
      // https://webassembly.github.io/spec/web-api/#streaming-modules
      if (!response_info->headers->GetNormalizedHeader(
              net::HttpRequestHeaders::kContentType, &raw_content_type)) {
        return false;
      }

      return base::ToLowerASCII(raw_content_type) == kWebAssemblyMime;
    }
  }
}

// Checks if `charset` is a valid charset, in lowercase ASCII. Takes `body` as
// well, to ensure it uses the specified charset.
bool IsAllowedCharset(base::StringPiece charset, const std::string& body) {
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

}  // namespace

AuctionDownloader::AuctionDownloader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    DownloadMode download_mode,
    MimeType mime_type,
    AuctionDownloaderCallback auction_downloader_callback,
    std::unique_ptr<NetworkEventsDelegate> network_events_delegate)
    : source_url_(source_url),
      mime_type_(mime_type),
      request_id_(base::UnguessableToken::Create().ToString()),
      auction_downloader_callback_(std::move(auction_downloader_callback)),
      network_events_delegate_(std::move(network_events_delegate)) {
  DCHECK(auction_downloader_callback_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = source_url;
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->enable_load_timing =
      *TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("devtools.timeline");
  resource_request->devtools_request_id = request_id_;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      MimeTypeToString(mime_type_));

  if (network_events_delegate_ != nullptr) {
    network_events_delegate_->OnNetworkSendRequest(*resource_request);
  }

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);

  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "devtools.timeline", "ResourceSendRequest", TRACE_EVENT_SCOPE_THREAD,
      base::TimeTicks::Now(), "data", [&](perfetto::TracedValue dest) {
        auto dict = std::move(dest).WriteDictionary();
        dict.Add("requestId", request_id_);
        dict.Add("url", source_url.spec());
      });

  // Abort on redirects.
  // TODO(mmenke): May want a browser-side proxy to block redirects instead.
  simple_url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &AuctionDownloader::OnRedirect, base::Unretained(this)));

  simple_url_loader_->SetOnResponseStartedCallback(base::BindRepeating(
      &AuctionDownloader::OnResponseStarted, base::Unretained(this)));

  simple_url_loader_->SetTimeoutDuration(base::Seconds(30));

  // TODO(mmenke): Consider limiting the size of response bodies.
  if (download_mode == DownloadMode::kActualDownload) {
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory, base::BindOnce(&AuctionDownloader::OnBodyReceived,
                                           base::Unretained(this)));
  } else {
    simple_url_loader_->DownloadHeadersOnly(
        url_loader_factory,
        base::BindOnce(&AuctionDownloader::OnHeadersOnlyReceived,
                       base::Unretained(this)));
  }
}

AuctionDownloader::NetworkEventsDelegate::~NetworkEventsDelegate() = default;
AuctionDownloader::~AuctionDownloader() = default;

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
  std::string allow_fledge;
  std::string auction_allowed;
  network::URLLoaderCompletionStatus completion_status =
      network::URLLoaderCompletionStatus(simple_url_loader->NetError());

  if (simple_url_loader->CompletionStatus()) {
    completion_status = simple_url_loader->CompletionStatus().value();
  }

  if (network_events_delegate_ != nullptr) {
    network_events_delegate_->OnNetworkRequestComplete(completion_status);
  }

  if (!body) {
    std::string error_msg;
    if (simple_url_loader->ResponseInfo() &&
        simple_url_loader->ResponseInfo()->headers &&
        simple_url_loader->ResponseInfo()->headers->response_code() / 100 !=
            2) {
      int status = simple_url_loader->ResponseInfo()->headers->response_code();
      error_msg = base::StringPrintf(
          "Failed to load %s HTTP status = %d %s.", source_url_.spec().c_str(),
          status,
          simple_url_loader->ResponseInfo()->headers->GetStatusText().c_str());
    } else {
      error_msg = base::StringPrintf(
          "Failed to load %s error = %s.", source_url_.spec().c_str(),
          net::ErrorToString(simple_url_loader->NetError()).c_str());
    }
    TraceResult(/*failure=*/true, /*completion_time=*/base::TimeTicks(),
                /*encoded_data_length=*/0,
                /*decoded_body_length=*/0);
    std::move(auction_downloader_callback_)
        .Run(/*body=*/nullptr, /*headers=*/nullptr, error_msg);
    return;
  }

  // Everything below is a network success even if it's a semantic failure.
  TraceResult(/*failure=*/false,
              simple_url_loader->CompletionStatus()->completion_time,
              simple_url_loader->ResponseInfo()->encoded_data_length,
              /*decoded_body_length=*/body->size());

  if (!simple_url_loader->ResponseInfo()->headers ||
      ((!simple_url_loader->ResponseInfo()->headers->GetNormalizedHeader(
            "X-Allow-FLEDGE", &allow_fledge) ||
        !base::EqualsCaseInsensitiveASCII(allow_fledge, "true")) &&
       (!simple_url_loader->ResponseInfo()->headers->GetNormalizedHeader(
            "Ad-Auction-Allowed", &auction_allowed) ||
        !base::EqualsCaseInsensitiveASCII(auction_allowed, "true")))) {
    std::move(auction_downloader_callback_)
        .Run(/*body=*/nullptr, /*headers=*/nullptr,
             base::StringPrintf(
                 "Rejecting load of %s due to lack of Ad-Auction-Allowed: true "
                 "(or the deprecated X-Allow-FLEDGE: true).",
                 source_url_.spec().c_str()));
  } else if (!MimeTypeIsConsistent(mime_type_,
                                   simple_url_loader->ResponseInfo())) {
    std::move(auction_downloader_callback_)
        .Run(/*body=*/nullptr, /*headers=*/nullptr,
             base::StringPrintf(
                 "Rejecting load of %s due to unexpected MIME type.",
                 source_url_.spec().c_str()));
  } else if ((mime_type_ != MimeType::kWebAssembly) &&
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
             /*error_msg=*/absl::nullopt);
  }
}

void AuctionDownloader::OnRedirect(
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(auction_downloader_callback_);

  // Need to cancel the load, to prevent the request from continuing.
  simple_url_loader_.reset();

  TraceResult(/*failure=*/true, /*completion_time=*/base::TimeTicks(),
              /*encoded_data_length=*/0,
              /*decoded_body_length=*/0);

  std::move(auction_downloader_callback_)
      .Run(/*body=*/nullptr, /*headers=*/nullptr,
           base::StringPrintf("Unexpected redirect on %s.",
                              source_url_.spec().c_str()));
}

void AuctionDownloader::OnResponseStarted(
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
          dict.Add("responseTime", response_head.response_time.ToJsTime());
        }

        // Only send load timing if it exists, e.g. not a cache hit.
        if (!response_head.load_timing.receive_headers_end.is_null()) {
          WriteTraceTiming(response_head.load_timing, dict.AddItem("timing"));
        }
      });
}

void AuctionDownloader::TraceResult(bool failure,
                                    base::TimeTicks completion_time,
                                    int64_t encoded_data_length,
                                    int64_t decoded_body_length) {
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
