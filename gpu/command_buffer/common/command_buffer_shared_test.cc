// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file contains the tests for the CommandBufferSharedState class.

#include "gpu/command_buffer/common/command_buffer_shared.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class CommandBufferSharedTest : public testing::Test {
 protected:
  void SetUp() override {
    shared_state_ = std::make_unique<CommandBufferSharedState>();
    shared_state_->Initialize();
  }

  std::unique_ptr<CommandBufferSharedState> shared_state_;
};

TEST_F(CommandBufferSharedTest, TestBasic) {
  CommandBuffer::State state;

  shared_state_->Read(&state);

  EXPECT_LT(state.generation, 0x80000000);
  EXPECT_EQ(state.get_offset, 0);
  EXPECT_EQ(state.token, -1);
  EXPECT_EQ(state.error, gpu::error::kNoError);
  EXPECT_EQ(state.context_lost_reason, gpu::error::kUnknown);
}

static const int kSize = 100000;

void WriteToState(int32_t* buffer, CommandBufferSharedState* shared_state) {
  CommandBuffer::State state;
  for (int i = 0; i < kSize; i++) {
    state.token = i - 1;
    state.get_offset = i + 1;
    state.generation = i + 2;
    state.error =
        static_cast<gpu::error::Error>((i + 3) % (gpu::error::kErrorLast + 1));
    // Ensure that the producer doesn't update the buffer until after the
    // consumer reads from it.
    EXPECT_EQ(buffer[i], 0);

    shared_state->Write(state);
  }
}

TEST_F(CommandBufferSharedTest, TestConsistency) {
  std::unique_ptr<int32_t[]> buffer;
  buffer.reset(new int32_t[kSize]);
  base::Thread consumer("Reader Thread");

  memset(buffer.get(), 0, kSize * sizeof(int32_t));

  consumer.Start();
  consumer.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteToState, buffer.get(), shared_state_.get()));

  CommandBuffer::State last_state;
  while (true) {
    CommandBuffer::State state = last_state;

    shared_state_->Read(&state);

    if (state.generation < last_state.generation)
      continue;

    if (state.get_offset >= 1) {
      buffer[state.get_offset - 1] = 1;
      // Check that the state is consistent
      EXPECT_LE(last_state.token, state.token);
      EXPECT_LE(last_state.generation, state.generation);
      last_state = state;
      EXPECT_EQ(state.token, state.get_offset - 2);
      EXPECT_EQ(state.generation,
                static_cast<unsigned int>(state.get_offset) + 1);
      EXPECT_EQ(state.error,
                (state.get_offset + 2) % (gpu::error::kErrorLast + 1));

      if (state.get_offset == kSize)
        break;
    }
  }
}

}  // namespace gpu
