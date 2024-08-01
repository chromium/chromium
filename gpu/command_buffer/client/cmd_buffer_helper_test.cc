// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Tests for the Command Buffer Helper.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/client/command_buffer_direct_locked.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::DoAll;
using testing::Invoke;
using testing::_;

const int32_t kTotalNumCommandEntries = 32;
const int32_t kCommandBufferSizeBytes =
    kTotalNumCommandEntries * sizeof(CommandBufferEntry);
const int32_t kUnusedCommandId = 5;  // we use 0 and 2 currently.

// Test fixture for CommandBufferHelper test - Creates a CommandBufferHelper,
// using a CommandBufferServiceLocked with a mock AsyncAPIInterface for its
// interface (calling it directly, not through the RPC mechanism).
class CommandBufferHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    command_buffer_ = std::make_unique<CommandBufferDirectLocked>();
    api_mock_ = std::make_unique<AsyncAPIMock>(true, command_buffer_.get(),
                                               command_buffer_->service());

    // ignore noops in the mock - we don't want to inspect the internals of the
    // helper.
    EXPECT_CALL(*api_mock_, DoCommand(cmd::kNoop, _, _))
        .WillRepeatedly(Return(error::kNoError));

    helper_ = std::make_unique<CommandBufferHelper>(command_buffer_.get());
    helper_->Initialize(kCommandBufferSizeBytes);

    test_command_next_id_ = kUnusedCommandId;
  }

  void TearDown() override {
    // If the CommandExecutor posts any tasks, this forces them to run.
    base::RunLoop().RunUntilIdle();
    test_command_args_.clear();
  }

  int32_t ImmediateEntryCount() const {
    return helper_->immediate_entry_count_;
  }

  // Adds a command to the buffer through the helper, while adding it as an
  // expected call on the API mock.
  void AddCommandWithExpect(error::Error _return,
                            unsigned int command,
                            int arg_count,
                            CommandBufferEntry *args) {
    CommandHeader header;
    header.size = arg_count + 1;
    header.command = command;
    CommandBufferEntry* cmds =
        static_cast<CommandBufferEntry*>(helper_->GetSpace(arg_count + 1));
    CommandBufferOffset put = 0;
    cmds[put++].value_header = header;
    for (int ii = 0; ii < arg_count; ++ii) {
      cmds[put++] = args[ii];
    }

    EXPECT_CALL(*api_mock_, DoCommand(command, arg_count,
        Truly(AsyncAPIMock::IsArgs(arg_count, args))))
        .InSequence(sequence_)
        .WillOnce(Return(_return));
  }

  void AddUniqueCommandWithExpect(error::Error _return, int cmd_size) {
    EXPECT_GE(cmd_size, 1);
    EXPECT_LT(cmd_size, kTotalNumCommandEntries);
    int arg_count = cmd_size - 1;

    // Allocate array for args.
    auto args_ptr =
        std::make_unique<CommandBufferEntry[]>(arg_count ? arg_count : 1);

    for (int32_t ii = 0; ii < arg_count; ++ii) {
      args_ptr[ii].value_uint32 = 0xF00DF00D + ii;
    }

    // Add command and save args in test_command_args_ until the test completes.
    AddCommandWithExpect(
        _return, test_command_next_id_++, arg_count, args_ptr.get());
    test_command_args_.push_back(std::move(args_ptr));
  }

  void TestCommandWrappingFull(int32_t cmd_size, int32_t start_commands) {
    const int32_t num_args = cmd_size - 1;
    EXPECT_EQ(kTotalNumCommandEntries % cmd_size, 0);

    std::vector<CommandBufferEntry> args(num_args);
    for (int32_t ii = 0; ii < num_args; ++ii) {
      args[ii].value_uint32 = ii + 1;
    }

    // Initially insert commands up to start_commands and Finish().
    for (int32_t ii = 0; ii < start_commands; ++ii) {
      AddCommandWithExpect(
          error::kNoError, ii + kUnusedCommandId, num_args, &args[0]);
    }
    helper_->Finish();

    EXPECT_EQ(GetPutOffset(),
              (start_commands * cmd_size) % kTotalNumCommandEntries);
    EXPECT_EQ(GetGetOffset(),
              (start_commands * cmd_size) % kTotalNumCommandEntries);

    // Lock flushing to force the buffer to get full.
    command_buffer_->LockFlush();

    // Add enough commands to over fill the buffer.
    for (int32_t ii = 0; ii < kTotalNumCommandEntries / cmd_size + 2; ++ii) {
      AddCommandWithExpect(error::kNoError,
                           start_commands + ii + kUnusedCommandId,
                           num_args,
                           &args[0]);
    }

    // Flush all commands.
    command_buffer_->UnlockFlush();
    helper_->Finish();

    // Check that the commands did happen.
    Mock::VerifyAndClearExpectations(api_mock_.get());

    // Check the error status.
    EXPECT_EQ(error::kNoError, GetError());
  }

  // Checks that the buffer from put to put+size is free in the parser.
  void CheckFreeSpace(CommandBufferOffset put, unsigned int size) {
    CommandBufferOffset parser_put = GetPutOffset();
    CommandBufferOffset parser_get = GetGetOffset();
    CommandBufferOffset limit = put + size;
    if (parser_get > parser_put) {
      // "busy" buffer wraps, so "free" buffer is between put (inclusive) and
      // get (exclusive).
      EXPECT_LE(parser_put, put);
      EXPECT_GT(parser_get, limit);
    } else {
      // "busy" buffer does not wrap, so the "free" buffer is the top side (from
      // put to the limit) and the bottom side (from 0 to get).
      if (put >= parser_put) {
        // we're on the top side, check we are below the limit.
        EXPECT_GE(kTotalNumCommandEntries, limit);
      } else {
        // we're on the bottom side, check we are below get.
        EXPECT_GT(parser_get, limit);
      }
    }
  }

  int32_t GetGetOffset() {
    return command_buffer_->service()->GetState().get_offset;
  }

  int32_t GetPutOffset() { return command_buffer_->GetServicePutOffset(); }

  int32_t GetHelperGetOffset() { return helper_->cached_get_offset_; }

  int32_t GetHelperPutOffset() { return helper_->put_; }

  uint32_t GetHelperFlushGeneration() { return helper_->flush_generation(); }

  error::Error GetError() {
    return command_buffer_->GetLastState().error;
  }

  CommandBufferOffset get_helper_put() { return helper_->put_; }

  void WaitForGetOffsetInRange(int32_t start, int32_t end) {
    helper_->WaitForGetOffsetInRange(start, end);
  }

  std::unique_ptr<CommandBufferDirectLocked> command_buffer_;
  std::unique_ptr<AsyncAPIMock> api_mock_;
  std::unique_ptr<CommandBufferHelper> helper_;
  std::vector<std::unique_ptr<CommandBufferEntry[]>> test_command_args_;
  unsigned int test_command_next_id_;
  Sequence sequence_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Checks immediate_entry_count_ changes based on RingBuffer state.
