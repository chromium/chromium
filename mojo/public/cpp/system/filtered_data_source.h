// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_FILTERED_DATA_SOURCE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_FILTERED_DATA_SOURCE_H_

#include <limits>
#include <memory>

#include "base/containers/span.h"
#include "base/macros.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// A class wraps any other DataPipeProducer::DataSource interface and provides
// the Filter interface to monitor read manipulations.
class MOJO_CPP_SYSTEM_EXPORT FilteredDataSource final
    : public DataPipeProducer::DataSource {
 public:
  class Filter {
   public:
    virtual ~Filter() {}

    // Called once after each |source|'s read attempt. Filter instance can use
    // this interface both to monitor read payloads and to modify read results.
    // |buffer| contains the read buffer. |result->bytes_read| is the actual
    // bytes number that |source| read that should be less than buffer.size().
    // 0 indicates EOF. Both parameters are only valid when |result->error| is
    // MOJO_RESULT_OK.
    // Filter can modify contents of |buffer| and |result|, but it could not
    // expand |buffer|'s size. |result| should be updated to be aligned with
    // modified result and size.
    // Can be called on any sequence.
    virtual void OnRead(base::span<char> buffer, ReadResult* result) = 0;

    // Called when the DataPipeProducer has finished reading all data. Will be
    // called even if there was an error opening the file or reading the data.
    // Can be called on any sequence.
    virtual void OnDone() = 0;
  };

  FilteredDataSource(std::unique_ptr<DataPipeProducer::DataSource> source,
                     std::unique_ptr<Filter> filter);
  ~FilteredDataSource() override;

 private:
  // DataPipeProducer::DataSource:
  uint64_t GetLength() const override;
  ReadResult Read(uint64_t offset, base::span<char> buffer) override;
  void Abort() override;

  std::unique_ptr<DataPipeProducer::DataSource> source_;
  std::unique_ptr<Filter> filter_;

  DISALLOW_COPY_AND_ASSIGN(FilteredDataSource);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_FILTERED_DATA_SOURCE_H_
