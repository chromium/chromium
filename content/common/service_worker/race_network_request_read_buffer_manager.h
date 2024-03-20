// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_READ_BUFFER_MANAGER_H_
#define CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_READ_BUFFER_MANAGER_H_

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"

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

  // Returns MojoResult of DataPipe::ReadData() result, and actual read data.
  // The caller must call this only when |RemainingBuffer()| size is zero.
  std::pair<MojoResult, base::span<const char>> ReadData();
  // Consumes |buffer_| by given |num_bytes_read| bytes.
  void ConsumeData(size_t num_bytes_read);

  size_t BytesRemaining() const;
  base::span<const char> RemainingBuffer() const;

 private:
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::SimpleWatcher watcher_;
  scoped_refptr<net::DrainableIOBuffer> buffer_;
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_READ_BUFFER_MANAGER_H_
