// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/buffering_data_pipe_writer.h"

#include <memory>
#include <random>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {
namespace {

TEST(BufferingDataPipeWriterTest, WriteMany) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  constexpr int kCapacity = 4096;

  std::minstd_rand engine(99);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = kCapacity;

  MojoResult result = mojo::CreateDataPipe(&options, &producer, &consumer);
  ASSERT_EQ(MOJO_RESULT_OK, result);

  constexpr size_t total = kCapacity * 3;
  constexpr size_t writing_chunk_size = 5;
  constexpr size_t reading_chunk_size = 7;
  Vector<char> input, output;

  for (size_t i = 0; i < total; ++i)
    input.push_back(static_cast<char>(engine() % 26 + 'A'));

  auto writer = std::make_unique<BufferingDataPipeWriter>(
      std::move(producer), platform->test_task_runner().get());

  for (size_t i = 0; i < total;) {
    // We use a temporary buffer to check that the buffer is copied immediately.
    char temp[writing_chunk_size] = {};
    size_t size = std::min(total - i, writing_chunk_size);

    std::copy(input.data() + i, input.data() + i + size, temp);
    ASSERT_TRUE(writer->Write(temp, size));

    i += size;
  }

  writer->Finish();

  while (true) {
    constexpr auto kNone = MOJO_READ_DATA_FLAG_NONE;
    char buffer[reading_chunk_size] = {};
    uint32_t size = reading_chunk_size;
    result = consumer->ReadData(buffer, &size, kNone);

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      platform->RunUntilIdle();
      continue;
    }
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;

    ASSERT_EQ(MOJO_RESULT_OK, result);
    output.Append(buffer, size);
  }
  EXPECT_EQ(output, input);
}

}  // namespace
}  // namespace blink
