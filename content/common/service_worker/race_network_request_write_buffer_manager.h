// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_WRITE_BUFFER_MANAGER_H_
#define CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_WRITE_BUFFER_MANAGER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace content {
class CONTENT_EXPORT RaceNetworkRequestWriteBufferManager {
 public:
  // The static function to override the data pipe buffer size from tests.
  static void SetDataPipeCapacityBytesForTesting(uint32_t size);

  RaceNetworkRequestWriteBufferManager();
  RaceNetworkRequestWriteBufferManager(
      const RaceNetworkRequestWriteBufferManager&) = delete;
  RaceNetworkRequestWriteBufferManager& operator=(
      const RaceNetworkRequestWriteBufferManager&) = delete;
  ~RaceNetworkRequestWriteBufferManager();

  bool is_data_pipe_created() const { return is_data_pipe_created_; }
  mojo::ScopedDataPipeConsumerHandle ReleaseConsumerHandle();
  void Abort();
  void ResetProducer();
  void Watch(mojo::SimpleWatcher::ReadyCallbackWithState callback);
  bool IsWatching() const { return watcher_.IsWatching(); }
  void CancelWatching();
  MojoResult BeginWriteData();
  MojoResult EndWriteData(uint32_t num_bytes_written);
  std::tuple<MojoResult, size_t> WriteData(base::span<const char> buffer);
  void ArmOrNotify();
  size_t buffer_size() const { return buffer_.size(); }
  size_t num_bytes_written() const { return num_bytes_written_; }
  size_t CopyAndCompleteWriteData(base::span<const char> read_buffer);
  size_t CopyAndCompleteWriteDataWithSize(base::span<const char> read_buffer,
                                          size_t max_num_bytes_to_consume);

 private:
  static uint32_t data_pipe_size_for_test_;
  uint32_t GetDataPipeCapacityBytes();

  size_t EndWriteData(base::span<const char> read_buffer,
                      std::optional<uint32_t> max_num_bytes_to_consume);

  uint32_t data_pipe_buffer_size_;
  bool is_data_pipe_created_;
  mojo::ScopedDataPipeProducerHandle producer_;
  mojo::ScopedDataPipeConsumerHandle consumer_;
  base::raw_span<uint8_t> buffer_;
  mojo::SimpleWatcher watcher_;
  size_t num_bytes_written_ = 0;
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_WRITE_BUFFER_MANAGER_H_
