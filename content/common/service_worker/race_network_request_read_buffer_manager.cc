// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_read_buffer_manager.h"
#include "base/debug/crash_logging.h"
#include "base/memory/scoped_refptr.h"
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
  uint32_t num_bytes = network::features::GetDataPipeDefaultAllocationSize(
      network::features::DataPipeAllocationSize::kLargerSizeIfPossible);
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(num_bytes);
  MojoResult result = consumer_handle_->ReadData(buffer->data(), &num_bytes,
                                                 MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK) {
    buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(std::move(buffer),
                                                           num_bytes);
  }

  return std::make_pair(result,
                        buffer_ ? buffer_->span() : base::span<const char>());
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
  return buffer_->span();
}
}  // namespace content
