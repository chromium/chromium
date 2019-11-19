// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/byte_stream.h"

#include <stddef.h>

#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

void CountCallbacks(int* counter) {
  ++*counter;
}

}  // namespace

class ByteStreamTest : public testing::Test {
 public:
  ByteStreamTest();

  // Create a new IO buffer of the given |buffer_size|.  Details of the
  // contents of the created buffer will be kept, and can be validated
  // by ValidateIOBuffer.
  scoped_refptr<net::IOBuffer> NewIOBuffer(size_t buffer_size) {
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(buffer_size);
    char *bufferp = buffer->data();
    for (size_t i = 0; i < buffer_size; i++)
      bufferp[i] = (i + producing_seed_key_) % (1 << sizeof(char));
    pointer_queue_.push_back(bufferp);
    length_queue_.push_back(buffer_size);
    ++producing_seed_key_;
    return buffer;
  }

  // Create an IOBuffer of the appropriate size and add it to the
  // ByteStream, returning the result of the ByteStream::Write.
  // Separate function to avoid duplication of buffer_size in test
  // calls.
  bool Write(ByteStreamWriter* byte_stream_input, size_t buffer_size) {
    return byte_stream_input->Write(NewIOBuffer(buffer_size), buffer_size);
  }

  // Validate that we have the IOBuffer we expect.  This routine must be
  // called on buffers that were allocated from NewIOBuffer, and in the
  // order that they were allocated.  Calls to NewIOBuffer &&
  // ValidateIOBuffer may be interleaved.
  bool ValidateIOBuffer(
      scoped_refptr<net::IOBuffer> buffer, size_t buffer_size) {
    char *bufferp = buffer->data();

    char *expected_ptr = pointer_queue_.front();
    size_t expected_length = length_queue_.front();
    pointer_queue_.pop_front();
    length_queue_.pop_front();
    ++consuming_seed_key_;

    EXPECT_EQ(expected_ptr, bufferp);
    if (expected_ptr != bufferp)
      return false;

    EXPECT_EQ(expected_length, buffer_size);
    if (expected_length != buffer_size)
      return false;

    for (size_t i = 0; i < buffer_size; i++) {
      // Already incremented, so subtract one from the key.
      EXPECT_EQ(static_cast<int>((i + consuming_seed_key_ - 1)
                                 % (1 << sizeof(char))),
                bufferp[i]);
      if (static_cast<int>((i + consuming_seed_key_ - 1) %
                           (1 << sizeof(char))) != bufferp[i]) {
        return false;
      }
    }
    return true;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  int producing_seed_key_;
  int consuming_seed_key_;
  base::circular_deque<char*> pointer_queue_;
  base::circular_deque<size_t> length_queue_;
};

ByteStreamTest::ByteStreamTest()
    : producing_seed_key_(0),
      consuming_seed_key_(0) { }

// Confirm that filling and emptying the stream works properly, and that
// we get full triggers when we expect.
TEST_F(ByteStreamTest, ByteStream_PushBack) {
  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(), 3 * 1024,
                   &byte_stream_input, &byte_stream_output);

  // Push a series of IO buffers on; test pushback happening and
  // that it's advisory.
  EXPECT_TRUE(Write(byte_stream_input.get(), 1024));
  EXPECT_TRUE(Write(byte_stream_input.get(), 1024));
  EXPECT_TRUE(Write(byte_stream_input.get(), 1024));
  EXPECT_FALSE(Write(byte_stream_input.get(), 1));
  EXPECT_FALSE(Write(byte_stream_input.get(), 1024));
  // Flush
  byte_stream_input->Close(0);
  EXPECT_EQ(4 * 1024U + 1U, byte_stream_input->GetTotalBufferedBytes());
  base::RunLoop().RunUntilIdle();
  // Data already sent to reader is also counted in.
  EXPECT_EQ(4 * 1024U + 1U, byte_stream_input->GetTotalBufferedBytes());

