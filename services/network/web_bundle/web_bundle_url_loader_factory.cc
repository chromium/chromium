// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle/web_bundle_url_loader_factory.h"

#include <optional>

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/web_package/web_bundle_parser.h"
#include "components/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/web_bundle/web_bundle_chunked_buffer.h"
#include "services/network/web_bundle/web_bundle_memory_quota_consumer.h"

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
      !IsSuccessfulStatus(response_head.headers->response_code())) {
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
  mojo::ScopedDataPipeConsumerHandle HandleReceiveBody(
      mojo::ScopedDataPipeConsumerHandle body) {
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
      return mojo::ScopedDataPipeConsumerHandle();
    }

    return consumer;
  }

  // network::mojom::URLLoaderClient implementation:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    wrapped_->OnReceiveEarlyHints(std::move(early_hints));
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
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
    mojo::ScopedDataPipeConsumerHandle consumer;
    if (body)
      consumer = HandleReceiveBody(std::move(body));
    wrapped_->OnReceiveResponse(std::move(response_head), std::move(consumer),
                                std::move(cached_metadata));
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    // TODO(crbug.com/40786524): Support redirection for WebBundle requests.
    if (factory_) {
      factory_->ReportErrorAndCancelPendingLoaders(
          WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::
              kWebBundleRedirected,
          mojom::WebBundleErrorType::kWebBundleRedirected,
          "URL redirection of Subresource Web Bundles is currently not "
          "supported.");
    }
    wrapped_->OnComplete(
        URLLoaderCompletionStatus(net::ERR_INVALID_WEB_BUNDLE));
    completed_ = true;
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    wrapped_->OnUploadProgress(current_position, total_size,
                               std::move(ack_callback));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    network::RecordOnTransferSizeUpdatedUMA(
        network::OnTransferSizeUpdatedFrom::kWebBundleURLLoaderClient);

    wrapped_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (status.error_code != net::OK) {
      if (factory_)
        factory_->OnWebBundleFetchFailed();
      base::UmaHistogramSparse("SubresourceWebBundles.BundleFetchErrorCode",
                               -status.error_code);
    }
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
            mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client,
            base::Time request_start_time,
            base::TimeTicks request_start_time_ticks,
            base::OnceCallback<void(URLLoader*)> disconnected_callback)
      : url_(request.url),
        bundle_url_(request.web_bundle_token_params->bundle_url),
        request_mode_(request.mode),
        request_initiator_(request.request_initiator),
        request_destination_(request.destination),
        request_headers_(request.headers),
        devtools_request_id_(request.devtools_request_id),
        is_trusted_(request.trusted_params),
        receiver_(this, std::move(loader)),
        client_(std::move(client)),
        trusted_header_client_(std::move(trusted_header_client)),
        will_be_deleted_callback_(std::move(disconnected_callback)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&URLLoader::OnMojoDisconnect, GetWeakPtr()));
    if (trusted_header_client_) {
      trusted_header_client_.set_disconnect_handler(
          base::BindOnce(&URLLoader::OnMojoDisconnect, GetWeakPtr()));
    }
    load_timing_.request_start_time = request_start_time;
    load_timing_.request_start = request_start_time_ticks;
    load_timing_.send_start = request_start_time_ticks;
    load_timing_.send_end = request_start_time_ticks;
  }
  URLLoader(const URLLoader&) = delete;
  URLLoader& operator=(const URLLoader&) = delete;

  const GURL& url() const { return url_; }
  const GURL& bundle_url() const { return bundle_url_; }
  const mojom::RequestMode& request_mode() const { return request_mode_; }
  const net::HttpRequestHeaders& request_headers() const {
    return request_headers_;
  }
  const std::optional<std::string>& devtools_request_id() const {
    return devtools_request_id_;
  }

  const std::optional<url::Origin>& request_initiator() const {
    return request_initiator_;
  }

  mojom::RequestDestination request_destination() const {
    return request_destination_;
  }

  base::WeakPtr<URLLoader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void deleteThis() {
    std::move(will_be_deleted_callback_).Run(this);
    delete this;
  }

  void OnResponse(mojom::URLResponseHeadPtr response,
                  mojo::ScopedDataPipeConsumerHandle consumer) {
    client_->OnReceiveResponse(std::move(response), std::move(consumer),
                               std::nullopt);
  }

  void OnFail(net::Error error) {
    client_->OnComplete(URLLoaderCompletionStatus(error));
    deleteThis();
  }

  void OnWriteCompleted(MojoResult result) {
    URLLoaderCompletionStatus status(
        result == MOJO_RESULT_OK ? net::OK : net::ERR_INVALID_WEB_BUNDLE);
    status.encoded_data_length = body_length_ + headers_bytes_;
    // For these values we use the same `body_length_` as we don't currently
    // provide encoding in WebBundles.
    status.encoded_body_length = body_length_;
    status.decoded_body_length = body_length_;
    client_->OnComplete(status);
    deleteThis();
  }

  void BlockResponseForOrb(mojom::URLResponseHeadPtr response_head) {
    // A minimum implementation to block ORB-protected resources.
    //
    // TODO(crbug.com/40130781): Re-use
    // network::URLLoader::BlockResponseForOrb(), instead of copying
    // essential parts from there, so that the two implementations won't
    // diverge further. That requires non-trivial refactoring.
    orb::SanitizeBlockedResponseHeaders(*response_head);

    // Send empty body to the URLLoaderClient.
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    if (CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
      OnFail(net::ERR_INSUFFICIENT_RESOURCES);
      return;
    }
    producer.reset();
    client_->OnReceiveResponse(std::move(response_head), std::move(consumer),
                               std::nullopt);

    // ORB responses are reported as a success.
    CompleteBlockedResponse(net::OK, std::nullopt);
  }

  bool is_trusted() const { return is_trusted_; }

  void CompleteBlockedResponse(
      int error_code,
      std::optional<mojom::BlockedByResponseReason> reason) {
    URLLoaderCompletionStatus status;
    status.error_code = error_code;
    status.completion_time = base::TimeTicks::Now();
    status.encoded_data_length = 0;
    status.encoded_body_length = 0;
    status.decoded_body_length = 0;
    status.blocked_by_response_reason = reason;
    client_->OnComplete(status);

    // Reset the connection to the URLLoaderClient.  This helps ensure that we
    // won't accidentally leak any data to the renderer from this point on.
    client_.reset();
    deleteThis();
  }

  mojo::Remote<mojom::TrustedHeaderClient>& trusted_header_client() {
    return trusted_header_client_;
  }

  net::LoadTimingInfo load_timing() { return load_timing_; }
  void SetBodyLength(uint64_t body_length) { body_length_ = body_length; }
  void SetHeadersBytes(size_t headers_bytes) { headers_bytes_ = headers_bytes; }
  void SetResponseStartTime(base::TimeTicks response_start_time) {
    load_timing_.receive_headers_start = response_start_time;
    load_timing_.receive_headers_end = response_start_time;
  }

 private:
  // mojom::URLLoader
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    NOTREACHED_IN_MIGRATION();
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    // Not supported (do nothing).
  }

  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  void OnMojoDisconnect() { deleteThis(); }

  const GURL url_;
  const GURL bundle_url_;
  mojom::RequestMode request_mode_;
  std::optional<url::Origin> request_initiator_;
  mojom::RequestDestination request_destination_;
  net::HttpRequestHeaders request_headers_;
  std::optional<std::string> devtools_request_id_;
  const bool is_trusted_;
  mojo::Receiver<mojom::URLLoader> receiver_;
  mojo::Remote<mojom::URLLoaderClient> client_;
  mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client_;
  uint64_t body_length_;
  size_t headers_bytes_;
  net::LoadTimingInfo load_timing_;
  base::TimeTicks request_send_time_;
  base::TimeTicks response_start_time_;
  base::OnceCallback<void(URLLoader*)> will_be_deleted_callback_;
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

  // Implements mojom::BundleDataSource.
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
    uint64_t read_len = buffer_.ReadData(offset, output);
    output.resize(base::checked_cast<size_t>(read_len));
    std::move(callback).Run(std::move(output));
  }

  void Length(LengthCallback callback) override { std::move(callback).Run(-1); }

  void IsRandomAccessContext(IsRandomAccessContextCallback callback) override {
    std::move(callback).Run(false);
  }

  void Close(CloseCallback callback) override {
    NOTIMPLEMENTED() << "Close() is not implemented";
  }

  // Implements mojo::DataPipeDrainer::Client.
  void OnDataAvailable(base::span<const uint8_t> data) override {
    DCHECK(!finished_loading_);
    if (!web_bundle_memory_quota_consumer_->AllocateMemory(data.size())) {
      AbortPendingReads();
      if (memory_quota_exceeded_closure_) {
        // Defer calling |memory_quota_exceeded_closure_| to avoid the
        // UAF call in DataPipeDrainer::ReadData().
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(memory_quota_exceeded_closure_));
      }
      return;
    }
    buffer_.Append(data);
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
    const ResourceRequest::WebBundleTokenParams& web_bundle_token_params,
    mojo::Remote<mojom::WebBundleHandle> web_bundle_handle,
    std::unique_ptr<WebBundleMemoryQuotaConsumer>
        web_bundle_memory_quota_consumer,
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
    std::optional<std::string> devtools_request_id,
    const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojom::CrossOriginEmbedderPolicyReporter* coep_reporter)
    : bundle_url_(bundle_url),
      web_bundle_handle_(std::move(web_bundle_handle)),
      web_bundle_memory_quota_consumer_(
          std::move(web_bundle_memory_quota_consumer)),
      devtools_observer_(std::move(devtools_observer)),
      devtools_request_id_(std::move(devtools_request_id)),
      cross_origin_embedder_policy_(cross_origin_embedder_policy),
      coep_reporter_(coep_reporter) {
  if (bundle_url != web_bundle_token_params.bundle_url) {
    // This happens when WebBundle request is redirected by WebRequest extension
    // API.
    // TODO(crbug.com/40786524): Support redirection for WebBundle requests.
    ReportErrorAndCancelPendingLoaders(
        SubresourceWebBundleLoadResult::kWebBundleRedirected,
        mojom::WebBundleErrorType::kWebBundleRedirected,
        "URL redirection of Subresource Web Bundles is currently not "
        "supported.");
  }
}

