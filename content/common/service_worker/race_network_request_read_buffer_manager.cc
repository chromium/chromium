// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/common/service_worker/race_network_request_read_buffer_manager.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/features.h"
#include "mojo/public/c/system/types.h"
#include "net/base/io_buffer.h"
#include "services/network/public/cpp/features.h"

namespace content {
RaceNetworkRequestReadBufferManager::RaceNetworkRequestReadBufferManager(
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : consumer_handle_(std::move(consumer_handle)),
      watcher_(FROM_HERE,
               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
               base::SequencedTaskRunner::GetCurrentDefault()) {}

RaceNetworkRequestReadBufferManager::~RaceNetworkRequestReadBufferManager() =
    default;

void RaceNetworkRequestReadBufferManager::Watch(
    mojo::SimpleWatcher::ReadyCallbackWithState callback) {
  watcher_.Watch(consumer_handle_.get(),
                 MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                 MOJO_WATCH_CONDITION_SATISFIED, std::move(callback));
}

void RaceNetworkRequestReadBufferManager::ArmOrNotify() {
  watcher_.ArmOrNotify();
}


void RaceNetworkRequestReadBufferManager::CancelWatching() {
  watcher_.Cancel();
}

std::pair<MojoResult, base::span<const char>>
RaceNetworkRequestReadBufferManager::ReadData() {
  CHECK_EQ(BytesRemaining(), 0u);
  size_t num_bytes = 0;
  MojoResult result;
  bool is_query_data_size_mode = base::GetFieldTrialParamByFeatureAsBool(
      features::kServiceWorkerAutoPreload, "query_data_size", false);
  if (is_query_data_size_mode) {
    result = consumer_handle_->ReadData(MOJO_READ_DATA_FLAG_QUERY,
                                        base::span<uint8_t>(), num_bytes);
    CHECK_EQ(result, MOJO_RESULT_OK);
    // Sometimes queried |num_bytes| is zero. So explicitly set >=1 byte size
    // here to avoid receiving |MOJO_RESULT_INVALID_ARGUMENT| from
    // DataPipe::ReadData(), which happens if the |num_bytes| is zero.
    if (num_bytes == 0) {
      num_bytes = network::features::GetDataPipeDefaultAllocationSize();
      CHECK_GT(num_bytes, 0u);
    }
  } else {
    num_bytes = base::GetFieldTrialParamByFeatureAsInt(
        features::kServiceWorkerAutoPreload, "read_buffer_size",
        network::features::GetDataPipeDefaultAllocationSize(
            network::features::DataPipeAllocationSize::kLargerSizeIfPossible));
  }
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(num_bytes);
  result = consumer_handle_->ReadData(MOJO_READ_DATA_FLAG_NONE, buffer->span(),
                                      num_bytes);

  if (result == MOJO_RESULT_OK) {
    buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(std::move(buffer),
                                                           num_bytes);
  }

  return std::make_pair(result, buffer_ ? base::as_chars(buffer_->span())
                                        : base::span<const char>());
}

void RaceNetworkRequestReadBufferManager::ConsumeData(size_t num_bytes_read) {
  CHECK(buffer_);
  buffer_->DidConsume(num_bytes_read);
}

size_t RaceNetworkRequestReadBufferManager::BytesRemaining() const {
  return buffer_ ? buffer_->BytesRemaining() : 0;
}

base::span<const char> RaceNetworkRequestReadBufferManager::RemainingBuffer()
    const {
  CHECK(buffer_);
  // When there are remaining bytes in |buffer_|, returns them as
  // base::span with the actual data size. IOBuffer::span() returns the span
  // with the size of the whole buffer, even if data is partially consumed. So
  // subspan it with the remaining data size.
  return base::as_chars(buffer_->span()).subspan(0, buffer_->BytesRemaining());
}
}  // namespace content
