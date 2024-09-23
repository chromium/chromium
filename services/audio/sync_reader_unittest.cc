// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/sync_reader.h"

#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "services/audio/output_glitch_counter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using ::testing::Test;
using ::testing::TestWithParam;

using media::AudioBus;
using media::AudioOutputBuffer;
using media::AudioOutputBufferParameters;
using media::AudioParameters;

namespace audio {

namespace {

void NoLog(const std::string&) {}

static_assert(
    std::is_unsigned<
        decltype(AudioOutputBufferParameters::bitstream_data_size)>::value,
    "If |bitstream_data_size| is ever made signed, add tests for negative "
    "buffer sizes.");

enum OverflowTestCase {
  kZero,
  kNoOverflow,
  kOverflowByOne,
  kOverflowByOneThousand,
  kOverflowByMax
};

static const OverflowTestCase overflow_test_case_values[]{
    kZero, kNoOverflow, kOverflowByOne, kOverflowByOneThousand, kOverflowByMax};

class SyncReaderBitstreamTest : public TestWithParam<OverflowTestCase> {
 public:
  SyncReaderBitstreamTest() {}
  ~SyncReaderBitstreamTest() override {}

 private:
  base::test::TaskEnvironment env_;
};

TEST_P(SyncReaderBitstreamTest, BitstreamBufferOverflow_DoesNotWriteOOB) {
  const int kSampleRate = 44100;
  const int kFramesPerBuffer = 1;
  AudioParameters params(AudioParameters::AUDIO_BITSTREAM_AC3,
                         media::ChannelLayoutConfig::Stereo(), kSampleRate,
                         kFramesPerBuffer);

  auto socket = std::make_unique<base::CancelableSyncSocket>();
  SyncReader reader(base::BindRepeating(&NoLog), params, socket.get());
  ASSERT_TRUE(reader.IsValid());
  const base::WritableSharedMemoryMapping shmem =
      reader.TakeSharedMemoryRegion().Map();
  ASSERT_TRUE(shmem.IsValid());
  auto* const buffer = shmem.GetMemoryAs<media::AudioOutputBuffer>();
  ASSERT_TRUE(buffer);
  reader.RequestMoreData(base::TimeDelta(), base::TimeTicks(), {});

  uint32_t signal;
  EXPECT_EQ(socket->Receive(base::byte_span_from_ref(signal)), sizeof(signal));

  // So far, this is an ordinary stream.
  // Now |reader| expects data to be written to the shared memory. The renderer
  // says how much data was written.
  switch (GetParam()) {
    case kZero:
      buffer->params.bitstream_data_size = 0;
      break;
    case kNoOverflow:
      buffer->params.bitstream_data_size =
          shmem.mapped_size() - sizeof(AudioOutputBufferParameters);
      break;
    case kOverflowByOne:
      buffer->params.bitstream_data_size =
          shmem.mapped_size() - sizeof(AudioOutputBufferParameters) + 1;
      break;
    case kOverflowByOneThousand:
      buffer->params.bitstream_data_size =
          shmem.mapped_size() - sizeof(AudioOutputBufferParameters) + 1000;
      break;
    case kOverflowByMax:
      buffer->params.bitstream_data_size = std::numeric_limits<
          decltype(buffer->params.bitstream_data_size)>::max();
      break;
  }

  ++signal;
  EXPECT_EQ(socket->Send(base::byte_span_from_ref(signal)), sizeof(signal));

  // The purpose of the test is to ensure this call doesn't result in undefined
  // behavior, which should be verified by sanitizers.
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params);
  reader.Read(output_bus.get(), false);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncReaderBitstreamTest,
                         ::testing::ValuesIn(overflow_test_case_values));

class MockOutputGlitchCounter : public OutputGlitchCounter {
 public:
  MockOutputGlitchCounter()
      : OutputGlitchCounter(media::AudioLatency::Type::kRtc) {}
  MockOutputGlitchCounter(const MockOutputGlitchCounter&) = delete;
  MockOutputGlitchCounter& operator=(const MockOutputGlitchCounter&) = delete;

  MOCK_METHOD2(ReportMissedCallback, void(bool, bool));
};

class SyncReaderTest : public ::testing::Test {
 public:
  SyncReaderTest()
      : params_(AudioParameters(AudioParameters::AUDIO_BITSTREAM_AC3,
                                media::ChannelLayoutConfig::Stereo(),
                                kSampleRate,
                                kFramesPerBuffer)) {}

