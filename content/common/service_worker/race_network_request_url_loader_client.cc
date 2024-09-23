// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_url_loader_client.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/common/features.h"
#include "content/common/service_worker/service_worker_resource_loader.h"
#include "content/public/common/content_features.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace content {
namespace {
const char kMainResourceHistogramLoadTiming[] =
    "ServiceWorker.LoadTiming.MainFrame.MainResource";
const char kSubresourceHistogramLoadTiming[] =
    "ServiceWorker.LoadTiming.Subresource";
}  // namespace

ServiceWorkerRaceNetworkRequestURLLoaderClient::
    ServiceWorkerRaceNetworkRequestURLLoaderClient(
        const network::ResourceRequest& request,
        base::WeakPtr<ServiceWorkerResourceLoader> owner,
        mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client)
    : request_(request),
      owner_(std::move(owner)),
      forwarding_client_(std::move(forwarding_client)),
      is_main_resource_(owner_->IsMainResourceLoader()),
      request_start_(base::TimeTicks::Now()),
      request_start_time_(base::Time::Now()) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerRaceNetworkRequestURLLoaderClient::"
                         "ServiceWorkerRaceNetworkRequestURLLoaderClient",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT);

  // Create two data pipes. One is for RaceNetworkRequest. The other is for the
  // corresponding request in the fetch handler.
  if (!write_buffer_manager_for_race_network_request_.is_data_pipe_created()) {
    TransitionState(State::kAborted);
    return;
  }
  if (!write_buffer_manager_for_fetch_handler_.is_data_pipe_created()) {
    TransitionState(State::kAborted);
    return;
  }
}

