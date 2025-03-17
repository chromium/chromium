// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SOURCE_STREAM_TO_DATA_PIPE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SOURCE_STREAM_TO_DATA_PIPE_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/cpp/net_adapters.h"

namespace net {
class SourceStream;
}

namespace network {

// A convenient adapter class to read out data from net::SourceStream
// and write them into a data pipe.
class COMPONENT_EXPORT(NETWORK_CPP) SourceStreamToDataPipe {
 public:
  // Reads out the data from |source| and write into |dest|.
  SourceStreamToDataPipe(std::unique_ptr<net::SourceStream> source,
                         mojo::ScopedDataPipeProducerHandle dest,
                         scoped_refptr<base::SequencedTaskRunner> task_runner =
                             base::SequencedTaskRunner::GetCurrentDefault());
  ~SourceStreamToDataPipe();

  // Start reading the source.
  void Start(base::OnceCallback<void(int)> completion_callback);
  int64_t TransferredBytes() const { return transferred_bytes_; }

 private:
  enum class State {
    kNone,
    kBeginWrite,
    kBeginWriteComplete,
    kReadData,
    kReadDataComplete,
  };

  int DoLoop(int result);

  int DoBeginWrite();
  int DoBeginWriteComplete(int result);
  int DoReadData();
  int DoReadDataComplete(int result);

  void DidRead(int result);
  void OnDataPipeWritable(MojoResult result);

  void OnIOComplete(int result);

  void CleanupAndRunCallback(int result);

  State next_state_ = State::kNone;

  std::unique_ptr<net::SourceStream> source_;
  mojo::ScopedDataPipeProducerHandle dest_;
  base::OnceCallback<void(int)> completion_callback_;
  int64_t transferred_bytes_ = 0;

  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  mojo::SimpleWatcher writable_handle_watcher_;

  base::WeakPtrFactory<SourceStreamToDataPipe> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SOURCE_STREAM_TO_DATA_PIPE_H_
