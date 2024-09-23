// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_debug_file_writer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_bus_pool.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

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

// Serves only to expose the protected Create method of AudioDebugFileWriter.
class AudioDebugFileWriterUnderTest : public AudioDebugFileWriter {
 public:
  // Should not be instantiated
  AudioDebugFileWriterUnderTest() = delete;

  static Ptr Create(const AudioParameters& params,
                    base::File file,
                    std::unique_ptr<AudioBusPool> audio_bus_pool) {
    return AudioDebugFileWriter::Create(params, std::move(file),
                                        std::move(audio_bus_pool));
  }
};

class MockAudioBusPool : public AudioBusPool {
 public:
  explicit MockAudioBusPool(const AudioParameters& params) : params_(params) {}

  MockAudioBusPool(const MockAudioBusPool&) = delete;
  MockAudioBusPool& operator=(const MockAudioBusPool&) = delete;
  ~MockAudioBusPool() override = default;

  std::unique_ptr<AudioBus> GetAudioBus() override {
    if (audio_bus_to_return_) {
      return std::move(audio_bus_to_return_);
    }
    return AudioBus::Create(params_);
  }

  MOCK_METHOD1(OnInsertAudioBus, void(AudioBus*));

  void InsertAudioBus(std::unique_ptr<AudioBus> audio_bus) override {
    OnInsertAudioBus(audio_bus.get());
  }

  void SetNextAudioBus(std::unique_ptr<AudioBus> audio_bus) {
    audio_bus_to_return_ = std::move(audio_bus);
  }

 private:
  const AudioParameters params_;

  std::unique_ptr<AudioBus> audio_bus_to_return_;
};

}  // namespace

// <channel layout, sample rate, frames per buffer, number of buffer writes
typedef std::tuple<ChannelLayoutConfig, int, int, int>
    AudioDebugFileWriterTestData;

class AudioDebugFileWriterTest
    : public testing::TestWithParam<AudioDebugFileWriterTestData> {
 public:
  explicit AudioDebugFileWriterTest(
      base::test::TaskEnvironment::ThreadPoolExecutionMode execution_mode)
      : task_environment_(base::test::TaskEnvironment::MainThreadType::DEFAULT,
                          execution_mode),
        debug_writer_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
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

  AudioDebugFileWriterTest(const AudioDebugFileWriterTest&) = delete;
  AudioDebugFileWriterTest& operator=(const AudioDebugFileWriterTest&) = delete;

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
      EXPECT_LE(std::abs(source_interleaved[i] - result_interleaved[i]), 1)
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

  void DoDebugRecording(bool expect_buses_returned_to_pool = true) {
    EXPECT_CALL(*mock_audio_bus_pool_, OnInsertAudioBus(_))
        .Times(expect_buses_returned_to_pool ? writes_ : 0);

    for (int i = 0; i < writes_; ++i) {
      std::unique_ptr<AudioBus> bus =
          AudioBus::Create(params_.channels(), params_.frames_per_buffer());

      bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
          source_interleaved_.get() +
              i * params_.channels() * params_.frames_per_buffer(),
          params_.frames_per_buffer());

      debug_writer_->Write(*bus);
    }
  }

  void CreateDebugWriter(base::File file) {
    auto audio_bus_pool = std::make_unique<MockAudioBusPool>(params_);
    mock_audio_bus_pool_ = audio_bus_pool.get();
    debug_writer_ = AudioDebugFileWriterUnderTest::Create(
        params_, std::move(file), std::move(audio_bus_pool));
  }

  void DestroyDebugWriter() {
    // Drop unowned reference before deleting owner.
    mock_audio_bus_pool_ = nullptr;
    debug_writer_.reset();
  }

 protected:
  // The test task environment.
  base::test::TaskEnvironment task_environment_;

  // Writer under test.
  AudioDebugFileWriter::Ptr debug_writer_;

  // Pointer to the AudioBusPool of the most recently created writer.
  raw_ptr<MockAudioBusPool> mock_audio_bus_pool_;

  // AudioBus parameters.
  AudioParameters params_;

  // Number of times to write AudioBus to the file.
  int writes_;

  // Number of samples in the source data.
  int source_samples_;

  // Source data.
  std::unique_ptr<int16_t[]> source_interleaved_;
};

class AudioDebugFileWriterBehavioralTest : public AudioDebugFileWriterTest {};

class AudioDebugFileWriterSingleThreadTest : public AudioDebugFileWriterTest {
 public:
  AudioDebugFileWriterSingleThreadTest()
      : AudioDebugFileWriterTest(
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}
};

TEST_P(AudioDebugFileWriterTest, WaveRecordingTest) {
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  base::File file = OpenFile(file_path);
  ASSERT_TRUE(file.IsValid());

  CreateDebugWriter(std::move(file));
  DoDebugRecording();
  DestroyDebugWriter();

  task_environment_.RunUntilIdle();

  VerifyRecording(file_path);

  if (::testing::Test::HasFailure()) {
    LOG(ERROR) << "Test failed; keeping recording(s) at ["
               << file_path.value().c_str() << "].";
  } else {
    ASSERT_TRUE(base::DeleteFile(file_path));
  }
}