  SyncReaderTest(const SyncReaderTest&) = delete;
  SyncReaderTest& operator=(const SyncReaderTest&) = delete;

 protected:
  void SetUp() override {
    socket_ = std::make_unique<base::CancelableSyncSocket>();
    mock_audio_glitch_counter_ptr_ =
        std::make_unique<NiceMock<MockOutputGlitchCounter>>();
    mock_output_glitch_counter_ = mock_audio_glitch_counter_ptr_.get();
    reader_ = std::make_unique<SyncReader>(
        base::BindRepeating(&NoLog), params_, socket_.get(),
        std::move(mock_audio_glitch_counter_ptr_));
    CHECK(reader_->IsValid());
    reader_->set_max_wait_timeout_for_test(base::Milliseconds(999));
    shmem_ = reader_->TakeSharedMemoryRegion().Map();
    CHECK(shmem_.IsValid());
    buffer_ = shmem_.GetMemoryAs<media::AudioOutputBuffer>();
    CHECK(buffer_);
  }

  void TearDown() override {
    mock_output_glitch_counter_ = nullptr;
    reader_.reset();
    mock_audio_glitch_counter_ptr_.reset();
    socket_.reset();
    auto shmem = std::move(shmem_);
  }

  const int kSampleRate = 44100;
  const int kFramesPerBuffer = 1;
  const AudioParameters params_;
  std::unique_ptr<base::CancelableSyncSocket> socket_;

 private:
  std::unique_ptr<MockOutputGlitchCounter> mock_audio_glitch_counter_ptr_;

 protected:
  raw_ptr<MockOutputGlitchCounter> mock_output_glitch_counter_ = nullptr;
  std::unique_ptr<SyncReader> reader_;
  base::WritableSharedMemoryMapping shmem_;
  raw_ptr<media::AudioOutputBuffer> buffer_ = nullptr;
};

TEST_F(SyncReaderTest, CallsGlitchCounter) {
  // Provoke all four combinations of arguments for
  // OutputGlitchCounter::ReportMissedCallback.
  uint32_t buffer_index = 0;
  {
    reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), {});
    uint32_t signal;
    EXPECT_EQ(socket_->Receive(base::byte_span_from_ref(signal)),
              sizeof(signal));
    buffer_->params.bitstream_data_size =
        shmem_.mapped_size() - sizeof(AudioOutputBufferParameters);
    std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params_);

    ++buffer_index;
    EXPECT_EQ(socket_->Send(base::byte_span_from_ref(buffer_index)),
              sizeof(buffer_index));
    EXPECT_CALL(*mock_output_glitch_counter_,
                ReportMissedCallback(/*missed_callback = */ false,
                                     /*is_mixing = */ false));
    reader_->Read(output_bus.get(), false);
  }

  {
    reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), {});
    uint32_t signal;
    EXPECT_EQ(socket_->Receive(base::byte_span_from_ref(signal)),
              sizeof(signal));
    buffer_->params.bitstream_data_size =
        shmem_.mapped_size() - sizeof(AudioOutputBufferParameters);
    std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params_);

    ++buffer_index;
    EXPECT_EQ(socket_->Send(base::byte_span_from_ref(buffer_index)),
              sizeof(buffer_index));
    EXPECT_CALL(*mock_output_glitch_counter_,
                ReportMissedCallback(/*missed_callback = */ false,
                                     /*is_mixing = */ true));
    reader_->Read(output_bus.get(), true);
  }

  {
    reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), {});
    uint32_t signal;
    EXPECT_EQ(socket_->Receive(base::byte_span_from_ref(signal)),
              sizeof(signal));
    buffer_->params.bitstream_data_size =
        shmem_.mapped_size() - sizeof(AudioOutputBufferParameters);
    std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params_);

    // Send an incorrect buffer index, which will count as a missed callback.
    buffer_index = 123;
    EXPECT_EQ(socket_->Send(base::byte_span_from_ref(buffer_index)),
              sizeof(buffer_index));
    EXPECT_CALL(*mock_output_glitch_counter_,
                ReportMissedCallback(/*missed_callback = */ true,
                                     /*is_mixing = */ false));
    reader_->Read(output_bus.get(), false);
  }

  {
    reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), {});
    uint32_t signal;
    EXPECT_EQ(socket_->Receive(base::byte_span_from_ref(signal)),
              sizeof(signal));
    buffer_->params.bitstream_data_size =
        shmem_.mapped_size() - sizeof(AudioOutputBufferParameters);
    std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params_);

    // Send an incorrect buffer index, which will count as a missed callback.
    buffer_index = 123;
    EXPECT_EQ(socket_->Send(base::byte_span_from_ref(buffer_index)),
              sizeof(buffer_index));
    EXPECT_CALL(*mock_output_glitch_counter_,
                ReportMissedCallback(/*missed_callback = */ true,
                                     /*is_mixing = */ true));
    reader_->Read(output_bus.get(), true);
  }
}