TEST_F(CommandBufferHelperTest, TestCalcImmediateEntriesNoRingBuffer) {
  helper_->SetAutomaticFlushes(false);
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries - 1);
  helper_->FreeRingBuffer();
  EXPECT_TRUE(helper_->usable());
  EXPECT_EQ(ImmediateEntryCount(), 0);
  command_buffer_->set_fail_create_transfer_buffer(true);
  helper_->WaitForAvailableEntries(1);
  EXPECT_FALSE(helper_->usable());
  EXPECT_EQ(ImmediateEntryCount(), 0);
}

// Checks immediate_entry_count_ calc when Put >= Get and Get == 0.
TEST_F(CommandBufferHelperTest, TestCalcImmediateEntriesGetAtZero) {
  // No internal auto flushing.
  helper_->SetAutomaticFlushes(false);
  command_buffer_->LockFlush();

  // Start at Get = Put = 0.
  EXPECT_EQ(GetHelperPutOffset(), 0);
  EXPECT_EQ(GetHelperGetOffset(), 0);

  // Immediate count should be 1 less than the end of the buffer.
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries - 1);
  AddUniqueCommandWithExpect(error::kNoError, 2);
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries - 3);

  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks immediate_entry_count_ calc when Put >= Get and Get > 0.