  // Pull the IO buffers out; do we get the same buffers and do they
  // have the same contents?
  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_COMPLETE,
            byte_stream_output->Read(&output_io_buffer, &output_length));

  base::RunLoop().RunUntilIdle();
  // Reader now knows that all data is read out.
  EXPECT_EQ(1024U, byte_stream_input->GetTotalBufferedBytes());
}

// Confirm that Flush() method makes the writer to send written contents to
// the reader.
TEST_F(ByteStreamTest, ByteStream_Flush) {
  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(), 1024,
                   &byte_stream_input, &byte_stream_output);

  EXPECT_TRUE(Write(byte_stream_input.get(), 1));
  base::RunLoop().RunUntilIdle();

  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length = 0;
  // Check that data is not sent to the reader yet.
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));

  byte_stream_input->Flush();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  // Check that it's ok to Flush() an empty writer.
  byte_stream_input->Flush();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));

  byte_stream_input->Close(0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ByteStreamReader::STREAM_COMPLETE,
            byte_stream_output->Read(&output_io_buffer, &output_length));
}

// Same as above, only use knowledge of the internals to confirm
// that we're getting pushback even when data's split across the two
// objects
TEST_F(ByteStreamTest, ByteStream_PushBackSplit) {
  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(), 9 * 1024,
                   &byte_stream_input, &byte_stream_output);

  // Push a series of IO buffers on; test pushback happening and
  // that it's advisory.
  EXPECT_TRUE(Write(byte_stream_input.get(), 1024));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Write(byte_stream_input.get(), 1024));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Write(byte_stream_input.get(), 1024));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Write(byte_stream_input.get(), 1024));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(Write(byte_stream_input.get(), 6 * 1024));
  base::RunLoop().RunUntilIdle();

  // Pull the IO buffers out; do we get the same buffers and do they
  // have the same contents?
  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));
}

// Confirm that a Close() notification transmits in-order
// with data on the stream.
TEST_F(ByteStreamTest, ByteStream_CompleteTransmits) {
  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;

  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;

  // Empty stream, non-error case.
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(), 3 * 1024,
                   &byte_stream_input, &byte_stream_output);
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  byte_stream_input->Close(0);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ByteStreamReader::STREAM_COMPLETE,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_EQ(0, byte_stream_output->GetStatus());

  // Non-empty stream, non-error case.
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(), 3 * 1024,
                   &byte_stream_input, &byte_stream_output);
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(Write(byte_stream_input.get(), 1024));
  byte_stream_input->Close(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));
  ASSERT_EQ(ByteStreamReader::STREAM_COMPLETE,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_EQ(0, byte_stream_output->GetStatus());

  const int kFakeErrorCode = 22;

  // Empty stream, error case.
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(), 3 * 1024,
                   &byte_stream_input, &byte_stream_output);
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  byte_stream_input->Close(kFakeErrorCode);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ByteStreamReader::STREAM_COMPLETE,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_EQ(kFakeErrorCode, byte_stream_output->GetStatus());

  // Non-empty stream, error case.
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(), 3 * 1024,
                   &byte_stream_input, &byte_stream_output);
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(Write(byte_stream_input.get(), 1024));
  byte_stream_input->Close(kFakeErrorCode);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));
  ASSERT_EQ(ByteStreamReader::STREAM_COMPLETE,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_EQ(kFakeErrorCode, byte_stream_output->GetStatus());
}

