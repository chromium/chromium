// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/web_bundle_subresource_loader.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/web_package/web_bundle_parser.h"
#include "components/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

namespace {

class WebBundleSubresourceLoader : public network::mojom::URLLoader {
 public:
  WebBundleSubresourceLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)
      : url_(request.url),
        receiver_(this, std::move(loader)),
        client_(std::move(client)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &WebBundleSubresourceLoader::OnMojoDisconnect, GetWeakPtr()));
  }
  WebBundleSubresourceLoader(const WebBundleSubresourceLoader&) = delete;
  WebBundleSubresourceLoader& operator=(const WebBundleSubresourceLoader&) =
      delete;

  const GURL& Url() const { return url_; }

  base::WeakPtr<WebBundleSubresourceLoader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnResponse(network::mojom::URLResponseHeadPtr response) {
    client_->OnReceiveResponse(std::move(response));
  }

  void OnData(mojo::ScopedDataPipeConsumerHandle consumer) {
    client_->OnStartLoadingResponseBody(std::move(consumer));
  }

  void OnFail(net::Error error) {
    client_->OnComplete(network::URLLoaderCompletionStatus(error));
    delete this;
  }

  void OnWriteCompleted(MojoResult result) {
    network::URLLoaderCompletionStatus status(
        result == MOJO_RESULT_OK ? net::OK : net::ERR_INVALID_WEB_BUNDLE);
    client_->OnComplete(status);
    delete this;
  }

 private:
  // network::mojom::URLLoader
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
  mojo::Receiver<network::mojom::URLLoader> receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  base::WeakPtrFactory<WebBundleSubresourceLoader> weak_ptr_factory_{this};
};

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
    memcpy(buffer.data(), &data_[offset], len);
    result.bytes_read = len;
    return result;
  }

 private:
  // Since mojo::DataPipeProducer runs in its own sequence, we can't just have
  // a reference to the SharedBuffer in LinkWebBundleDataSource.
  std::vector<uint8_t> data_;
};

void DeleteProducerAndRunCallback(
    std::unique_ptr<mojo::DataPipeProducer> producer,
    base::OnceCallback<void(MojoResult result)> callback,
    MojoResult result) {
  std::move(callback).Run(result);
}

