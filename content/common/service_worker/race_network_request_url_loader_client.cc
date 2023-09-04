// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_url_loader_client.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/common/features.h"
#include "content/common/service_worker/service_worker_resource_loader.h"
#include "content/public/common/content_features.h"
#include "mojo/public/c/system/data_pipe.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace content {
namespace {
const char kMainResourceHistogramLoadTiming[] =
    "ServiceWorker.LoadTiming.MainFrame.MainResource";
const char kSubresourceHistogramLoadTiming[] =
    "ServiceWorker.LoadTiming.Subresource";

MojoResult CreateDataPipe(mojo::ScopedDataPipeProducerHandle& producer_handle,
                          mojo::ScopedDataPipeConsumerHandle& consumer_handle,
                          uint32_t capacity_num_bytes) {
  MojoCreateDataPipeOptions options;

  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = capacity_num_bytes;

  return mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
}
}  // namespace

ServiceWorkerRaceNetworkRequestURLLoaderClient::
    ServiceWorkerRaceNetworkRequestURLLoaderClient(
        const network::ResourceRequest& request,
        base::WeakPtr<ServiceWorkerResourceLoader> owner,
        mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
        uint32_t data_pipe_capacity_num_bytes)
    : request_(request),
      owner_(std::move(owner)),
      forwarding_client_(std::move(forwarding_client)),
      body_consumer_watcher_(FROM_HERE,
                             mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                             base::SequencedTaskRunner::GetCurrentDefault()) {
  // The feature param may override the buffer size.
  uint32_t data_pipe_size = base::GetFieldTrialParamByFeatureAsInt(
      features::kServiceWorkerBypassFetchHandler,
      "data_pipe_capacity_num_bytes", data_pipe_capacity_num_bytes);
  // Create two data pipes. One is for RaceNetworkRequest. The other is for the
  // corresponding request in the fetch handler.
  if (CreateDataPipe(data_pipe_for_race_network_request_.producer,
                     data_pipe_for_race_network_request_.consumer,
                     data_pipe_size) != MOJO_RESULT_OK) {
    TransitionState(State::kAborted);
    return;
  }
  if (CreateDataPipe(data_pipe_for_fetch_handler_.producer,
                     data_pipe_for_fetch_handler_.consumer,
                     data_pipe_size) != MOJO_RESULT_OK) {
    TransitionState(State::kAborted);
    return;
  }
}

