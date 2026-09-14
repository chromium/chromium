// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_simple_buffer_manager.h"

#include "base/containers/span.h"
#include "base/location.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "content/common/features.h"

namespace content {
namespace {
// (crbug.com/548916010): When enabled, calls Finish() on Mojo errors and
// disconnects in RaceNetworkRequestSimpleBufferManager to prevent hanging
// callbacks.
BASE_FEATURE(
    kServiceWorkerRaceNetworkRequestSimpleBufferManagerHandleDisconnect,
    base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace
RaceNetworkRequestSimpleBufferManager::RaceNetworkRequestSimpleBufferManager(
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : drainer_(std::make_unique<mojo::DataPipeDrainer>(
          this,
          std::move(consumer_handle))) {}

RaceNetworkRequestSimpleBufferManager::
    ~RaceNetworkRequestSimpleBufferManager() = default;

void RaceNetworkRequestSimpleBufferManager::OnDataAvailable(
    base::span<const uint8_t> data) {
  TRACE_EVENT("ServiceWorker",
              "RaceNetworkRequestSimpleBufferManager::OnDataAvailable");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  buffered_body_.append(base::as_string_view(data));
  MaybeWriteData();
}

void RaceNetworkRequestSimpleBufferManager::OnDataComplete() {
  TRACE_EVENT("ServiceWorker",
              "RaceNetworkRequestSimpleBufferManager::OnDataComplete");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // All data transferred to `buffered_body_`.
  drain_complete_ = true;
  MaybeWriteData();
}

void RaceNetworkRequestSimpleBufferManager::Clone(
    mojo::ScopedDataPipeProducerHandle producer_handle,
    base::OnceClosure callback) {
  CHECK(!producer_handle_.is_valid());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  producer_handle_ = std::move(producer_handle);
  clone_complete_callback_ = std::move(callback);
  producer_handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  const MojoHandleSignals watch_signals =
      base::FeatureList::IsEnabled(
          kServiceWorkerRaceNetworkRequestSimpleBufferManagerHandleDisconnect)
          ? MOJO_HANDLE_SIGNAL_WRITABLE
          : (MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  producer_handle_watcher_->Watch(
      producer_handle_.get(), watch_signals, MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(
          &RaceNetworkRequestSimpleBufferManager::OnWriteAvailable,
          weak_factory_.GetWeakPtr()));
  if (base::FeatureList::IsEnabled(
          kServiceWorkerRaceNetworkRequestSimpleBufferManagerHandleDisconnect)) {
    peer_closed_watcher_ = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
        base::SequencedTaskRunner::GetCurrentDefault());
    peer_closed_watcher_->Watch(
        producer_handle_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_WATCH_CONDITION_SATISFIED,
        base::BindRepeating(
            &RaceNetworkRequestSimpleBufferManager::OnPeerClosed,
            weak_factory_.GetWeakPtr()));
  }
  producer_handle_watcher_->ArmOrNotify();
}

void RaceNetworkRequestSimpleBufferManager::OnWriteAvailable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(
          kServiceWorkerRaceNetworkRequestSimpleBufferManagerHandleDisconnect) &&
      (result != MOJO_RESULT_OK || state.peer_closed())) {
    Finish();
    return;
  }
  CHECK(producer_handle_.is_valid());
  MaybeWriteData();
}

void RaceNetworkRequestSimpleBufferManager::OnPeerClosed(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == MOJO_RESULT_CANCELLED) {
    return;
  }
  DCHECK(base::FeatureList::IsEnabled(
      kServiceWorkerRaceNetworkRequestSimpleBufferManagerHandleDisconnect));
  Finish();
}

void RaceNetworkRequestSimpleBufferManager::Finish() {
  TRACE_EVENT("ServiceWorker", "RaceNetworkRequestSimpleBufferManager::Finish");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!producer_handle_.is_valid()) {
    return;
  }
  write_position_ = 0;
  producer_handle_watcher_.reset();
  peer_closed_watcher_.reset();
  producer_handle_.reset();
  if (clone_complete_callback_) {
    std::move(clone_complete_callback_).Run();
  }
}

void RaceNetworkRequestSimpleBufferManager::MaybeWriteData() {
  TRACE_EVENT("ServiceWorker",
              "RaceNetworkRequestSimpleBufferManager::MaybeWriteData");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!producer_handle_.is_valid()) {
    // A producer handle may not be valid here. For example, a teeing clone
    // operation for the body has just finished, and the manager is waiting for
    // the next clone operation to start. A redundant OnDataComplete() call from
    // the drainer in this intermediate state could lead to a crash.
    return;
  }
  while (true) {
    std::string_view data = GetDataFromBuffer();
    if (data.empty()) {
      if (write_position_ == buffered_body_.size() && drain_complete_) {
        Finish();
      }
      break;
    }
    size_t actual_written_bytes = 0;
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
        if (base::FeatureList::IsEnabled(
                kServiceWorkerRaceNetworkRequestSimpleBufferManagerHandleDisconnect)) {
          Finish();
        }
        return;
    }
  }
}

std::string_view RaceNetworkRequestSimpleBufferManager::GetDataFromBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (drain_complete_ && write_position_ == buffered_body_.size()) {
    return std::string_view();
  }
  return std::string_view(buffered_body_).substr(write_position_);
}
}  // namespace content
