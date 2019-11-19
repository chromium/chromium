// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_byteorder.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/audio/audio_debug_file_writer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

static const uint16_t kBytesPerSample = sizeof(uint16_t);
static const uint16_t kPcmEncoding = 1;
static const size_t kWavHeaderSize = 44;

uint16_t ReadLE2(const char* buf) {
  return static_cast<uint8_t>(buf[0]) | (static_cast<uint8_t>(buf[1]) << 8);
}

uint32_t ReadLE4(const char* buf) {
  return static_cast<uint8_t>(buf[0]) | (static_cast<uint8_t>(buf[1]) << 8) |
         (static_cast<uint8_t>(buf[2]) << 16) |
         (static_cast<uint8_t>(buf[3]) << 24);
}

base::File OpenFile(const base::FilePath& file_path) {
  return base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
}

}  // namespace

// <channel layout, sample rate, frames per buffer, number of buffer writes
typedef std::tuple<ChannelLayout, int, int, int> AudioDebugFileWriterTestData;

class AudioDebugFileWriterTest
    : public testing::TestWithParam<AudioDebugFileWriterTestData> {
 public:
  explicit AudioDebugFileWriterTest(
      base::test::TaskEnvironment::ThreadPoolExecutionMode execution_mode)
      : task_environment_(base::test::TaskEnvironment::MainThreadType::DEFAULT,
                          execution_mode),
        params_(AudioParameters::Format::AUDIO_PCM_LINEAR,
                std::get<0>(GetParam()),
                std::get<1>(GetParam()),
                std::get<2>(GetParam())),
        writes_(std::get<3>(GetParam())),
        source_samples_(params_.frames_per_buffer() * params_.channels() *
                        writes_),
        source_interleaved_(source_samples_ ? new int16_t[source_samples_]
                                            : nullptr) {
    InitSourceInterleaved(source_interleaved_.get(), source_samples_);
  }
  AudioDebugFileWriterTest()
      : AudioDebugFileWriterTest(
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC) {}

 protected:
  virtual ~AudioDebugFileWriterTest() = default;

  static void InitSourceInterleaved(int16_t* source_interleaved,
                                    int source_samples) {
    if (source_samples) {
      // equal steps to cover int16_t range of values
      int16_t step = 0xffff / source_samples;
      int16_t val = std::numeric_limits<int16_t>::min();
      for (int i = 0; i < source_samples; ++i, val += step)
        source_interleaved[i] = val;
    }
  }

  static void VerifyHeader(const char (&wav_header)[kWavHeaderSize],
                           const AudioParameters& params,
                           int writes,
                           int64_t file_length) {
    uint32_t block_align = params.channels() * kBytesPerSample;
    uint32_t data_size =
        static_cast<uint32_t>(params.frames_per_buffer() * params.channels() *
                              writes * kBytesPerSample);
    // Offset Length  Content
    //  0      4     "RIFF"
    EXPECT_EQ(0, strncmp(wav_header, "RIFF", 4));
    //  4      4     <file length - 8>
    ASSERT_GT(file_length, 8);
    EXPECT_EQ(static_cast<uint64_t>(file_length - 8), ReadLE4(wav_header + 4));
    EXPECT_EQ(static_cast<uint32_t>(data_size + kWavHeaderSize - 8),
              ReadLE4(wav_header + 4));
    //  8      4     "WAVE"
    // 12      4     "fmt "
    EXPECT_EQ(0, strncmp(wav_header + 8, "WAVEfmt ", 8));
    // 16      4     <length of the fmt data> (=16)
    EXPECT_EQ(16U, ReadLE4(wav_header + 16));
    // 20      2     <WAVE file encoding tag>
    EXPECT_EQ(kPcmEncoding, ReadLE2(wav_header + 20));
    // 22      2     <channels>
    EXPECT_EQ(params.channels(), ReadLE2(wav_header + 22));
    // 24      4     <sample rate>
    EXPECT_EQ(static_cast<uint32_t>(params.sample_rate()),
              ReadLE4(wav_header + 24));
    // 28      4     <bytes per second> (sample rate * block align)
    EXPECT_EQ(static_cast<uint32_t>(params.sample_rate()) * block_align,
              ReadLE4(wav_header + 28));
    // 32      2     <block align>  (channels * bits per sample / 8)
    EXPECT_EQ(block_align, ReadLE2(wav_header + 32));
    // 34      2     <bits per sample>
    EXPECT_EQ(kBytesPerSample * 8, ReadLE2(wav_header + 34));
    // 36      4     "data"
    EXPECT_EQ(0, strncmp(wav_header + 36, "data", 4));
    // 40      4     <sample data size(n)>
    EXPECT_EQ(data_size, ReadLE4(wav_header + 40));
  }

  // |result_interleaved| is expected to be little-endian.
  static void VerifyDataRecording(const int16_t* source_interleaved,
                                  const int16_t* result_interleaved,
                                  int16_t source_samples) {
    // Allow mismatch by 1 due to rounding error in int->float->int
    // calculations.
    for (int i = 0; i < source_samples; ++i)
      EXPECT_LE(std::abs(static_cast<int16_t>(
                             base::ByteSwapToLE16(source_interleaved[i])) -
                         result_interleaved[i]),
                1)
          << "i = " << i << " source " << source_interleaved[i] << " result "
          << result_interleaved[i];
  }

  void VerifyRecording(const base::FilePath& file_path) {
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());

    char wav_header[kWavHeaderSize];
    EXPECT_EQ(file.Read(0, wav_header, kWavHeaderSize),
              static_cast<int>(kWavHeaderSize));
    VerifyHeader(wav_header, params_, writes_, file.GetLength());

    if (source_samples_ > 0) {
      std::unique_ptr<int16_t[]> result_interleaved(
          new int16_t[source_samples_]);
      memset(result_interleaved.get(), 0, source_samples_ * kBytesPerSample);

      // Recording is read from file as a byte sequence, so it stored as
      // little-endian.
      int read = file.Read(kWavHeaderSize,
                           reinterpret_cast<char*>(result_interleaved.get()),
                           source_samples_ * kBytesPerSample);
      EXPECT_EQ(static_cast<int>(file.GetLength() - kWavHeaderSize), read);

      VerifyDataRecording(source_interleaved_.get(), result_interleaved.get(),
                          source_samples_);
    }
  }

  void DoDebugRecording() {
    for (int i = 0; i < writes_; ++i) {
      std::unique_ptr<AudioBus> bus =
          AudioBus::Create(params_.channels(), params_.frames_per_buffer());

      bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
          source_interleaved_.get() +
              i * params_.channels() * params_.frames_per_buffer(),
          params_.frames_per_buffer());

      debug_writer_->Write(std::move(bus));
    }
  }

  void RecordAndVerifyOnce() {
    base::FilePath file_path;
    ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
    base::File file = OpenFile(file_path);
    ASSERT_TRUE(file.IsValid());

    debug_writer_->Start(std::move(file));

    DoDebugRecording();

    debug_writer_->Stop();

    task_environment_.RunUntilIdle();

    VerifyRecording(file_path);

    if (::testing::Test::HasFailure()) {
      LOG(ERROR) << "Test failed; keeping recording(s) at ["
                 << file_path.value().c_str() << "].";
    } else {
      ASSERT_TRUE(base::DeleteFile(file_path, false));
    }
  }

 protected:
  // The test task environment.
  base::test::TaskEnvironment task_environment_;

  // Writer under test.
  std::unique_ptr<AudioDebugFileWriter> debug_writer_;

  // AudioBus parameters.
  AudioParameters params_;

  // Number of times to write AudioBus to the file.
  int writes_;

  // Number of samples in the source data.
  int source_samples_;

  // Source data.
  std::unique_ptr<int16_t[]> source_interleaved_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDebugFileWriterTest);
};

