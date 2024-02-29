// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_READ_BUFFER_MANAGER_H_
#define CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_READ_BUFFER_MANAGER_H_

#include <optional>

#include "base/containers/span.h"
#include "content/common/content_export.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace content {
class CONTENT_EXPORT RaceNetworkRequestReadBufferManager {
 public:
  explicit RaceNetworkRequestReadBufferManager(
      mojo::ScopedDataPipeConsumerHandle consumer_handle);
  RaceNetworkRequestReadBufferManager(
      const RaceNetworkRequestReadBufferManager&) = delete;
  RaceNetworkRequestReadBufferManager& operator=(
      const RaceNetworkRequestReadBufferManager&) = delete;
  ~RaceNetworkRequestReadBufferManager();

  void Watch(mojo::SimpleWatcher::ReadyCallbackWithState callback);
  void ArmOrNotify();
  void CancelWatching();
  bool IsWatching() { return watcher_.IsWatching(); }

  std::pair<MojoResult, base::span<const char>> BeginReadData();
  MojoResult EndReadData(size_t num_bytes_read);

 private:
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::SimpleWatcher watcher_;
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_READ_BUFFER_MANAGER_H_
