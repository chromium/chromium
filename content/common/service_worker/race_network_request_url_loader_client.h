// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_URL_LOADER_CLIENT_H_
#define CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_URL_LOADER_CLIENT_H_

#include <optional>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/race_network_request_read_buffer_manager.h"
#include "content/common/service_worker/race_network_request_write_buffer_manager.h"
#include "content/common/service_worker/service_worker_resource_loader.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
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
    : public network::mojom::URLLoaderClient,
      public mojo::DataPipeDrainer::Client {
 public:

  using FetchResponseFrom = ServiceWorkerResourceLoader::FetchResponseFrom;
  enum class State {
    // The initial state.
    kWaitForBody,
    // Transferred from |kWaitForBody|. The state indicates a redirect response
    // is received.
    kRedirect,
    // Transferred from |kWaitForBody| or |kRedirect|. The state indicates data
    // has been received.
    kResponseReceived,
    // Transferred from |kResponseReceived|. This state indicates the response
    // from the original data pipe is read and start transferring the response
    // to new data pipes.
    kDataTransferStarted,
    // Transferred from |kDataTransferStarted| or |kDataTransferFinished|. Once
    // data is available, the consumer handle will be committed to the original
    // client based on |owner_|'s commit responsibility.
    //
    // The response commit timing may be slower than the data transfer
    // completion depending on the timing and bufferd data size.
    kResponseCommitted,
    // Transferred from |kResponseReceived| or |kResponseCommitted|. This state
    // indicates all buffered data has been sent to the data pipe.
    kDataTransferFinished,
    // Indicates the commit is completed. This state closes the data pipe.
    kCompleted,
    // Used when the pipe is closed unexpectedly.
    kAborted,
  };

  // The enum class that indicates how response data is consumed.
  enum class DataConsumePolicy {
    // Tee response data into 1) the data pipe for RaceNetworkRequest and 2) the
    // data pipe for the fetch handler.
    kTeeResponse,
    // Just forward data to the data pipe for the fetch handler. This value
    // doesn't invoke |ReadAndWrite()|.
    kForwardingOnly,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // MojoResult for UMA. We create a dedicated MojoResult enum class here not to
  // enforce the original to follow the rule from the histogram guidelines.
  //
  // The full set of MojoResult is defined in mojo/public/c/system/types.h.
  enum class MojoResultForUMA {
    MOJO_RESULT_OK = 0,
    MOJO_RESULT_CANCELLED = 1,
    MOJO_RESULT_UNKNOWN = 2,
    MOJO_RESULT_INVALID_ARGUMENT = 3,
    MOJO_RESULT_DEADLINE_EXCEEDED = 4,
    MOJO_RESULT_NOT_FOUND = 5,
    MOJO_RESULT_ALREADY_EXISTS = 6,
    MOJO_RESULT_PERMISSION_DENIED = 7,
    MOJO_RESULT_RESOURCE_EXHAUSTED = 8,
    MOJO_RESULT_FAILED_PRECONDITION = 9,
    MOJO_RESULT_ABORTED = 10,
    MOJO_RESULT_OUT_OF_RANGE = 11,
    MOJO_RESULT_UNIMPLEMENTED = 12,
    MOJO_RESULT_INTERNAL = 13,
    MOJO_RESULT_UNAVAILABLE = 14,
    MOJO_RESULT_DATA_LOSS = 15,
    MOJO_RESULT_BUSY = 16,
    MOJO_RESULT_SHOULD_WAIT = 17,
    kMaxValue = MOJO_RESULT_SHOULD_WAIT,
  };

  ServiceWorkerRaceNetworkRequestURLLoaderClient(
      const network::ResourceRequest& request,
      base::WeakPtr<ServiceWorkerResourceLoader> owner,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);
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
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // Just drains data from the consumer handle. The data pipe for the fetch
  // handler may not be consumed by the fetch handler itself if the fetch
  // handler doesn't dispatch the corresponding fetch request. In that case the
  // pipe may be stacked. So this method provides a way to just consume data.
  //
  // TODO(crbug.com/40278676): Consider migrating this to CancelWriteData().
  void DrainData(mojo::ScopedDataPipeConsumerHandle source);

  // Close the corresponding data pipe based on |commit_responsibility|, and
  // cancel watching. The data pipe may not be consumed in some cases e.g. the
  // fetch handler doesn't dispatch the corresponding fetch request, or
  // ServiceWorkerAutoPreload is enabled and the fetch result is not a fallback.
  // In those cases the pipe may be stacked due to the lack of consuming.
  void CancelWriteData(FetchResponseFrom commit_responsibility);

  // Commit and complete the response. Those can be called from |owner_|.
  void CommitAndCompleteResponseIfDataTransferFinished();

  void MaybeRecordResponseReceivedToFetchHandlerEndTiming(
      base::TimeTicks fetch_handler_end_time,
      bool is_fallback);

 private:
  uint32_t GetDataPipeCapacityBytes();
  MojoResultForUMA ConvertMojoResultForUMA(MojoResult mojo_result);

  // mojo::DataPipeDrainer::Client overrides:
  // These just do nothing.
  void OnDataAvailable(base::span<const uint8_t> data) override {}
  void OnDataComplete() override {}

  // Commits the head and body through |owner_|'s commit methods.
  // This method does not complete the commit process.
  void CommitResponse();
  void MaybeCommitResponse();

  // Completes the commit process through |owner_|'s CommitCompleted().
  void CompleteResponse();
  void MaybeCompleteResponse();

  void OnDataTransferComplete();
  void TransitionState(State new_state);

  // Reads data from RaceNetworkRequestReadBufferManager. If there is a buffer
  // to read, notifies the write buffer manager to start write operations.
  // If no buffer to read, calls |OnDataTransferComplete()| and return nothing.
  void Read(MojoResult result, const mojo::HandleSignalsState& state);
  // Writes data in RaceNetworkRequestReadBufferManager into the data
  // pipe producer that handles for both the race network request and the fetch
  // handler respectively.
  //
  // To guarantee the consistent data between the race network request and the
  // fetch handler, this method always writes a same chunk of data into two data
  // pipe handles. If one side fails the data write process for some reason, we
  // don't consume the buffer, and retry it later. the buffer is consumed only
  // when the both producer handles successfully write data.
  //
  // When the first chunk of data is written to the data pipes, this starts the
  // commit process. And when the data transfer is finished, this complete the
  // commit process.
  //
  // TODO(crbug.com/40258805) Add more UMAs to measure how long time to take
  // this process, and there could be the case if the response is not returned
  // due to the long fetch handler execution. and test case the mechanism to
  // wait for the fetch handler
  void TwoPhaseWrite(MojoResult result, const mojo::HandleSignalsState& state);

  bool IsReadyToHandleReadWrite(MojoResult result);

  void CompleteReadData(uint32_t num_bytes_to_consume);
  void WatchDataUpdate();

  void Abort();

  void RecordResponseReceivedToFetchHandlerEndTiming();
  // Record the time between the response received time and the fetch handler
  // end time iff both events are already reached.
  void MaybeRecordResponseReceivedToFetchHandlerEndTiming();
  void RecordMojoResultForDataTransfer(MojoResult result,
                                       const std::string& suffix);
  void RecordMojoResultForWrite(MojoResult result);

  void SetFetchHandlerEndTiming(base::TimeTicks fetch_handler_end_time,
                                bool is_fallback);

  State state_ = State::kWaitForBody;
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
  const network::ResourceRequest request_;
  base::WeakPtr<ServiceWorkerResourceLoader> owner_;
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  network::mojom::URLResponseHeadPtr head_;
  std::optional<mojo_base::BigBuffer> cached_metadata_;

  std::optional<RaceNetworkRequestReadBufferManager> read_buffer_manager_;
  RaceNetworkRequestWriteBufferManager
      write_buffer_manager_for_race_network_request_;
  RaceNetworkRequestWriteBufferManager write_buffer_manager_for_fetch_handler_;
  std::optional<network::URLLoaderCompletionStatus> completion_status_;
  bool redirected_ = false;
  std::unique_ptr<mojo::DataPipeDrainer> data_drainer_;
  DataConsumePolicy data_consume_policy_ = DataConsumePolicy::kTeeResponse;
  std::optional<base::TimeTicks> response_received_time_;
  std::optional<base::TimeTicks> fetch_handler_end_time_;
  std::optional<bool> is_fetch_handler_fallback_;
  bool is_main_resource_;

  base::TimeTicks request_start_;
  base::Time request_start_time_;

  base::WeakPtrFactory<ServiceWorkerRaceNetworkRequestURLLoaderClient>
      weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_URL_LOADER_CLIENT_H_
