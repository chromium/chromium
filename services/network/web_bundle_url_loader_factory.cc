// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle_url_loader_factory.h"

#include "base/optional.h"
#include "components/web_package/web_bundle_parser.h"
#include "components/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/cross_origin_read_blocking.h"

namespace network {

namespace {

constexpr size_t kBlockedBodyAllocationSize = 1;

class PipeDataSource : public mojo::DataPipeProducer::DataSource {
 public:
  explicit PipeDataSource(std::vector<uint8_t> data) : data_(std::move(data)) {}
  uint64_t GetLength() const override { return data_.size(); }

  ReadResult Read(uint64_t uint64_offset, base::span<char> buffer) override {
    ReadResult result;
    if (uint64_offset > data_.size()) {
      result.result = MOJO_RESULT_OUT_OF_RANGE;
      return result;
    }
    size_t offset = base::checked_cast<size_t>(uint64_offset);
    size_t len = std::min(data_.size() - offset, buffer.size());
    if (len > 0) {
      DCHECK_LT(offset, data_.size());
      memcpy(buffer.data(), &data_[offset], len);
    }
    result.bytes_read = len;
    return result;
  }

 private:
  // Since mojo::DataPipeProducer runs in its own sequence, we can't just have
  // a reference to the buffer in BundleDataSource.
  std::vector<uint8_t> data_;
};

void DeleteProducerAndRunCallback(
    std::unique_ptr<mojo::DataPipeProducer> producer,
    base::OnceCallback<void(MojoResult result)> callback,
    MojoResult result) {
  std::move(callback).Run(result);
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
    MojoResult result = mojo::CreateDataPipe(&options, &producer, &consumer);
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
            const base::Optional<url::Origin>& request_initiator_origin_lock)
      : url_(request.url),
        request_mode_(request.mode),
        request_initiator_(request.request_initiator),
        request_initiator_origin_lock_(request_initiator_origin_lock),
        receiver_(this, std::move(loader)),
        client_(std::move(client)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&URLLoader::OnMojoDisconnect, GetWeakPtr()));
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
    if (CreateDataPipe(nullptr, &producer, &consumer) != MOJO_RESULT_OK) {
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
  base::WeakPtrFactory<URLLoader> weak_ptr_factory_{this};
};

class WebBundleURLLoaderFactory::BundleDataSource
    : public web_package::mojom::BundleDataSource,
      public mojo::DataPipeDrainer::Client {
 public:
  using ReadToDataPipeCallback = base::OnceCallback<void(MojoResult result)>;

  BundleDataSource(mojo::PendingReceiver<web_package::mojom::BundleDataSource>
                       data_source_receiver,
                   mojo::ScopedDataPipeConsumerHandle bundle_body)
      : data_source_receiver_(this, std::move(data_source_receiver)),
        pipe_drainer_(
            std::make_unique<mojo::DataPipeDrainer>(this,
                                                    std::move(bundle_body))) {}

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
    if (!finished_loading_ && offset + length > data_.size()) {
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

    auto writer = std::make_unique<mojo::DataPipeProducer>(std::move(producer));
    mojo::DataPipeProducer* raw_writer = writer.get();
    raw_writer->Write(std::make_unique<PipeDataSource>(GetData(offset, length)),
                      base::BindOnce(&DeleteProducerAndRunCallback,
                                     std::move(writer), std::move(callback)));
  }

  // mojom::BundleDataSource
  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    TRACE_EVENT0("loading", "BundleDataSource::Read");
    if (!finished_loading_ && offset + length > data_.size()) {
      PendingRead pending;
      pending.offset = offset;
      pending.length = length;
      pending.callback = std::move(callback);
      pending_reads_.push_back(std::move(pending));
      return;
    }
    std::move(callback).Run(GetData(offset, length));
  }

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    DCHECK(!finished_loading_);
    const uint8_t* data_uint8 = reinterpret_cast<const uint8_t*>(data);
    // TODO(crbug.com/1082020): Set a threshold for buffer size, so that Network
    // Service does not use memory indefinitely.
    data_.insert(data_.end(), data_uint8, data_uint8 + num_bytes);
    ProcessPendingReads();
  }

  void OnDataComplete() override {
    DCHECK(!finished_loading_);
    finished_loading_ = true;
    ProcessPendingReads();
  }

 private:
  void ProcessPendingReads() {
    std::vector<PendingRead> pendings;
    pendings.swap(pending_reads_);
    for (auto& pending : pendings)
      Read(pending.offset, pending.length, std::move(pending.callback));

    std::vector<PendingReadToDataPipe> pipe_pendings;
    pipe_pendings.swap(pending_reads_to_data_pipe_);
    for (auto& pending : pipe_pendings) {
      ReadToDataPipe(std::move(pending.producer), pending.offset,
                     pending.length, std::move(pending.callback));
    }
  }

  std::vector<uint8_t> GetData(uint64_t uint64_offset, uint64_t uint64_length) {
    size_t offset = base::checked_cast<size_t>(uint64_offset);
    size_t length = base::checked_cast<size_t>(uint64_length);
    if (offset >= data_.size())
      return {};
    if (length > data_.size() - offset)
      length = data_.size() - offset;

    std::vector<uint8_t> output(length);
    memcpy(output.data(), data_.data() + offset, length);
    return output;
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
  std::vector<uint8_t> data_;
  std::vector<PendingRead> pending_reads_;
  std::vector<PendingReadToDataPipe> pending_reads_to_data_pipe_;
  bool finished_loading_ = false;
  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;
};

