// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_decoding_interceptor.h"

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/filter/filter_source_stream.h"
#include "services/network/public/cpp/data_pipe_to_source_stream.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/loading_params.h"
#include "services/network/public/cpp/source_stream_to_data_pipe.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

namespace {

uint32_t GetRendererSideContentDecodingPipeSize() {
  const int feature_param_value =
      features::kRendererSideContentDecodingPipeSize.Get();
  if (feature_param_value != 0) {
    return base::checked_cast<uint32_t>(feature_param_value);
  }
  return network::GetDataPipeDefaultAllocationSize(
      network::DataPipeAllocationSize::kLargerSizeIfPossible);
}

// Implements the URLLoaderClient and URLLoader interfaces to intercept a
// request after receiving a response and perform content decoding. This class
// acts as a middleman between the original URLLoader/URLLoaderClient pair and
// the new URLLoader/URLLoaderClient pair that the caller sees after
// interception.
class Interceptor : public network::mojom::URLLoaderClient,
                    public network::mojom::URLLoader {
 public:
  // Creates a new `Interceptor` and starts the interception process. The
  // created object is owned by `destination_url_loader_receiver`.
  //
  // The data flow is illustrated below:
  // Blink-side =================================================== Network-side
  // [Destination]                                                      [Source]
  //   =URLLoader=======> |                     (remote)| ==URLLoader=======>
  //   <=URLLoaderClient= |(remote)   `this`  (receiver)| <=URLLoaderClient==
  //   <=DataPipe======== |(producer)         (consumer)| <=DataPipe=========
  static void CreateAndStart(
      const std::vector<net::SourceStreamType>& types,
      mojo::ScopedDataPipeConsumerHandle source,
      mojo::ScopedDataPipeProducerHandle dest,
      mojo::PendingRemote<network::mojom::URLLoader> source_url_loader_remote,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          source_url_client_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient>
          destination_url_loader_client,
      mojo::PendingReceiver<network::mojom::URLLoader>
          destination_url_loader_receiver,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner) {
    auto interceptor =
        std::make_unique<Interceptor>(base::PassKey<Interceptor>());
    auto* interceptor_ptr = interceptor.get();
    mojo::MakeSelfOwnedReceiver(std::move(interceptor),
                                std::move(destination_url_loader_receiver));
    interceptor_ptr->Start(types, std::move(source), std::move(dest),
                           std::move(source_url_loader_remote),
                           std::move(source_url_client_receiver),
                           std::move(destination_url_loader_client),
                           std::move(worker_task_runner));
  }

  // Private constructor. Use `CreateAndStart()` to create instances.
  explicit Interceptor(base::PassKey<Interceptor>) {}
  Interceptor(const Interceptor&) = delete;
  Interceptor& operator=(const Interceptor&) = delete;
  ~Interceptor() override = default;

 private:
  // Struct to hold the result of the decoding operation.
  struct DecodeResult {
    int net_err;
    int64_t transferred_bytes;
  };

  // Starts the interception and decoding process.
  void Start(
      const std::vector<net::SourceStreamType>& types,
      mojo::ScopedDataPipeConsumerHandle source,
      mojo::ScopedDataPipeProducerHandle dest,
      mojo::PendingRemote<network::mojom::URLLoader> source_url_loader_remote,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          source_url_client_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient>
          destination_url_loader_client,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner) {
    // Create `source_stream_to_data_pipe_` with FilterSourceStream to perform
    // content decoding.
    source_stream_to_data_pipe_ = std::make_unique<SourceStreamToDataPipe>(
        net::FilterSourceStream::CreateDecodingSourceStream(
            // Create `DataPipeToSourceStream` to convert from `source` data
            // pipe to `net::SourceStream`.
            std::make_unique<DataPipeToSourceStream>(std::move(source),
                                                     worker_task_runner),
            types),
        std::move(dest), worker_task_runner);

    // Starts reading and decoding the data. The decoded data will be written
    // to `dest`.
    source_stream_to_data_pipe_->Start(
        base::BindOnce(&Interceptor::OnFinishDecode, base::Unretained(this)));
    if (source_url_loader_remote) {
      // For some requests (eg: NavigationPreloadRequest), we don't bind
      // URLLoader.
      source_url_loader_.Bind(std::move(source_url_loader_remote));
    }
    source_url_client_receiver_.Bind(std::move(source_url_client_receiver));
    destination_url_loader_client_.Bind(
        std::move(destination_url_loader_client));
  }

  // network::mojom::URLLoaderClient implementation
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    // `this` is created after receiving a response. So OnReceiveEarlyHints()
    // must not be called.
    NOTREACHED();
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    // `this` is created after receiving a response. So OnReceiveResponse()
    // must not be called.
    NOTREACHED();
  }
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    // `this` is created after receiving a response. So OnReceiveRedirect()
    // must not be called.
    NOTREACHED();
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    // `this` is created after receiving a response. So OnUploadProgress() must
    // not be called.
    NOTREACHED();
  }
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    // Forward transfer size updates to the original client.
    destination_url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
  }
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    // Store the completion status and check if decoding is also complete.
    completion_status_ = status;
    MaybeSendOnComplete();
  }

  // network::mojom::URLLoader implementation
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    // Redirects should be handled before interception.
    NOTREACHED();
  }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    // Forward priority changes to the original URLLoader.
    CHECK(source_url_loader_);
    source_url_loader_->SetPriority(priority, intra_priority_value);
  }

  // Called when the decoding process finishes. `net_err` has a result of the
  // decoding.
  void OnFinishDecode(int net_err) {
    decode_result_ = DecodeResult(
        {net_err, source_stream_to_data_pipe_->TransferredBytes()});
    source_stream_to_data_pipe_.reset();
    MaybeSendOnComplete();
  }

  // Sends the OnComplete message to the original client if both the decoding
  // and the original request are complete.
  void MaybeSendOnComplete() {
    if (!decode_result_ || !completion_status_) {
      return;
    }

    // If the original request completed successfully, update the completion
    // status with the decoding result.
    if (completion_status_->error_code == net::OK) {
      if (decode_result_->net_err != net::OK) {
        completion_status_ =
            network::URLLoaderCompletionStatus(decode_result_->net_err);
      } else {
        completion_status_->decoded_body_length =
            decode_result_->transferred_bytes;
      }
    }
    destination_url_loader_client_->OnComplete(*completion_status_);
  }

  // Created with a FilterSourceStream which performs the content decoding.
  std::unique_ptr<SourceStreamToDataPipe> source_stream_to_data_pipe_;

  // The original URLLoader. Used for forwarding priority changes.
  mojo::Remote<network::mojom::URLLoader> source_url_loader_;

  // Receives messages from the original URLLoaderClient.
  mojo::Receiver<network::mojom::URLLoaderClient> source_url_client_receiver_{
      this};

  // Forwards messages to the original URLLoaderClient.
  mojo::Remote<network::mojom::URLLoaderClient> destination_url_loader_client_;

  // Stores the result of the decoding operation.
  std::optional<DecodeResult> decode_result_;

  // Stores the completion status received from the original URLLoaderClient.
  std::optional<network::URLLoaderCompletionStatus> completion_status_;
};
}  // namespace

