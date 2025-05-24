// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_FILE_STREAM_DATA_SOURCE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_FILE_STREAM_DATA_SOURCE_H_

#include "base/containers/span.h"
#include "base/files/file.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// A class to wrap base::File as DataPipeProducer::DataSource class. This source
// does not call seek(). Read() must always call with `offset` at the current
// position.
class MOJO_CPP_SYSTEM_EXPORT FileStreamDataSource final
    : public DataPipeProducer::DataSource {
 public:
  FileStreamDataSource(base::File file, int64_t length);

  FileStreamDataSource(const FileStreamDataSource&) = delete;
  FileStreamDataSource& operator=(const FileStreamDataSource&) = delete;

  ~FileStreamDataSource() override;

  // DataPipeProducer::DataSource:
  uint64_t GetLength() const override;
  ReadResult Read(uint64_t offset, base::span<char> buffer) override;

 private:
  base::File file_;
  const int64_t length_;
  uint64_t current_offset_ = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_FILE_STREAM_DATA_SOURCE_H_
