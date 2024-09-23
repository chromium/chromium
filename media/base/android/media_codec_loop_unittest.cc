// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_loop.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/base/waiting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace media {

// The client is a strict mock, since we don't want random calls into it.  We
// want to be sure about the call sequence.
class MockMediaCodecLoopClient : public StrictMock<MediaCodecLoop::Client> {
 public:
  MOCK_CONST_METHOD0(IsAnyInputPending, bool());
  MOCK_METHOD0(ProvideInputData, MediaCodecLoop::InputData());
  MOCK_METHOD1(OnInputDataQueued, void(bool));
  MOCK_METHOD1(OnDecodedEos, bool(const MediaCodecLoop::OutputBuffer&));
  MOCK_METHOD1(OnDecodedFrame, bool(const MediaCodecLoop::OutputBuffer&));
  MOCK_METHOD1(OnWaiting, void(WaitingReason reason));
  MOCK_METHOD0(OnOutputFormatChanged, bool());
  MOCK_METHOD0(OnCodecLoopError, void());
};

class MediaCodecLoopTest : public testing::Test {
 public:
  MediaCodecLoopTest()
      : task_runner_current_default_handle_(mock_task_runner_),
        client_(std::make_unique<MockMediaCodecLoopClient>()) {}

  MediaCodecLoopTest(const MediaCodecLoopTest&) = delete;
  MediaCodecLoopTest& operator=(const MediaCodecLoopTest&) = delete;

  ~MediaCodecLoopTest() override {}

 protected:
  enum IdleExpectation {
    ShouldBeIdle,
    ShouldNotBeIdle,
  };

  // Wait until |codec_loop_| is idle.
  // Do not call this in a sequence.
  void WaitUntilIdle(IdleExpectation idleExpectation = ShouldBeIdle) {
    switch (idleExpectation) {
      case ShouldBeIdle:
        EXPECT_CALL(*client_, IsAnyInputPending()).Times(0);
        EXPECT_CALL(Codec(), DequeueOutputBuffer(_, _, _, _, _, _, _)).Times(0);
        break;
      case ShouldNotBeIdle:
        // Expect at least one call to see if more work is ready.  We will
        // return 'no'.
        EXPECT_CALL(*client_, IsAnyInputPending())
            .Times(AtLeast(1))
            .WillRepeatedly(Return(false));
        EXPECT_CALL(Codec(), DequeueOutputBuffer(_, _, _, _, _, _, _))
            .Times(AtLeast(1))
            .WillRepeatedly(Return(MediaCodecResult::Codes::kTryAgainLater));
        break;
    }

    // Either way, we expect that MCL should not attempt to dequeue input
    // buffers, either because it's idle or because we said that no input
    // is pending.
    EXPECT_CALL(Codec(), DequeueInputBuffer(_, _)).Times(0);

    // TODO(liberato): assume that MCL doesn't retry for 30 seconds.  Note
    // that this doesn't actually wall-clock wait.
    mock_task_runner_->FastForwardBy(base::Seconds(30));
  }

  void ConstructCodecLoop() {
    int sdk_int = base::android::SDK_VERSION_NOUGAT;
    auto codec = std::make_unique<MockMediaCodecBridge>();
    // Since we're providing a codec, we do not expect an error.
    EXPECT_CALL(*client_, OnCodecLoopError()).Times(0);
    codec_loop_ = std::make_unique<MediaCodecLoop>(
        sdk_int, client_.get(), std::move(codec), mock_task_runner_);
    codec_loop_->SetTestTickClock(mock_task_runner_->GetMockTickClock());
    Mock::VerifyAndClearExpectations(client_.get());
  }

