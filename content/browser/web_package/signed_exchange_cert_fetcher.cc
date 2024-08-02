// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_cert_fetcher.h"

#include <optional>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kCertChainMimeType[] = "application/cert-chain+cbor";

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
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
    const GURL& cert_url,
    bool force_fetch,
    CertificateCallback callback,
    SignedExchangeDevToolsProxy* devtools_proxy,
    const std::optional<base::UnguessableToken>& throttling_profile_id,
    net::IsolationInfo isolation_info,
    const std::optional<url::Origin>& initiator) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::CreateAndStart");
  std::unique_ptr<SignedExchangeCertFetcher> cert_fetcher(
      new SignedExchangeCertFetcher(
          std::move(shared_url_loader_factory), std::move(throttles), cert_url,
          force_fetch, std::move(callback), devtools_proxy,
          throttling_profile_id, std::move(isolation_info), initiator));
  cert_fetcher->Start();
  return cert_fetcher;
}

// https://wicg.github.io/webpackage/loading.html#handling-cert-url
SignedExchangeCertFetcher::SignedExchangeCertFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
    const GURL& cert_url,
    bool force_fetch,
    CertificateCallback callback,
    SignedExchangeDevToolsProxy* devtools_proxy,
    const std::optional<base::UnguessableToken>& throttling_profile_id,
    net::IsolationInfo isolation_info,
    const std::optional<url::Origin>& initiator)
    : shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      throttles_(std::move(throttles)),
      resource_request_(std::make_unique<network::ResourceRequest>()),
      callback_(std::move(callback)),
      devtools_proxy_(devtools_proxy) {
  // TODO(crbug.com/40558902): Revisit more ResourceRequest flags.
  resource_request_->url = cert_url;
  resource_request_->request_initiator = initiator;
  resource_request_->resource_type =
      static_cast<int>(blink::mojom::ResourceType::kSubResource);
  resource_request_->destination = network::mojom::RequestDestination::kEmpty;
  // Cert requests should not send credential information, because the default
  // credentials mode of Fetch is "omit".
  resource_request_->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request_->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                       kCertChainMimeType);
  if (force_fetch) {
    resource_request_->load_flags |=
        net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  }
  if (devtools_proxy_) {
    cert_request_id_ = base::UnguessableToken::Create();
    resource_request_->enable_load_timing = true;
  }
  resource_request_->throttling_profile_id = throttling_profile_id;
  if (!isolation_info.IsEmpty()) {
    resource_request_->trusted_params =
        network::ResourceRequest::TrustedParams();
    resource_request_->trusted_params->isolation_info = isolation_info;
  }
}

SignedExchangeCertFetcher::~SignedExchangeCertFetcher() = default;

void SignedExchangeCertFetcher::Start() {
  if (devtools_proxy_) {
    DCHECK(cert_request_id_);
    devtools_proxy_->CertificateRequestSent(*cert_request_id_,
                                            *resource_request_);
  }
  // When NetworkService enabled, data URL is not handled by the passed
  // URLRequestContext's SharedURLLoaderFactory.
  if (resource_request_->url.SchemeIs(url::kDataScheme)) {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
            base::BindOnce(&SignedExchangeCertFetcher::OnDataURLRequest,
                           base::Unretained(this)));
  }
  url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      std::move(shared_url_loader_factory_), std::move(throttles_),
      signed_exchange_utils::MakeRequestID() /* request_id */,
      network::mojom::kURLLoadOptionNone, resource_request_.get(), this,
      kCertFetcherTrafficAnnotation,
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void SignedExchangeCertFetcher::Abort() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::Abort");
  MaybeNotifyCompletionToDevtools(
      network::URLLoaderCompletionStatus(net::ERR_ABORTED));
  DCHECK(callback_);
  url_loader_ = nullptr;
  body_.reset();
  handle_watcher_ = nullptr;
  body_string_.clear();
  devtools_proxy_ = nullptr;
  std::move(callback_).Run(SignedExchangeLoadResult::kCertFetchError, nullptr,
                           net::IPAddress());
}

