// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle_url_loader_factory.h"

#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/web_package/web_bundle_parser.h"
#include "components/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cross_origin_read_blocking.h"
#include "services/network/web_bundle_chunked_buffer.h"
#include "services/network/web_bundle_memory_quota_consumer.h"

namespace network {

namespace {

constexpr size_t kBlockedBodyAllocationSize = 1;

void DeleteProducerAndRunCallback(
    std::unique_ptr<mojo::DataPipeProducer> producer,
    base::OnceCallback<void(MojoResult result)> callback,
    MojoResult result) {
  std::move(callback).Run(result);
}

// Verify the serving constraints of Web Bundle HTTP responses.
// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#name-serving-constraints
bool CheckWebBundleServingConstraints(
    const network::mojom::URLResponseHead& response_head,
    std::string& out_error_message) {
  if (!response_head.headers ||
      !cors::IsOkStatus(response_head.headers->response_code())) {
    out_error_message = "Failed to fetch Web Bundle.";
    return false;
  }
  if (response_head.mime_type != "application/webbundle") {
    out_error_message =
        "Web Bundle response must have \"application/webbundle\" content-type.";
    return false;
  }
  if (!web_package::HasNoSniffHeader(response_head)) {
    out_error_message =
        "Web Bundle response must have \"X-Content-Type-Options: nosniff\" "
        "header.";
    return false;
  }
  return true;
}

// URLLoaderClient which wraps the real URLLoaderClient.
class WebBundleURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  WebBundleURLLoaderClient(
      base::WeakPtr<WebBundleURLLoaderFactory> factory,
      mojo::PendingRemote<network::mojom::URLLoaderClient> wrapped)
      : factory_(factory), wrapped_(std::move(wrapped)) {}