class AudioDebugFileWriterBehavioralTest : public AudioDebugFileWriterTest {};

class AudioDebugFileWriterSingleThreadTest : public AudioDebugFileWriterTest {
 public:
  AudioDebugFileWriterSingleThreadTest()
      : AudioDebugFileWriterTest(
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}
};

TEST_P(AudioDebugFileWriterTest, WaveRecordingTest) {
  debug_writer_.reset(new AudioDebugFileWriter(params_));
  RecordAndVerifyOnce();
}

TEST_P(AudioDebugFileWriterSingleThreadTest,
       DeletedBeforeRecordingFinishedOnFileThread) {
  debug_writer_.reset(new AudioDebugFileWriter(params_));

  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  base::File file = OpenFile(file_path);
  ASSERT_TRUE(file.IsValid());

  debug_writer_->Start(std::move(file));

  DoDebugRecording();

  debug_writer_.reset();

  task_environment_.RunUntilIdle();

  VerifyRecording(file_path);

  if (::testing::Test::HasFailure()) {
    LOG(ERROR) << "Test failed; keeping recording(s) at ["
               << file_path.value().c_str() << "].";
  } else {
    ASSERT_TRUE(base::DeleteFile(file_path, false));
  }
}

TEST_P(AudioDebugFileWriterBehavioralTest, StartWithInvalidFile) {
  debug_writer_.reset(new AudioDebugFileWriter(params_));
  base::File file;  // Invalid file, recording should not crash
  debug_writer_->Start(std::move(file));
  DoDebugRecording();
}