TEST_F(SyncReaderTest, PropagatesDelay) {
  base::TimeDelta delay = base::Milliseconds(123);
  base::TimeTicks delay_timestamp = base::TimeTicks() + base::Days(5);

  reader_->RequestMoreData(delay, delay_timestamp, {});
  uint32_t signal;
  EXPECT_EQ(socket_->Receive(base::byte_span_from_ref(signal)), sizeof(signal));
  buffer_->params.bitstream_data_size =
      shmem_.mapped_size() - sizeof(AudioOutputBufferParameters);
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params_);

  EXPECT_EQ(buffer_->params.delay_us, delay.InMicroseconds());
  EXPECT_EQ(buffer_->params.delay_timestamp_us,
            (delay_timestamp - base::TimeTicks()).InMicroseconds());
}

TEST_F(SyncReaderTest, PropagatesGlitchInfo) {
  {
    media::AudioGlitchInfo glitch_info{.duration = base::Seconds(1),
                                       .count = 123};

    reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), glitch_info);
    uint32_t signal;
    EXPECT_EQ(socket_->Receive(base::byte_span_from_ref(signal)),
              sizeof(signal));
    buffer_->params.bitstream_data_size =
        shmem_.mapped_size() - sizeof(AudioOutputBufferParameters);
    std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params_);

    EXPECT_EQ(buffer_->params.glitch_duration_us,
              base::Seconds(1).InMicroseconds());
    EXPECT_EQ(buffer_->params.glitch_count, 123u);

    // Set a clearly incorrect buffer index. This means that the reader will
    // assume it's got the wrong data back from the Renderer process, and will
    // proceed to drop it. This causes a glitch.
    uint32_t buffer_index = 321;
    EXPECT_EQ(socket_->Send(base::byte_span_from_ref(buffer_index)),
              sizeof(buffer_index));
    reader_->Read(output_bus.get(), false);
  }

  {
    media::AudioGlitchInfo glitch_info{.duration = base::Seconds(2),
                                       .count = 246};

    reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), glitch_info);
    uint32_t signal;
    EXPECT_EQ(socket_->Receive(base::byte_span_from_ref(signal)),
              sizeof(signal));
    buffer_->params.bitstream_data_size =
        shmem_.mapped_size() - sizeof(AudioOutputBufferParameters);
    std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params_);

    // Since there was a glitch on the last read, this time there will be an
    // extra glitch reflected in the propagated info.
    EXPECT_EQ(
        buffer_->params.glitch_duration_us,
        (base::Seconds(2) + params_.GetBufferDuration()).InMicroseconds());
    EXPECT_EQ(buffer_->params.glitch_count, 246u + 1u);

    // This time, send the correct buffer index.
    uint32_t buffer_index = 2;
    EXPECT_EQ(socket_->Send(base::byte_span_from_ref(buffer_index)),
              sizeof(buffer_index));
    reader_->Read(output_bus.get(), false);
  }

  {
    media::AudioGlitchInfo glitch_info{.duration = base::Seconds(3),
                                       .count = 321};

    reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), glitch_info);
    uint32_t signal;
    EXPECT_EQ(socket_->Receive(base::byte_span_from_ref(signal)),
              sizeof(signal));
    buffer_->params.bitstream_data_size =
        shmem_.mapped_size() - sizeof(AudioOutputBufferParameters);
    std::unique_ptr<AudioBus> output_bus = AudioBus::Create(params_);

    // This time there should be no added glitches.
    EXPECT_EQ(buffer_->params.glitch_duration_us,
              base::Seconds(3).InMicroseconds());
    EXPECT_EQ(buffer_->params.glitch_count, 321u);
  }
}

}  // namespace
}  // namespace audio
