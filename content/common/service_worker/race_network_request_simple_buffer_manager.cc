// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_simple_buffer_manager.h"

#include "base/containers/span.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/features.h"

namespace content {
RaceNetworkRequestSimpleBufferManager::RaceNetworkRequestSimpleBufferManager(
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : drainer_(std::make_unique<mojo::DataPipeDrainer>(
          this,
          std::move(consumer_handle))) {}

RaceNetworkRequestSimpleBufferManager::
    ~RaceNetworkRequestSimpleBufferManager() = default;

void RaceNetworkRequestSimpleBufferManager::OnDataAvailable(
    base::span<const uint8_t> data) {
  buffered_body_.append(base::as_string_view(data));
  if (producer_handle_.is_valid()) {
    MaybeWriteData();
  }
}

void RaceNetworkRequestSimpleBufferManager::OnDataComplete() {
  // All data transferred to `buffered_body_`.
  drain_complete_ = true;
  MaybeWriteData();
}

void RaceNetworkRequestSimpleBufferManager::Clone(
    mojo::ScopedDataPipeProducerHandle producer_handle,
    base::OnceClosure callback) {
  CHECK(!producer_handle_.is_valid());
  producer_handle_ = std::move(producer_handle);
  producer_handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  producer_handle_watcher_->Watch(
      producer_handle_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(
          &RaceNetworkRequestSimpleBufferManager::OnWriteAvailable,
          weak_factory_.GetWeakPtr()));
  producer_handle_watcher_->ArmOrNotify();
  clone_complete_callback_ = std::move(callback);
}

void RaceNetworkRequestSimpleBufferManager::OnWriteAvailable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  CHECK(producer_handle_.is_valid());
  MaybeWriteData();
}

void RaceNetworkRequestSimpleBufferManager::Finish() {
  write_position_ = 0;
  producer_handle_.reset();
  producer_handle_watcher_.reset();
  std::move(clone_complete_callback_).Run();
}

void RaceNetworkRequestSimpleBufferManager::MaybeWriteData() {
  while (true) {
    std::string_view data = GetDataFromBuffer();
    if (data.empty()) {
      if (write_position_ == buffered_body_.size() && drain_complete_) {
        Finish();
      }
      break;
    }
    size_t actual_written_bytes = 0;
    CHECK(producer_handle_);
    MojoResult result = producer_handle_->WriteData(base::as_byte_span(data),
                                                    MOJO_WRITE_DATA_FLAG_NONE,
                                                    actual_written_bytes);
    switch (result) {
      case MOJO_RESULT_OK:
        write_position_ += actual_written_bytes;
        break;
      case MOJO_RESULT_SHOULD_WAIT:
        producer_handle_watcher_->ArmOrNotify();
        return;
      default:
        // ERROR, disconnect
        return;
    }
  }
}

std::string_view RaceNetworkRequestSimpleBufferManager::GetDataFromBuffer() {
  if (drain_complete_ && write_position_ == buffered_body_.size()) {
    return std::string_view();
  }
  return std::string_view(buffered_body_).substr(write_position_);
}
}  // namespace content