void SignedExchangeCertFetcher::OnHandleReady(MojoResult result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnHandleReady");
  base::span<const uint8_t> buffer;
  MojoResult rv = body_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
  if (rv == MOJO_RESULT_OK) {
    if (body_string_.size() + buffer.size() >
        g_max_cert_size_for_signed_exchange) {
      body_->EndReadData(buffer.size());
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy_,
          "The response body size of certificate message exceeds the limit.");
      Abort();
      return;
    }
    body_string_.append(base::as_string_view(buffer));
    body_->EndReadData(buffer.size());
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

  // Notify the completion to the devtools here because |this| may be deleted
  // before OnComplete() is called.
  MaybeNotifyCompletionToDevtools(network::URLLoaderCompletionStatus(net::OK));

  std::unique_ptr<SignedExchangeCertificateChain> cert_chain =
      SignedExchangeCertificateChain::Parse(
          base::as_bytes(base::make_span(body_string_)), devtools_proxy_);
  body_string_.clear();
  if (!cert_chain) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_, "Failed to get certificate chain from message.");
    std::move(callback_).Run(SignedExchangeLoadResult::kCertParseError, nullptr,
                             cert_server_ip_address_);
    return;
  }
  std::move(callback_).Run(SignedExchangeLoadResult::kSuccess,
                           std::move(cert_chain), cert_server_ip_address_);
}

// network::mojom::URLLoaderClient
void SignedExchangeCertFetcher::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {}

void SignedExchangeCertFetcher::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnReceiveResponse");
  if (devtools_proxy_) {
    DCHECK(cert_request_id_);
    devtools_proxy_->CertificateResponseReceived(*cert_request_id_,
                                                 resource_request_->url, *head);
  }

  cert_server_ip_address_ = head->remote_endpoint.address();

  // |headers| is null when loading data URL.
  if (head->headers && head->headers->response_code() != net::HTTP_OK) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_, base::StringPrintf("Invalid reponse code: %d",
                                            head->headers->response_code()));
    Abort();
    return;
  }

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cert-chain-format
  // "The resource at a signature's cert-url MUST have the
  // application/cert-chain+cbor content type" [spec text]
  if (head->mime_type != kCertChainMimeType) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_,
        base::StringPrintf(
            "Content type of cert-url must be application/cert-chain+cbor. "
            "Actual content type: %s",
            head->mime_type.c_str()));
    Abort();
    return;
  }

  if (head->content_length > 0) {
    if (base::checked_cast<size_t>(head->content_length) >
        g_max_cert_size_for_signed_exchange) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy_,
          base::StringPrintf("Invalid content length: %" PRIu64,
                             head->content_length));
      Abort();
      return;
    }
    body_string_.reserve(head->content_length);
  }

  UMA_HISTOGRAM_BOOLEAN("SignedExchange.CertificateFetch.CacheHit",
                        head->was_fetched_via_cache);

  if (!body)
    return;

  body_ = std::move(body);
  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
      base::SequencedTaskRunner::GetCurrentDefault());
  handle_watcher_->Watch(
      body_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&SignedExchangeCertFetcher::OnHandleReady,
                          base::Unretained(this)));
}

void SignedExchangeCertFetcher::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
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
  NOTREACHED_IN_MIGRATION();
}

void SignedExchangeCertFetcher::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  // Do nothing.
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kSignedExchangeCertFetcher);
}

void SignedExchangeCertFetcher::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertFetcher::OnComplete");
  MaybeNotifyCompletionToDevtools(status);
  if (!handle_watcher_)
    Abort();
}

void SignedExchangeCertFetcher::OnDataURLRequest(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient>
        url_loader_client_remote) {
  mojo::Remote<network::mojom::URLLoaderFactory> factory(
      DataURLLoaderFactory::Create());
  factory->CreateLoaderAndStart(
      std::move(url_loader_receiver), 0, 0, resource_request,
      std::move(url_loader_client_remote),
      net::MutableNetworkTrafficAnnotationTag(kCertFetcherTrafficAnnotation));
}

void SignedExchangeCertFetcher::MaybeNotifyCompletionToDevtools(
    const network::URLLoaderCompletionStatus& status) {
  if (!devtools_proxy_ || has_notified_completion_to_devtools_)
    return;
  DCHECK(cert_request_id_);
  devtools_proxy_->CertificateRequestCompleted(*cert_request_id_, status);
  has_notified_completion_to_devtools_ = true;
}

// static
base::ScopedClosureRunner SignedExchangeCertFetcher::SetMaxCertSizeForTest(
    size_t max_cert_size) {
  g_max_cert_size_for_signed_exchange = max_cert_size;
  return base::ScopedClosureRunner(base::BindOnce(&ResetMaxCertSizeForTest));
}

}  // namespace content
