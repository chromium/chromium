// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/data_pipe.h"

#include <limits>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/test/task_environment.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(DataPipeCppTest, BeginWriteDataGracefullyHandlesBigSizeHint) {
  base::test::TaskEnvironment task_environment;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  base::span<uint8_t> data;
  size_t size_hint = std::numeric_limits<size_t>::max();
  ASSERT_EQ(producer_handle->BeginWriteData(
                size_hint, MOJO_BEGIN_WRITE_DATA_FLAG_NONE, data),
            MOJO_RESULT_OK);
  EXPECT_LE(data.size(), 16u);
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

    base::span<uint8_t> data;
    ASSERT_EQ(
        producer_handle->BeginWriteData(DataPipeProducerHandle::kNoSizeHint,
                                        MOJO_BEGIN_WRITE_DATA_FLAG_NONE, data),
        MOJO_RESULT_OK);
    EXPECT_LE(data.size(), 16u);

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
  std::vector<uint8_t> kData(1024, 0x00);
  base::span<const uint8_t> big_span = UNSAFE_BUFFERS(base::span<const uint8_t>(
      kData.data(),
      std::numeric_limits<size_t>::max()));  // subtle - see above why ok
  size_t bytes_written = 0;
  ASSERT_EQ(producer_handle->WriteData(
                big_span, MOJO_BEGIN_WRITE_DATA_FLAG_NONE, bytes_written),
            MOJO_RESULT_OK);
  EXPECT_EQ(bytes_written, 16u);
}

TEST(DataPipeCppTest, ReadDataGracefullyHandlesBigSize) {
  base::test::TaskEnvironment task_environment;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  const std::string kData = "0123456789";
  size_t bytes_written = 0;
  ASSERT_EQ(producer_handle->WriteData(base::as_byte_span(kData),
                                       MOJO_BEGIN_WRITE_DATA_FLAG_NONE,
                                       bytes_written),
            MOJO_RESULT_OK);
  EXPECT_EQ(bytes_written, 10u);

  // On one hand, we are asking to potentially read more bytes than will fit
  // into `read_buffer` (because we are using a huge `read_bytes` value as
  // input).  OTOH, the `ReadData` API guarantees that mojo will read at most 16
  // bytes (the buffer size of our data pipe).
  //
  // TODO(lukasza): Avoid UB risk by testing with a `std::vector` that can
  // accommodate `std::numeric_limits<uint32_t>::max() + 123` bytes.  (Once our
  // allocator supports such big vectors.)
  std::vector<uint8_t> read_buffer(100);
  base::span<uint8_t> big_span = UNSAFE_BUFFERS(base::span<uint8_t>(
      read_buffer.data(), std::numeric_limits<size_t>::max()));
  size_t actually_read_bytes;
  ASSERT_EQ(consumer_handle->ReadData(MOJO_READ_DATA_FLAG_NONE, big_span,
                                      actually_read_bytes),
            MOJO_RESULT_OK);
  EXPECT_EQ(actually_read_bytes, 10u);
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

    const std::string kData = "0123456789";
    size_t bytes_written = 0;
    ASSERT_EQ(producer_handle->WriteData(base::as_byte_span(kData),
                                         MOJO_BEGIN_WRITE_DATA_FLAG_NONE,
                                         bytes_written),
              MOJO_RESULT_OK);
    EXPECT_EQ(bytes_written, 10u);

    base::span<const uint8_t> read_buffer;
    ASSERT_EQ(
        consumer_handle->BeginReadData(MOJO_READ_DATA_FLAG_NONE, read_buffer),
        MOJO_RESULT_OK);
    EXPECT_EQ(read_buffer.size(), 10u);

    ASSERT_EQ(consumer_handle->EndReadData(big_size),
              MOJO_RESULT_INVALID_ARGUMENT);
  }
}

}  // namespace
}  // namespace mojo