// static
bool ContentDecodingInterceptor::
    force_mojo_create_data_pipe_failure_for_testing_ = false;

void ContentDecodingInterceptor::Intercept(
    const std::vector<net::SourceStreamType>& types,
    network::mojom::URLLoaderClientEndpointsPtr& endpoints,
    mojo::ScopedDataPipeConsumerHandle& body,
    scoped_refptr<base::SequencedTaskRunner> worker_task_runner) {
  Intercept(
      types,
      base::BindOnce(
          [](network::mojom::URLLoaderClientEndpointsPtr* original_endpoints,
             mojo::ScopedDataPipeConsumerHandle* original_body,
             network::mojom::URLLoaderClientEndpointsPtr& endpoints,
             mojo::ScopedDataPipeConsumerHandle& body) {
            endpoints.Swap(original_endpoints);
            body.swap(*original_body);
          },
          base::Unretained(&endpoints), base::Unretained(&body)),
      std::move(worker_task_runner));
}

void ContentDecodingInterceptor::Intercept(
    const std::vector<net::SourceStreamType>& types,
    base::OnceCallback<
        void(network::mojom::URLLoaderClientEndpointsPtr& endpoints,
             mojo::ScopedDataPipeConsumerHandle& body)> swap_callback,
    scoped_refptr<base::SequencedTaskRunner> worker_task_runner) {
  CHECK(!types.empty());
  mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
  // Creates a data pipe for communication between Interceptor and
  // URLLoaderClient in the caller side.
  const MojoCreateDataPipeOptions options{
      .struct_size = sizeof(MojoCreateDataPipeOptions),
      .flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE,
      .element_num_bytes = 1,
      .capacity_num_bytes = GetRendererSideContentDecodingPipeSize()};
  const auto mojo_result = mojo::CreateDataPipe(&options, pipe_producer_handle,
                                                pipe_consumer_handle);
  base::UmaHistogramExactLinear(
      "Network.RendererSideContentDecoding.CreateDataPipe", mojo_result,
      MOJO_RESULT_SHOULD_WAIT + 1);

  // Create new endpoints for the intercepted URLLoader and URLLoaderClient.
  mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver;
  mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client;
  auto endpoints = network::mojom::URLLoaderClientEndpoints::New(
      url_loader_receiver.InitWithNewPipeAndPassRemote(),
      url_loader_client.InitWithNewPipeAndPassReceiver());
  // Calls `swap_callback` to connect the new created endpoints to the caller
  // side.
  std::move(swap_callback).Run(endpoints, pipe_consumer_handle);

  if (mojo_result != MOJO_RESULT_OK ||
      force_mojo_create_data_pipe_failure_for_testing_) {
    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(url_loader_client));
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }
  Intercept(types, std::move(pipe_consumer_handle),
            std::move(pipe_producer_handle), std::move(endpoints->url_loader),
            std::move(endpoints->url_loader_client),
            std::move(url_loader_receiver), std::move(url_loader_client),
            worker_task_runner);
}