ServiceWorkerRaceNetworkRequestURLLoaderClient::
    ~ServiceWorkerRaceNetworkRequestURLLoaderClient() = default;

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTREACHED();
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
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  TRACE_EVENT0(
      "ServiceWorker",
      "ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveResponse");
  if (!owner_) {
    return;
  }

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
      cached_metadata_ = std::move(cached_metadata);
      body_ = std::move(body);
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
  TransitionState(State::kRedirect);
  // If redirect happened, we don't have to create another data pipe.
  data_consume_policy_ = DataConsumePolicy::kForwardingOnly;
  response_received_time_ = base::TimeTicks::Now();

  // TODO(crbug.com/1420517): Return a redirect response to |owner| as a
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
      NOTREACHED_NORETURN();
  }
  redirected_ = true;
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (!owner_) {
    return;
  }
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
      head_, std::move(data_pipe_for_race_network_request_.consumer),
      std::move(cached_metadata_));
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::MaybeCommitResponse() {
  if (!owner_) {
    return;
  }

  if (state_ == State::kResponseReceived) {
    TransitionState(State::kDataTransferStarted);
    forwarding_client_->OnReceiveResponse(
        head_->Clone(), std::move(data_pipe_for_fetch_handler_.consumer),
        absl::nullopt);
  }

  if (state_ != State::kDataTransferStarted) {
    return;
  }

  switch (owner_->commit_responsibility()) {
    case FetchResponseFrom::kNoResponseYet:
    case FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect:
      // If the fetch handler result is a fallback, commit the
      // RaceNetworkRequest response. If the result is not a fallback and the
      // response is not 200, use the other response from the fetch handler
      // instead because it may have a response from the cache.
      // TODO(crbug.com/1420517): More comprehensive error handling may be
      // needed, especially the case when HTTP cache hit or redirect happened.
      //
      // When the AutoPreload is enabled, RaceNetworkRequest works just for the
      // dedupe purpose. The fetch handler should always commit the response.
      if (head_->headers->response_code() != net::HttpStatusCode::HTTP_OK) {
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
      NOTREACHED_NORETURN();
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
      TransitionState(State::kCompleted);
      owner_->CommitCompleted(completion_status_->error_code,
                              "RaceNetworkRequest has completed.");
      break;
    case FetchResponseFrom::kAutoPreloadHandlingFallback:
      NOTREACHED_NORETURN();
  }
  data_pipe_for_race_network_request_.producer.reset();
  forwarding_client_->OnComplete(completion_status_.value());
  data_pipe_for_fetch_handler_.producer.reset();
  // Cancel watching data pipes here not to call watcher callbacks after
  // complete the response.
  data_pipe_for_race_network_request_.watcher.Cancel();
  data_pipe_for_fetch_handler_.watcher.Cancel();
  body_consumer_watcher_.Cancel();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::
    CommitAndCompleteResponseIfDataTransferFinished() {
  if (state_ == State::kDataTransferFinished) {
    CommitResponse();
    CompleteResponse();
  }
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnDataTransferComplete() {
  MaybeCommitResponse();
  TRACE_EVENT0(
      "ServiceWorker",
      "ServiceWorkerRaceNetworkRequestURLLoaderClient::OnDataTransferComplete");
  TransitionState(State::kDataTransferFinished);
  MaybeCompleteResponse();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::WatchDataUpdate() {
  body_consumer_watcher_.Watch(
      body_.get(), MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(
          &ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite,
          weak_factory_.GetWeakPtr()));
  body_consumer_watcher_.ArmOrNotify();
  data_pipe_for_race_network_request_.watcher.Watch(
      data_pipe_for_race_network_request_.producer.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(
          &ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite,
          weak_factory_.GetWeakPtr()));
  data_pipe_for_fetch_handler_.watcher.Watch(
      data_pipe_for_fetch_handler_.producer.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(
          &ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite,
          weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite(
    MojoResult aresult) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerRaceNetworkRequestURLLoaderClient::ReadAndWrite");

  if (!owner_) {
    return;
  }

  std::string histogram_prefix = base::StrCat(
      {"ServiceWorker.FetchEvent",
       owner_->IsMainResourceLoader() ? ".MainResource" : ".Subresource",
       ".RaceNetworkRequest.DataTransfer"});

  // Read data from |body_| data pipe.
  const void* buffer;
  // Contains the actual byte size for read/write data. The smallest number from
  // 1) read size, 2) write size for the RaceNetworkRequest, 3) write size for
  // the fetch handler, will be used.
  uint32_t num_bytes_to_consume = 0;

  MojoResult result = body_->BeginReadData(&buffer, &num_bytes_to_consume,
                                           MOJO_READ_DATA_FLAG_NONE);
  base::UmaHistogramEnumeration(base::StrCat({histogram_prefix, ".Read"}),
                                ConvertMojoResultForUMA(result));
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Successfully read the whole data.
      OnDataTransferComplete();
      return;
    case MOJO_RESULT_BUSY:
    case MOJO_RESULT_SHOULD_WAIT:
      return;
    default:
      NOTREACHED() << "BeginReadData result:" << result;
      return;
  }

  void* write_buffer = nullptr;
  void* write_buffer_for_fetch_handler = nullptr;

  // Begin the write process for the response of the race network request.
  result = data_pipe_for_race_network_request_.producer->BeginWriteData(
      &write_buffer, &data_pipe_for_race_network_request_.num_write_bytes,
      MOJO_WRITE_DATA_FLAG_NONE);
  base::UmaHistogramEnumeration(
      base::StrCat({histogram_prefix, ".WriteForRaceNetworkRequset"}),
      ConvertMojoResultForUMA(result));
  switch (result) {
    case MOJO_RESULT_OK:
      // Perhaps writable size may be smaller than the readable size. Choose the
      // most smallest size.
      num_bytes_to_consume =
          std::min(num_bytes_to_consume,
                   data_pipe_for_race_network_request_.num_write_bytes);
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The data pipe consumer is aborted.
      TransitionState(State::kAborted);
      Abort();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      // The data pipe is not writable yet. We don't consume data from |body_|
      // and write any data in this case. And retry it later.
      body_->EndReadData(0);
      data_pipe_for_race_network_request_.producer->EndWriteData(0);
      data_pipe_for_race_network_request_.watcher.ArmOrNotify();
      return;
  }

  // If the data consuming for the fetch handler is canceled, we process the
  // consuming only for the race network request.
  if (!data_pipe_for_fetch_handler_.watcher.IsWatching()) {
    // Copy data and complete read/write process.
    memcpy(write_buffer, buffer, num_bytes_to_consume);
    result = data_pipe_for_race_network_request_.producer->EndWriteData(
        num_bytes_to_consume);
    CHECK_EQ(result, MOJO_RESULT_OK);
    result = body_->EndReadData(num_bytes_to_consume);
    CHECK_EQ(result, MOJO_RESULT_OK);
    // Once data is written to the data pipe, start the commit process.
    MaybeCommitResponse();
    body_consumer_watcher_.ArmOrNotify();
    return;
  }

  // Begin the write process for the response of the fetch handler.
  result = data_pipe_for_fetch_handler_.producer->BeginWriteData(
      &write_buffer_for_fetch_handler,
      &data_pipe_for_fetch_handler_.num_write_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  base::UmaHistogramEnumeration(
      base::StrCat({histogram_prefix, ".WriteForFetchHandler"}),
      ConvertMojoResultForUMA(result));
  switch (result) {
    case MOJO_RESULT_OK:
      // Perhaps writable size may be smaller than the readable size. Choose
      // the most smallest size.
      num_bytes_to_consume = std::min(
          num_bytes_to_consume, data_pipe_for_fetch_handler_.num_write_bytes);
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The data pipe consumer is aborted.
      TransitionState(State::kAborted);
      Abort();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      // When the data pipe returns MOJO_RESULT_SHOULD_WAIT, the data pipe is
      // not consumed yet but the buffer is full. Stop processing the data pipe
      // for the fetch handler side, not to make the data transfer process for
      // the race network request side being stuck.
      body_->EndReadData(0);
      data_pipe_for_race_network_request_.producer->EndWriteData(0);
      data_pipe_for_fetch_handler_.producer->EndWriteData(0);
      data_pipe_for_fetch_handler_.watcher.Cancel();
      data_pipe_for_race_network_request_.watcher.ArmOrNotify();
      return;
  }

  // Copy data and complete read/write process.
  memcpy(write_buffer, buffer, num_bytes_to_consume);
  result = data_pipe_for_race_network_request_.producer->EndWriteData(
      num_bytes_to_consume);
  CHECK_EQ(result, MOJO_RESULT_OK);
  memcpy(write_buffer_for_fetch_handler, buffer, num_bytes_to_consume);
  result =
      data_pipe_for_fetch_handler_.producer->EndWriteData(num_bytes_to_consume);
  CHECK_EQ(result, MOJO_RESULT_OK);
  result = body_->EndReadData(num_bytes_to_consume);
  CHECK_EQ(result, MOJO_RESULT_OK);

  // Once data is written to the data pipe, start the commit process.
  MaybeCommitResponse();
  body_consumer_watcher_.ArmOrNotify();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::Abort() {
  data_pipe_for_race_network_request_.producer.reset();
  data_pipe_for_race_network_request_.consumer.reset();
  data_pipe_for_race_network_request_.watcher.Cancel();
  data_pipe_for_fetch_handler_.producer.reset();
  data_pipe_for_fetch_handler_.consumer.reset();
  data_pipe_for_fetch_handler_.watcher.Cancel();
  body_consumer_watcher_.Cancel();
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

void ServiceWorkerRaceNetworkRequestURLLoaderClient::TransitionState(
    State new_state) {
  switch (new_state) {
    case State::kWaitForBody:
      NOTREACHED_NORETURN();
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
      NOTREACHED_NORETURN();
  }
}

ServiceWorkerRaceNetworkRequestURLLoaderClient::DataPipeInfo::DataPipeInfo()
    : watcher(FROM_HERE,
              mojo::SimpleWatcher::ArmingPolicy::MANUAL,
              base::SequencedTaskRunner::GetCurrentDefault()),
      num_write_bytes(0) {}
ServiceWorkerRaceNetworkRequestURLLoaderClient::DataPipeInfo::~DataPipeInfo() =
    default;

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
        "for the page and the ServiceWorkerBypassFetchHandler feature and the "
        "RaceNetworkRequest param are enabled."
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
