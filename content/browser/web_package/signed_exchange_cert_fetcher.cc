// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_cert_fetcher.h"

#include "base/format_macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/throttling_url_loader.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

namespace {

// Limit certificate messages to 100k, matching BoringSSL's default limit.
const size_t kMaxCertSizeForSignedExchange = 100 * 1024;
static size_t g_max_cert_size_for_signed_exchange =
    kMaxCertSizeForSignedExchange;

void ResetMaxCertSizeForTest() {
  g_max_cert_size_for_signed_exchange = kMaxCertSizeForSignedExchange;
}

const net::NetworkTrafficAnnotationTag kCertFetcherTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("sigined_exchange_cert_fetcher", R"(
    semantics {
      sender: "Certificate Fetcher for Signed HTTP Exchanges"
      description:
        "Retrieves the X.509v3 certificates to verify the signed headers of "
        "Signed HTTP Exchanges."
      trigger:
        "Navigating Chrome (ex: clicking on a link) to an URL and the server "
        "returns a Signed HTTP Exchange."
      data: "Arbitrary site-controlled data can be included in the URL."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature cannot be disabled by settings. This feature is not "
        "enabled by default yet."
      policy_exception_justification: "Not implemented."
    }
    comments:
      "Chrome would be unable to handle Signed HTTP Exchanges without this "
      "type of request."
    )");

}  // namespace

// static
std::unique_ptr<SignedExchangeCertFetcher>
SignedExchangeCertFetcher::CreateAndStart(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    const GURL& cert_url,
    url::Origin request_initiator,
    bool force_fetch,
    SignedExchangeVersion version,
    CertificateCallback callback,
    SignedExchangeDevToolsProxy* devtools_proxy,
    const base::Optional<base::UnguessableToken>& throttling_profile_id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::CreateAndStart");
  std::unique_ptr<SignedExchangeCertFetcher> cert_fetcher(
      new SignedExchangeCertFetcher(
          std::move(shared_url_loader_factory), std::move(throttles), cert_url,
          std::move(request_initiator), force_fetch, version,
          std::move(callback), devtools_proxy, throttling_profile_id));
  cert_fetcher->Start();
  return cert_fetcher;
}

SignedExchangeCertFetcher::SignedExchangeCertFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    const GURL& cert_url,
    url::Origin request_initiator,
    bool force_fetch,
    SignedExchangeVersion version,
    CertificateCallback callback,
    SignedExchangeDevToolsProxy* devtools_proxy,
    const base::Optional<base::UnguessableToken>& throttling_profile_id)
    : shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      throttles_(std::move(throttles)),
      resource_request_(std::make_unique<network::ResourceRequest>()),
      version_(version),
      callback_(std::move(callback)),
      devtools_proxy_(devtools_proxy) {
  // TODO(https://crbug.com/803774): Revisit more ResourceRequest flags.
  resource_request_->url = cert_url;
  resource_request_->request_initiator = std::move(request_initiator);
  resource_request_->resource_type = RESOURCE_TYPE_SUB_RESOURCE;
  // Cert requests should not send credential informartion, because the default
  // credentials mode of Fetch is "omit".
  resource_request_->load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA |
                                  net::LOAD_DO_NOT_SAVE_COOKIES |
                                  net::LOAD_DO_NOT_SEND_COOKIES;
  if (force_fetch) {
    resource_request_->load_flags |=
        net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  }
  resource_request_->render_frame_id = MSG_ROUTING_NONE;
  if (devtools_proxy_) {
    cert_request_id_ = base::UnguessableToken::Create();
    resource_request_->enable_load_timing = true;
  }
  resource_request_->throttling_profile_id = throttling_profile_id;
}

SignedExchangeCertFetcher::~SignedExchangeCertFetcher() = default;

void SignedExchangeCertFetcher::Start() {
  if (devtools_proxy_) {
    DCHECK(cert_request_id_);
    devtools_proxy_->CertificateRequestSent(*cert_request_id_,
                                            *resource_request_);
  }
  url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
      std::move(shared_url_loader_factory_), std::move(throttles_),
      0 /* routing_id */,
      ResourceDispatcherHostImpl::Get()->MakeRequestID() /* request_id */,
      network::mojom::kURLLoadOptionNone, resource_request_.get(), this,
      kCertFetcherTrafficAnnotation, base::ThreadTaskRunnerHandle::Get());
}