 private:
  // network::mojom::URLLoaderClient implementation:
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override {
    std::string error_message;
    if (!CheckWebBundleServingConstraints(*response_head, error_message)) {
      if (factory_) {
        factory_->ReportErrorAndCancelPendingLoaders(
            WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::
                kServingConstraintsNotMet,
            mojom::WebBundleErrorType::kServingConstraintsNotMet,
            error_message);
      }
    }

    base::UmaHistogramCustomCounts(
        "SubresourceWebBundles.ContentLength",
        response_head->content_length < 0 ? 0 : response_head->content_length,
        1, 50000000, 50);
    wrapped_->OnReceiveResponse(std::move(response_head));
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    wrapped_->OnReceiveRedirect(redirect_info, std::move(response_head));
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    wrapped_->OnUploadProgress(current_position, total_size,
                               std::move(ack_callback));
  }

  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override {
    wrapped_->OnReceiveCachedMetadata(std::move(data));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    wrapped_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    if (factory_)
      factory_->SetBundleStream(std::move(body));

    // Send empty body to the wrapped URLLoaderClient.
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = kBlockedBodyAllocationSize;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    MojoResult result = mojo::CreateDataPipe(&options, producer, consumer);
    if (result != MOJO_RESULT_OK) {
      wrapped_->OnComplete(
          URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      completed_ = true;
      return;
    }
    wrapped_->OnStartLoadingResponseBody(std::move(consumer));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (completed_)
      return;
    wrapped_->OnComplete(status);
  }

  base::WeakPtr<WebBundleURLLoaderFactory> factory_;
  mojo::Remote<network::mojom::URLLoaderClient> wrapped_;
  bool completed_ = false;
};

}  // namespace

class WebBundleURLLoaderFactory::URLLoader : public mojom::URLLoader {
 public:
  URLLoader(mojo::PendingReceiver<mojom::URLLoader> loader,
            const ResourceRequest& request,
            mojo::PendingRemote<mojom::URLLoaderClient> client,
            const base::Optional<url::Origin>& request_initiator_origin_lock,
            mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client)
      : url_(request.url),
        request_mode_(request.mode),
        request_initiator_(request.request_initiator),
        request_initiator_origin_lock_(request_initiator_origin_lock),
        receiver_(this, std::move(loader)),
        client_(std::move(client)),
        trusted_header_client_(std::move(trusted_header_client)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&URLLoader::OnMojoDisconnect, GetWeakPtr()));
    if (trusted_header_client_) {
      trusted_header_client_.set_disconnect_handler(
          base::BindOnce(&URLLoader::OnMojoDisconnect, GetWeakPtr()));
    }
  }
  URLLoader(const URLLoader&) = delete;
  URLLoader& operator=(const URLLoader&) = delete;

  const GURL& url() const { return url_; }
  const mojom::RequestMode& request_mode() const { return request_mode_; }

  const base::Optional<url::Origin>& request_initiator() const {
    return request_initiator_;
  }

  const base::Optional<url::Origin>& request_initiator_origin_lock() const {
    return request_initiator_origin_lock_;
  }

  base::WeakPtr<URLLoader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnResponse(mojom::URLResponseHeadPtr response) {
    client_->OnReceiveResponse(std::move(response));
  }

  void OnData(mojo::ScopedDataPipeConsumerHandle consumer) {
    client_->OnStartLoadingResponseBody(std::move(consumer));
  }

  void OnFail(net::Error error) {
    client_->OnComplete(URLLoaderCompletionStatus(error));
    delete this;
  }

  void OnWriteCompleted(MojoResult result) {
    URLLoaderCompletionStatus status(
        result == MOJO_RESULT_OK ? net::OK : net::ERR_INVALID_WEB_BUNDLE);
    client_->OnComplete(status);
    delete this;
  }

  void BlockResponseForCorb(mojom::URLResponseHeadPtr response_head) {
    // A minimum implementation to block CORB-protected resources.
    //
    // TODO(crbug.com/1082020): Re-use
    // network::URLLoader::BlockResponseForCorb(), instead of copying
    // essential parts from there, so that the two implementations won't
    // diverge further. That requires non-trivial refactoring.
    CrossOriginReadBlocking::SanitizeBlockedResponse(response_head.get());
    client_->OnReceiveResponse(std::move(response_head));

    // Send empty body to the URLLoaderClient.
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    if (CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
      OnFail(net::ERR_INSUFFICIENT_RESOURCES);
      return;
    }
    producer.reset();
    client_->OnStartLoadingResponseBody(std::move(consumer));

    URLLoaderCompletionStatus status;
    status.error_code = net::OK;
    status.completion_time = base::TimeTicks::Now();
    status.encoded_data_length = 0;
    status.encoded_body_length = 0;
    status.decoded_body_length = 0;
    client_->OnComplete(status);

    // Reset the connection to the URLLoaderClient.  This helps ensure that we
    // won't accidentally leak any data to the renderer from this point on.
    client_.reset();
  }

  mojo::Remote<mojom::TrustedHeaderClient>& trusted_header_client() {
    return trusted_header_client_;
  }

 private:
  // mojom::URLLoader
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const base::Optional<GURL>& new_url) override {
    NOTREACHED();
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    // Not supported (do nothing).
  }

  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  void OnMojoDisconnect() { delete this; }

  const GURL url_;
  mojom::RequestMode request_mode_;
  base::Optional<url::Origin> request_initiator_;
  // It is safe to hold |request_initiator_origin_lock_| in this factory because
  // 1). |request_initiator_origin_lock| is a property of |URLLoaderFactory|
  // (or, more accurately a property of |URLLoaderFactoryParams|), and
  // 2) |WebURLLoader| is always associated with the same URLLoaderFactory
  // (via URLLoaderFactory -> WebBundleManager -> WebBundleURLLoaderFactory
  // -> WebBundleURLLoader).
  const base::Optional<url::Origin> request_initiator_origin_lock_;
  mojo::Receiver<mojom::URLLoader> receiver_;
  mojo::Remote<mojom::URLLoaderClient> client_;
  mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client_;
  base::WeakPtrFactory<URLLoader> weak_ptr_factory_{this};
};