WebBundleURLLoaderFactory::WebBundleURLLoaderFactory(
    const GURL& bundle_url,
    mojo::Remote<mojom::WebBundleHandle> web_bundle_handle,
    const base::Optional<url::Origin>& request_initiator_origin_lock)
    : bundle_url_(bundle_url),
      web_bundle_handle_(std::move(web_bundle_handle)),
      request_initiator_origin_lock_(request_initiator_origin_lock) {}

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

void WebBundleURLLoaderFactory::SetBundleStream(
    mojo::ScopedDataPipeConsumerHandle body) {
  mojo::PendingRemote<web_package::mojom::BundleDataSource> data_source;
  source_ = std::make_unique<BundleDataSource>(
      data_source.InitWithNewPipeAndPassReceiver(), std::move(body));
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

void WebBundleURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  TRACE_EVENT0("loading", "WebBundleURLLoaderFactory::CreateLoaderAndStart");
  URLLoader* loader =
      new URLLoader(std::move(receiver), url_request, std::move(client),
                    request_initiator_origin_lock_);
  if (metadata_error_) {
    loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    return;
  }
  if (!metadata_) {
    pending_loaders_.push_back(loader->GetWeakPtr());
    return;
  }
  StartLoad(loader);
}

void WebBundleURLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  NOTREACHED();
}

void WebBundleURLLoaderFactory::StartLoad(URLLoader* loader) {
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

void WebBundleURLLoaderFactory::OnMetadataParsed(
    web_package::mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  TRACE_EVENT0("loading", "WebBundleURLLoaderFactory::OnMetadataParsed");
  if (error) {
    metadata_error_ = std::move(error);
    web_bundle_handle_->OnWebBundleError(
        mojom::WebBundleErrorType::kMetadataParseError,
        metadata_error_->message);
    for (auto loader : pending_loaders_) {
      if (loader)
        loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    }
    pending_loaders_.clear();
    return;
  }

  metadata_ = std::move(metadata);
  for (auto loader : pending_loaders_)
    StartLoad(loader.get());
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
  // Currently we allow only net::HTTP_OK responses in bundles.
  // TODO(crbug.com/990733): Revisit this once
  // https://github.com/WICG/webpackage/issues/478 is resolved.
  if (response->response_code != net::HTTP_OK) {
    web_bundle_handle_->OnWebBundleError(
        mojom::WebBundleErrorType::kResponseParseError,
        "Invalid response code " +
            base::NumberToString(response->response_code));
    loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
    return;
  }

  mojom::URLResponseHeadPtr response_head =
      web_package::CreateResourceResponse(response);
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
  if (CreateDataPipe(nullptr, &producer, &consumer) != MOJO_RESULT_OK) {
    loader->OnFail(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }
  loader->OnData(std::move(consumer));
  source_->ReadToDataPipe(
      std::move(producer), response->payload_offset, response->payload_length,
      base::BindOnce(&URLLoader::OnWriteCompleted, loader->GetWeakPtr()));
}

}  // namespace network