void SignedExchangeCertFetcher::Abort() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::Abort");
  DCHECK(callback_);
  url_loader_ = nullptr;
  body_.reset();
  handle_watcher_ = nullptr;
  body_string_.clear();
  devtools_proxy_ = nullptr;
  std::move(callback_).Run(SignedExchangeLoadResult::kCertFetchError, nullptr);
}

void SignedExchangeCertFetcher::OnHandleReady(MojoResult result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnHandleReady");
  const void* buffer = nullptr;
  uint32_t num_bytes = 0;
  MojoResult rv =
      body_->BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (rv == MOJO_RESULT_OK) {
    if (body_string_.size() + num_bytes > g_max_cert_size_for_signed_exchange) {
      body_->EndReadData(num_bytes);
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy_,
          "The response body size of certificate message exceeds the limit.");
      Abort();
      return;
    }
    body_string_.append(static_cast<const char*>(buffer), num_bytes);
    body_->EndReadData(num_bytes);
  } else if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
    OnDataComplete();
  } else {
    DCHECK_EQ(MOJO_RESULT_SHOULD_WAIT, rv);
  }
}

void SignedExchangeCertFetcher::OnDataComplete() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnDataComplete");
  DCHECK(callback_);
  url_loader_ = nullptr;
  body_.reset();
  handle_watcher_ = nullptr;

  std::unique_ptr<SignedExchangeCertificateChain> cert_chain =
      SignedExchangeCertificateChain::Parse(
          version_, base::as_bytes(base::make_span(body_string_)),
          devtools_proxy_);
  body_string_.clear();
  if (!cert_chain) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_, "Failed to get certificate chain from message.");
    std::move(callback_).Run(SignedExchangeLoadResult::kCertParseError,
                             nullptr);
    return;
  }
  std::move(callback_).Run(SignedExchangeLoadResult::kSuccess,
                           std::move(cert_chain));
}

// network::mojom::URLLoaderClient
void SignedExchangeCertFetcher::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnReceiveResponse");
  if (devtools_proxy_) {
    DCHECK(cert_request_id_);
    devtools_proxy_->CertificateResponseReceived(*cert_request_id_,
                                                 resource_request_->url, head);
  }

  // |headers| is null when loading data URL.
  if (head.headers && head.headers->response_code() != net::HTTP_OK) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_, base::StringPrintf("Invalid reponse code: %d",
                                            head.headers->response_code()));
    Abort();
    return;
  }

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cert-chain-format
  // "The resource at a signature's cert-url MUST have the
  // application/cert-chain+cbor content type" [spec text]
  if (head.mime_type != "application/cert-chain+cbor") {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_,
        base::StringPrintf(
            "Content type of cert-url must be application/cert-chain+cbor. "
            "Actual content type: %s",
            head.mime_type.c_str()));
    Abort();
    return;
  }

  if (head.content_length > 0) {
    if (base::checked_cast<size_t>(head.content_length) >
        g_max_cert_size_for_signed_exchange) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy_,
          base::StringPrintf("Invalid content length: %" PRIu64,
                             head.content_length));
      Abort();
      return;
    }
    body_string_.reserve(head.content_length);
  }

  UMA_HISTOGRAM_BOOLEAN("SignedExchange.CertificateFetch.CacheHit",
                        head.was_fetched_via_cache);
}

void SignedExchangeCertFetcher::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnReceiveRedirect");
  // Currently the cert fetcher doesn't allow any redirects.
  Abort();
}

void SignedExchangeCertFetcher::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // Cert fetching doesn't have request body.
  NOTREACHED();
}

void SignedExchangeCertFetcher::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  // Cert fetching doesn't use cached metadata.
  NOTREACHED();
}

void SignedExchangeCertFetcher::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  // Do nothing.
}

void SignedExchangeCertFetcher::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnStartLoadingResponseBody");
  body_ = std::move(body);
  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
      base::SequencedTaskRunnerHandle::Get());
  handle_watcher_->Watch(
      body_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&SignedExchangeCertFetcher::OnHandleReady,
                          base::Unretained(this)));
}

void SignedExchangeCertFetcher::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnComplete");
  if (devtools_proxy_) {
    DCHECK(cert_request_id_);
    devtools_proxy_->CertificateRequestCompleted(*cert_request_id_, status);
  }
  if (!handle_watcher_)
    Abort();
}

// static
base::ScopedClosureRunner SignedExchangeCertFetcher::SetMaxCertSizeForTest(
    size_t max_cert_size) {
  g_max_cert_size_for_signed_exchange = max_cert_size;
  return base::ScopedClosureRunner(base::BindOnce(&ResetMaxCertSizeForTest));
}

}  // namespace content