class WebBundleURLLoaderFactory::BundleDataSource
    : public web_package::mojom::BundleDataSource,
      public mojo::DataPipeDrainer::Client {
 public:
  using ReadToDataPipeCallback = base::OnceCallback<void(MojoResult result)>;

  BundleDataSource(mojo::PendingReceiver<web_package::mojom::BundleDataSource>
                       data_source_receiver,
                   mojo::ScopedDataPipeConsumerHandle bundle_body,
                   std::unique_ptr<WebBundleMemoryQuotaConsumer>
                       web_bundle_memory_quota_consumer,
                   base::OnceClosure memory_quota_exceeded_closure,
                   base::OnceClosure data_completed_closure)
      : data_source_receiver_(this, std::move(data_source_receiver)),
        pipe_drainer_(
            std::make_unique<mojo::DataPipeDrainer>(this,
                                                    std::move(bundle_body))),
        web_bundle_memory_quota_consumer_(
            std::move(web_bundle_memory_quota_consumer)),
        memory_quota_exceeded_closure_(
            std::move(memory_quota_exceeded_closure)),
        data_completed_closure_(std::move(data_completed_closure)) {}

  ~BundleDataSource() override {
    // The receiver must be closed before destructing pending callbacks in
    // |pending_reads_| / |pending_reads_to_data_pipe_|.
    data_source_receiver_.reset();
  }

  BundleDataSource(const BundleDataSource&) = delete;
  BundleDataSource& operator=(const BundleDataSource&) = delete;

  void ReadToDataPipe(mojo::ScopedDataPipeProducerHandle producer,
                      uint64_t offset,
                      uint64_t length,
                      ReadToDataPipeCallback callback) {
    TRACE_EVENT0("loading", "BundleDataSource::ReadToDataPipe");
    if (!finished_loading_ && !buffer_.ContainsAll(offset, length)) {
      // Current implementation does not support progressive loading of inner
      // response body.
      PendingReadToDataPipe pending;
      pending.producer = std::move(producer);
      pending.offset = offset;
      pending.length = length;
      pending.callback = std::move(callback);
      pending_reads_to_data_pipe_.push_back(std::move(pending));
      return;
    }

    auto data_source = buffer_.CreateDataSource(offset, length);
    if (!data_source) {
      // When there is no body to send, returns OK here without creating a
      // DataPipeProducer.
      std::move(callback).Run(MOJO_RESULT_OK);
      return;
    }

    auto writer = std::make_unique<mojo::DataPipeProducer>(std::move(producer));
    mojo::DataPipeProducer* raw_writer = writer.get();
    raw_writer->Write(std::move(data_source),
                      base::BindOnce(&DeleteProducerAndRunCallback,
                                     std::move(writer), std::move(callback)));
  }

  // mojom::BundleDataSource
  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    TRACE_EVENT0("loading", "BundleDataSource::Read");
    if (!finished_loading_ && !buffer_.ContainsAll(offset, length)) {
      PendingRead pending;
      pending.offset = offset;
      pending.length = length;
      pending.callback = std::move(callback);
      pending_reads_.push_back(std::move(pending));
      return;
    }
    uint64_t out_len = buffer_.GetAvailableLength(offset, length);
    std::vector<uint8_t> output(base::checked_cast<size_t>(out_len));
    buffer_.ReadData(offset, out_len, output.data());
    std::move(callback).Run(std::move(output));
  }

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    DCHECK(!finished_loading_);
    if (!web_bundle_memory_quota_consumer_->AllocateMemory(num_bytes)) {
      AbortPendingReads();
      if (memory_quota_exceeded_closure_) {
        // Defer calling |memory_quota_exceeded_closure_| to avoid the
        // UAF call in DataPipeDrainer::ReadData().
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, std::move(memory_quota_exceeded_closure_));
      }
      return;
    }
    buffer_.Append(reinterpret_cast<const uint8_t*>(data), num_bytes);
    ProcessPendingReads();
  }

  void OnDataComplete() override {
    DCHECK(!finished_loading_);
    base::UmaHistogramCustomCounts(
        "SubresourceWebBundles.ReceivedSize",
        base::saturated_cast<base::Histogram::Sample>(buffer_.size()), 1,
        50000000, 50);
    DCHECK(data_completed_closure_);
    // Defer calling |data_completed_closure_| not to run
    // |data_completed_closure_| before |memory_quota_exceeded_closure_|.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(data_completed_closure_));
    finished_loading_ = true;
    ProcessPendingReads();
  }

 private:
  void ProcessPendingReads() {
    std::vector<PendingRead> pendings(std::move(pending_reads_));
    std::vector<PendingReadToDataPipe> pipe_pendings(
        std::move(pending_reads_to_data_pipe_));

    for (auto& pending : pendings) {
      Read(pending.offset, pending.length, std::move(pending.callback));
    }

    for (auto& pending : pipe_pendings) {
      ReadToDataPipe(std::move(pending.producer), pending.offset,
                     pending.length, std::move(pending.callback));
    }
  }

  void AbortPendingReads() {
    std::vector<PendingRead> pendings(std::move(pending_reads_));
    std::vector<PendingReadToDataPipe> pipe_pendings(
        std::move(pending_reads_to_data_pipe_));

    for (auto& pending : pendings) {
      std::move(pending.callback).Run(std::vector<uint8_t>());
    }
    for (auto& pending : pipe_pendings) {
      std::move(pending.callback).Run(MOJO_RESULT_NOT_FOUND);
    }
  }

  struct PendingRead {
    uint64_t offset;
    uint64_t length;
    ReadCallback callback;
  };
  struct PendingReadToDataPipe {
    mojo::ScopedDataPipeProducerHandle producer;
    uint64_t offset;
    uint64_t length;
    ReadToDataPipeCallback callback;
  };

  mojo::Receiver<web_package::mojom::BundleDataSource> data_source_receiver_;
  WebBundleChunkedBuffer buffer_;
  std::vector<PendingRead> pending_reads_;
  std::vector<PendingReadToDataPipe> pending_reads_to_data_pipe_;
  bool finished_loading_ = false;
  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;
  std::unique_ptr<WebBundleMemoryQuotaConsumer>
      web_bundle_memory_quota_consumer_;
  base::OnceClosure memory_quota_exceeded_closure_;
  base::OnceClosure data_completed_closure_;
};

