// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_STRING_DATA_SOURCE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_STRING_DATA_SOURCE_H_

#include <string>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// A class to wrap base::StringPiece as DataPipeProducer::DataSource class.
class MOJO_CPP_SYSTEM_EXPORT StringDataSource final
    : public DataPipeProducer::DataSource {
 public:
  enum class AsyncWritingMode {
    // The |data| given to the constructor may be invalidated before completion
    // |callback| is called. The pending |data| is copied and owned by this
    // class until all bytes are written.
    STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION,
    // The |data| given to the constructor stays valid until this instance is
    // moved to DataPipeProducer and the completion callback is called.
    STRING_STAYS_VALID_UNTIL_COMPLETION
  };

  StringDataSource(base::StringPiece data, AsyncWritingMode mode);
  ~StringDataSource() override;

 private:
  // DataPipeProducer::DataSource:
  uint64_t GetLength() const override;
  ReadResult Read(uint64_t offset, base::span<char> buffer) override;

  std::string data_;
  base::span<const char> data_view_;

  DISALLOW_COPY_AND_ASSIGN(StringDataSource);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_STRING_DATA_SOURCE_H_
