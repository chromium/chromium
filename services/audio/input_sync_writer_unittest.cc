// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/audio/input_sync_writer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sync_socket.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "services/audio/input_glitch_counter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace audio {

namespace {

// Number of audio buffers in the faked ring buffer.
const int kSegments = 10;

}  // namespace

// Mocked out sockets used for Send/ReceiveWithTimeout. Counts the number of
// outstanding reads, i.e. the diff between send and receive calls.
class MockCancelableSyncSocket : public base::CancelableSyncSocket {
 public:
  explicit MockCancelableSyncSocket(int buffer_size)
      : in_failure_mode_(false),
        writes_(0),
        reads_(0),
        receives_(0),
        buffer_size_(buffer_size),
        read_buffer_index_(0) {}

  MockCancelableSyncSocket(const MockCancelableSyncSocket&) = delete;
  MockCancelableSyncSocket& operator=(const MockCancelableSyncSocket&) = delete;

  size_t Send(base::span<const uint8_t> buffer) override {
    EXPECT_EQ(buffer.size(), sizeof(uint32_t));

    ++writes_;
    EXPECT_LE(NumberOfBuffersFilled(), buffer_size_);
    return buffer.size();
  }

  size_t Receive(base::span<uint8_t> buffer) override {
    EXPECT_EQ(0u, buffer.size() % sizeof(uint32_t));

    if (in_failure_mode_)
      return 0;
    if (receives_ == reads_)
      return 0;

    base::SpanWriter writer(buffer);
    while (receives_ < reads_ && writer.remaining()) {
      ++receives_;
      writer.WriteU32LittleEndian(++read_buffer_index_);
    }
    return writer.num_written();
  }

  size_t Peek() override { return (reads_ - receives_) * sizeof(uint32_t); }

  // Simluates reading |buffers| number of buffers from the ring buffer.
  void Read(int buffers) {
    reads_ += buffers;
    EXPECT_LE(reads_, writes_);
  }

  // When |in_failure_mode_| == true, the socket fails to receive.
  void SetFailureMode(bool in_failure_mode) {
    in_failure_mode_ = in_failure_mode;
  }

  int NumberOfBuffersFilled() { return writes_ - reads_; }

 private:
  bool in_failure_mode_;
  int writes_;
  int reads_;
  int receives_;
  int buffer_size_;
  uint32_t read_buffer_index_;
};

class MockInputGlitchCounter : public InputGlitchCounter {
 public:
  explicit MockInputGlitchCounter(
      base::RepeatingCallback<void(const std::string&)> log_callback)
      : InputGlitchCounter(std::move(log_callback)) {}
  MockInputGlitchCounter(const MockInputGlitchCounter&) = delete;
  MockInputGlitchCounter& operator=(const MockInputGlitchCounter&) = delete;

  MOCK_METHOD1(ReportDroppedData, void(bool));
  MOCK_METHOD1(ReportMissedReadDeadline, void(bool));
};

class InputSyncWriterTest : public testing::Test {
 public:
  InputSyncWriterTest() {
    const int sampling_frequency_hz = 16000;
    const int frames = sampling_frequency_hz / 100;  // 10 ms
    const media::AudioParameters audio_params(
        media::AudioParameters::AUDIO_FAKE, media::ChannelLayoutConfig::Mono(),
        sampling_frequency_hz, frames);
    const uint32_t data_size =
        ComputeAudioInputBufferSize(audio_params, kSegments);

    auto shared_memory = base::ReadOnlySharedMemoryRegion::Create(data_size);
    EXPECT_TRUE(shared_memory.IsValid());

    auto socket = std::make_unique<MockCancelableSyncSocket>(kSegments);
    socket_ = socket.get();

    auto mock_input_glitch_counter =
        std::make_unique<MockInputGlitchCounter>(mock_logger_.Get());
    mock_input_glitch_counter_ = mock_input_glitch_counter.get();

    writer_ = std::make_unique<InputSyncWriter>(
        mock_logger_.Get(), std::move(shared_memory), std::move(socket),
        kSegments, audio_params, std::move(mock_input_glitch_counter));
    audio_bus_ = media::AudioBus::Create(audio_params);
    params_ = audio_params;
  }

