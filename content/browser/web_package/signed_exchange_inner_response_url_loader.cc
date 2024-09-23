// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_inner_response_url_loader.h"

#include <stdint.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "content/browser/loader/cross_origin_read_blocking_checker.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/mojo_blob_reader.h"

namespace content {

namespace {

// A utility subclass of MojoBlobReader::Delegate that calls the passed callback
// in OnComplete().
class MojoBlobReaderDelegate : public storage::MojoBlobReader::Delegate {
 public:
  explicit MojoBlobReaderDelegate(base::OnceCallback<void(net::Error)> callback)
      : callback_(std::move(callback)) {}

 private:
  // storage::MojoBlobReader::Delegate
  RequestSideData DidCalculateSize(uint64_t total_size,
                                   uint64_t content_size) override {
    return DONT_REQUEST_SIDE_DATA;
  }

  void OnComplete(net::Error result, uint64_t total_written_bytes) override {
    std::move(callback_).Run(result);
  }

  base::OnceCallback<void(net::Error)> callback_;
};

}  // namespace

SignedExchangeInnerResponseURLLoader::SignedExchangeInnerResponseURLLoader(
    const network::ResourceRequest& request,
    network::mojom::URLResponseHeadPtr inner_response,
    std::unique_ptr<const storage::BlobDataHandle> blob_data_handle,
    const network::URLLoaderCompletionStatus& completion_status,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    bool is_navigation_request,
    scoped_refptr<base::RefCountedData<network::orb::PerFactoryState>>
        orb_state)
    : response_(std::move(inner_response)),
      blob_data_handle_(std::move(blob_data_handle)),
      completion_status_(completion_status),
      client_(std::move(client)),
      orb_state_(std::move(orb_state)) {
  DCHECK(response_->headers);

  // The `request.request_initiator` is assumed to be present and trustworthy
  // - it comes either from:
  // 1, The trustworthy navigation stack (via
  //    PrefetchedNavigationLoaderInterceptor::StartInnerResponse).
  // or
  // 2. SubresourceSignedExchangeURLLoaderFactory::CreateLoaderAndStart which
  //    validates the untrustworthy IPC payload as its very first action.
  DCHECK(request.request_initiator);

  // Keep the SSLInfo only when the request is for main frame main resource,
  // or devtools_request_id is set. Users can inspect the certificate for the
  // main frame using the info bubble in Omnibox, and for the subresources in
  // DevTools' Security panel.
  if (request.destination != network::mojom::RequestDestination::kDocument &&
      !request.devtools_request_id) {
    response_->ssl_info = std::nullopt;
  }
  UpdateRequestResponseStartTime(response_.get());
  response_->encoded_data_length = 0;
  if (is_navigation_request) {
    SendResponseBody();
    return;
  }

  if (network::cors::ShouldCheckCors(request.url, request.request_initiator,
                                     request.mode)) {
    const auto result = network::cors::CheckAccessAndReportMetrics(
        request.url,
        GetHeaderString(*response_,
                        network::cors::header_names::kAccessControlAllowOrigin),
        GetHeaderString(
            *response_,
            network::cors::header_names::kAccessControlAllowCredentials),
        request.credentials_mode, *request.request_initiator);
    if (!result.has_value()) {
      client_->OnComplete(network::URLLoaderCompletionStatus(result.error()));
      return;
    }
  }

  orb_checker_ = std::make_unique<CrossOriginReadBlockingChecker>(
      request, *response_, *blob_data_handle_, &orb_state_->data,
      base::BindOnce(&SignedExchangeInnerResponseURLLoader::
                         OnCrossOriginReadBlockingCheckComplete,
                     base::Unretained(this)));
}

SignedExchangeInnerResponseURLLoader::~SignedExchangeInnerResponseURLLoader() =
    default;

// static
std::optional<std::string>
SignedExchangeInnerResponseURLLoader::GetHeaderString(
    const network::mojom::URLResponseHead& response,
    const std::string& header_name) {
  DCHECK(response.headers);
  std::string header_value;
  if (!response.headers->GetNormalizedHeader(header_name, &header_value)) {
    return std::nullopt;
  }
  return header_value;
}

void SignedExchangeInnerResponseURLLoader::
    OnCrossOriginReadBlockingCheckComplete(
        CrossOriginReadBlockingChecker::Result result) {
  switch (result) {
    case CrossOriginReadBlockingChecker::Result::kAllowed:
      SendResponseBody();
      return;
    case CrossOriginReadBlockingChecker::Result::kNetError:
      client_->OnComplete(
          network::URLLoaderCompletionStatus(orb_checker_->GetNetError()));
      return;
    case CrossOriginReadBlockingChecker::Result::kBlocked_ShouldReport:
      break;
    case CrossOriginReadBlockingChecker::Result::kBlocked_ShouldNotReport:
      break;
  }

  // Send sanitized response.
  network::orb::SanitizeBlockedResponseHeaders(*response_);

  // Send an empty response's body.
  mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(nullptr, pipe_producer_handle, pipe_consumer_handle);
  if (rv != MOJO_RESULT_OK) {
    client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }
  client_->OnReceiveResponse(std::move(response_),
                             std::move(pipe_consumer_handle), std::nullopt);

  // Send a dummy OnComplete message.
  network::URLLoaderCompletionStatus status =
      network::URLLoaderCompletionStatus(net::OK);
  status.should_report_orb_blocking =
      result == CrossOriginReadBlockingChecker::Result::kBlocked_ShouldReport;
  client_->OnComplete(status);
}

void SignedExchangeInnerResponseURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  NOTREACHED_IN_MIGRATION();
}