WebBundleURLLoaderFactory::~WebBundleURLLoaderFactory() {
  for (auto loader : pending_loaders_) {
    if (loader)
      loader->OnFail(net::ERR_FAILED);
  }
}

base::WeakPtr<WebBundleURLLoaderFactory>
WebBundleURLLoaderFactory::GetWeakPtr() {
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

  mojo::MakeSelfOwnedReceiver(std::make_unique<web_package::WebBundleParser>(
                                  std::move(data_source), bundle_url_),
                              parser_.BindNewPipeAndPassReceiver());

  parser_->ParseMetadata(
      /*offset=*/std::nullopt,
      base::BindOnce(&WebBundleURLLoaderFactory::OnMetadataParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

mojo::PendingRemote<mojom::URLLoaderClient>
WebBundleURLLoaderFactory::MaybeWrapURLLoaderClient(
    mojo::PendingRemote<mojom::URLLoaderClient> wrapped) {
  if (HasError()) {
    mojo::Remote<mojom::URLLoaderClient>(std::move(wrapped))
        ->OnComplete(URLLoaderCompletionStatus(net::ERR_INVALID_WEB_BUNDLE));
    return {};
  }
  mojo::PendingRemote<mojom::URLLoaderClient> client;
  auto client_impl = std::make_unique<WebBundleURLLoaderClient>(
      weak_ptr_factory_.GetWeakPtr(), std::move(wrapped));
  mojo::MakeSelfOwnedReceiver(std::move(client_impl),
                              client.InitWithNewPipeAndPassReceiver());
  return client;
}

// static
base::WeakPtr<WebBundleURLLoaderFactory::URLLoader>
WebBundleURLLoaderFactory::CreateURLLoader(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client,
    base::Time request_start_time,
    base::TimeTicks request_start_time_ticks,
    base::OnceCallback<void(URLLoader*)> delete_me_cleanup) {
  URLLoader* loader =
      new URLLoader(std::move(receiver), url_request, std::move(client),
                    std::move(trusted_header_client), request_start_time,
                    request_start_time_ticks, std::move(delete_me_cleanup));
  return loader->GetWeakPtr();
}

void WebBundleURLLoaderFactory::StartLoader(base::WeakPtr<URLLoader> loader) {
  TRACE_EVENT0("loading", "WebBundleURLLoaderFactory::StartLoader");

  if (!loader)
    return;
  if (HasError()) {
    loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    return;
  }

  // Verify that WebBundle URL associated with the request is correct.
  if (loader->bundle_url() != bundle_url_) {
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
      loader->request_headers(),
      base::BindOnce(&WebBundleURLLoaderFactory::OnBeforeSendHeadersComplete,
                     weak_ptr_factory_.GetWeakPtr(), loader->GetWeakPtr()));
}

void WebBundleURLLoaderFactory::OnBeforeSendHeadersComplete(
    base::WeakPtr<URLLoader> loader,
    int result,
    const std::optional<net::HttpRequestHeaders>& headers) {
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

  parser_->ParseResponse(
      it->second->offset, it->second->length,
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
    if (devtools_request_id_) {
      devtools_observer_->OnSubresourceWebBundleMetadataError(
          *devtools_request_id_, error->message);
    }
    return;
  }

  if (!base::ranges::all_of(metadata->requests, [this](const auto& entry) {
        return IsAllowedExchangeUrl(entry.first);
      })) {
    std::string error_message = "Exchange URL is not valid.";
    ReportErrorAndCancelPendingLoaders(
        SubresourceWebBundleLoadResult::kMetadataParseError,
        mojom::WebBundleErrorType::kMetadataParseError, error_message);
    if (devtools_request_id_) {
      devtools_observer_->OnSubresourceWebBundleMetadataError(
          *devtools_request_id_, error_message);
    }
    return;
  }

  metadata_ = std::move(metadata);
  if (devtools_observer_ && devtools_request_id_) {
    std::vector<GURL> urls;
    urls.reserve(metadata_->requests.size());
    for (const auto& item : metadata_->requests) {
      urls.push_back(item.first);
    }
    devtools_observer_->OnSubresourceWebBundleMetadata(*devtools_request_id_,
                                                       std::move(urls));
  }
  base::UmaHistogramCounts10000("SubresourceWebBundles.ResourceCount",
                                metadata_->requests.size());

  if (metadata_->version == web_package::mojom::BundleFormatVersion::kB1) {
    web_bundle_handle_->OnWebBundleError(
        mojom::WebBundleErrorType::kDeprecationWarning,
        "WebBundle format \"b1\" is deprecated. See migration guide at "
        "https://bit.ly/3rpDuEX.");
  }

  if (data_completed_)
    MaybeReportLoadResult(SubresourceWebBundleLoadResult::kSuccess);
  for (auto loader : pending_loaders_)
    StartLoad(loader);
  pending_loaders_.clear();
}

bool WebBundleURLLoaderFactory::IsAllowedExchangeUrl(const GURL& relative_url) {
  GURL url = bundle_url_.Resolve(relative_url.spec());
  return url.SchemeIsHTTPOrHTTPS() || web_package::IsValidUuidInPackageURL(url);
}

void WebBundleURLLoaderFactory::OnResponseParsed(
    base::WeakPtr<URLLoader> loader,
    web_package::mojom::BundleResponsePtr response,
    web_package::mojom::BundleResponseParseErrorPtr error) {
  TRACE_EVENT0("loading", "WebBundleURLLoaderFactory::OnResponseParsed");
  if (!loader)
    return;
  if (error) {
    if (devtools_observer_ && loader->devtools_request_id()) {
      devtools_observer_->OnSubresourceWebBundleInnerResponseError(
          *loader->devtools_request_id(), loader->url(), error->message,
          devtools_request_id_);
    }
    web_bundle_handle_->OnWebBundleError(
        mojom::WebBundleErrorType::kResponseParseError, error->message);
    loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    return;
  }
  if (devtools_observer_) {
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers;
    headers.reserve(response->response_headers.size());
    for (const auto& it : response->response_headers) {
      headers.push_back(
          network::mojom::HttpRawHeaderPair::New(it.first, it.second));
    }
    if (loader->devtools_request_id()) {
      devtools_observer_->OnSubresourceWebBundleInnerResponse(
          *loader->devtools_request_id(), loader->url(), devtools_request_id_);
    }
  }
  // Add an artificial "X-Content-Type-Options: "nosniff" header, which is
  // explained at
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#name-responses.
  response->response_headers["X-Content-Type-Options"] = "nosniff";
  const std::string header_string = web_package::CreateHeaderString(response);

  loader->SetResponseStartTime(base::TimeTicks::Now());
  loader->SetHeadersBytes(header_string.size());
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
    const std::optional<std::string>& headers,
    const std::optional<GURL>& preserve_fragment_on_redirect_url) {
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
  // TODO(crbug.com/41474458): Revisit this once
  // https://github.com/WICG/webpackage/issues/478 is resolved.
  if (response_head->headers->response_code() != net::HTTP_OK) {
    web_bundle_handle_->OnWebBundleError(
        mojom::WebBundleErrorType::kResponseParseError,
        "Invalid response code " +
            base::NumberToString(response_head->headers->response_code()));
    loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    return;
  }

  response_head->is_web_bundle_inner_response = true;

  response_head->load_timing = loader->load_timing();
  loader->SetBodyLength(payload_length);

  // Enforce the Cross-Origin-Resource-Policy (CORP) header.
  //
  // TODO(crbug.com/333708501)
  // Implement support for Document-Isolation-Policy in Web Bundles if needed,
  // by passing a Document-Isolation-Policy at creation time and using it in the
  // call below.
  if (std::optional<mojom::BlockedByResponseReason> blocked_reason =
          CrossOriginResourcePolicy::IsBlocked(
              loader->url(), loader->url(), loader->request_initiator(),
              *response_head, loader->request_mode(),
              loader->request_destination(), cross_origin_embedder_policy_,
              coep_reporter_, DocumentIsolationPolicy())) {
    loader->CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE,
                                    blocked_reason);
    return;
  }

  // Enforce ad-auction-only signals -- the renderer process isn't allowed
  // to read auction-only signals for ad auctions; only the browser process
  // is allowed to read those, and only the browser process can issue trusted
  // requests.
  std::string auction_only;
  // TODO(crbug.com/40269364): Remove old names once API users have migrated to
  // new names.
  if (!loader->is_trusted() && response_head->headers &&
      (response_head->headers->GetNormalizedHeader("Ad-Auction-Only",
                                                   &auction_only) ||
       response_head->headers->GetNormalizedHeader("X-FLEDGE-Auction-Only",
                                                   &auction_only)) &&
      base::EqualsCaseInsensitiveASCII(auction_only, "true")) {
    loader->CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE,
                                    /*reason=*/std::nullopt);
    return;
  }

  auto orb_analyzer = orb::ResponseAnalyzer::Create(&orb_state_);
  auto decision = orb_analyzer->Init(
      loader->url(), loader->request_initiator(), loader->request_mode(),
      loader->request_destination(), *response_head);
  switch (decision) {
    case network::orb::ResponseAnalyzer::Decision::kBlock:
      loader->BlockResponseForOrb(std::move(response_head));
      return;
    case network::orb::ResponseAnalyzer::Decision::kAllow:
    case network::orb::ResponseAnalyzer::Decision::kSniffMore:
      break;
  }

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
    loader->OnFail(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }
  loader->OnResponse(std::move(response_head), std::move(consumer));
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

void WebBundleURLLoaderFactory::OnWebBundleFetchFailed() {
  ReportErrorAndCancelPendingLoaders(
      SubresourceWebBundleLoadResult::kWebBundleFetchFailed,
      mojom::WebBundleErrorType::kWebBundleFetchFailed,
      "Failed to fetch the Web Bundle.");
}

}  // namespace network