TEST_P(AudioDebugFileWriterBehavioralTest, StartStopStartStop) {
  debug_writer_.reset(new AudioDebugFileWriter(params_));
  RecordAndVerifyOnce();
  RecordAndVerifyOnce();
}

TEST_P(AudioDebugFileWriterBehavioralTest, DestroyNotStarted) {
  debug_writer_.reset(new AudioDebugFileWriter(params_));
  debug_writer_.reset();
}

TEST_P(AudioDebugFileWriterBehavioralTest, DestroyStarted) {
  debug_writer_.reset(new AudioDebugFileWriter(params_));
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  base::File file = OpenFile(file_path);
  ASSERT_TRUE(file.IsValid());
  debug_writer_->Start(std::move(file));
  debug_writer_.reset();
}

INSTANTIATE_TEST_SUITE_P(
    AudioDebugFileWriterTest,
    AudioDebugFileWriterTest,
    // Using 10ms frames per buffer everywhere.
    testing::Values(
        // No writes.
        std::make_tuple(ChannelLayout::CHANNEL_LAYOUT_MONO,
                        44100,
                        44100 / 100,
                        0),
        // 1 write of mono.
        std::make_tuple(ChannelLayout::CHANNEL_LAYOUT_MONO,
                        44100,
                        44100 / 100,
                        1),
        // 1 second of mono.
        std::make_tuple(ChannelLayout::CHANNEL_LAYOUT_MONO,
                        44100,
                        44100 / 100,
                        100),
        // 1 second of mono, higher rate.
        std::make_tuple(ChannelLayout::CHANNEL_LAYOUT_MONO,
                        48000,
                        48000 / 100,
                        100),
        // 1 second of stereo.
        std::make_tuple(ChannelLayout::CHANNEL_LAYOUT_STEREO,
                        44100,
                        44100 / 100,
                        100),
        // 15 seconds of stereo, higher rate.
        std::make_tuple(ChannelLayout::CHANNEL_LAYOUT_STEREO,
                        48000,
                        48000 / 100,
                        1500)));

INSTANTIATE_TEST_SUITE_P(AudioDebugFileWriterBehavioralTest,
                         AudioDebugFileWriterBehavioralTest,
                         // Using 10ms frames per buffer everywhere.
                         testing::Values(
                             // No writes.
                             std::make_tuple(ChannelLayout::CHANNEL_LAYOUT_MONO,
                                             44100,
                                             44100 / 100,
                                             100)));

INSTANTIATE_TEST_SUITE_P(AudioDebugFileWriterSingleThreadTest,
                         AudioDebugFileWriterSingleThreadTest,
                         // Using 10ms frames per buffer everywhere.
                         testing::Values(
                             // No writes.
                             std::make_tuple(ChannelLayout::CHANNEL_LAYOUT_MONO,
                                             44100,
                                             44100 / 100,
                                             100)));
}  // namespace media
