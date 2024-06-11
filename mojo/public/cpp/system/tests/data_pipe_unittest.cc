// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/data_pipe.h"

#include <limits>
#include <vector>

#include "base/containers/span.h"
#include "base/test/task_environment.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(DataPipeCppTest, BeginWriteDataGracefullyHandlesBigSize) {
  base::test::TaskEnvironment task_environment;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  void* data = nullptr;
  size_t size = std::numeric_limits<size_t>::max();
  ASSERT_EQ(producer_handle->BeginWriteData(&data, &size,
                                            MOJO_BEGIN_WRITE_DATA_FLAG_NONE),
            MOJO_RESULT_OK);
  EXPECT_LE(size, 16u);
}

TEST(DataPipeCppTest, EndWriteDataErrorWhenSizeTooBig) {
  base::test::TaskEnvironment task_environment;
  const std::array<size_t, 2> kTooBigSizes = {
      // `20` tests C-layer behavior (because 20 fits into `uint32_t`).
      20,
      // This tests C++-layer behavior (which needs to realize that this size
      // won't fit into `uint32_t).
      std::numeric_limits<size_t>::max()};
  for (size_t big_size : kTooBigSizes) {
    ScopedDataPipeProducerHandle producer_handle;
    ScopedDataPipeConsumerHandle consumer_handle;
    ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
              MOJO_RESULT_OK);

    void* data = nullptr;
    size_t size = 0;  // No size hint.
    ASSERT_EQ(producer_handle->BeginWriteData(&data, &size,
                                              MOJO_BEGIN_WRITE_DATA_FLAG_NONE),
              MOJO_RESULT_OK);
    EXPECT_LE(size, 16u);

    // Main test:
    ASSERT_EQ(producer_handle->EndWriteData(big_size),
              MOJO_RESULT_INVALID_ARGUMENT);
  }
}

// Test that `WriteData` continues to work well when `*num_bytes` is greater
// than `std::numeric_limits<uint32.::max()`.
TEST(DataPipeCppTest, WriteDataGracefullyHandlesBigSize) {
  // Important: `CreateDataPipe` overload below sets
  // `MojoCreateDataPipeOptions::element_num_bytes` to 1.  This is important,
  // because it allows `num_bytes` to be any number (it has to be a multiple of
  // `element_num_bytes`).  Without this assumption `WriteData` may return
  // `MOJO_RESULT_INVALID_ARGUMENT` after `saturated_cast<uint32_t>(big_size)`.
  base::test::TaskEnvironment task_environment;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  // This relies on implementation details of mojo to avoid Undefined Behavior.
  // On one hand, we are asking to potentially write more bytes than we have
  // available in the `kData`.  OTOH, we know that mojo will write at most 16
  // bytes (the buffer size of our data pipe) and will not read more bytes
  // than that from `kData`.
  //
  // TODO(lukasza): Avoid UB risk by testing with a `std::vector` that contains
  // `std::numeric_limits<uint32_t>::max() + 123` bytes.  (Once our allocator
  // supports such big vectors.)
  const char kData[] = "1234567890123456";  // 16 bytes
  size_t written_bytes = std::numeric_limits<size_t>::max();
  ASSERT_EQ(producer_handle->WriteData(kData, &written_bytes,
                                       MOJO_BEGIN_WRITE_DATA_FLAG_NONE),
            MOJO_RESULT_OK);
  EXPECT_EQ(written_bytes, 16u);
}

TEST(DataPipeCppTest, ReadDataGracefullyHandlesBigSize) {
  base::test::TaskEnvironment task_environment;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  const char kData[] = "0123456789";
  size_t written_bytes = strlen(kData);
  ASSERT_EQ(producer_handle->WriteData(kData, &written_bytes,
                                       MOJO_BEGIN_WRITE_DATA_FLAG_NONE),
            MOJO_RESULT_OK);
  EXPECT_EQ(written_bytes, 10u);

  // On one hand, we are asking to potentially read more bytes than will fit
  // into `read_buffer` (because we are using a huge `read_bytes` value as
  // input).  OTOH, the `ReadData` API guarantees that mojo will read at most 16
  // bytes (the buffer size of our data pipe).
  //
  // TODO(lukasza): Avoid UB risk by testing with a `std::vector` that can
  // accommodate `std::numeric_limits<uint32_t>::max() + 123` bytes.  (Once our
  // allocator supports such big vectors.)
  std::vector<uint8_t> read_buffer(100);
  size_t read_bytes = std::numeric_limits<size_t>::max();
  ASSERT_EQ(consumer_handle->ReadData(read_buffer.data(), &read_bytes,
                                      MOJO_READ_DATA_FLAG_NONE),
            MOJO_RESULT_OK);
  EXPECT_EQ(read_bytes, 10u);
  EXPECT_EQ(base::as_byte_span(read_buffer).first(10u),
            base::as_byte_span(std::string_view(kData)));
}

TEST(DataPipeCppTest, EndReadDataErrorWhenSizeTooBig) {
  base::test::TaskEnvironment task_environment;
  const std::array<size_t, 2> kTooBigSizes = {
      // `11` tests C-layer behavior (because 11 fits into `uint32_t`).
      // Note that `11` is `strlen(kData) + 1`.
      11,
      // This tests C++-layer behavior (which needs to realize that this size
      // won't fit into `uint32_t).
      std::numeric_limits<size_t>::max()};
  for (size_t big_size : kTooBigSizes) {
    ScopedDataPipeProducerHandle producer_handle;
    ScopedDataPipeConsumerHandle consumer_handle;
    ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
              MOJO_RESULT_OK);

    const char kData[] = "0123456789";
    size_t written_bytes = strlen(kData);
    EXPECT_EQ(written_bytes, 10u);
    ASSERT_EQ(producer_handle->WriteData(kData, &written_bytes,
                                         MOJO_BEGIN_WRITE_DATA_FLAG_NONE),
              MOJO_RESULT_OK);
    EXPECT_EQ(written_bytes, 10u);

    const void* read_buffer = nullptr;
    size_t read_bytes = std::numeric_limits<size_t>::max();
    ASSERT_EQ(consumer_handle->BeginReadData(&read_buffer, &read_bytes,
                                             MOJO_READ_DATA_FLAG_NONE),
              MOJO_RESULT_OK);
    EXPECT_EQ(read_bytes, 10u);

    ASSERT_EQ(consumer_handle->EndReadData(big_size),
              MOJO_RESULT_INVALID_ARGUMENT);
  }
}

}  // namespace
}  // namespace mojo
