// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/test/mock_bytes_provider.h"

#include "base/threading/thread_restrictions.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

MockBytesProvider::MockBytesProvider(
    std::vector<uint8_t> data,
    size_t* reply_request_count,
    size_t* stream_request_count,
    size_t* file_request_count,
    std::optional<base::Time> file_modification_time)
    : data_(std::move(data)),
      reply_request_count_(reply_request_count),
      stream_request_count_(stream_request_count),
      file_request_count_(file_request_count),
      file_modification_time_(file_modification_time) {}

MockBytesProvider::~MockBytesProvider() = default;

void MockBytesProvider::RequestAsReply(RequestAsReplyCallback callback) {
  if (reply_request_count_)
    ++*reply_request_count_;
  std::move(callback).Run(data_);
}

void MockBytesProvider::RequestAsStream(
    mojo::ScopedDataPipeProducerHandle pipe) {
  if (stream_request_count_)
    ++*stream_request_count_;
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
  mojo::BlockingCopyFromString(
      std::string(reinterpret_cast<const char*>(data_.data()), data_.size()),
      pipe);
}

void MockBytesProvider::RequestAsFile(uint64_t source_offset,
                                      uint64_t source_size,
                                      base::File file,
                                      uint64_t file_offset,
                                      RequestAsFileCallback callback) {
  if (file_request_count_)
    ++*file_request_count_;
  EXPECT_LE(source_offset + source_size, data_.size());
  EXPECT_EQ(source_size,
            static_cast<uint64_t>(file.Write(
                file_offset,
                reinterpret_cast<const char*>(data_.data() + source_offset),
                source_size)));
  EXPECT_TRUE(file.Flush());
  std::move(callback).Run(file_modification_time_);
}

}  // namespace storage
