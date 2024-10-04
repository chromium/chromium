// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_
#define CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace content::indexed_db {

void OpenFileAndReadIntoPipe(const base::FilePath& file_path,
                             uint64_t offset,
                             uint64_t read_length,
                             mojo::ScopedDataPipeProducerHandle dest,
                             base::OnceCallback<void(int)> completion_callback);

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_FILE_STREAM_READER_TO_DATA_PIPE_H_