// Confirm that callbacks on the sink side are triggered when they should be.
TEST_F(ByteStreamTest, ByteStream_SinkCallback) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner(
      new base::TestSimpleTaskRunner());

  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(), task_runner, 10000,
                   &byte_stream_input, &byte_stream_output);

  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;

  // Note that the specifics of when the callbacks are called with regard
  // to how much data is pushed onto the stream is not (currently) part
  // of the interface contract.  If it becomes part of the contract, the
  // tests below should get much more precise.

  // Confirm callback called when you add more than 33% of the buffer.

  // Setup callback
  int num_callbacks = 0;
  byte_stream_output->RegisterCallback(
      base::BindRepeating(CountCallbacks, &num_callbacks));

  EXPECT_TRUE(Write(byte_stream_input.get(), 4000));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, num_callbacks);
  task_runner->RunUntilIdle();
  EXPECT_EQ(1, num_callbacks);

  // Check data and stream state.
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));

  // Confirm callback *isn't* called at less than 33% (by lack of
  // unexpected call on task runner).
  EXPECT_TRUE(Write(byte_stream_input.get(), 3000));
  base::RunLoop().RunUntilIdle();

  // This reflects an implementation artifact that data goes with callbacks,
  // which should not be considered part of the interface guarantee.
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));
}

// Confirm that callbacks on the source side are triggered when they should
// be.
TEST_F(ByteStreamTest, ByteStream_SourceCallback) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner(
      new base::TestSimpleTaskRunner());

  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(task_runner, base::ThreadTaskRunnerHandle::Get(), 10000,
                   &byte_stream_input, &byte_stream_output);

  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;

  // Note that the specifics of when the callbacks are called with regard
  // to how much data is pulled from the stream is not (currently) part
  // of the interface contract.  If it becomes part of the contract, the
  // tests below should get much more precise.

  // Confirm callback called when about 33% space available, and not
  // at other transitions.

  // Add data.
  int num_callbacks = 0;
  byte_stream_input->RegisterCallback(
      base::BindRepeating(CountCallbacks, &num_callbacks));
  EXPECT_TRUE(Write(byte_stream_input.get(), 2000));
  EXPECT_TRUE(Write(byte_stream_input.get(), 2001));
  EXPECT_FALSE(Write(byte_stream_input.get(), 6000));

  // Allow bytes to transition (needed for message passing implementation),
  // and get and validate the data.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  // Grab data, triggering callback.  Recorded on dispatch, but doesn't
  // happen because it's caught by the mock.
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  // Confirm that the callback passed to the mock does what we expect.
  EXPECT_EQ(0, num_callbacks);
  task_runner->RunUntilIdle();
  EXPECT_EQ(1, num_callbacks);

  // Same drill with final buffer.
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_EQ(1, num_callbacks);
  task_runner->RunUntilIdle();
  // Should have updated the internal structures but not called the
  // callback.
  EXPECT_EQ(1, num_callbacks);
}

// Confirm that racing a change to a sink callback with a post results
// in the new callback being called.
TEST_F(ByteStreamTest, ByteStream_SinkInterrupt) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner(
      new base::TestSimpleTaskRunner());

  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(), task_runner, 10000,
                   &byte_stream_input, &byte_stream_output);

  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;
  base::Closure intermediate_callback;

  // Record initial state.
  int num_callbacks = 0;
  byte_stream_output->RegisterCallback(
      base::BindRepeating(CountCallbacks, &num_callbacks));

  // Add data, and pass it across.
  EXPECT_TRUE(Write(byte_stream_input.get(), 4000));
  base::RunLoop().RunUntilIdle();

  // The task runner should have been hit, but the callback count
  // isn't changed until we actually run the callback.
  EXPECT_EQ(0, num_callbacks);

  // If we change the callback now, the new one should be run
  // (simulates race with post task).
  int num_alt_callbacks = 0;
  byte_stream_output->RegisterCallback(
      base::BindRepeating(CountCallbacks, &num_alt_callbacks));
  task_runner->RunUntilIdle();
  EXPECT_EQ(0, num_callbacks);
  EXPECT_EQ(1, num_alt_callbacks);

  // Final cleanup.
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));

}

