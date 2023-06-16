// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_URL_LOADER_CLIENT_H_
#define CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_URL_LOADER_CLIENT_H_

#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_resource_loader.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {
// RaceNetworkRequestURLLoaderClient handles the response when the request is
// triggered in the RaceNetworkRequest mode.
// If the response from the RaceNetworkRequest mode is faster than the one from
// the fetch handler, this client handles the response and commit it via owner's
// CommitResponse methods.
// If the response from the fetch handler is faster, this class doesn't do
// anything, and discards the response.
class CONTENT_EXPORT ServiceWorkerRaceNetworkRequestURLLoaderClient
    : public network::mojom::URLLoaderClient {
 public:
  using FetchResponseFrom = ServiceWorkerResourceLoader::FetchResponseFrom;
  enum class State {
    // The initial state.
    kWaitForBody,
    // Transferred from |kWaitForBody|. Once data is available, the consumer
    // handle will be committed to the original client.
    kResponseCommitted,
    // Transferred from |kResponseCommitted|. This state indicates buffered data
    // has been sent to the data pipe.
    kDataTransferFinished,
    // Indicates the commit is completed. This state closes the data pipe.
    kCompleted,
    // Used when the pipe is closed unexpectedly.
    kAborted,
  };
  // TODO(crbug.com/1420517) Remove optional from |forwarding_client| once we
  // support subresource request deduping. Currently |forwarding_client| might
  // be absl::nullopt.
  //
  // |data_pipe_capacity_num_bytes| indicates the byte size of the data pipe
  // which is newly created in the constructor.
  ServiceWorkerRaceNetworkRequestURLLoaderClient(
      const network::ResourceRequest& request,
      base::WeakPtr<ServiceWorkerResourceLoader> owner,
      absl::optional<mojo::PendingRemote<network::mojom::URLLoaderClient>>
          forwarding_client,
      uint32_t data_pipe_capacity_num_bytes);
  ServiceWorkerRaceNetworkRequestURLLoaderClient(
      const ServiceWorkerRaceNetworkRequestURLLoaderClient&) = delete;
  ServiceWorkerRaceNetworkRequestURLLoaderClient& operator=(
      const ServiceWorkerRaceNetworkRequestURLLoaderClient&) = delete;
  ~ServiceWorkerRaceNetworkRequestURLLoaderClient() override;

  void Bind(mojo::PendingRemote<network::mojom::URLLoaderClient>* remote);
  const net::LoadTimingInfo& GetLoadTimingInfo() { return head_->load_timing; }

  static net::NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag();

  State state() const { return state_; }

  // network::mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  struct DataPipeInfo {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::SimpleWatcher watcher;
    uint32_t num_write_bytes;
    DataPipeInfo();
    ~DataPipeInfo();
  };

  // Commits the head and body through |owner_|'s commit methods.
  // This method does not complete the commit process.
  void CommitResponse();
  void MaybeCommitResponse();

  // Completes the commit process through |owner_|'s CommitCompleted().
  void CompleteResponse();
  void MaybeCompleteResponse();

  void OnDataTransferComplete();
  void TransitionState(State new_state);

  // Reads data from |body_|, and writes it into the data pipe producer handles
  // for both the race network request and the fetch handler respectively.
  //
  // To guarantee the consistent data between the race network request and the
  // fetch handler, this method always writes a same chunk of data into two data
  // pipe handles. If one side fails the data write process for some reason, we
  // don't consume |body_| data, and retry it later. |body_| data is consumed
  // only when the both producer handles successfully write data.
  //
  // When the first chunk of data is written to the data pipes, this starts the
  // commit process. And when the data transfer is finished, this complete the
  // commit process.
  //
  // TODO(crbug.com/1420517) Add more UMAs to measure how long time to take this
  // process, and there could be the case if the response is not returned due to
  // the long fetch handler execution. and test case the mechanism to wait for
  // the fetch handler
  void ReadAndWrite(MojoResult);
  void Abort();

  State state_ = State::kWaitForBody;
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
  const network::ResourceRequest request_;
  base::WeakPtr<ServiceWorkerResourceLoader> owner_;
  absl::optional<mojo::Remote<network::mojom::URLLoaderClient>>
      forwarding_client_;
  mojo::SimpleWatcher body_consumer_watcher_;
  mojo::ScopedDataPipeConsumerHandle body_;

  network::mojom::URLResponseHeadPtr head_;
  absl::optional<mojo_base::BigBuffer> cached_metadata_;

  DataPipeInfo data_pipe_for_race_network_request_;
  DataPipeInfo data_pipe_for_fetch_handler_;
  absl::optional<network::URLLoaderCompletionStatus> completion_status_;

  base::WeakPtrFactory<ServiceWorkerRaceNetworkRequestURLLoaderClient>
      weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_URL_LOADER_CLIENT_H_