  // Set an expectation that MCL will try to get another input / output buffer,
  // and not get one in ExpectWork.
  void ExpectEmptyIOLoop() {
    ExpectIsAnyInputPending(false);
    EXPECT_CALL(Codec(), DequeueOutputBuffer(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(MediaCodecResult::Codes::kTryAgainLater));
  }

  void ExpectIsAnyInputPending(bool pending) {
    EXPECT_CALL(*client_, IsAnyInputPending()).WillOnce(Return(pending));
  }

  void ExpectDequeueInputBuffer(int input_buffer_index,
                                MediaCodecResult status = OkStatus()) {
    EXPECT_CALL(Codec(), DequeueInputBuffer(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(input_buffer_index), Return(status)));
  }

  void ExpectInputDataQueued(bool success) {
    EXPECT_CALL(*client_, OnInputDataQueued(success)).Times(1);
  }

  // Expect a call to queue |data| into MC buffer |input_buffer_index|.
  void ExpectQueueInputBuffer(int input_buffer_index,
                              const MediaCodecLoop::InputData& data,
                              MediaCodecResult status = OkStatus()) {
    EXPECT_CALL(Codec(), QueueInputBuffer(input_buffer_index, data.memory.get(),
                                          data.length, data.presentation_time))
        .Times(1)
        .WillOnce(Return(status));
  }

  void ExpectProvideInputData(const MediaCodecLoop::InputData& data) {
    EXPECT_CALL(*client_, ProvideInputData()).WillOnce(Return(data));
  }

  MediaCodecLoop::InputData BigBuckBunny() {
    MediaCodecLoop::InputData data;
    data.memory = reinterpret_cast<const uint8_t*>("big buck bunny");
    data.length = 14;
    data.presentation_time = base::Seconds(1);
    return data;
  }

  struct OutputBuffer {
    int index = 1;
    size_t offset = 0;
    size_t size = 1024;
    base::TimeDelta pts = base::Seconds(1);
    bool eos = false;
    bool key_frame = true;
  };

  struct EosOutputBuffer : public OutputBuffer {
    EosOutputBuffer() { eos = true; }
  };

  void ExpectDequeueOutputBuffer(MediaCodecResult status) {
    EXPECT_CALL(Codec(), DequeueOutputBuffer(_, _, _, _, _, _, _))
        .WillOnce(Return(status));
  }

  void ExpectDequeueOutputBuffer(const OutputBuffer& buffer) {
    EXPECT_CALL(Codec(), DequeueOutputBuffer(_, _, _, _, _, _, _))
        .WillOnce(DoAll(
            SetArgPointee<1>(buffer.index), SetArgPointee<2>(buffer.offset),
            SetArgPointee<3>(buffer.size), SetArgPointee<4>(buffer.pts),
            SetArgPointee<5>(buffer.eos), SetArgPointee<6>(buffer.key_frame),
            Return(OkStatus())));
  }

  void ExpectOnDecodedFrame(const OutputBuffer& buf) {
    EXPECT_CALL(*client_,
                OnDecodedFrame(
                    Field(&MediaCodecLoop::OutputBuffer::index, Eq(buf.index))))
        .Times(1)
        .WillOnce(Return(true));
  }

  MockMediaCodecBridge& Codec() {
    return *static_cast<MockMediaCodecBridge*>(codec_loop_->GetCodec());
  }

 public:
  // Mocks the current thread's task runner which will also be used as the
  // MediaCodecLoop's task runner.
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_ =
      new base::TestMockTimeTaskRunner;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;

  std::unique_ptr<MediaCodecLoop> codec_loop_;
  std::unique_ptr<MockMediaCodecLoopClient> client_;
};

TEST_F(MediaCodecLoopTest, TestConstructionWithNullCodec) {
  std::unique_ptr<MediaCodecBridge> codec;
  EXPECT_CALL(*client_, OnCodecLoopError()).Times(1);
  const int sdk_int = base::android::SDK_VERSION_NOUGAT;
  codec_loop_ = std::make_unique<MediaCodecLoop>(
      sdk_int, client_.get(), std::move(codec),
      scoped_refptr<base::SingleThreadTaskRunner>());
  // Do not WaitUntilIdle() here, since that assumes that we have a codec.

  ASSERT_FALSE(codec_loop_->GetCodec());
}

TEST_F(MediaCodecLoopTest, TestConstructionWithCodec) {
  ConstructCodecLoop();
  ASSERT_EQ(codec_loop_->GetCodec(), &Codec());
  WaitUntilIdle(ShouldBeIdle);
}

TEST_F(MediaCodecLoopTest, TestPendingWorkWithoutInput) {
  ConstructCodecLoop();
  // MCL should try ask if there is pending input, and try to dequeue output.
  ExpectIsAnyInputPending(false);
  EXPECT_CALL(Codec(), DequeueOutputBuffer(_, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(Return(MediaCodecResult::Codes::kTryAgainLater));
  codec_loop_->ExpectWork();
  WaitUntilIdle(ShouldNotBeIdle);
}

TEST_F(MediaCodecLoopTest, TestPendingWorkWithInput) {
  ConstructCodecLoop();
  // MCL should try ask if there is pending input, and try to dequeue both an
  // output and input buffer.
  ExpectIsAnyInputPending(true);
  EXPECT_CALL(Codec(), DequeueOutputBuffer(_, _, _, _, _, _, _)).Times(1);
  EXPECT_CALL(Codec(), DequeueInputBuffer(_, _)).Times(1);
  codec_loop_->ExpectWork();
  WaitUntilIdle(ShouldNotBeIdle);
}

TEST_F(MediaCodecLoopTest, TestPendingWorkWithOutputBuffer) {
  ConstructCodecLoop();
  {
    InSequence _s;

    // MCL will first request input, then try to dequeue output.
    ExpectIsAnyInputPending(false);
    OutputBuffer buf;
    ExpectDequeueOutputBuffer(buf);
    ExpectOnDecodedFrame(buf);

    // MCL will try again for another set of buffers before ExpectWork()
    // returns.  This is why we don't just leave them for WaitUntilIdle().
    ExpectEmptyIOLoop();
  }
  codec_loop_->ExpectWork();
  WaitUntilIdle(ShouldNotBeIdle);
}

TEST_F(MediaCodecLoopTest, TestQueueEos) {
  // Test sending an EOS to MCL => MCB =dequeue EOS=> MCL .
  ConstructCodecLoop();
  {
    InSequence _s;

    ExpectIsAnyInputPending(true);
    int input_buffer_index = 123;
    ExpectDequeueInputBuffer(input_buffer_index);

    MediaCodecLoop::InputData data;
    data.is_eos = true;
    ExpectProvideInputData(data);
    EXPECT_CALL(Codec(), QueueEOS(input_buffer_index));
    ExpectInputDataQueued(true);

    // Now send the EOS back on the output queue.
    EosOutputBuffer eos;
    ExpectDequeueOutputBuffer(eos);
    EXPECT_CALL(Codec(), ReleaseOutputBuffer(eos.index, false));
    EXPECT_CALL(*client_, OnDecodedEos(_)).Times(1).WillOnce(Return(true));

    // See TestUnqueuedEos.
    EXPECT_CALL(Codec(), DequeueOutputBuffer(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(MediaCodecResult::Codes::kTryAgainLater));
  }
  codec_loop_->ExpectWork();
  // Don't WaitUntilIdle() here.  See TestUnqueuedEos.
}

TEST_F(MediaCodecLoopTest, TestQueueEosFailure) {
  // Test sending an EOS to MCL => MCB =dequeue EOS fails=> MCL error.
  ConstructCodecLoop();
  {
    InSequence _s;

    ExpectIsAnyInputPending(true);
    int input_buffer_index = 123;
    ExpectDequeueInputBuffer(input_buffer_index);

    MediaCodecLoop::InputData data;
    data.is_eos = true;
    ExpectProvideInputData(data);
    EXPECT_CALL(Codec(), QueueEOS(input_buffer_index));
    ExpectInputDataQueued(true);

    // Now send the EOS back on the output queue.
    EosOutputBuffer eos;
    ExpectDequeueOutputBuffer(eos);
    EXPECT_CALL(Codec(), ReleaseOutputBuffer(eos.index, false));
    EXPECT_CALL(*client_, OnDecodedEos(_)).Times(1).WillOnce(Return(false));
    EXPECT_CALL(*client_, OnCodecLoopError()).Times(1);
  }
  codec_loop_->ExpectWork();
  // Don't WaitUntilIdle() here.
}

TEST_F(MediaCodecLoopTest, TestQueueInputData) {
  // Send a buffer full of data into MCL and make sure that it gets queued with
  // MediaCodecBridge correctly.
  ConstructCodecLoop();
  {
    InSequence _s;

    ExpectIsAnyInputPending(true);
    int input_buffer_index = 123;
    ExpectDequeueInputBuffer(input_buffer_index);

    MediaCodecLoop::InputData data = BigBuckBunny();
    ExpectProvideInputData(data);

    // MCL should send the buffer into MediaCodec and notify the client.
    ExpectQueueInputBuffer(input_buffer_index, data);
    ExpectInputDataQueued(true);

    // MCL will try to dequeue an output buffer too.
    EXPECT_CALL(Codec(), DequeueOutputBuffer(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(MediaCodecResult::Codes::kTryAgainLater));

    // ExpectWork will try again.
    ExpectEmptyIOLoop();
  }
  codec_loop_->ExpectWork();
  WaitUntilIdle(ShouldNotBeIdle);
}

TEST_F(MediaCodecLoopTest, TestQueueInputDataFails) {
  // Send a buffer full of data into MCL, but MediaCodecBridge fails to queue
  // it successfully.
  ConstructCodecLoop();
  {
    InSequence _s;

    ExpectIsAnyInputPending(true);
    int input_buffer_index = 123;
    ExpectDequeueInputBuffer(input_buffer_index);

    MediaCodecLoop::InputData data = BigBuckBunny();
    ExpectProvideInputData(data);

    // MCL should send the buffer into MediaCodec and notify the client.
    ExpectQueueInputBuffer(input_buffer_index, data,
                           MediaCodecResult::Codes::kError);
    ExpectInputDataQueued(false);
    EXPECT_CALL(*client_, OnCodecLoopError()).Times(1);
  }
  codec_loop_->ExpectWork();
  // MCL is now in the error state.
}

TEST_F(MediaCodecLoopTest, TestQueueInputDataTryAgain) {
  // Signal that there is input pending, but don't provide an input buffer.
  ConstructCodecLoop();
  {
    InSequence _s;

    ExpectIsAnyInputPending(true);
    ExpectDequeueInputBuffer(-1, MediaCodecResult::Codes::kTryAgainLater);
    // MCL will try for output too.
    ExpectDequeueOutputBuffer(MediaCodecResult::Codes::kTryAgainLater);
  }
  codec_loop_->ExpectWork();
  // Note that the client might not be allowed to change from "input pending"
  // to "no input pending" without actually being asked for input.  For now,
  // MCL doesn't assume this.
  WaitUntilIdle(ShouldNotBeIdle);
}

TEST_F(MediaCodecLoopTest, TestSeveralPendingIOBuffers) {
  // Provide several input and output buffers to MCL.
  ConstructCodecLoop();
  int input_buffer_index = 123;
  const int num_loops = 4;

  InSequence _s;
  for (int i = 0; i < num_loops; i++, input_buffer_index++) {
    ExpectIsAnyInputPending(true);
    ExpectDequeueInputBuffer(input_buffer_index);

    MediaCodecLoop::InputData data = BigBuckBunny();
    ExpectProvideInputData(data);

    ExpectQueueInputBuffer(input_buffer_index, data);
    ExpectInputDataQueued(true);

    OutputBuffer buffer;
    buffer.index = i;
    buffer.size += i;
    buffer.pts = base::Seconds(i + 1);
    ExpectDequeueOutputBuffer(buffer);
    ExpectOnDecodedFrame(buffer);
  }

  ExpectEmptyIOLoop();

  codec_loop_->ExpectWork();
}

TEST_F(MediaCodecLoopTest, TestOnKeyAdded) {
  ConstructCodecLoop();

  int input_buffer_index = 123;
  MediaCodecLoop::InputData data = BigBuckBunny();

  // First provide input, but have MediaCodecBridge require a key.
  {
    InSequence _s;

    // First ExpectWork()
    ExpectIsAnyInputPending(true);
    ExpectDequeueInputBuffer(input_buffer_index);

    ExpectProvideInputData(data);

    // Notify MCL that it's missing the key.
    ExpectQueueInputBuffer(input_buffer_index, data,
                           MediaCodecResult::Codes::kNoKey);

    EXPECT_CALL(*client_, OnWaiting(WaitingReason::kNoDecryptionKey)).Times(1);

    // MCL should now try for output buffers.
    ExpectDequeueOutputBuffer(MediaCodecResult::Codes::kTryAgainLater);

    // MCL will try again, since trying to queue the input buffer is considered
    // doing work, for some reason.  It would be nice to make this optional.
    // Note that it should not ask us for more input, since it has not yet sent
    // the buffer we just provided.
    ExpectDequeueOutputBuffer(MediaCodecResult::Codes::kTryAgainLater);
  }
  codec_loop_->ExpectWork();

  // Try again, to be sure that MCL doesn't request more input.  Note that this
  // is also done in the above loop, but that one could be made optional.  This
  // forces MCL to try again as part of an entirely new ExpectWork cycle.
  {
    InSequence _s;
    // MCL should only try for output buffers, since it's still waiting for a
    // key to be added.
    ExpectDequeueOutputBuffer(MediaCodecResult::Codes::kTryAgainLater);
  }
  codec_loop_->ExpectWork();

  // When we add the key, MCL will DoPending work again.  This time, it should
  // succeed since the key has been added.
  {
    InSequence _s;
    // MCL should not retain the original pointer.
    data.memory = nullptr;
    ExpectQueueInputBuffer(input_buffer_index, data);
    ExpectInputDataQueued(true);
    ExpectDequeueOutputBuffer(MediaCodecResult::Codes::kTryAgainLater);

    // MCL did work, so it will try again.
    ExpectEmptyIOLoop();
  }

  codec_loop_->OnKeyAdded();
  WaitUntilIdle(ShouldNotBeIdle);
}

}  // namespace media
