// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_DATA_PIPE_WRITER_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_DATA_PIPE_WRITER_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace network {

class SharedDictionaryWriter;

// SharedDictionaryDataPipeWriter is used to send data in mojo DataPipe to a
// SharedDictionaryWriter.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryDataPipeWriter {
 public:
  // Create a new SharedDictionaryDataPipeWriter. Returns nullptr if creating a
  // new data pipe for output fails. `body` will be replaced with the new data
  // pipe's consumer handle which can be used to read the output data.
  //
  // If the data pipe consumer is aborted, or OnComplete() is called with false:
  //   SharedDictionaryWriter::Finish() will not be called.
  //   And `finish_callback` will be called with true.
  // If successfully read the whole data, and OnComplete() is called with true:
  //   SharedDictionaryWriter::Finish() will be called.
  //   And `finish_callback` will be called with false.
  static std::unique_ptr<SharedDictionaryDataPipeWriter> Create(
      mojo::ScopedDataPipeConsumerHandle& body,
      scoped_refptr<SharedDictionaryWriter> writer,
      base::OnceCallback<void(bool)> finish_callback);
  ~SharedDictionaryDataPipeWriter();

  SharedDictionaryDataPipeWriter(const SharedDictionaryDataPipeWriter&) =
      delete;
  SharedDictionaryDataPipeWriter& operator=(
      const SharedDictionaryDataPipeWriter&) = delete;

  // Signal success status from upstream.
  void OnComplete(bool success);

 private:
  friend class SharedDictionaryDataPipeWriterTest;

  // Returns the size of data pipe for output.
  static uint32_t GetDataPipeBufferSize();

  SharedDictionaryDataPipeWriter(
      mojo::ScopedDataPipeConsumerHandle consumer_handle,
      mojo::ScopedDataPipeProducerHandle producer_handle,
      scoped_refptr<SharedDictionaryWriter> writer,
      base::OnceCallback<void(bool)> finish_callback);

  void ContinueReadWrite(MojoResult, const mojo::HandleSignalsState& state);
  void OnPeerClosed(MojoResult, const mojo::HandleSignalsState& state);

  void FinishDataPipeOperation(bool success);

  void MaybeFinish();

  // The data flow looks like:
  //   `consumer_handle_` --> `this` --> `producer_handle_`
  //                            |
  //                            +-> `writer_`
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  scoped_refptr<SharedDictionaryWriter> writer_;

  mojo::SimpleWatcher consumer_watcher_;
  mojo::SimpleWatcher producer_writable_watcher_;
  mojo::SimpleWatcher producer_closed_watcher_;

  std::optional<bool> completion_result_;
  std::optional<bool> data_pipe_operation_result_;

  base::OnceCallback<void(bool)> finish_callback_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_DATA_PIPE_WRITER_H_