  InputSyncWriterTest(const InputSyncWriterTest&) = delete;
  InputSyncWriterTest& operator=(const InputSyncWriterTest&) = delete;

  ~InputSyncWriterTest() override {}

  // Get total number of expected log calls. On non-Android we expect one log
  // call at first Write() call, zero on Android. We also expect all call in the
  // with a glitch summary from the destructor. Besides that only for errors
  // and fifo info.
  int GetTotalNumberOfExpectedLogCalls(int expected_calls_due_to_error) {
#if BUILDFLAG(IS_ANDROID)
    return expected_calls_due_to_error + 1;
#else
    return expected_calls_due_to_error + 2;
#endif
  }

  // Tests expected numbers which are given as arguments.
  bool TestSocketAndFifoExpectations(int number_of_buffers_in_socket,
                                     size_t number_of_verifications_in_socket,
                                     size_t number_of_buffers_in_fifo) {
    EXPECT_EQ(number_of_buffers_in_socket, socket_->NumberOfBuffersFilled());
    EXPECT_EQ(number_of_verifications_in_socket, socket_->Peek());
    EXPECT_EQ(number_of_buffers_in_fifo, writer_->overflow_data_.size());

    return number_of_buffers_in_socket == socket_->NumberOfBuffersFilled() &&
           number_of_verifications_in_socket == socket_->Peek() &&
           number_of_buffers_in_fifo == writer_->overflow_data_.size();
  }

  void TestGlitchInfoExpectations(
      int segment_id,
      const media::AudioGlitchInfo expected_glitch_info) {
    media::AudioInputBuffer* buffer = writer_->GetSharedInputBuffer(segment_id);
    EXPECT_EQ(buffer->params.glitch_duration_us,
              expected_glitch_info.duration.InMicroseconds());
    EXPECT_EQ(buffer->params.glitch_count, expected_glitch_info.count);
  }

 protected:
  using MockLogger =
      base::MockCallback<base::RepeatingCallback<void(const std::string&)>>;

  base::test::TaskEnvironment env_;
  MockLogger mock_logger_;
  std::unique_ptr<InputSyncWriter> writer_;
  raw_ptr<MockCancelableSyncSocket> socket_;
  raw_ptr<MockInputGlitchCounter> mock_input_glitch_counter_;
  std::unique_ptr<media::AudioBus> audio_bus_;
  media::AudioParameters params_;
};

TEST_F(InputSyncWriterTest, SingleWriteAndRead) {
  EXPECT_CALL(mock_logger_, Run(_)).Times(GetTotalNumberOfExpectedLogCalls(0));

  EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false));
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
  media::AudioGlitchInfo glitch_info{.duration = base::Milliseconds(123),
                                     .count = 5};
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(),
                 glitch_info);
  EXPECT_TRUE(TestSocketAndFifoExpectations(1, 0, 0));

  socket_->Read(1);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, 1 * sizeof(uint32_t), 0));
  TestGlitchInfoExpectations(0, glitch_info);
}

TEST_F(InputSyncWriterTest, MultipleWritesAndReads) {
  EXPECT_CALL(mock_logger_, Run(_)).Times(GetTotalNumberOfExpectedLogCalls(0));

  for (int i = 1; i <= 2 * kSegments; ++i) {
    media::AudioGlitchInfo glitch_info{.duration = base::Milliseconds(123 + i),
                                       .count = 5};
    EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false));
    EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(),
                   glitch_info);
    EXPECT_TRUE(TestSocketAndFifoExpectations(1, 0, 0));
    socket_->Read(1);
    EXPECT_TRUE(TestSocketAndFifoExpectations(0, 1 * sizeof(uint32_t), 0));

    // The shared memory is 0-indexed, and this loop is 1-indexed.
    TestGlitchInfoExpectations((i - 1) % kSegments, glitch_info);
  }
}

