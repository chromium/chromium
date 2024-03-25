// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_
#define CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_

#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace storage {
class FileStreamReader;
}

namespace content {

// A convenience function to read out data from a FileStreamReader
// and write them into a data pipe. Must be called on an IO thread.
void MakeFileStreamAdapterAndRead(
    std::unique_ptr<storage::FileStreamReader> reader,
    mojo::ScopedDataPipeProducerHandle dest,
    base::OnceCallback<void(int)> completion_callback,
    uint64_t read_length);

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_