// Confirm that racing a change to a source callback with a post results
// in the new callback being called.
TEST_F(ByteStreamTest, ByteStream_SourceInterrupt) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner(
      new base::TestSimpleTaskRunner());

  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(task_runner, base::ThreadTaskRunnerHandle::Get(), 10000,
                   &byte_stream_input, &byte_stream_output);

  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;
  base::Closure intermediate_callback;

  // Setup state for test.
  int num_callbacks = 0;
  byte_stream_input->RegisterCallback(
      base::BindRepeating(CountCallbacks, &num_callbacks));
  EXPECT_TRUE(Write(byte_stream_input.get(), 2000));
  EXPECT_TRUE(Write(byte_stream_input.get(), 2001));
  EXPECT_FALSE(Write(byte_stream_input.get(), 6000));
  base::RunLoop().RunUntilIdle();

  // Initial get should not trigger callback.
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));
  base::RunLoop().RunUntilIdle();

  // Second get *should* trigger callback.
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));

  // Which should do the right thing when it's run.
  int num_alt_callbacks = 0;
  byte_stream_input->RegisterCallback(
      base::BindRepeating(CountCallbacks, &num_alt_callbacks));
  task_runner->RunUntilIdle();
  EXPECT_EQ(0, num_callbacks);
  EXPECT_EQ(1, num_alt_callbacks);

  // Third get should also trigger callback.
  EXPECT_EQ(ByteStreamReader::STREAM_HAS_DATA,
            byte_stream_output->Read(&output_io_buffer, &output_length));
  EXPECT_TRUE(ValidateIOBuffer(output_io_buffer, output_length));
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));
}

// Confirm that callback is called on zero data transfer but source
// complete.
TEST_F(ByteStreamTest, ByteStream_ZeroCallback) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner(
      new base::TestSimpleTaskRunner());

  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(), task_runner, 10000,
                   &byte_stream_input, &byte_stream_output);

  base::Closure intermediate_callback;

  // Record initial state.
  int num_callbacks = 0;
  byte_stream_output->RegisterCallback(
      base::BindRepeating(CountCallbacks, &num_callbacks));

  // Immediately close the stream.
  byte_stream_input->Close(0);
  task_runner->RunUntilIdle();
  EXPECT_EQ(1, num_callbacks);
}

TEST_F(ByteStreamTest, ByteStream_CloseWithoutAnyWrite) {
  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(), 3 * 1024,
                   &byte_stream_input, &byte_stream_output);

  byte_stream_input->Close(0);
  base::RunLoop().RunUntilIdle();

  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;
  EXPECT_EQ(ByteStreamReader::STREAM_COMPLETE,
            byte_stream_output->Read(&output_io_buffer, &output_length));
}

TEST_F(ByteStreamTest, ByteStream_FlushWithoutAnyWrite) {
  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(), 3 * 1024,
                   &byte_stream_input, &byte_stream_output);

  byte_stream_input->Flush();
  base::RunLoop().RunUntilIdle();

  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));

  byte_stream_input->Close(0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ByteStreamReader::STREAM_COMPLETE,
            byte_stream_output->Read(&output_io_buffer, &output_length));
}

TEST_F(ByteStreamTest, ByteStream_WriteOverflow) {
  std::unique_ptr<ByteStreamWriter> byte_stream_input;
  std::unique_ptr<ByteStreamReader> byte_stream_output;
  CreateByteStream(base::ThreadTaskRunnerHandle::Get(),
                   base::ThreadTaskRunnerHandle::Get(),
                   std::numeric_limits<size_t>::max(), &byte_stream_input,
                   &byte_stream_output);

  EXPECT_TRUE(Write(byte_stream_input.get(), 1));
  // 1 + size_t max -> Overflow.
  scoped_refptr<net::IOBuffer> empty_io_buffer;
  EXPECT_FALSE(byte_stream_input->Write(empty_io_buffer,
                                        std::numeric_limits<size_t>::max()));
  base::RunLoop().RunUntilIdle();

  // The first write is below PostToPeer threshold. We shouldn't get anything
  // from the output.
  scoped_refptr<net::IOBuffer> output_io_buffer;
  size_t output_length;
  EXPECT_EQ(ByteStreamReader::STREAM_EMPTY,
            byte_stream_output->Read(&output_io_buffer, &output_length));
}

}  // namespace content
