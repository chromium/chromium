// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_FILE_DATA_SOURCE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_FILE_DATA_SOURCE_H_

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// A class to wrap base::File as DataPipeProducer::DataSource class. Reads at
// most |max_bytes| bytes from the file.
class MOJO_CPP_SYSTEM_EXPORT FileDataSource final
    : public DataPipeProducer::DataSource {
 public:
  static MojoResult ConvertFileErrorToMojoResult(base::File::Error error);

  FileDataSource(base::File file);
  ~FileDataSource() override;

  // |end| should be greater than or equal to |start|. Otherwise subsequent
  // Read() fails with MOJO_RESULT_INVALID_ARGUMENT. [start, end) will be read.
  void SetRange(uint64_t start, uint64_t end);

 private:
  // DataPipeProducer::DataSource:
  uint64_t GetLength() const override;
  ReadResult Read(uint64_t offset, base::span<char> buffer) override;

  base::File file_;
  MojoResult error_;
  uint64_t start_offset_;
  uint64_t end_offset_;

  DISALLOW_COPY_AND_ASSIGN(FileDataSource);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_FILE_DATA_SOURCE_H_