TEST_F(CommandBufferHelperTest, TestCalcImmediateEntriesGetInMiddle) {
  // No internal auto flushing.
  helper_->SetAutomaticFlushes(false);
  command_buffer_->LockFlush();

  // Move to Get = Put = 2.
  AddUniqueCommandWithExpect(error::kNoError, 2);
  helper_->Finish();
  EXPECT_EQ(GetHelperPutOffset(), 2);
  EXPECT_EQ(GetHelperGetOffset(), 2);

  // Immediate count should be up to the end of the buffer.
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries - 2);
  AddUniqueCommandWithExpect(error::kNoError, 2);
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries - 4);

  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks immediate_entry_count_ calc when Put < Get.
TEST_F(CommandBufferHelperTest, TestCalcImmediateEntriesGetBeforePut) {
  // Move to Get = kTotalNumCommandEntries / 4, Put = 0.
  const int kInitGetOffset = kTotalNumCommandEntries / 4;
  helper_->SetAutomaticFlushes(false);
  command_buffer_->LockFlush();
  AddUniqueCommandWithExpect(error::kNoError, kInitGetOffset);
  helper_->Finish();
  AddUniqueCommandWithExpect(error::kNoError,
                             kTotalNumCommandEntries - kInitGetOffset);

  // Flush instead of Finish will let Put wrap without the command buffer
  // immediately processing the data between Get and Put.
  helper_->Flush();

  EXPECT_EQ(GetHelperGetOffset(), kInitGetOffset);
  EXPECT_EQ(GetHelperPutOffset(), 0);

  // Immediate count should be up to Get - 1.
  EXPECT_EQ(ImmediateEntryCount(), kInitGetOffset - 1);
  AddUniqueCommandWithExpect(error::kNoError, 2);
  EXPECT_EQ(ImmediateEntryCount(), kInitGetOffset - 3);

  helper_->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks immediate_entry_count_ calc when automatic flushing is enabled.
TEST_F(CommandBufferHelperTest, TestCalcImmediateEntriesAutoFlushing) {
  command_buffer_->LockFlush();

  // Start at Get = Put = 0.
  EXPECT_EQ(GetHelperPutOffset(), 0);
  EXPECT_EQ(GetHelperGetOffset(), 0);

  // Without auto flushes, up to kTotalNumCommandEntries - 1 is available.
  helper_->SetAutomaticFlushes(false);
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries - 1);

  // With auto flushes, and Get == Last Put,
  // up to kTotalNumCommandEntries / kAutoFlushSmall is available.
  helper_->SetAutomaticFlushes(true);
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries / kAutoFlushSmall);

  // With auto flushes, and Get != Last Put,
  // up to kTotalNumCommandEntries / kAutoFlushBig is available.
  AddUniqueCommandWithExpect(error::kNoError, 2);
  helper_->Flush();
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries / kAutoFlushBig);

  helper_->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks that automatic flushing treats ordering barriers as flushes.