TEST_F(InputSyncWriterTest, MultipleWritesNoReads) {
  EXPECT_CALL(mock_logger_, Run(_)).Times(GetTotalNumberOfExpectedLogCalls(1));

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false));
    EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
    EXPECT_TRUE(TestSocketAndFifoExpectations(i, 0, 0));
  }

  // Now the ring buffer is full, do more writes. We should start filling the
  // fifo and should get one extra log call for that.
  for (size_t i = 1; i <= kSegments; ++i) {
    EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(true));
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
    EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, i));
  }
}

TEST_F(InputSyncWriterTest, FillAndEmptyRingBuffer) {
  EXPECT_CALL(mock_logger_, Run(_)).Times(GetTotalNumberOfExpectedLogCalls(2));

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false));
    EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 0));

  // Empty half of the ring buffer.
  const int buffers_to_read = kSegments / 2;
  socket_->Read(buffers_to_read);
  EXPECT_TRUE(TestSocketAndFifoExpectations(
      kSegments - buffers_to_read, buffers_to_read * sizeof(uint32_t), 0));

  // Fill up again. The first write should do receive until that queue is
  // empty.
  for (int i = kSegments - buffers_to_read + 1; i <= kSegments; ++i) {
    EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false));
    EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
    EXPECT_TRUE(TestSocketAndFifoExpectations(i, 0, 0));
  }

  // Another write, should put the data in the fifo, and render an extra log
  // call.
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(true));
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 1));

  // Empty the ring buffer.
  socket_->Read(kSegments);
  EXPECT_TRUE(
      TestSocketAndFifoExpectations(0, kSegments * sizeof(uint32_t), 1));

  // Another write, should do receive until that queue is empty and write both
  // the data in the fifo and the new data, and render a log call.
  EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false)).Times(2);
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  EXPECT_TRUE(TestSocketAndFifoExpectations(2, 0, 0));

  // Read the two data blocks.
  socket_->Read(2);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, 2 * sizeof(uint32_t), 0));
}

TEST_F(InputSyncWriterTest, FillRingBufferAndFifo) {
  EXPECT_CALL(mock_logger_, Run(_)).Times(GetTotalNumberOfExpectedLogCalls(2));

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false));
    EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 0));

  // Fill the fifo. Should render one log call for starting filling it.
  const size_t max_fifo_size = InputSyncWriter::kMaxOverflowBusesSize;
  for (size_t i = 1; i <= max_fifo_size; ++i) {
    EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(true));
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, max_fifo_size));

  // Another write, data should be dropped and render one log call.
  EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(true));
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(true));
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, max_fifo_size));
}

TEST_F(InputSyncWriterTest, MultipleFillAndEmptyRingBufferAndPartOfFifo) {
  EXPECT_CALL(mock_logger_, Run(_)).Times(GetTotalNumberOfExpectedLogCalls(4));

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false));
    EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 0));

  // Write more data, should be put in the fifo and render one log call for
  // starting filling it.
  for (size_t i = 1; i <= 2 * kSegments; ++i) {
    EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(true));
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 2 * kSegments));

  // Empty the ring buffer.
  socket_->Read(kSegments);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, kSegments * sizeof(uint32_t),
                                            2 * kSegments));

  // Another write should fill up the ring buffer with data from the fifo and
  // put this data into the fifo.
  EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false))
      .Times(kSegments);
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, kSegments + 1));

  // Empty the ring buffer again.
  socket_->Read(kSegments);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, kSegments * sizeof(uint32_t),
                                            kSegments + 1));

  // Another write should fill up the ring buffer with data from the fifo and
  // put this data into the fifo.
  EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false))
      .Times(kSegments);
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 2));

  // Empty the ring buffer again.
  socket_->Read(kSegments);
  EXPECT_TRUE(
      TestSocketAndFifoExpectations(0, kSegments * sizeof(uint32_t), 2));

  // Another write should put the remaining data in the fifo in the ring buffer
  // together with this data. Should render a log call for emptying the fifo.
  EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false)).Times(3);
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  EXPECT_TRUE(TestSocketAndFifoExpectations(3, 0, 0));

  // Read the remaining data.
  socket_->Read(3);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, 3 * sizeof(uint32_t), 0));

  // Fill the ring buffer and part of the fifo. Should render one log call for
  // starting filling it.
  for (int i = 1; i <= kSegments + 2; ++i) {
    if (i <= kSegments) {
      EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false));
      EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
    } else {
      EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(true));
    }
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 2));

  // Empty both. Should render a log call for emptying the fifo.
  socket_->Read(kSegments);
  EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(false)).Times(3);
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(false));
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  socket_->Read(3);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, 3 * sizeof(uint32_t), 0));
}