class LinkWebBundleDataSource : public web_package::mojom::BundleDataSource,
                                public mojo::DataPipeDrainer::Client {
 public:
  using ReadToDataPipeCallback = base::OnceCallback<void(MojoResult result)>;

  LinkWebBundleDataSource(
      mojo::PendingReceiver<web_package::mojom::BundleDataSource>
          data_source_receiver,
      mojo::ScopedDataPipeConsumerHandle bundle_body)
      : data_source_receiver_(this, std::move(data_source_receiver)),
        data_(SharedBuffer::Create()),
        pipe_drainer_(
            std::make_unique<mojo::DataPipeDrainer>(this,
                                                    std::move(bundle_body))) {}

  ~LinkWebBundleDataSource() override {
    // The receiver must be closed before destructing pending callbacks in
    // |pending_reads_| / |pending_reads_to_data_pipe_|.
    data_source_receiver_.reset();
  }

  LinkWebBundleDataSource(const LinkWebBundleDataSource&) = delete;
  LinkWebBundleDataSource& operator=(const LinkWebBundleDataSource&) = delete;

  void ReadToDataPipe(mojo::ScopedDataPipeProducerHandle producer,
                      uint64_t offset,
                      uint64_t length,
                      ReadToDataPipeCallback callback) {
    TRACE_EVENT0("loading", "LinkWebBundleDataSource::ReadToDataPipe");
    if (!finished_loading_ && offset + length > data_->size()) {
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
    TRACE_EVENT0("loading", "LinkWebBundleDataSource::Read");
    if (!finished_loading_ && offset + length > data_->size()) {
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
    data_->Append(reinterpret_cast<const char*>(data), num_bytes);
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
    std::vector<uint8_t> output(length);
    size_t bytes_copied = 0;
    for (auto it = data_->GetIteratorAt(offset);
         bytes_copied < length && it != data_->cend(); it++) {
      size_t n = std::min(it->size(), length - bytes_copied);
      memcpy(output.data() + bytes_copied, it->data(), n);
      bytes_copied += n;
    }
    output.resize(bytes_copied);
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
  scoped_refptr<SharedBuffer> data_;
  std::vector<PendingRead> pending_reads_;
  std::vector<PendingReadToDataPipe> pending_reads_to_data_pipe_;
  bool finished_loading_ = false;
  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;
};

// Self destroys when no more bindings exist.
class WebBundleSubresourceLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  WebBundleSubresourceLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::ScopedDataPipeConsumerHandle bundle_body,
      scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
      WebBundleErrorCallback error_callback)
      : callback_task_runner_(callback_task_runner),
        error_callback_(error_callback) {
    receivers_.Add(this, std::move(receiver));
    receivers_.set_disconnect_handler(base::BindRepeating(
        &WebBundleSubresourceLoaderFactory::OnMojoDisconnect,
        base::Unretained(this)));
    mojo::PendingRemote<web_package::mojom::BundleDataSource> data_source;
    source_ = std::make_unique<LinkWebBundleDataSource>(
        data_source.InitWithNewPipeAndPassReceiver(), std::move(bundle_body));
    // WebBundleParser will self-destruct on remote mojo ends' disconnection.
    new web_package::WebBundleParser(parser_.BindNewPipeAndPassReceiver(),
                                     std::move(data_source));

    parser_->ParseMetadata(
        WTF::Bind(&WebBundleSubresourceLoaderFactory::OnMetadataParsed,
                  weak_ptr_factory_.GetWeakPtr()));
  }

  ~WebBundleSubresourceLoaderFactory() override {
    for (auto loader : pending_loaders_) {
      if (loader)
        loader->OnFail(net::ERR_FAILED);
    }
  }

  WebBundleSubresourceLoaderFactory(const WebBundleSubresourceLoaderFactory&) =
      delete;
  WebBundleSubresourceLoaderFactory& operator=(
      const WebBundleSubresourceLoaderFactory&) = delete;

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory)
      override {
    receivers_.Add(this, std::move(factory));
  }

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    TRACE_EVENT0("loading",
                 "WebBundleSubresourceLoaderFactory::CreateLoaderAndStart");
    WebBundleSubresourceLoader* loader = new WebBundleSubresourceLoader(
        std::move(pending_receiver), request, std::move(client));
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

 private:
  void OnMojoDisconnect() {
    if (!receivers_.empty())
      return;
    delete this;
  }

  void StartLoad(WebBundleSubresourceLoader* loader) {
    DCHECK(metadata_);
    if (!loader)
      return;
    auto it = metadata_->requests.find(loader->Url());
    if (it == metadata_->requests.end()) {
      RunErrorCallback(WebBundleErrorType::kResourceNotFound,
                       loader->Url().possibly_invalid_spec() +
                           " is not found in the WebBundle.");
      loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
      return;
    }
    // Currently, we just return the first response for the URL.
    // TODO(crbug.com/1082020): Support variant matching.
    auto& location = it->second->response_locations[0];
    parser_->ParseResponse(
        location->offset, location->length,
        WTF::Bind(&WebBundleSubresourceLoaderFactory::OnResponseParsed,
                  weak_ptr_factory_.GetWeakPtr(), loader->GetWeakPtr()));
  }

  void OnMetadataParsed(web_package::mojom::BundleMetadataPtr metadata,
                        web_package::mojom::BundleMetadataParseErrorPtr error) {
    TRACE_EVENT0("loading",
                 "WebBundleSubresourceLoaderFactory::OnMetadataParsed");
    if (error) {
      metadata_error_ = std::move(error);
      RunErrorCallback(WebBundleErrorType::kMetadataParseError,
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

  void OnResponseParsed(base::WeakPtr<WebBundleSubresourceLoader> loader,
                        web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error) {
    TRACE_EVENT0("loading",
                 "WebBundleSubresourceLoaderFactory::OnResponseParsed");
    if (!loader)
      return;
    if (error) {
      RunErrorCallback(WebBundleErrorType::kResponseParseError, error->message);
      loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
      return;
    }
    // Currently we allow only net::HTTP_OK responses in bundles.
    // TODO(crbug.com/990733): Revisit this once
    // https://github.com/WICG/webpackage/issues/478 is resolved.
    if (response->response_code != net::HTTP_OK) {
      RunErrorCallback(WebBundleErrorType::kResponseParseError,
                       "Invalid response code " +
                           base::NumberToString(response->response_code));
      loader->OnFail(net::ERR_INVALID_WEB_BUNDLE);
      return;
    }

    loader->OnResponse(web_package::CreateResourceResponse(response));

    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    if (CreateDataPipe(nullptr, &producer, &consumer) != MOJO_RESULT_OK) {
      loader->OnFail(net::ERR_INSUFFICIENT_RESOURCES);
      return;
    }
    loader->OnData(std::move(consumer));
    source_->ReadToDataPipe(
        std::move(producer), response->payload_offset, response->payload_length,
        base::BindOnce(&WebBundleSubresourceLoader::OnWriteCompleted,
                       loader->GetWeakPtr()));
  }

  void RunErrorCallback(WebBundleErrorType type, const std::string& message) {
    PostCrossThreadTask(*callback_task_runner_, FROM_HERE,
                        WTF::CrossThreadBindOnce(error_callback_, type,
                                                 String(message.c_str())));
  }

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  std::unique_ptr<LinkWebBundleDataSource> source_;
  mojo::Remote<web_package::mojom::WebBundleParser> parser_;
  web_package::mojom::BundleMetadataPtr metadata_;
  web_package::mojom::BundleMetadataParseErrorPtr metadata_error_;
  std::vector<base::WeakPtr<WebBundleSubresourceLoader>> pending_loaders_;
  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
  WebBundleErrorCallback error_callback_;

  base::WeakPtrFactory<WebBundleSubresourceLoaderFactory> weak_ptr_factory_{
      this};
};

// Runs on a background thread.
void CreateFactoryOnBackground(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::ScopedDataPipeConsumerHandle bundle_body,
    scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
    WebBundleErrorCallback error_callback) {
  new WebBundleSubresourceLoaderFactory(
      std::move(receiver), std::move(bundle_body), callback_task_runner,
      std::move(error_callback));
}

}  // namespace

void CreateWebBundleSubresourceLoaderFactory(
    CrossVariantMojoReceiver<network::mojom::URLLoaderFactoryInterfaceBase>
        factory_receiver,
    mojo::ScopedDataPipeConsumerHandle bundle_body,
    WebBundleErrorCallback error_callback) {
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateFactoryOnBackground, std::move(factory_receiver),
                     std::move(bundle_body),
                     base::ThreadTaskRunnerHandle::Get(),
                     std::move(error_callback)));
}

}  // namespace blink