TEST_F(CommandBufferHelperTest,
       TestCalcImmediateEntriesAutoFlushingOrderingBarrier) {
  // Check that auto flush happens without an ordering barrier.
  AddUniqueCommandWithExpect(error::kNoError, kAutoFlushSmall - 1);
  EXPECT_EQ(0, command_buffer_->FlushCount());
  AddUniqueCommandWithExpect(error::kNoError, 1);
  // Auto flush should be triggered by going past the threshold.
  EXPECT_EQ(1, command_buffer_->FlushCount());
  helper_->Finish();
  EXPECT_EQ(2, command_buffer_->FlushCount());

  // Check that an ordering barrier prevents auto flush.
  AddUniqueCommandWithExpect(error::kNoError, kAutoFlushSmall - 1);
  EXPECT_EQ(2, command_buffer_->FlushCount());
  helper_->OrderingBarrier();
  EXPECT_EQ(3, command_buffer_->FlushCount());
  AddUniqueCommandWithExpect(error::kNoError, 1);
  // Adding a command should not have caused a flush because there was an
  // ordering barrier.
  EXPECT_EQ(3, command_buffer_->FlushCount());

  // Check that the commands did happen.
  helper_->Finish();
  Mock::VerifyAndClearExpectations(api_mock_.get());
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks immediate_entry_count_ calc when automatic flushing is enabled, and
// we allocate commands over the immediate_entry_count_ size.
TEST_F(CommandBufferHelperTest, TestCalcImmediateEntriesOverFlushLimit) {
  // Lock internal flushing.
  command_buffer_->LockFlush();

  // Start at Get = Put = 0.
  EXPECT_EQ(GetHelperPutOffset(), 0);
  EXPECT_EQ(GetHelperGetOffset(), 0);

  // Pre-check ImmediateEntryCount is limited with automatic flushing enabled.
  helper_->SetAutomaticFlushes(true);
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries / kAutoFlushSmall);

  // Add a command larger than ImmediateEntryCount().
  AddUniqueCommandWithExpect(error::kNoError, ImmediateEntryCount() + 1);

  // ImmediateEntryCount() should now be 0, to force a flush check on the next
  // command.
  EXPECT_EQ(ImmediateEntryCount(), 0);

  // Add a command when ImmediateEntryCount() == 0.
  AddUniqueCommandWithExpect(error::kNoError, ImmediateEntryCount() + 1);

  helper_->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks that commands in the buffer are properly executed, and that the
// status/error stay valid.
TEST_F(CommandBufferHelperTest, TestCommandProcessing) {
  // Check initial state of the engine - it should have been configured by the
  // helper.
  EXPECT_EQ(error::kNoError, GetError());
  EXPECT_EQ(0, GetGetOffset());

  // Add 3 commands through the helper
  AddCommandWithExpect(error::kNoError, kUnusedCommandId, 0, nullptr);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  AddCommandWithExpect(error::kNoError, kUnusedCommandId, 2, args1);

  CommandBufferEntry args2[2];
  args2[0].value_uint32 = 5;
  args2[1].value_float = 6.f;
  AddCommandWithExpect(error::kNoError, kUnusedCommandId, 2, args2);

  // Wait until it's done.
  helper_->Finish();
  // Check that the engine has no more work to do.
  EXPECT_EQ(GetGetOffset(), GetPutOffset());

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks that commands in the buffer are properly executed when wrapping the
// buffer, and that the status/error stay valid.
TEST_F(CommandBufferHelperTest, TestCommandWrapping) {
  // Add num_commands * commands of size 3 through the helper to make sure we
  // do wrap.  kTotalNumCommandEntries must not be a multiple of 3.
  static_assert(kTotalNumCommandEntries % 3 != 0,
                "kTotalNumCommandEntries must not be a multiple of 3");
  const int kNumCommands = (kTotalNumCommandEntries / 3) * 2;
  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 5;
  args1[1].value_float = 4.f;

  for (int i = 0; i < kNumCommands; ++i) {
    AddCommandWithExpect(error::kNoError, kUnusedCommandId + i, 2, args1);
  }

  helper_->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks the case where the command inserted exactly matches the space left in
// the command buffer.
TEST_F(CommandBufferHelperTest, TestCommandWrappingExactMultiple) {
  const int32_t kCommandSize = kTotalNumCommandEntries / 2;
  const size_t kNumArgs = kCommandSize - 1;
  static_assert(kTotalNumCommandEntries % kCommandSize == 0,
                "kTotalNumCommandEntries should be a multiple of kCommandSize");
  CommandBufferEntry args1[kNumArgs];
  for (size_t ii = 0; ii < kNumArgs; ++ii) {
    args1[ii].value_uint32 = ii + 1;
  }

  for (unsigned int i = 0; i < 5; ++i) {
    AddCommandWithExpect(
        error::kNoError, i + kUnusedCommandId, kNumArgs, args1);
  }

  helper_->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks exact wrapping condition with Get = 0.
TEST_F(CommandBufferHelperTest, TestCommandWrappingFullAtStart) {
  TestCommandWrappingFull(2, 0);
}

// Checks exact wrapping condition with 0 < Get < kTotalNumCommandEntries.
TEST_F(CommandBufferHelperTest, TestCommandWrappingFullInMiddle) {
  TestCommandWrappingFull(2, 1);
}

// Checks exact wrapping condition with Get = kTotalNumCommandEntries.
// Get should wrap back to 0, but making sure.
TEST_F(CommandBufferHelperTest, TestCommandWrappingFullAtEnd) {
  TestCommandWrappingFull(2, kTotalNumCommandEntries / 2);
}

// Checks that asking for available entries work, and that the parser
// effectively won't use that space.
TEST_F(CommandBufferHelperTest, TestAvailableEntries) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Add 2 commands through the helper - 8 entries
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 1, 0, nullptr);
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 2, 0, nullptr);
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 3, 2, args);
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 4, 2, args);

  // Ask for 5 entries.
  helper_->WaitForAvailableEntries(5);

  CommandBufferOffset put = get_helper_put();
  CheckFreeSpace(put, 5);

  // Add more commands.
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 5, 2, args);

  // Wait until everything is done done.
  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks that the InsertToken/WaitForToken work.