TEST_F(InputSyncWriterTest, ShouldNotDropGlitchInfoInFifo) {
  // We are not testing the logger or glitch counter in this test.
  EXPECT_CALL(mock_logger_, Run(_)).Times(testing::AnyNumber());
  EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(_))
      .Times(testing::AnyNumber());

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 0));

  // Write another buffer that contains glitch info. This will go into the fifo.
  media::AudioGlitchInfo glitch_info_1{.duration = base::Milliseconds(123),
                                       .count = 5};
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(),
                 glitch_info_1);
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 1));

  // Write a different buffer that contains glitch info. This will also go into
  // the fifo.
  media::AudioGlitchInfo glitch_info_2{.duration = base::Milliseconds(321),
                                       .count = 7};
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(),
                 glitch_info_2);
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 2));

  // The buffers with the glitch info will go into slots 1 and 2 in the buffer,
  // respectively.
  socket_->Read(kSegments);
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  TestGlitchInfoExpectations(0, glitch_info_1);
  TestGlitchInfoExpectations(1, glitch_info_2);
}

TEST_F(InputSyncWriterTest, ShouldNotDropGlitchInfoWhenDroppingAudio) {
  // We are not testing the logger or glitch counter in this test.
  EXPECT_CALL(mock_logger_, Run(_)).Times(testing::AnyNumber());
  EXPECT_CALL(*mock_input_glitch_counter_, ReportDroppedData(_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_input_glitch_counter_, ReportMissedReadDeadline(_))
      .Times(testing::AnyNumber());

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 0));

  // Fill the fifo.
  const size_t max_fifo_size = InputSyncWriter::kMaxOverflowBusesSize;
  for (size_t i = 1; i <= max_fifo_size; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, max_fifo_size));

  media::AudioGlitchInfo glitch_info{.duration = base::Milliseconds(123),
                                     .count = 5};
  // Another write, with glitch info, data should be dropped.
  writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(),
                 glitch_info);
  size_t index_of_dropped_buffer = kSegments + max_fifo_size;
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, max_fifo_size));

  // Empty the fifo until the glitch is written to shared memory. We start at
  // index kSegments, because that is the first buffer which has not yet been
  // written to the shared memory.
  for (size_t i = kSegments; i <= index_of_dropped_buffer; i += kSegments) {
    socket_->Read(kSegments);
    // We have to write to flush data from the fifo to the ring buffer.
    writer_->Write(audio_bus_.get(), 0, false, base::TimeTicks::Now(), {});
  }

  // Since a buffer was dropped, we expect glitch info about this to be added.
  media::AudioGlitchInfo one_glitch_info_{
      .duration = params_.GetBufferDuration(), .count = 1};
  glitch_info += one_glitch_info_;
  // We expect the glitch info from the dropped buffer to be written to shared
  // memory for the next buffer.
  TestGlitchInfoExpectations((index_of_dropped_buffer) % kSegments,
                             glitch_info);
}

}  // namespace audio
