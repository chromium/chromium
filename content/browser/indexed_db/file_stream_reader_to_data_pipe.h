// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_
#define CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_

#include <stdint.h>

#include <optional>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"

namespace network {
class NetToMojoPendingBuffer;
}

namespace content::indexed_db {

// Reads a file into a data pipe.
class CONTENT_EXPORT FileStreamReaderToDataPipe {
 public:
  using TransferCompletionCallback =
      base::OnceCallback<void(net::Error /*result*/,
                              uint64_t /*transferred_bytes*/)>;

  FileStreamReaderToDataPipe(const base::FilePath& file_path,
                             uint64_t offset,
                             uint64_t read_length,
                             mojo::ScopedDataPipeProducerHandle dest);
  ~FileStreamReaderToDataPipe();

  // May complete synchronously. The callback may destroy this object.
  void Start(TransferCompletionCallback completion_callback);

 private:
  void ReadMore();

  void OnDataPipeSignaled(MojoResult result,
                          const mojo::HandleSignalsState& state);
  void OnComplete(net::Error result);

  base::File file_;
  mojo::ScopedDataPipeProducerHandle dest_;

  // Called when the read completes, successfully or not, or when `this` is
  // deleted before the read completes.
  TransferCompletionCallback completion_callback_;

  uint64_t transferred_bytes_ = 0;
  const uint64_t offset_;
  const uint64_t read_length_;

  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  // Optional so that its construction can be deferred.
  std::optional<mojo::SimpleWatcher> writable_handle_watcher_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_
