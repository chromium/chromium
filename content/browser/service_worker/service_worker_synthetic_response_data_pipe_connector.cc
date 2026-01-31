// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_synthetic_response_data_pipe_connector.h"

#include "base/sequence_checker.h"
#include "base/trace_event/trace_event.h"

namespace content {

ServiceWorkerSyntheticResponseDataPipeConnector::
    ServiceWorkerSyntheticResponseDataPipeConnector(
        mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : drainer_(
          std::make_unique<mojo::DataPipeDrainer>(this,
                                                  std::move(consumer_handle))) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseDataPipeConnector::"
              "ServiceWorkerSyntheticResponseDataPipeConnector");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ServiceWorkerSyntheticResponseDataPipeConnector::Transfer(
    mojo::ScopedDataPipeProducerHandle producer_handle,
    base::OnceClosure on_finished) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(producer_handle.is_valid());
  CHECK_EQ(write_position_, 0u);
  producer_handle_ = std::move(producer_handle);
  on_complete_ = std::move(on_finished);
  producer_handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  producer_handle_watcher_->Watch(
      producer_handle_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(
          &ServiceWorkerSyntheticResponseDataPipeConnector::OnWriteAvailable,
          weak_factory_.GetWeakPtr()));
  producer_handle_watcher_->ArmOrNotify();
}

ServiceWorkerSyntheticResponseDataPipeConnector::
    ~ServiceWorkerSyntheticResponseDataPipeConnector() = default;

void ServiceWorkerSyntheticResponseDataPipeConnector::OnDataAvailable(
    base::span<const uint8_t> data) {
  TRACE_EVENT(
      "ServiceWorker",
      "ServiceWorkerSyntheticResponseDataPipeConnector::OnDataAvailable");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!data_complete_);
  if (!producer_handle_.is_valid()) {
    return;
  }
  if (!internal_buffer_.empty()) {
    internal_buffer_.insert(internal_buffer_.end(), data.begin(), data.end());
    return;
  }
  CHECK_EQ(write_position_, 0u);
  do {
    size_t actual_written_bytes = 0;
    MojoResult result = producer_handle_->WriteData(
        data, MOJO_WRITE_DATA_FLAG_NONE, actual_written_bytes);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      producer_handle_watcher_->ArmOrNotify();
      internal_buffer_.insert(internal_buffer_.end(), data.begin(), data.end());
      return;
    }
    if (result != MOJO_RESULT_OK) {
      Finish();
      return;
    }
    data = data.subspan(actual_written_bytes);
    DCHECK_GT(actual_written_bytes, 0u);
  } while (!data.empty());
}

void ServiceWorkerSyntheticResponseDataPipeConnector::OnDataComplete() {
  TRACE_EVENT(
      "ServiceWorker",
      "ServiceWorkerSyntheticResponseDataPipeConnector::OnDataComplete");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!data_complete_);
  if (!producer_handle_.is_valid()) {
    return;
  }
  data_complete_ = true;
  if (internal_buffer_.empty()) {
    Finish();
  }
}

void ServiceWorkerSyntheticResponseDataPipeConnector::OnWriteAvailable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  TRACE_EVENT(
      "ServiceWorker",
      "ServiceWorkerSyntheticResponseDataPipeConnector::OnWriteAvailable");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != MOJO_RESULT_OK) {
    Finish();
    return;
  }

  CHECK_LE(write_position_, internal_buffer_.size());
  CHECK(!internal_buffer_.empty() || !data_complete_);
  base::span<const uint8_t> data =
      base::span(internal_buffer_).subspan(write_position_);
  while (!data.empty()) {
    size_t actual_written_bytes = 0;
    MojoResult write_result = producer_handle_->WriteData(
        data, MOJO_WRITE_DATA_FLAG_NONE, actual_written_bytes);
    if (write_result == MOJO_RESULT_SHOULD_WAIT) {
      producer_handle_watcher_->ArmOrNotify();
      return;
    }
    if (write_result != MOJO_RESULT_OK) {
      Finish();
      return;
    }
    write_position_ += actual_written_bytes;
    data = data.subspan(actual_written_bytes);
  }
  internal_buffer_.clear();
  write_position_ = 0;
  if (data_complete_) {
    Finish();
  }
}

void ServiceWorkerSyntheticResponseDataPipeConnector::Finish() {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseDataPipeConnector::Finish");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  producer_handle_.reset();
  producer_handle_watcher_.reset();
  std::move(on_complete_).Run();
}

}  // namespace content