TEST_P(AudioDebugFileWriterBehavioralTest, ShouldReuseAudioBusesWithPool) {
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  base::File file = OpenFile(file_path);
  ASSERT_TRUE(file.IsValid());

  CreateDebugWriter(std::move(file));

  // Set a specific audio bus to be returned by the pool.
  std::unique_ptr<AudioBus> reference_audio_bus = AudioBus::Create(params_);
  AudioBus* reference_audio_bus_ptr = reference_audio_bus.get();
  mock_audio_bus_pool_->SetNextAudioBus(std::move(reference_audio_bus));

  // Expect that same audio bus to be returned to the pool.
  EXPECT_CALL(*mock_audio_bus_pool_, OnInsertAudioBus(reference_audio_bus_ptr));

  std::unique_ptr<AudioBus> bus = AudioBus::Create(params_);
  bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
      source_interleaved_.get(), params_.frames_per_buffer());

  debug_writer_->Write(*bus);
  task_environment_.RunUntilIdle();
}

TEST_P(AudioDebugFileWriterSingleThreadTest,
       DeletedBeforeRecordingFinishedOnFileThread) {
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  base::File file = OpenFile(file_path);
  ASSERT_TRUE(file.IsValid());

  CreateDebugWriter(std::move(file));
  DoDebugRecording();
  DestroyDebugWriter();

  task_environment_.RunUntilIdle();

  VerifyRecording(file_path);

  if (::testing::Test::HasFailure()) {
    LOG(ERROR) << "Test failed; keeping recording(s) at ["
               << file_path.value().c_str() << "].";
  } else {
    ASSERT_TRUE(base::DeleteFile(file_path));
  }
}

TEST_P(AudioDebugFileWriterBehavioralTest, StartWithInvalidFile) {
  base::File file;  // Invalid file, recording should not crash
  CreateDebugWriter(std::move(file));

  DoDebugRecording(/*expect_buses_returned_to_pool = */ false);
  task_environment_.RunUntilIdle();
}

TEST_P(AudioDebugFileWriterBehavioralTest, StartStopStartStop) {
  base::FilePath file_path1;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path1));
  base::File file1 = OpenFile(file_path1);
  ASSERT_TRUE(file1.IsValid());

  base::FilePath file_path2;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path2));
  base::File file2 = OpenFile(file_path2);
  ASSERT_TRUE(file2.IsValid());

  CreateDebugWriter(std::move(file1));
  DoDebugRecording();
  DestroyDebugWriter();

  CreateDebugWriter(std::move(file2));
  DoDebugRecording();
  DestroyDebugWriter();

  task_environment_.RunUntilIdle();

  VerifyRecording(file_path1);
  VerifyRecording(file_path2);

  if (::testing::Test::HasFailure()) {
    LOG(ERROR) << "Test failed; keeping recording(s) at ["
               << file_path1.value().c_str() << ", "
               << file_path2.value().c_str() << "].";
  } else {
    ASSERT_TRUE(base::DeleteFile(file_path1));
    ASSERT_TRUE(base::DeleteFile(file_path2));
  }
}

TEST_P(AudioDebugFileWriterBehavioralTest, DestroyStarted) {
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  base::File file = OpenFile(file_path);
  ASSERT_TRUE(file.IsValid());
  CreateDebugWriter(std::move(file));
  DestroyDebugWriter();
  task_environment_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    AudioDebugFileWriterTest,
    AudioDebugFileWriterTest,
    // Using 10ms frames per buffer everywhere.
    testing::Values(
        // No writes.
        std::make_tuple(ChannelLayoutConfig::Mono(), 44100, 44100 / 100, 0),
        // 1 write of mono.
        std::make_tuple(ChannelLayoutConfig::Mono(), 44100, 44100 / 100, 1),
        // 1 second of mono.
        std::make_tuple(ChannelLayoutConfig::Mono(), 44100, 44100 / 100, 100),
        // 1 second of mono, higher rate.
        std::make_tuple(ChannelLayoutConfig::Mono(), 48000, 48000 / 100, 100),
        // 1 second of stereo.
        std::make_tuple(ChannelLayoutConfig::Stereo(), 44100, 44100 / 100, 100),
        // 15 seconds of stereo, higher rate.
        std::make_tuple(ChannelLayoutConfig::Stereo(),
                        48000,
                        48000 / 100,
                        1500)));

INSTANTIATE_TEST_SUITE_P(
    AudioDebugFileWriterBehavioralTest,
    AudioDebugFileWriterBehavioralTest,
    // Using 10ms frames per buffer everywhere.
    testing::Values(
        // No writes.
        std::make_tuple(ChannelLayoutConfig::Mono(), 44100, 44100 / 100, 100)));

INSTANTIATE_TEST_SUITE_P(
    AudioDebugFileWriterSingleThreadTest,
    AudioDebugFileWriterSingleThreadTest,
    // Using 10ms frames per buffer everywhere.
    testing::Values(
        // No writes.
        std::make_tuple(ChannelLayoutConfig::Mono(), 44100, 44100 / 100, 100)));
}  // namespace media
