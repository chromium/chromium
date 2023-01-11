// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_
#define CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/cpp/net_adapters.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// A convenient adapter class to read out data from a FileStreamReader
// and write them into a data pipe.
class FileStreamReaderToDataPipe {
 public:
  // Reads out the data from |reader| and write into |dest|.
  // Can be called from any sequence.
  FileStreamReaderToDataPipe(std::unique_ptr<storage::FileStreamReader> reader,
                             mojo::ScopedDataPipeProducerHandle dest);
  ~FileStreamReaderToDataPipe();

  // Start reading the source.  After this call, FileStreamReaderToDataPipe is
  // now bound to the current sequence and will make internal callbacks (and
  // reader) callbacks on this sequence.
  void Start(base::OnceCallback<void(int)> completion_callback,
             uint64_t read_length);
  int64_t TransferredBytes() const { return transferred_bytes_; }

 private:
  void ReadMore();
  void DidRead(int result);

  void OnDataPipeWritable(MojoResult result);
  void OnDataPipeClosed(MojoResult result);
  void OnComplete(int result);

  std::unique_ptr<storage::FileStreamReader> reader_;
  mojo::ScopedDataPipeProducerHandle dest_;
  base::OnceCallback<void(int)> completion_callback_;
  uint64_t transferred_bytes_ = 0;
  uint64_t read_length_ = 0;

  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  // Optional so that its construction can be deferred.
  absl::optional<mojo::SimpleWatcher> writable_handle_watcher_;

  base::WeakPtrFactory<FileStreamReaderToDataPipe> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_