TEST_F(CommandBufferHelperTest, TestToken) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Add a first command.
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 3, 2, args);
  // keep track of the buffer position.
  CommandBufferOffset command1_put = get_helper_put();
  int32_t token = helper_->InsertToken();

  EXPECT_CALL(*api_mock_.get(), DoCommand(cmd::kSetToken, 1, _))
      .WillOnce(DoAll(Invoke(api_mock_.get(), &AsyncAPIMock::SetToken),
                      Return(error::kNoError)));
  // Add another command.
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 4, 2, args);
  helper_->WaitForToken(token);
  // check that the get pointer is beyond the first command.
  EXPECT_LE(command1_put, GetGetOffset());
  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks WaitForToken doesn't Flush if token is already read.
TEST_F(CommandBufferHelperTest, TestWaitForTokenFlush) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Add a first command.
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 3, 2, args);
  int32_t token = helper_->InsertToken();

  EXPECT_CALL(*api_mock_.get(), DoCommand(cmd::kSetToken, 1, _))
      .WillOnce(DoAll(Invoke(api_mock_.get(), &AsyncAPIMock::SetToken),
                      Return(error::kNoError)));

  int flush_count = command_buffer_->FlushCount();

  // Test that waiting for pending token causes a Flush.
  helper_->WaitForToken(token);
  EXPECT_EQ(command_buffer_->FlushCount(), flush_count + 1);

  // Test that we don't Flush repeatedly.
  helper_->WaitForToken(token);
  EXPECT_EQ(command_buffer_->FlushCount(), flush_count + 1);

  // Add another command.
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 4, 2, args);

  // Test that we don't Flush repeatedly even if commands are pending.
  helper_->WaitForToken(token);
  EXPECT_EQ(command_buffer_->FlushCount(), flush_count + 1);

  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