void SignedExchangeInnerResponseURLLoader::SetPriority(
    net::RequestPriority priority,
    int intra_priority_value) {
  // There is nothing to do, because there is no prioritization mechanism for
  // reading a blob.
}

void SignedExchangeInnerResponseURLLoader::PauseReadingBodyFromNet() {
  // There is nothing to do, because we don't fetch the resource from the
  // network.
}

void SignedExchangeInnerResponseURLLoader::ResumeReadingBodyFromNet() {
  // There is nothing to do, because we don't fetch the resource from the
  // network.
}

void SignedExchangeInnerResponseURLLoader::SendResponseBody() {
  mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      network::features::GetDataPipeDefaultAllocationSize();
  MojoResult rv = mojo::CreateDataPipe(&options, pipe_producer_handle,
                                       pipe_consumer_handle);
  if (rv != MOJO_RESULT_OK) {
    client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SignedExchangeInnerResponseURLLoader::CreateMojoBlobReader,
          weak_factory_.GetWeakPtr(), std::move(pipe_producer_handle),
          std::make_unique<storage::BlobDataHandle>(*blob_data_handle_)));

  client_->OnReceiveResponse(std::move(response_),
                             std::move(pipe_consumer_handle), std::nullopt);
}

void SignedExchangeInnerResponseURLLoader::BlobReaderComplete(
    net::Error result) {
  network::URLLoaderCompletionStatus status;
  if (result == net::OK) {
    status = completion_status_;
    status.exists_in_cache = true;
    status.completion_time = base::TimeTicks::Now();
    status.encoded_data_length = 0;
  } else {
    status = network::URLLoaderCompletionStatus(status);
  }
  client_->OnComplete(status);
}

// static
void SignedExchangeInnerResponseURLLoader::CreateMojoBlobReader(
    base::WeakPtr<SignedExchangeInnerResponseURLLoader> loader,
    mojo::ScopedDataPipeProducerHandle pipe_producer_handle,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
  storage::MojoBlobReader::Create(
      blob_data_handle.get(), net::HttpByteRange(),
      std::make_unique<MojoBlobReaderDelegate>(base::BindOnce(
          &SignedExchangeInnerResponseURLLoader::BlobReaderCompleteOnIO,
          std::move(loader))),
      std::move(pipe_producer_handle));
}

// static
void SignedExchangeInnerResponseURLLoader::BlobReaderCompleteOnIO(
    base::WeakPtr<SignedExchangeInnerResponseURLLoader> loader,
    net::Error result) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SignedExchangeInnerResponseURLLoader::BlobReaderComplete,
                     std::move(loader), result));
}

// static
void SignedExchangeInnerResponseURLLoader::UpdateRequestResponseStartTime(
    network::mojom::URLResponseHead* response_head) {
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  const base::Time now = base::Time::Now();
  response_head->request_start = now_ticks;
  response_head->response_start = now_ticks;
  response_head->load_timing.request_start_time = now;
  response_head->load_timing.request_start = now_ticks;
}

}  // namespace content