ServiceWorkerRaceNetworkRequestURLLoaderClient::
    ~ServiceWorkerRaceNetworkRequestURLLoaderClient() {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerRaceNetworkRequestURLLoaderClient::"
                         "~ServiceWorkerRaceNetworkRequestURLLoaderClient",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTREACHED_IN_MIGRATION();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kServiceWorkerRaceNetworkRequest);
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  // Do nothing. Early Hints response will be handled by owner's
  // |url_loader_client_|.
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  if (!owner_) {
    return;
  }
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker",
      "ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveResponse",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", request_.url,
      "state", state_);
  TransitionState(State::kResponseReceived);

  // Set the response received time, and record the time delta between the
  // response received time and the fetch handler end time if the fetch handler
  // is already completed.
  if (!response_received_time_) {
    response_received_time_ = base::TimeTicks::Now();
  }
  MaybeRecordResponseReceivedToFetchHandlerEndTiming();

  switch (data_consume_policy_) {
    case DataConsumePolicy::kTeeResponse:
      head_ = std::move(head);
      head_->load_timing.request_start = request_start_;
      head_->load_timing.request_start_time = request_start_time_;
      cached_metadata_ = std::move(cached_metadata);
      read_buffer_manager_.emplace(std::move(body));
      WatchDataUpdate();
      break;
    case DataConsumePolicy::kForwardingOnly:
      forwarding_client_->OnReceiveResponse(std::move(head), std::move(body),
                                            std::move(cached_metadata));
      break;
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  if (!owner_) {
    return;
  }
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker",
      "ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveRedirect",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", request_.url,
      "state", state_);
  TransitionState(State::kRedirect);
  // If redirect happened, we don't have to create another data pipe.
  data_consume_policy_ = DataConsumePolicy::kForwardingOnly;
  response_received_time_ = base::TimeTicks::Now();

  // TODO(crbug.com/40258805): Return a redirect response to |owner| as a
  // RaceNetworkRequest result without breaking the cache storage compatibility.
  // We need a mechanism to wait for the fetch handler completion.
  //
  // ServiceWorker allows its fetch handler to fetch and cache cross-origin
  // resources. Also ServiceWorker may handle the request which has a redirect
  // mode "manual". In those cases, cached responses may be `opaque filtered
  // responses` or `opaque-redirect filtered responses`. The fetch handler may
  // want to cache those responses.
  //
  // For now, if a redirect response is received, we don't back the response to
  // |owner| as a RaceNetworkResponse's response, we stop the race and back the
  // response to the fetch handler only instead, so that we guarantee the fetch
  // handler completion.
  switch (owner_->commit_responsibility()) {
    case FetchResponseFrom::kNoResponseYet:
    case FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect:
      // This happens when the response is faster than the fetch handler.
      owner_->SetCommitResponsibility(FetchResponseFrom::kServiceWorker);
      forwarding_client_->OnReceiveRedirect(redirect_info, head->Clone());
      break;
    case FetchResponseFrom::kServiceWorker:
      // This happens when the fetch handler is faster, so basically
      // RaceNetworkRequest does not handle the response anymore. The fetch
      // handler is already executed but in rare case in-flight request may be
      // used. Let the fetch handler side client to handle the rest. The fetch
      // handler side close the connection if it's not needed anyway.
      forwarding_client_->OnReceiveRedirect(redirect_info, head->Clone());
      break;
    case FetchResponseFrom::kWithoutServiceWorker:
      // This happens when the fetch handler is faster and the result is
      // fallback. In this case in-flight RaceNetworkRequest will be used as a
      // fallback request.
      owner_->HandleRedirect(redirect_info, head);
      break;
    case FetchResponseFrom::kAutoPreloadHandlingFallback:
      NOTREACHED();
  }
  redirected_ = true;
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (!owner_) {
    return;
  }
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker",
      "ServiceWorkerRaceNetworkRequestURLLoaderClient::OnComplete",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", request_.url,
      "state", state_);
  if (owner_->IsMainResourceLoader()) {
    base::UmaHistogramBoolean(
        "ServiceWorker.FetchEvent.MainResource.RaceNetworkRequest.Redirect",
        redirected_);
  } else {
    base::UmaHistogramBoolean(
        "ServiceWorker.FetchEvent.Subresource.RaceNetworkRequest.Redirect",
        redirected_);
  }

  switch (data_consume_policy_) {
    case DataConsumePolicy::kTeeResponse:
      completion_status_ = status;
      MaybeCompleteResponse();
      break;
    case DataConsumePolicy::kForwardingOnly:
      forwarding_client_->OnComplete(status);
      break;
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::Bind(
    mojo::PendingRemote<network::mojom::URLLoaderClient>* remote) {
  receiver_.Bind(remote->InitWithNewPipeAndPassReceiver());
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::CommitResponse() {
  if (!owner_) {
    return;
  }
  TransitionState(State::kResponseCommitted);
  owner_->RecordFetchResponseFrom();
  owner_->CommitResponseHeaders(head_);
  owner_->CommitResponseBody(
      head_,
      write_buffer_manager_for_race_network_request_.ReleaseConsumerHandle(),
      std::move(cached_metadata_));
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::MaybeCommitResponse() {
  if (!owner_) {
    return;
  }

  if (state_ == State::kResponseReceived) {
    TransitionState(State::kDataTransferStarted);
    forwarding_client_->OnReceiveResponse(
        head_->Clone(),
        write_buffer_manager_for_fetch_handler_.ReleaseConsumerHandle(),
        std::nullopt);
  }

  if (state_ != State::kDataTransferStarted) {
    return;
  }

  switch (owner_->commit_responsibility()) {
    case FetchResponseFrom::kNoResponseYet:
    case FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect:
      // If the fetch handler result is a fallback, commit the
      // RaceNetworkRequest response. If the result is not a fallback and the
      // response is not ok status, use the other response from the fetch
      // handler instead because it may have a response from the cache.
      // TODO(crbug.com/40258805): More comprehensive error handling may be
      // needed, especially the case when HTTP cache hit or redirect happened.
      //
      // When the AutoPreload is enabled, RaceNetworkRequest works just for the
      // dedupe purpose. The fetch handler should always commit the response.
      if (!network::IsSuccessfulStatus(head_->headers->response_code())) {
        owner_->SetCommitResponsibility(FetchResponseFrom::kServiceWorker);
      } else {
        owner_->SetCommitResponsibility(
            FetchResponseFrom::kWithoutServiceWorker);
        CommitResponse();
      }
      break;
    case FetchResponseFrom::kServiceWorker:
      // If commit responsibility is FetchResponseFrom::kServiceWorker, that
      // means the response was already received from the fetch handler, or the
      // AutoPreload is enabled. The response from RaceNetworkRequest is
      // consumed only for the dedupe purpose.
      break;
    case FetchResponseFrom::kWithoutServiceWorker:
      // kWithoutServiceWorker is set When the fetch handler response comes
      // first and the result is a fallback. Commit the RaceNetworkRequest
      // response.
      CommitResponse();
      break;
    case FetchResponseFrom::kAutoPreloadHandlingFallback:
      NOTREACHED();
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::MaybeCompleteResponse() {
  if (!completion_status_) {
    return;
  }

  // If the data transfer finished, or a network error happened, complete the
  // commit.
  if (state_ == State::kDataTransferFinished ||
      completion_status_->error_code != net::OK) {
    CompleteResponse();
    return;
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::CompleteResponse() {
  if (!owner_) {
    return;
  }
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker",
      "ServiceWorkerRaceNetworkRequestURLLoaderClient::CompleteResponse",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", request_.url,
      "state", state_);
  bool is_aborted = false;
  switch (state_) {
    case State::kAborted:
      is_aborted = true;
      break;
    default:
      break;
  }
  switch (owner_->commit_responsibility()) {
    case FetchResponseFrom::kNoResponseYet:
    case FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect:
      // If a network error happens, there is a case that OnComplete can be
      // directly called, in that case |owner_->commit_responsibility()| is not
      // set yet. Ask the fetch handler side to handle response.
      owner_->SetCommitResponsibility(FetchResponseFrom::kServiceWorker);
      break;
    case FetchResponseFrom::kServiceWorker:
      // If the fetch handler wins or there is a network error in
      // RaceNetworkRequest, do nothing. Defer the handling to the owner.
      break;
    case FetchResponseFrom::kWithoutServiceWorker:
      if (is_aborted) {
        owner_->CommitCompleted(completion_status_->error_code,
                                "RaceNetworkRequest has aborted.");
        return;
      }
      TransitionState(State::kCompleted);
      owner_->CommitCompleted(completion_status_->error_code,
                              "RaceNetworkRequest has completed.");
      break;
    case FetchResponseFrom::kAutoPreloadHandlingFallback:
      NOTREACHED();
  }
  write_buffer_manager_for_race_network_request_.ResetProducer();
  forwarding_client_->OnComplete(completion_status_.value());
  write_buffer_manager_for_fetch_handler_.ResetProducer();
  // Cancel watching data pipes here not to call watcher callbacks after
  // complete the response.
  write_buffer_manager_for_race_network_request_.CancelWatching();
  write_buffer_manager_for_fetch_handler_.CancelWatching();
  if (read_buffer_manager_.has_value() && read_buffer_manager_->IsWatching()) {
    read_buffer_manager_->CancelWatching();
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::
    CommitAndCompleteResponseIfDataTransferFinished() {
  if (state_ == State::kDataTransferFinished) {
    CommitResponse();
    // Step back to State::kDataTransferFinished since MaybeCompleteResponse()
    // completes the response only when kDataTransferFinished is set.
    TransitionState(State::kDataTransferFinished);
    // When handling the fallback, the network request may return the initial
    // response, but may not be completed yet. Commit is done only when the
    // request is completed.
    MaybeCompleteResponse();
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnDataTransferComplete() {
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker",
      "ServiceWorkerRaceNetworkRequestURLLoaderClient::OnDataTransferComplete",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", request_.url,
      "state", state_);
  MaybeCommitResponse();
  TransitionState(State::kDataTransferFinished);
  MaybeCompleteResponse();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::WatchDataUpdate() {
  auto write_callback =
      &ServiceWorkerRaceNetworkRequestURLLoaderClient::TwoPhaseWrite;
  CHECK(read_buffer_manager_.has_value());
  read_buffer_manager_->Watch(
      base::BindRepeating(&ServiceWorkerRaceNetworkRequestURLLoaderClient::Read,
                          weak_factory_.GetWeakPtr()));
  read_buffer_manager_->ArmOrNotify();
  write_buffer_manager_for_race_network_request_.Watch(
      base::BindRepeating(write_callback, weak_factory_.GetWeakPtr()));
  write_buffer_manager_for_fetch_handler_.Watch(
      base::BindRepeating(write_callback, weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::Read(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (!IsReadyToHandleReadWrite(result)) {
    return;
  }

  CHECK(read_buffer_manager_.has_value());
  if (read_buffer_manager_->BytesRemaining() > 0) {
    write_buffer_manager_for_race_network_request_.ArmOrNotify();
    write_buffer_manager_for_fetch_handler_.ArmOrNotify();
    return;
  }

  auto [read_result, read_buffer] = read_buffer_manager_->ReadData();
  TRACE_EVENT_WITH_FLOW2("ServiceWorker",
                         "ServiceWorkerRaceNetworkRequestURLLoaderClient::Read",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "url", request_.url, "read_data_result", read_result);
  RecordMojoResultForDataTransfer(read_result, "Read");
  switch (read_result) {
    case MOJO_RESULT_OK:
      write_buffer_manager_for_race_network_request_.ArmOrNotify();
      write_buffer_manager_for_fetch_handler_.ArmOrNotify();
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      OnDataTransferComplete();
      return;
    case MOJO_RESULT_BUSY:
    case MOJO_RESULT_SHOULD_WAIT:
      return;
    default:
      NOTREACHED_IN_MIGRATION() << "ReadData result:" << read_result;
      return;
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::TwoPhaseWrite(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (!IsReadyToHandleReadWrite(result)) {
    return;
  }

  CHECK(read_buffer_manager_.has_value());
  if (read_buffer_manager_->BytesRemaining() == 0) {
    read_buffer_manager_->ArmOrNotify();
    return;
  }
  base::span<const char> read_buffer = read_buffer_manager_->RemainingBuffer();

  uint32_t num_bytes_to_consume = 0;
  if (write_buffer_manager_for_race_network_request_.IsWatching() &&
      write_buffer_manager_for_fetch_handler_.IsWatching()) {
    // If both data pipes are watched, write data to both pipes. Cancel writing
    // process if one of them is failed.
    result = write_buffer_manager_for_race_network_request_.BeginWriteData();
    RecordMojoResultForWrite(result);
    switch (result) {
      case MOJO_RESULT_OK:
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        // The data pipe consumer is aborted.
        TransitionState(State::kAborted);
        Abort();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        // The data pipe is not writable yet. We don't consume data from |body_|
        // and write any data in this case. And retry it later.
        write_buffer_manager_for_race_network_request_.EndWriteData(0);
        write_buffer_manager_for_race_network_request_.ArmOrNotify();
        return;
    }
    result = write_buffer_manager_for_fetch_handler_.BeginWriteData();
    RecordMojoResultForWrite(result);
    switch (result) {
      case MOJO_RESULT_OK:
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        TransitionState(State::kAborted);
        Abort();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        // When the data pipe returns MOJO_RESULT_SHOULD_WAIT, the data pipe is
        // not consumed yet but the buffer is full. Stop processing the data
        // pipe for the fetch handler side, not to make the data transfer
        // process for the race network request side being stuck.
        write_buffer_manager_for_race_network_request_.EndWriteData(0);
        write_buffer_manager_for_fetch_handler_.EndWriteData(0);
        write_buffer_manager_for_fetch_handler_.CancelWatching();
        write_buffer_manager_for_race_network_request_.ArmOrNotify();
        return;
    }
    // The maximum byte size to consume data. Use the smallest number from
    // 1) write size for the RaceNetworkRequest or 2) write size for the fetch
    // handler. This cap is needed because read/write operations are processed
    // sequentially, we should write the same size of data even if the available
    // buffer sizes in 1) and 2) are different per buffer.
    uint32_t max_num_bytes_to_consume =
        std::min(write_buffer_manager_for_race_network_request_.buffer_size(),
                 write_buffer_manager_for_fetch_handler_.buffer_size());
    // Copy data and call EndWriteData.
    uint32_t bytes_for_race_network_request =
        write_buffer_manager_for_race_network_request_
            .CopyAndCompleteWriteDataWithSize(read_buffer,
                                              max_num_bytes_to_consume);
    uint32_t bytes_for_fetch_handler =
        write_buffer_manager_for_fetch_handler_
            .CopyAndCompleteWriteDataWithSize(read_buffer,
                                              max_num_bytes_to_consume);
    CHECK_EQ(bytes_for_race_network_request, bytes_for_fetch_handler);
    num_bytes_to_consume = bytes_for_race_network_request;
  } else if (write_buffer_manager_for_race_network_request_.IsWatching()) {
    // If the data pipe for RaceNetworkRequest is the only watcher, don't write
    // data to the data pipe for the fetch handler.
    result = write_buffer_manager_for_race_network_request_.BeginWriteData();
    RecordMojoResultForWrite(result);
    switch (result) {
      case MOJO_RESULT_OK:
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        TransitionState(State::kAborted);
        Abort();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        write_buffer_manager_for_race_network_request_.EndWriteData(0);
        write_buffer_manager_for_race_network_request_.ArmOrNotify();
        return;
    }
    num_bytes_to_consume =
        write_buffer_manager_for_race_network_request_.CopyAndCompleteWriteData(
            read_buffer);
  } else if (write_buffer_manager_for_fetch_handler_.IsWatching()) {
    // If the data pipe for the fetch handler is the only watcher, don't write
    // data to the data pipe for RaceNetworkRequest.
    result = write_buffer_manager_for_fetch_handler_.BeginWriteData();
    RecordMojoResultForWrite(result);
    switch (result) {
      case MOJO_RESULT_OK:
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        TransitionState(State::kAborted);
        Abort();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        write_buffer_manager_for_fetch_handler_.EndWriteData(0);
        write_buffer_manager_for_fetch_handler_.ArmOrNotify();
        return;
    }
    num_bytes_to_consume =
        write_buffer_manager_for_fetch_handler_.CopyAndCompleteWriteData(
            read_buffer);
  }
  CompleteReadData(num_bytes_to_consume);
}

bool ServiceWorkerRaceNetworkRequestURLLoaderClient::IsReadyToHandleReadWrite(
    MojoResult result) {
  if (!owner_) {
    return false;
  }
  if (state_ == State::kDataTransferFinished) {
    return false;
  }

  RecordMojoResultForDataTransfer(result, "Initial");
  if (result != MOJO_RESULT_OK) {
    return false;
  }

  return true;
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::RecordMojoResultForWrite(
    MojoResult result) {
  RecordMojoResultForDataTransfer(result, "WriteForRaceNetworkRequset");
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::CompleteReadData(
    uint32_t num_bytes_to_consume) {
  CHECK(read_buffer_manager_.has_value());
  read_buffer_manager_->ConsumeData(num_bytes_to_consume);
  // Once data is written to the data pipe, start the commit process.
  MaybeCommitResponse();
  read_buffer_manager_->ArmOrNotify();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::Abort() {
  write_buffer_manager_for_race_network_request_.Abort();
  write_buffer_manager_for_fetch_handler_.Abort();
  if (read_buffer_manager_.has_value()) {
    read_buffer_manager_->CancelWatching();
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::SetFetchHandlerEndTiming(
    base::TimeTicks fetch_handler_end_time,
    bool is_fallback) {
  fetch_handler_end_time_ = fetch_handler_end_time;
  is_fetch_handler_fallback_ = is_fallback;
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::
    MaybeRecordResponseReceivedToFetchHandlerEndTiming() {
  if (response_received_time_ && fetch_handler_end_time_ &&
      is_fetch_handler_fallback_.has_value()) {
    RecordResponseReceivedToFetchHandlerEndTiming();
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::
    MaybeRecordResponseReceivedToFetchHandlerEndTiming(
        base::TimeTicks fetch_handler_end_time,
        bool is_fallback) {
  SetFetchHandlerEndTiming(fetch_handler_end_time, is_fallback);
  MaybeRecordResponseReceivedToFetchHandlerEndTiming();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::
    RecordResponseReceivedToFetchHandlerEndTiming() {
  if (!owner_) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({owner_->IsMainResourceLoader()
                        ? kMainResourceHistogramLoadTiming
                        : kSubresourceHistogramLoadTiming,
                    ".AutoPreloadResponseReceivedToFetchHandlerEnd.",
                    is_fetch_handler_fallback_ ? "WithoutServiceWorker"
                                               : "ServiceWorker"}),
      fetch_handler_end_time_.value() - response_received_time_.value());
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::
    RecordMojoResultForDataTransfer(MojoResult result,
                                    const std::string& suffix) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ServiceWorker.FetchEvent",
                    is_main_resource_ ? ".MainResource" : ".Subresource",
                    ".RaceNetworkRequest.DataTransfer.", suffix}),
      ConvertMojoResultForUMA(result));
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::TransitionState(
    State new_state) {
  switch (new_state) {
    case State::kWaitForBody:
      NOTREACHED();
    case State::kRedirect:
      CHECK(state_ == State::kWaitForBody || state_ == State::kRedirect)
          << "state_:" << static_cast<int>(state_);
      break;
    case State::kResponseReceived:
      CHECK(state_ == State::kWaitForBody || state_ == State::kRedirect)
          << "state_:" << static_cast<int>(state_);
      break;
    case State::kDataTransferStarted:
      CHECK_EQ(state_, State::kResponseReceived);
      break;
    case State::kResponseCommitted:
      CHECK(state_ == State::kDataTransferStarted ||
            state_ == State::kDataTransferFinished)
          << "state_:" << static_cast<int>(state_);
      break;
    case State::kDataTransferFinished:
      CHECK(state_ == State::kDataTransferStarted ||
            state_ == State::kResponseCommitted)
          << "state_:" << static_cast<int>(state_);
      break;
    case State::kCompleted:
      CHECK(state_ == State::kWaitForBody ||
            state_ == State::kResponseReceived ||
            state_ == State::kDataTransferStarted ||
            state_ == State::kResponseCommitted ||
            state_ == State::kDataTransferFinished)
          << "state_:" << static_cast<int>(state_);
      break;
    case State::kAborted:
      break;
  }
  state_ = new_state;
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::DrainData(
    mojo::ScopedDataPipeConsumerHandle source) {
  data_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(source));
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::CancelWriteData(
    FetchResponseFrom commit_responsibility) {
  switch (commit_responsibility) {
    case FetchResponseFrom::kServiceWorker:
      write_buffer_manager_for_race_network_request_.CancelWatching();
      write_buffer_manager_for_race_network_request_.ResetProducer();
      break;
    case FetchResponseFrom::kWithoutServiceWorker:
      NOTIMPLEMENTED();
      break;
    default:
      break;
  }
  // Calls body_consumer_watcher_.ArmOrNotify() to start the data transfer
  // again as we create two data pipes and propergate data from the consumer
  // handle |body_|. Even though one data pipe is canceled, the data transfer
  // process to the other data pipe has to be continued.
  if (read_buffer_manager_.has_value() && read_buffer_manager_->IsWatching()) {
    read_buffer_manager_->ArmOrNotify();
  }
}

ServiceWorkerRaceNetworkRequestURLLoaderClient::MojoResultForUMA
ServiceWorkerRaceNetworkRequestURLLoaderClient::ConvertMojoResultForUMA(
    MojoResult mojo_result) {
  switch (mojo_result) {
    case MOJO_RESULT_OK:
      return MojoResultForUMA::MOJO_RESULT_OK;
    case MOJO_RESULT_CANCELLED:
      return MojoResultForUMA::MOJO_RESULT_CANCELLED;
    case MOJO_RESULT_UNKNOWN:
      return MojoResultForUMA::MOJO_RESULT_UNKNOWN;
    case MOJO_RESULT_INVALID_ARGUMENT:
      return MojoResultForUMA::MOJO_RESULT_INVALID_ARGUMENT;
    case MOJO_RESULT_DEADLINE_EXCEEDED:
      return MojoResultForUMA::MOJO_RESULT_DEADLINE_EXCEEDED;
    case MOJO_RESULT_NOT_FOUND:
      return MojoResultForUMA::MOJO_RESULT_NOT_FOUND;
    case MOJO_RESULT_ALREADY_EXISTS:
      return MojoResultForUMA::MOJO_RESULT_ALREADY_EXISTS;
    case MOJO_RESULT_PERMISSION_DENIED:
      return MojoResultForUMA::MOJO_RESULT_PERMISSION_DENIED;
    case MOJO_RESULT_RESOURCE_EXHAUSTED:
      return MojoResultForUMA::MOJO_RESULT_RESOURCE_EXHAUSTED;
    case MOJO_RESULT_FAILED_PRECONDITION:
      return MojoResultForUMA::MOJO_RESULT_FAILED_PRECONDITION;
    case MOJO_RESULT_ABORTED:
      return MojoResultForUMA::MOJO_RESULT_ABORTED;
    case MOJO_RESULT_OUT_OF_RANGE:
      return MojoResultForUMA::MOJO_RESULT_OUT_OF_RANGE;
    case MOJO_RESULT_UNIMPLEMENTED:
      return MojoResultForUMA::MOJO_RESULT_UNIMPLEMENTED;
    case MOJO_RESULT_INTERNAL:
      return MojoResultForUMA::MOJO_RESULT_INTERNAL;
    case MOJO_RESULT_UNAVAILABLE:
      return MojoResultForUMA::MOJO_RESULT_UNAVAILABLE;
    case MOJO_RESULT_DATA_LOSS:
      return MojoResultForUMA::MOJO_RESULT_DATA_LOSS;
    case MOJO_RESULT_BUSY:
      return MojoResultForUMA::MOJO_RESULT_BUSY;
    case MOJO_RESULT_SHOULD_WAIT:
      return MojoResultForUMA::MOJO_RESULT_SHOULD_WAIT;
    default:
      NOTREACHED();
  }
}

net::NetworkTrafficAnnotationTag
ServiceWorkerRaceNetworkRequestURLLoaderClient::NetworkTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation(
      "service_worker_race_network_request",
      R"(
    semantics {
      sender: "ServiceWorkerRaceNetworkRequest"
      description:
        "This request is issued by a navigation to fetch the content of the "
        "page that is being navigated to, or by a renderer to fetch "
        "subresources in the case where a service worker has been registered "
        "for the page and the ServiceWorkerAutoPreload feature is enabled, or "
        "`race-network-and-fetch-handler` source in the Service Worker Static "
        "Routing API is specified."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc) and subsequent resource loading."
      data:
        "Arbitrary site-controlled data can be included in the URL, HTTP "
        "headers, and request body. Requests may include cookies and "
        "site-specific credentials."
      destination: WEBSITE
      internal {
        contacts {
          email: "chrome-worker@google.com"
        }
      }
      user_data {
        type: ARBITRARY_DATA
      }
      last_reviewed: "2023-03-22"
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "This request can be prevented by disabling service workers, which can "
        "be done by disabling cookie and site data under Settings, Content "
        "Settings, Cookies."
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "Chrome would be unable to use service workers if this feature were "
      "disabled, which could result in a degraded experience for websites that "
      "register a service worker. Using either URLBlocklist or URLAllowlist "
      "policies (or a combination of both) limits the scope of these requests."
)");
}

}  // namespace content