TEST_F(CommandBufferHelperTest, FreeRingBuffer) {
  EXPECT_TRUE(helper_->HaveRingBuffer());

  // Test freeing ring buffer.
  helper_->FreeRingBuffer();
  EXPECT_FALSE(helper_->HaveRingBuffer());

  // Test that InsertToken allocates a new one
  int32_t token = helper_->InsertToken();
  EXPECT_TRUE(helper_->HaveRingBuffer());
  EXPECT_CALL(*api_mock_.get(), DoCommand(cmd::kSetToken, 1, _))
      .WillOnce(DoAll(Invoke(api_mock_.get(), &AsyncAPIMock::SetToken),
                      Return(error::kNoError)));
  helper_->WaitForToken(token);
  helper_->FreeRingBuffer();
  EXPECT_FALSE(helper_->HaveRingBuffer());

  // Test that WaitForAvailableEntries allocates a new one
  AddCommandWithExpect(error::kNoError, kUnusedCommandId, 0, nullptr);
  EXPECT_TRUE(helper_->HaveRingBuffer());
  helper_->Finish();
  helper_->FreeRingBuffer();
  EXPECT_FALSE(helper_->HaveRingBuffer());

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Test that FreeRingBuffer doesn't force a finish
  AddCommandWithExpect(error::kNoError, kUnusedCommandId, 0, nullptr);
  EXPECT_TRUE(helper_->HaveRingBuffer());
  int32_t old_get_offset = command_buffer_->GetLastState().get_offset;
  EXPECT_NE(helper_->GetPutOffsetForTest(), old_get_offset);
  int old_flush_count = command_buffer_->FlushCount();

  helper_->FreeRingBuffer();
  EXPECT_FALSE(helper_->HaveRingBuffer());
  // FreeRingBuffer should have caused an ordering barrier and a flush.
  EXPECT_EQ(command_buffer_->FlushCount(), old_flush_count + 2);
  // However it shouldn't force a finish.
  EXPECT_EQ(command_buffer_->GetLastState().get_offset, old_get_offset);

  // Finish should not cause extra flushes, or recreate the ring buffer, but it
  // should work.
  helper_->Finish();
  EXPECT_FALSE(helper_->HaveRingBuffer());
  EXPECT_EQ(command_buffer_->FlushCount(), old_flush_count + 2);
  EXPECT_EQ(command_buffer_->GetLastState().get_offset,
            helper_->GetPutOffsetForTest());
}

TEST_F(CommandBufferHelperTest, Noop) {
  for (int ii = 1; ii < 4; ++ii) {
    CommandBufferOffset put_before = get_helper_put();
    helper_->Noop(ii);
    CommandBufferOffset put_after = get_helper_put();
    EXPECT_EQ(ii, put_after - put_before);
  }
}

TEST_F(CommandBufferHelperTest, IsContextLost) {
  EXPECT_FALSE(helper_->IsContextLost());
  command_buffer_->service()->SetParseError(error::kGenericError);
  EXPECT_TRUE(helper_->IsContextLost());
}

