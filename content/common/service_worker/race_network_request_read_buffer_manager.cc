// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_read_buffer_manager.h"
#include "base/debug/crash_logging.h"
#include "mojo/public/c/system/types.h"

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

MojoResult RaceNetworkRequestReadBufferManager::EndReadData(
    size_t num_bytes_read) {
  return consumer_handle_->EndReadData(num_bytes_read);
}

void RaceNetworkRequestReadBufferManager::CancelWatching() {
  watcher_.Cancel();
}

std::pair<MojoResult, base::span<const char>>
RaceNetworkRequestReadBufferManager::BeginReadData() {
  const void* buffer;
  uint32_t buffer_num_bytes = 0;
  base::span<const char> read_buffer;
  MojoResult result = consumer_handle_->BeginReadData(
      &buffer, &buffer_num_bytes, MOJO_BEGIN_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK) {
    SCOPED_CRASH_KEY_NUMBER("SWRace", "num_bytes_read_buffer",
                            buffer_num_bytes);
    volatile const char* buffer_v = static_cast<volatile const char*>(buffer);
    for (size_t i = 0; i < buffer_num_bytes; ++i) {
      buffer_v[i];
    }
    read_buffer =
        base::make_span(static_cast<const char*>(buffer), buffer_num_bytes);
  }

  return std::make_pair(result, read_buffer);
}
}  // namespace content