WebBundleURLLoaderFactory::WebBundleURLLoaderFactory(
    const GURL& bundle_url,
    mojo::Remote<mojom::WebBundleHandle> web_bundle_handle,
    const base::Optional<url::Origin>& request_initiator_origin_lock,
    std::unique_ptr<WebBundleMemoryQuotaConsumer>
        web_bundle_memory_quota_consumer)
    : bundle_url_(bundle_url),
      web_bundle_handle_(std::move(web_bundle_handle)),
      request_initiator_origin_lock_(request_initiator_origin_lock),
      web_bundle_memory_quota_consumer_(
          std::move(web_bundle_memory_quota_consumer)) {}

WebBundleURLLoaderFactory::~WebBundleURLLoaderFactory() {
  for (auto loader : pending_loaders_) {
    if (loader)
      loader->OnFail(net::ERR_FAILED);
  }
}

base::WeakPtr<WebBundleURLLoaderFactory> WebBundleURLLoaderFactory::GetWeakPtr()
    const {
  return weak_ptr_factory_.GetWeakPtr();
}

bool WebBundleURLLoaderFactory::HasError() const {
  return load_result_.has_value() &&
         *load_result_ != SubresourceWebBundleLoadResult::kSuccess;
}

void WebBundleURLLoaderFactory::SetBundleStream(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (HasError())
    return;
  mojo::PendingRemote<web_package::mojom::BundleDataSource> data_source;
  source_ = std::make_unique<BundleDataSource>(
      data_source.InitWithNewPipeAndPassReceiver(), std::move(body),
      std::move(web_bundle_memory_quota_consumer_),
      base::BindOnce(&WebBundleURLLoaderFactory::OnMemoryQuotaExceeded,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WebBundleURLLoaderFactory::OnDataCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  // WebBundleParser will self-destruct on remote mojo ends' disconnection.
  new web_package::WebBundleParser(parser_.BindNewPipeAndPassReceiver(),
                                   std::move(data_source));

  parser_->ParseMetadata(
      base::BindOnce(&WebBundleURLLoaderFactory::OnMetadataParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

mojo::PendingRemote<mojom::URLLoaderClient>
WebBundleURLLoaderFactory::WrapURLLoaderClient(
    mojo::PendingRemote<mojom::URLLoaderClient> wrapped) {
  mojo::PendingRemote<mojom::URLLoaderClient> client;
  auto client_impl = std::make_unique<WebBundleURLLoaderClient>(
      weak_ptr_factory_.GetWeakPtr(), std::move(wrapped));
  mojo::MakeSelfOwnedReceiver(std::move(client_impl),
                              client.InitWithNewPipeAndPassReceiver());
  return client;
}

void WebBundleURLLoaderFactory::StartSubresourceRequest(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client) {
  TRACE_EVENT0("loading", "WebBundleURLLoaderFactory::StartSubresourceRequest");
  URLLoader* loader = new URLLoader(
      std::move(receiver), url_request, std::move(client),
      request_initiator_origin_lock_, std::move(trusted_header_client));

  // Verify that WebBundle URL associated with the request is correct.
  DCHECK(url_request.web_bundle_token_params.has_value());
  if (url_request.web_bundle_token_params->bundle_url != bundle_url_) {
    mojo::ReportBadMessage(
        "WebBundleURLLoaderFactory: Bundle URL does not match");
    loader->OnFail(net::ERR_INVALID_ARGUMENT);
    return;
  }

  if (!loader->trusted_header_client()) {
    QueueOrStartLoader(loader->GetWeakPtr());
    return;
  }
  loader->trusted_header_client()->OnBeforeSendHeaders(
      url_request.headers,
      base::BindOnce(&WebBundleURLLoaderFactory::OnBeforeSendHeadersComplete,
                     weak_ptr_factory_.GetWeakPtr(), loader->GetWeakPtr()));
}

void WebBundleURLLoaderFactory::OnBeforeSendHeadersComplete(
    base::WeakPtr<URLLoader> loader,
    int result,
    const base::Optional<net::HttpRequestHeaders>& headers) {
  if (!loader)
    return;
  QueueOrStartLoader(loader);
}

void WebBundleURLLoaderFactory::QueueOrStartLoader(
    base::WeakPtr<URLLoader> loader) {
  if (!loader)
    return;
  if (HasError()) {
    loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    return;
  }
  if (!metadata_) {
    pending_loaders_.push_back(loader);
    return;
  }
  StartLoad(loader);
}

void WebBundleURLLoaderFactory::StartLoad(base::WeakPtr<URLLoader> loader) {
  DCHECK(metadata_);
  if (!loader)
    return;
  auto it = metadata_->requests.find(loader->url());
  if (it == metadata_->requests.end()) {
    web_bundle_handle_->OnWebBundleError(
        mojom::WebBundleErrorType::kResourceNotFound,
        loader->url().possibly_invalid_spec() +
            " is not found in the WebBundle.");
    loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    return;
  }
  // Currently, we just return the first response for the URL.
  // TODO(crbug.com/1082020): Support variant matching.
  auto& location = it->second->response_locations[0];

  parser_->ParseResponse(
      location->offset, location->length,
      base::BindOnce(&WebBundleURLLoaderFactory::OnResponseParsed,
                     weak_ptr_factory_.GetWeakPtr(), loader->GetWeakPtr()));
}

void WebBundleURLLoaderFactory::ReportErrorAndCancelPendingLoaders(
    SubresourceWebBundleLoadResult result,
    mojom::WebBundleErrorType error,
    const std::string& message) {
  DCHECK_NE(SubresourceWebBundleLoadResult::kSuccess, result);
  web_bundle_handle_->OnWebBundleError(error, message);
  MaybeReportLoadResult(result);
  auto pending_loaders = std::move(pending_loaders_);
  for (auto loader : pending_loaders) {
    if (loader)
      loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
  }

  source_.reset();
  parser_.reset();
}

void WebBundleURLLoaderFactory::OnMetadataParsed(
    web_package::mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  TRACE_EVENT0("loading", "WebBundleURLLoaderFactory::OnMetadataParsed");
  if (error) {
    ReportErrorAndCancelPendingLoaders(
        SubresourceWebBundleLoadResult::kMetadataParseError,
        mojom::WebBundleErrorType::kMetadataParseError, error->message);
    return;
  }

  metadata_ = std::move(metadata);
  if (data_completed_)
    MaybeReportLoadResult(SubresourceWebBundleLoadResult::kSuccess);
  for (auto loader : pending_loaders_)
    StartLoad(loader);
  pending_loaders_.clear();
}

void WebBundleURLLoaderFactory::OnResponseParsed(
    base::WeakPtr<URLLoader> loader,
    web_package::mojom::BundleResponsePtr response,
    web_package::mojom::BundleResponseParseErrorPtr error) {
  TRACE_EVENT0("loading", "WebBundleURLLoaderFactory::OnResponseParsed");
  if (!loader)
    return;
  if (error) {
    web_bundle_handle_->OnWebBundleError(
        mojom::WebBundleErrorType::kResponseParseError, error->message);
    loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    return;
  }
  const std::string header_string = web_package::CreateHeaderString(response);
  if (!loader->trusted_header_client()) {
    SendResponseToLoader(loader, header_string, response->payload_offset,
                         response->payload_length);
    return;
  }
  loader->trusted_header_client()->OnHeadersReceived(
      header_string, net::IPEndPoint(),
      base::BindOnce(&WebBundleURLLoaderFactory::OnHeadersReceivedComplete,
                     weak_ptr_factory_.GetWeakPtr(), loader->GetWeakPtr(),
                     header_string, response->payload_offset,
                     response->payload_length));
}

void WebBundleURLLoaderFactory::OnHeadersReceivedComplete(
    base::WeakPtr<URLLoader> loader,
    const std::string& original_header,
    uint64_t payload_offset,
    uint64_t payload_length,
    int result,
    const base::Optional<std::string>& headers,
    const base::Optional<GURL>& preserve_fragment_on_redirect_url) {
  if (!loader)
    return;
  SendResponseToLoader(loader, headers ? *headers : original_header,
                       payload_offset, payload_length);
}

void WebBundleURLLoaderFactory::SendResponseToLoader(
    base::WeakPtr<URLLoader> loader,
    const std::string& headers,
    uint64_t payload_offset,
    uint64_t payload_length) {
  if (!loader)
    return;
  mojom::URLResponseHeadPtr response_head =
      web_package::CreateResourceResponseFromHeaderString(headers);
  // Currently we allow only net::HTTP_OK responses in bundles.
  // TODO(crbug.com/990733): Revisit this once
  // https://github.com/WICG/webpackage/issues/478 is resolved.
  if (response_head->headers->response_code() != net::HTTP_OK) {
    web_bundle_handle_->OnWebBundleError(
        mojom::WebBundleErrorType::kResponseParseError,
        "Invalid response code " +
            base::NumberToString(response_head->headers->response_code()));
    loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    return;
  }

  response_head->web_bundle_url = bundle_url_;
  // Add an artifical "X-Content-Type-Options: "nosniff" header, which is
  // explained at
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#name-responses.
  response_head->headers->SetHeader("X-Content-Type-Options", "nosniff");

  auto corb_analyzer =
      std::make_unique<CrossOriginReadBlocking::ResponseAnalyzer>(
          loader->url(), loader->request_initiator(), *response_head,
          loader->request_initiator_origin_lock(), loader->request_mode());

  if (corb_analyzer->ShouldBlock()) {
    loader->BlockResponseForCorb(std::move(response_head));
    return;
  }

  loader->OnResponse(std::move(response_head));

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
    loader->OnFail(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }
  loader->OnData(std::move(consumer));
  source_->ReadToDataPipe(
      std::move(producer), payload_offset, payload_length,
      base::BindOnce(&URLLoader::OnWriteCompleted, loader->GetWeakPtr()));
}

void WebBundleURLLoaderFactory::OnMemoryQuotaExceeded() {
  TRACE_EVENT0("loading", "WebBundleURLLoaderFactory::OnMemoryQuotaExceeded");
  ReportErrorAndCancelPendingLoaders(
      SubresourceWebBundleLoadResult::kMemoryQuotaExceeded,
      mojom::WebBundleErrorType::kMemoryQuotaExceeded,
      "Memory quota exceeded. Currently, there is an upper limit on the total "
      "size of subresource web bundles in a process. See "
      "https://crbug.com/1154140 for more details.");
}

void WebBundleURLLoaderFactory::OnDataCompleted() {
  DCHECK(!data_completed_);
  data_completed_ = true;
  if (metadata_)
    MaybeReportLoadResult(SubresourceWebBundleLoadResult::kSuccess);
}

void WebBundleURLLoaderFactory::MaybeReportLoadResult(
    SubresourceWebBundleLoadResult result) {
  if (load_result_.has_value())
    return;
  load_result_ = result;
  base::UmaHistogramEnumeration("SubresourceWebBundles.LoadResult", result);
  web_bundle_handle_->OnWebBundleLoadFinished(
      result == SubresourceWebBundleLoadResult::kSuccess);
}

}  // namespace network