// Checks helper's 'flush generation' updates.
TEST_F(CommandBufferHelperTest, TestFlushGeneration) {
  // Explicit flushing only.
  helper_->SetAutomaticFlushes(false);

  // Generation should change after Flush() but not before.
  uint32_t gen1, gen2, gen3;

  gen1 = GetHelperFlushGeneration();
  AddUniqueCommandWithExpect(error::kNoError, 2);
  gen2 = GetHelperFlushGeneration();
  helper_->Flush();
  gen3 = GetHelperFlushGeneration();
  EXPECT_EQ(gen2, gen1);
  EXPECT_NE(gen3, gen2);

  // Generation should change after Finish() but not before.
  gen1 = GetHelperFlushGeneration();
  AddUniqueCommandWithExpect(error::kNoError, 2);
  gen2 = GetHelperFlushGeneration();
  helper_->Finish();
  gen3 = GetHelperFlushGeneration();
  EXPECT_EQ(gen2, gen1);
  EXPECT_NE(gen3, gen2);

  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

TEST_F(CommandBufferHelperTest, TestOrderingBarrierFlushGeneration) {
  // Explicit flushing only.
  helper_->SetAutomaticFlushes(false);

  // Generation should change after OrderingBarrier() but not before.
  uint32_t gen1, gen2, gen3;

  gen1 = GetHelperFlushGeneration();
  AddUniqueCommandWithExpect(error::kNoError, 2);
  gen2 = GetHelperFlushGeneration();
  helper_->OrderingBarrier();
  gen3 = GetHelperFlushGeneration();
  EXPECT_EQ(gen2, gen1);
  EXPECT_NE(gen3, gen2);

  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Expect Flush() to always call CommandBuffer::Flush().
TEST_F(CommandBufferHelperTest, TestFlushToCommandBuffer) {
  // Explicit flushing only.
  helper_->SetAutomaticFlushes(false);

  int flush_count1, flush_count2, flush_count3;

  flush_count1 = command_buffer_->FlushCount();
  AddUniqueCommandWithExpect(error::kNoError, 2);
  helper_->Flush();
  flush_count2 = command_buffer_->FlushCount();
  helper_->Flush();
  flush_count3 = command_buffer_->FlushCount();

  EXPECT_EQ(flush_count2, flush_count1 + 1);
  EXPECT_EQ(flush_count3, flush_count2 + 1);
}

// Expect OrderingBarrier() to always call CommandBuffer::OrderingBarrier().
TEST_F(CommandBufferHelperTest, TestOrderingBarrierToCommandBuffer) {
  // Explicit flushing only.
  helper_->SetAutomaticFlushes(false);

  int flush_count1, flush_count2, flush_count3;

  flush_count1 = command_buffer_->FlushCount();
  AddUniqueCommandWithExpect(error::kNoError, 2);
  helper_->OrderingBarrier();
  flush_count2 = command_buffer_->FlushCount();
  helper_->OrderingBarrier();
  flush_count3 = command_buffer_->FlushCount();

  EXPECT_EQ(flush_count2, flush_count1 + 1);
  EXPECT_EQ(flush_count3, flush_count2 + 1);
}

TEST_F(CommandBufferHelperTest, TestWrapAroundAfterOrderingBarrier) {
  // Explicit flushing only.
  helper_->SetAutomaticFlushes(false);

  // Flush with put = 3. We will wrap around to flush this exact offset again.
  AddUniqueCommandWithExpect(error::kNoError, 3);
  helper_->Flush();

  // Add an ordering barrier that's never explicitly flushed by us.
  AddUniqueCommandWithExpect(error::kNoError, 2);
  helper_->OrderingBarrier();

  WaitForGetOffsetInRange(5, 5);

  // Add enough commands to wrap around to put = 2. Add commands of size 2 so
  // that last command inserts nop at the end and avoids an automatic flush.
  ASSERT_EQ(kTotalNumCommandEntries % 2, 0);
  for (int i = 0; i < kTotalNumCommandEntries / 2 - 2; ++i)
    AddUniqueCommandWithExpect(error::kNoError, 2);

  // We have 2 entries available. Asking for 1 entry will update put offset to
  // 3 which is equal to the put offset of the last explicit flush.
  AddUniqueCommandWithExpect(error::kNoError, 1);
  EXPECT_EQ(GetHelperPutOffset(), 3);
  EXPECT_EQ(GetHelperGetOffset(), 5);
  EXPECT_EQ(ImmediateEntryCount(), 1);

  // We have 1 entry available. Asking for 2 entries will automatically call
  // Flush and WaitForGetOffsetInRange.
  AddUniqueCommandWithExpect(error::kNoError, 2);
  EXPECT_EQ(GetHelperPutOffset(), 5);
  EXPECT_EQ(GetHelperGetOffset(), 3);
  EXPECT_EQ(ImmediateEntryCount(), kTotalNumCommandEntries - 5);

  // Flush the last command explicitly.
  helper_->Flush();

  Mock::VerifyAndClearExpectations(api_mock_.get());
}

}  // namespace gpu
