// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_SIMPLE_BUFFER_MANAGER_H_
#define CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_SIMPLE_BUFFER_MANAGER_H_

#include <string_view>

#include "base/containers/span.h"
#include "content/common/content_export.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace content {
// RaceNetworkRequestSimpleBufferManager performs read/write operations for the
// response data. It starts draining data from the original
// mojo::ScopedDataPipeConsumerHandle and keep it in the buffer. And the
// buffered data is copied to another data pipe which is given by `Clone()`.
class CONTENT_EXPORT RaceNetworkRequestSimpleBufferManager
    : public mojo::DataPipeDrainer::Client {
 public:
  explicit RaceNetworkRequestSimpleBufferManager(
      mojo::ScopedDataPipeConsumerHandle consumer_handle);
  RaceNetworkRequestSimpleBufferManager(
      const RaceNetworkRequestSimpleBufferManager&) = delete;
  RaceNetworkRequestSimpleBufferManager& operator=(
      const RaceNetworkRequestSimpleBufferManager&) = delete;
  ~RaceNetworkRequestSimpleBufferManager() override;

  // Implement mojo::DataPipeDrainer::Client
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  // Starts writing the buffered data into `producer_handle`. Run the `callback`
  // after all buffered data is written.
  void Clone(mojo::ScopedDataPipeProducerHandle producer_handle,
             base::OnceClosure callback);

 private:
  void OnWriteAvailable(MojoResult result,
                        const mojo::HandleSignalsState& state);
  void MaybeWriteData();
  void Finish();
  std::string_view GetDataFromBuffer();
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  std::string buffered_body_;
  bool drain_complete_ = false;
  size_t write_position_ = 0;

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> producer_handle_watcher_;
  base::OnceClosure clone_complete_callback_;

  base::WeakPtrFactory<RaceNetworkRequestSimpleBufferManager> weak_factory_{
      this};
};
}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_RACE_NETWORK_REQUEST_SIMPLE_BUFFER_MANAGER_H_
