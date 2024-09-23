// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Tests for the command parser.

#include <stddef.h>

#include <memory>

#include "base/check_op.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

using testing::_;
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::Sequence;
using testing::SetArgPointee;

// Test fixture for CommandBufferService test - Creates a mock
// AsyncAPIInterface, and a fixed size memory buffer. Also provides a simple API
// to create a CommandBufferService.
class CommandBufferServiceTest : public testing::Test,
                                 public CommandBufferServiceClient {
 public:
  MOCK_METHOD0(OnCommandProcessed, void());

 protected:
  void AddDoCommandsExpect(error::Error _return,
                           int num_entries,
                           int num_processed) {
    EXPECT_CALL(*api_mock_, DoCommands(_, _, num_entries, _))
        .InSequence(sequence_)
        .WillOnce(DoAll(SetArgPointee<3>(num_processed), Return(_return)));
  }

  // Creates a CommandBufferService, with a buffer of the specified size (in
  // entries).
  void MakeService(unsigned int entry_count) {
    command_buffer_service_ =
        std::make_unique<CommandBufferService>(this, nullptr);
    api_mock_ = std::make_unique<AsyncAPIMock>(false, nullptr,
                                               command_buffer_service_.get());
    SetNewGetBuffer(entry_count * sizeof(CommandBufferEntry));
  }

  AsyncAPIMock* api_mock() { return api_mock_.get(); }
  CommandBufferEntry* buffer() {
    return static_cast<CommandBufferEntry*>(buffer_->memory());
  }

  CommandBufferService* command_buffer_service() {
    return command_buffer_service_.get();
  }
  int32_t GetGet() { return command_buffer_service_->GetState().get_offset; }
  int32_t GetPut() { return command_buffer_service_->put_offset(); }

  error::Error SetPutAndProcessAllCommands(int32_t put) {
    command_buffer_service_->Flush(put, api_mock());
    EXPECT_EQ(put, GetPut());
    return command_buffer_service_->GetState().error;
  }

  int32_t SetNewGetBuffer(size_t size) {
    int32_t id = 0;
    buffer_ = command_buffer_service_->CreateTransferBuffer(size, &id);
    command_buffer_service_->SetGetBuffer(id);
    return id;
  }

  void AdvancePut(int32_t entries) {
    DCHECK(entries > 0);
    CommandBufferOffset put = GetPut();
    CommandHeader header;
    header.size = entries;
    header.command = 1;
    buffer()[put].value_header = header;
    put += entries;
    AddDoCommandsExpect(error::kNoError, entries, entries);
    EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
    EXPECT_EQ(put, GetPut());
    Mock::VerifyAndClearExpectations(api_mock());
  }

  // CommandBufferServiceBase implementation:
  CommandBatchProcessedResult OnCommandBatchProcessed() override {
    return kContinueExecution;
  }
  MOCK_METHOD0(OnParseError, void());

 private:
  std::unique_ptr<CommandBufferService> command_buffer_service_;
  std::unique_ptr<AsyncAPIMock> api_mock_;
  scoped_refptr<Buffer> buffer_;
  Sequence sequence_;
};

// Tests initialization conditions.
TEST_F(CommandBufferServiceTest, TestInit) {
  MakeService(10);
  CommandBuffer::State state = command_buffer_service()->GetState();
  EXPECT_EQ(0, GetGet());
  EXPECT_EQ(0, GetPut());
  EXPECT_EQ(0, state.token);
  EXPECT_EQ(error::kNoError, state.error);
}

TEST_F(CommandBufferServiceTest, TestEmpty) {
  MakeService(10);
  EXPECT_CALL(*api_mock(), DoCommands(_, _, _, _)).Times(0);

  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(0));
  EXPECT_EQ(0, GetGet());
}

// Tests simple commands.
TEST_F(CommandBufferServiceTest, TestSimple) {
  MakeService(10);
  CommandBufferOffset put = GetPut();
  CommandHeader header;

  // add a single command, no args
  header.size = 1;
  header.command = 123;
  buffer()[put++].value_header = header;

  AddDoCommandsExpect(error::kNoError, 1, 1);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());

  // add a single command, 2 args
  header.size = 3;
  header.command = 456;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 2134;
  buffer()[put++].value_float = 1.f;

  AddDoCommandsExpect(error::kNoError, 3, 3);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests having multiple commands in the buffer.