// static
void ContentDecodingInterceptor::Intercept(
    const std::vector<net::SourceStreamType>& types,
    mojo::ScopedDataPipeConsumerHandle source_body,
    mojo::ScopedDataPipeProducerHandle dest_body,
    mojo::PendingRemote<network::mojom::URLLoader> source_url_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        source_url_loader_client,
    mojo::PendingReceiver<network::mojom::URLLoader> dest_url_loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> dest_url_loader_client,
    scoped_refptr<base::SequencedTaskRunner> worker_task_runner) {
  // Post a task to create and start the `Interceptor` on the worker thread.
  worker_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&Interceptor::CreateAndStart, types,
                     std::move(source_body), std::move(dest_body),
                     std::move(source_url_loader),
                     std::move(source_url_loader_client),
                     std::move(dest_url_loader_client),
                     std::move(dest_url_loader), worker_task_runner));
}

// static
void ContentDecodingInterceptor::InterceptOnNetworkService(
    mojom::NetworkService& network_service,
    const std::vector<net::SourceStreamType>& types,
    network::mojom::URLLoaderClientEndpointsPtr& endpoints,
    mojo::ScopedDataPipeConsumerHandle& body) {
  mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
  const MojoCreateDataPipeOptions options{
      .struct_size = sizeof(MojoCreateDataPipeOptions),
      .flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE,
      .element_num_bytes = 1,
      .capacity_num_bytes = network::GetDataPipeDefaultAllocationSize(
          network::DataPipeAllocationSize::kLargerSizeIfPossible)};
  const auto mojo_result = mojo::CreateDataPipe(&options, pipe_producer_handle,
                                                pipe_consumer_handle);
  base::UmaHistogramExactLinear(
      "Network.ContentDecodingInterceptor.CreateDataPipe", mojo_result,
      MOJO_RESULT_SHOULD_WAIT + 1);
  if (mojo_result != MOJO_RESULT_OK ||
      force_mojo_create_data_pipe_failure_for_testing_) {
    mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver;
    mojo::Remote<network::mojom::URLLoaderClient> client_remote(
        client_receiver.InitWithNewPipeAndPassRemote());
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    endpoints = network::mojom::URLLoaderClientEndpoints::New(
        std::move(endpoints->url_loader), std::move(client_receiver));
    return;
  }

  mojo::PendingRemote<network::mojom::URLLoader> new_url_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> new_url_loader_client;
  network_service.InterceptUrlLoaderForBodyDecoding(
      types, std::move(body), std::move(pipe_producer_handle),
      std::move(endpoints->url_loader), std::move(endpoints->url_loader_client),
      new_url_loader.InitWithNewPipeAndPassReceiver(),
      new_url_loader_client.InitWithNewPipeAndPassRemote());
  body = std::move(pipe_consumer_handle);
  endpoints = network::mojom::URLLoaderClientEndpoints::New(
      std::move(new_url_loader), std::move(new_url_loader_client));
}

// static
void ContentDecodingInterceptor::SetForceMojoCreateDataPipeFailureForTesting(
    bool value) {
  force_mojo_create_data_pipe_failure_for_testing_ = value;
}

}  // namespace network