TEST_F(CommandBufferServiceTest, TestMultipleCommands) {
  MakeService(10);
  CommandBufferOffset put = GetPut();
  CommandHeader header;

  // add 2 commands, test with single ProcessAllCommands()
  header.size = 2;
  header.command = 789;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5151;

  header.size = 2;
  header.command = 876;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 3434;

  // Process commands.  4 entries remaining.
  AddDoCommandsExpect(error::kNoError, 4, 4);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 2 commands again, test with ProcessAllCommands()
  header.size = 2;
  header.command = 123;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5656;

  header.size = 2;
  header.command = 321;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 7878;

  // 4 entries remaining.
  AddDoCommandsExpect(error::kNoError, 4, 4);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests that the parser will wrap correctly at the end of the buffer.
TEST_F(CommandBufferServiceTest, TestWrap) {
  MakeService(5);
  CommandBufferOffset put = GetPut();
  CommandHeader header;

  // add 3 commands with no args (1 word each)
  for (unsigned int i = 0; i < 3; ++i) {
    header.size = 1;
    header.command = i;
    buffer()[put++].value_header = header;
  }

  // Process up to 10 commands.  3 entries remaining to put.
  AddDoCommandsExpect(error::kNoError, 3, 3);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 1 command with 1 arg (2 words). That should put us at the end of the
  // buffer.
  header.size = 2;
  header.command = 3;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5;

  DCHECK_EQ(5, put);
  put = 0;

  // add 1 command with 1 arg (2 words).
  header.size = 2;
  header.command = 4;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 6;

  // 2 entries remaining to end of buffer.
  AddDoCommandsExpect(error::kNoError, 2, 2);
  // 2 entries remaining to put.
  AddDoCommandsExpect(error::kNoError, 2, 2);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests error conditions.
TEST_F(CommandBufferServiceTest, TestError) {
  const unsigned int kNumEntries = 5;
  MakeService(kNumEntries);
  CommandBufferOffset put = GetPut();
  CommandHeader header;

  // Generate a command with size 0.
  header.size = 0;
  header.command = 3;
  buffer()[put++].value_header = header;

  AddDoCommandsExpect(error::kInvalidSize, 1, 0);
  EXPECT_CALL(*this, OnParseError()).Times(1);
  EXPECT_EQ(error::kInvalidSize, SetPutAndProcessAllCommands(put));
  // check that no DoCommand call was made.
  Mock::VerifyAndClearExpectations(api_mock());
  Mock::VerifyAndClearExpectations(this);

  MakeService(5);
  put = GetPut();

  // Generate a command with size 6, extends beyond the end of the buffer.
  header.size = 6;
  header.command = 3;
  buffer()[put++].value_header = header;

  AddDoCommandsExpect(error::kOutOfBounds, 1, 0);
  EXPECT_CALL(*this, OnParseError()).Times(1);
  EXPECT_EQ(error::kOutOfBounds, SetPutAndProcessAllCommands(put));
  // check that no DoCommand call was made.
  Mock::VerifyAndClearExpectations(api_mock());
  Mock::VerifyAndClearExpectations(this);

  MakeService(5);
  put = GetPut();

  // Generates 2 commands.
  header.size = 1;
  header.command = 3;
  buffer()[put++].value_header = header;
  CommandBufferOffset put_post_fail = put;
  header.size = 1;
  header.command = 4;
  buffer()[put++].value_header = header;

  // have the first command fail to parse.
  AddDoCommandsExpect(error::kUnknownCommand, 2, 1);
  EXPECT_CALL(*this, OnParseError()).Times(1);
  EXPECT_EQ(error::kUnknownCommand, SetPutAndProcessAllCommands(put));
  // check that only one command was executed, and that get reflects that
  // correctly.
  EXPECT_EQ(put_post_fail, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
  Mock::VerifyAndClearExpectations(this);
  // make the second one succeed, and check that the service doesn't try to
  // recover.
  EXPECT_CALL(*this, OnParseError()).Times(0);
  EXPECT_EQ(error::kUnknownCommand, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put_post_fail, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
  Mock::VerifyAndClearExpectations(this);

  // Try to flush out-of-bounds, should fail.
  MakeService(kNumEntries);
  AdvancePut(2);
  EXPECT_EQ(2, GetPut());

  EXPECT_CALL(*this, OnParseError()).Times(1);
  command_buffer_service()->Flush(kNumEntries + 1, api_mock());
  CommandBuffer::State state1 = command_buffer_service()->GetState();
  EXPECT_EQ(2, GetPut());
  EXPECT_EQ(error::kOutOfBounds, state1.error);
  Mock::VerifyAndClearExpectations(this);

  MakeService(kNumEntries);
  AdvancePut(2);
  EXPECT_EQ(2, GetPut());

  EXPECT_CALL(*this, OnParseError()).Times(1);
  command_buffer_service()->Flush(-1, api_mock());
  CommandBuffer::State state2 = command_buffer_service()->GetState();
  EXPECT_EQ(2, GetPut());
  EXPECT_EQ(error::kOutOfBounds, state2.error);
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(CommandBufferServiceTest, SetBuffer) {
  MakeService(5);
  AdvancePut(2);
  // We should have advanced 2 entries.
  EXPECT_EQ(2, GetGet());

  CommandBuffer::State state1 = command_buffer_service()->GetState();
  int32_t id = SetNewGetBuffer(5 * sizeof(CommandBufferEntry));
  CommandBuffer::State state2 = command_buffer_service()->GetState();
  // The put and get should have reset to 0.
  EXPECT_EQ(0, GetGet());
  EXPECT_EQ(0, GetPut());
  EXPECT_EQ(error::kNoError, state2.error);
  EXPECT_EQ(state1.token, state2.token);
  EXPECT_EQ(state1.set_get_buffer_count + 1, state2.set_get_buffer_count);

  AdvancePut(2);
  // We should have advanced 2 entries.
  EXPECT_EQ(2, GetGet());

  // Destroy current get buffer, should not reset.
  command_buffer_service()->DestroyTransferBuffer(id);
  CommandBuffer::State state3 = command_buffer_service()->GetState();
  EXPECT_EQ(2, GetGet());
  EXPECT_EQ(2, GetPut());
  EXPECT_EQ(error::kNoError, state3.error);
  // Should not update the set_get_buffer_count either.
  EXPECT_EQ(state2.set_get_buffer_count, state3.set_get_buffer_count);

  AdvancePut(2);
  // We should have advanced 2 entries.
  EXPECT_EQ(4, GetGet());

  // Reseting the get buffer should reset get and put
  command_buffer_service()->SetGetBuffer(-1);
  CommandBuffer::State state4 = command_buffer_service()->GetState();
  EXPECT_EQ(0, GetGet());
  EXPECT_EQ(0, GetPut());
  EXPECT_EQ(error::kNoError, state4.error);
  // Should not update the set_get_buffer_count either.
  EXPECT_EQ(state3.set_get_buffer_count + 1, state4.set_get_buffer_count);

  // Trying to execute commands should now fail.
  EXPECT_CALL(*this, OnParseError()).Times(1);
  command_buffer_service()->Flush(2, api_mock());
  CommandBuffer::State state5 = command_buffer_service()->GetState();
  EXPECT_EQ(0, GetPut());
  EXPECT_EQ(error::kOutOfBounds, state5.error);
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(CommandBufferServiceTest, InvalidSetBuffer) {
  MakeService(3);
  CommandBuffer::State state1 = command_buffer_service()->GetState();

  // Set an invalid transfer buffer, should succeed.
  command_buffer_service()->SetGetBuffer(-1);
  CommandBuffer::State state2 = command_buffer_service()->GetState();
  EXPECT_EQ(0, GetGet());
  EXPECT_EQ(0, GetPut());
  EXPECT_EQ(error::kNoError, state2.error);
  EXPECT_EQ(state1.token, state2.token);
  EXPECT_EQ(state1.set_get_buffer_count + 1, state2.set_get_buffer_count);

  // Trying to execute commands should fail however.
  EXPECT_CALL(*this, OnParseError()).Times(1);
  command_buffer_service()->Flush(2, api_mock());
  CommandBuffer::State state3 = command_buffer_service()->GetState();
  EXPECT_EQ(0, GetPut());
  EXPECT_EQ(error::kOutOfBounds, state3.error);
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(CommandBufferServiceTest, Token) {
  MakeService(3);
  command_buffer_service()->SetToken(7);
  EXPECT_EQ(7, command_buffer_service()->GetState().token);
}

TEST_F(CommandBufferServiceTest, CanSetParseError) {
  MakeService(3);

  EXPECT_CALL(*this, OnParseError()).Times(1);
  command_buffer_service()->SetParseError(error::kInvalidSize);
  EXPECT_EQ(error::kInvalidSize, command_buffer_service()->GetState().error);
  Mock::VerifyAndClearExpectations(this);
}

class CommandBufferServicePauseExecutionTest : public CommandBufferServiceTest {
 public:
  // Will pause the command buffer execution after 2 runs.
  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) {
    *entries_processed = 1;
    return error::kNoError;
  }

  CommandBatchProcessedResult OnCommandBatchProcessed() override {
    ++calls_;
    if (calls_ == 2)
      pause_ = true;
    return pause_ ? kPauseExecution : kContinueExecution;
  }

 protected:
  int calls_ = 0;
  bool pause_ = false;
};

TEST_F(CommandBufferServicePauseExecutionTest, CommandsProcessed) {
  MakeService(3);
  AddDoCommandsExpect(error::kNoError, 2, 1);
  AddDoCommandsExpect(error::kNoError, 1, 1);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(2));
  EXPECT_EQ(2, calls_);
}

TEST_F(CommandBufferServicePauseExecutionTest, PauseExecution) {
  MakeService(5);
  // Command buffer processing should stop after 2 commands.
  AddDoCommandsExpect(error::kNoError, 4, 1);
  AddDoCommandsExpect(error::kNoError, 3, 1);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(4));
  EXPECT_EQ(2, GetGet());
  EXPECT_EQ(4, GetPut());
  EXPECT_EQ(2, calls_);
  EXPECT_TRUE(pause_);

  // Processing should continue after resume.
  pause_ = false;
  AddDoCommandsExpect(error::kNoError, 2, 1);
  AddDoCommandsExpect(error::kNoError, 1, 1);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(4));
  EXPECT_EQ(4, GetGet());
  EXPECT_EQ(4, GetPut());
  EXPECT_EQ(4, calls_);
  EXPECT_FALSE(pause_);
}

class CommandBufferServiceUnscheduleExecutionTest
    : public CommandBufferServiceTest {
 public:
  enum { kUnscheduleAfterCalls = 2 };

  // Will unschedule the command buffer execution after 2 runs.
  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) {
    ++calls_;
    if (calls_ == kUnscheduleAfterCalls) {
      command_buffer_service()->SetScheduled(false);
      *entries_processed = 0;
      return error::kDeferCommandUntilLater;
    }
    *entries_processed = 1;
    return error::kNoError;
  }

 protected:
  int calls_ = 0;
};

TEST_F(CommandBufferServiceUnscheduleExecutionTest, Unschedule) {
  MakeService(5);
  EXPECT_CALL(*api_mock(), DoCommands(_, _, _, _))
      .WillRepeatedly(Invoke(
          this, &CommandBufferServiceUnscheduleExecutionTest::DoCommands));
  // Command buffer processing should stop after 2 commands.
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(4));
  EXPECT_EQ(1, GetGet());
  EXPECT_EQ(4, GetPut());
  EXPECT_EQ(kUnscheduleAfterCalls, calls_);
  EXPECT_FALSE(command_buffer_service()->scheduled());

  // Processing should continue after rescheduling.
  command_buffer_service()->SetScheduled(true);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(4));
  EXPECT_EQ(4, GetGet());
  EXPECT_EQ(4, GetPut());
  EXPECT_EQ(5, calls_);
  EXPECT_TRUE(command_buffer_service()->scheduled());
}

}  // namespace gpu
