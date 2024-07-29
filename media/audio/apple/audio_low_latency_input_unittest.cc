// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <memory>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/apple/audio_low_latency_input.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Ge;
using ::testing::NotNull;

namespace media {

ACTION_P4(CheckCountAndPostQuitTask, count, limit, task_runner, closure) {
  if (++*count >= limit) {
    task_runner->PostTask(FROM_HERE, closure);
  }
}

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD4(OnData,
               void(const AudioBus* src,
                    base::TimeTicks capture_time,
                    double volume,
                    const AudioGlitchInfo& glitch_info));
  MOCK_METHOD0(OnError, void());
};

// This audio sink implementation should be used for manual tests only since
// the recorded data is stored on a raw binary data file.
// The last test (WriteToFileAudioSink) - which is disabled by default -
// can use this audio sink to store the captured data on a file for offline
// analysis.
class WriteToFileAudioSink : public AudioInputStream::AudioInputCallback {
 public:
  // Allocate space for ~10 seconds of data @ 48kHz in stereo:
  // 2 bytes per sample, 2 channels, 10ms @ 48kHz, 10 seconds <=> 1920000 bytes.
  static const int kMaxBufferSize = 2 * 2 * 480 * 100 * 10;

  explicit WriteToFileAudioSink(const char* file_name)
      : buffer_(0, kMaxBufferSize),
        file_(fopen(file_name, "wb")),
        bytes_to_write_(0) {
  }

  ~WriteToFileAudioSink() override {
    int bytes_written = 0;
    while (bytes_written < bytes_to_write_) {
      const uint8_t* chunk;
      int chunk_size;

      // Stop writing if no more data is available.
      if (!buffer_.GetCurrentChunk(&chunk, &chunk_size))
        break;

      // Write recorded data chunk to the file and prepare for next chunk.
      fwrite(chunk, 1, chunk_size, file_);
      buffer_.Seek(chunk_size);
      bytes_written += chunk_size;
    }
    fclose(file_);
  }

  // AudioInputStream::AudioInputCallback implementation.
  void OnData(const AudioBus* src,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    const int num_samples = src->frames() * src->channels();
    std::unique_ptr<int16_t> interleaved(new int16_t[num_samples]);
    src->ToInterleaved<SignedInt16SampleTypeTraits>(src->frames(),
                                                    interleaved.get());

    // Store data data in a temporary buffer to avoid making blocking
    // fwrite() calls in the audio callback. The complete buffer will be
    // written to file in the destructor.
    const int bytes_per_sample = sizeof(*interleaved);
    const int size = bytes_per_sample * num_samples;
    if (buffer_.Append((const uint8_t*)interleaved.get(), size)) {
      bytes_to_write_ += size;
    }
  }

  void OnError() override {}

 private:
  media::SeekableBuffer buffer_;
  raw_ptr<FILE> file_;
  int bytes_to_write_;
};

class MacAudioInputTest : public testing::Test {
 protected:
  MacAudioInputTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        audio_manager_(AudioManager::CreateForTesting(
            std::make_unique<TestAudioThread>())) {
    // Wait for the AudioManager to finish any initialization on the audio loop.
    base::RunLoop().RunUntilIdle();
  }

  ~MacAudioInputTest() override { audio_manager_->Shutdown(); }

  bool InputDevicesAvailable() {
#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)
    // TODO(crbug.com/40719640): macOS on ARM64 says it has devices, but won't
    // let any of them be opened or listed.
    return false;
#else
    return AudioDeviceInfoAccessorForTests(audio_manager_.get())
        .HasAudioInputDevices();
#endif
  }

  int HardwareSampleRateForDefaultInputDevice() {
    // Determine the default input device's sample-rate.
    AudioDeviceID input_device_id = kAudioObjectUnknown;
#if BUILDFLAG(IS_MAC)
    AudioManagerMac::GetDefaultInputDevice(&input_device_id);
#endif
    auto* manager = static_cast<AudioManagerApple*>(audio_manager_.get());
    return manager->HardwareSampleRateForDevice(input_device_id);
  }

  // Convenience method which creates a default AudioInputStream object using
  // a 10ms frame size and a sample rate which is set to the hardware sample
  // rate.
  AudioInputStream* CreateDefaultAudioInputStream() {
    int fs = HardwareSampleRateForDefaultInputDevice();
    int samples_per_packet = fs / 100;
#if BUILDFLAG(IS_MAC)
    ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Stereo();
#else
    ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Mono();
#endif
    AudioInputStream* ais = audio_manager_->MakeAudioInputStream(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        channel_layout_config, fs, samples_per_packet),
        AudioDeviceDescription::kDefaultDeviceId,
        base::BindRepeating(&MacAudioInputTest::OnLogMessage,
                            base::Unretained(this)));
    EXPECT_TRUE(ais);
    return ais;
  }

  // Convenience method which creates an AudioInputStream object with a
  // specified channel layout.
  AudioInputStream* CreateAudioInputStream(
      ChannelLayoutConfig channel_layout_config) {
    int fs = HardwareSampleRateForDefaultInputDevice();
    int samples_per_packet = fs / 100;
    AudioInputStream* ais = audio_manager_->MakeAudioInputStream(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        channel_layout_config, fs, samples_per_packet),
        AudioDeviceDescription::kDefaultDeviceId,
        base::BindRepeating(&MacAudioInputTest::OnLogMessage,
                            base::Unretained(this)));
    EXPECT_TRUE(ais);
    return ais;
  }

  void OnLogMessage(const std::string& message) { log_message_ = message; }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<AudioManager> audio_manager_;
  std::string log_message_;
};

// Test Create(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamCreateAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  ais->Close();
}

// Test Open(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamOpenAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
  ais->Close();
}

// Test Open(), Start(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamOpenStartAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais->Close();
}

// Test Open(), Start(), Stop(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamOpenStartStopAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais->Stop();
  ais->Close();
}

// Verify that recording starts and stops correctly in mono using mocked sink.
TEST_F(MacAudioInputTest, AUAudioInputStreamVerifyMonoRecording) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());

  int count = 0;

  // Create an audio input stream which records in mono.
  AudioInputStream* ais = CreateAudioInputStream(ChannelLayoutConfig::Mono());
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);

  MockAudioInputCallback sink;

  // We use 10ms packets and will run the test until ten packets are received.
  // All should contain valid packets of the same size and a valid delay
  // estimate.
  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnData(NotNull(), _, _, _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(
          &count, 10, task_environment_.GetMainThreadTaskRunner(),
          run_loop.QuitClosure()));
  ais->Start(&sink);
  run_loop.Run();
  ais->Stop();
  ais->Close();

  EXPECT_FALSE(log_message_.empty());
}

// Verify that recording starts and stops correctly in mono using mocked sink.
TEST_F(MacAudioInputTest, AUAudioInputStreamVerifyStereoRecording) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());

  int count = 0;

  // Create an audio input stream which records in stereo.
  AudioInputStream* ais = CreateAudioInputStream(ChannelLayoutConfig::Stereo());
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);

  MockAudioInputCallback sink;

  // We use 10ms packets and will run the test until ten packets are received.
  // All should contain valid packets of the same size and a valid delay
  // estimate.
  // TODO(henrika): http://crbug.com/154352 forced us to run the capture side
  // using a native buffer size of 128 audio frames and combine it with a FIFO
  // to match the requested size by the client. This change might also have
  // modified the delay estimates since the existing Ge(bytes_per_packet) for
  // parameter #4 does no longer pass. I am removing this restriction here to
  // ensure that we can land the patch but will revisit this test again when
  // more analysis of the delay estimates are done.
  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnData(NotNull(), _, _, _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(
          &count, 10, task_environment_.GetMainThreadTaskRunner(),
          run_loop.QuitClosure()));
  ais->Start(&sink);
  run_loop.Run();
  ais->Stop();
  ais->Close();

  EXPECT_FALSE(log_message_.empty());
}

// This test is intended for manual tests and should only be enabled
// when it is required to store the captured data on a local file.
// By default, GTest will print out YOU HAVE 1 DISABLED TEST.
// To include disabled tests in test execution, just invoke the test program
// with --gtest_also_run_disabled_tests or set the GTEST_ALSO_RUN_DISABLED_TESTS
// environment variable to a value greater than 0.
TEST_F(MacAudioInputTest, DISABLED_AUAudioInputStreamRecordToFile) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  const char* file_name = "out_stereo_10sec.pcm";

  int fs = HardwareSampleRateForDefaultInputDevice();
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);

  fprintf(stderr, "               File name  : %s\n", file_name);
  fprintf(stderr, "               Sample rate: %d\n", fs);
  WriteToFileAudioSink file_sink(file_name);
  fprintf(stderr, "               >> Speak into the mic while recording...\n");
  ais->Start(&file_sink);
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());
  ais->Stop();
  fprintf(stderr, "               >> Recording has stopped.\n");
  ais->Close();
}

TEST(MacAudioInputUpmixerTest, Upmix16bit) {
  constexpr int kNumFrames = 512;
  constexpr int kBytesPerSample = sizeof(int16_t);
  int16_t mono[kNumFrames];
  int16_t stereo[kNumFrames * 2];

  // Fill the mono buffer and the first half of the stereo buffer with data
  for (int i = 0; i != kNumFrames; ++i) {
    mono[i] = i;
    stereo[i] = i;
  }

  AudioBuffer audio_buffer;
  audio_buffer.mNumberChannels = 2;
  audio_buffer.mDataByteSize = kNumFrames * kBytesPerSample * 2;
  audio_buffer.mData = stereo;
  AUAudioInputStream::UpmixMonoToStereoInPlace(&audio_buffer, kBytesPerSample);

  // Assert that the samples have been distributed properly
  for (int i = 0; i != kNumFrames; ++i) {
    ASSERT_EQ(mono[i], stereo[i * 2]);
    ASSERT_EQ(mono[i], stereo[i * 2 + 1]);
  }
}

TEST(MacAudioInputUpmixerTest, Upmix32bit) {
  constexpr int kNumFrames = 512;
  constexpr int kBytesPerSample = sizeof(int32_t);
  int32_t mono[kNumFrames];
  int32_t stereo[kNumFrames * 2];

  // Fill the mono buffer and the first half of the stereo buffer with data
  for (int i = 0; i != kNumFrames; ++i) {
    mono[i] = i;
    stereo[i] = i;
  }

  AudioBuffer audio_buffer;
  audio_buffer.mNumberChannels = 2;
  audio_buffer.mDataByteSize = kNumFrames * kBytesPerSample * 2;
  audio_buffer.mData = stereo;
  AUAudioInputStream::UpmixMonoToStereoInPlace(&audio_buffer, kBytesPerSample);

  // Assert that the samples have been distributed properly
  for (int i = 0; i != kNumFrames; ++i) {
    ASSERT_EQ(mono[i], stereo[i * 2]);
    ASSERT_EQ(mono[i], stereo[i * 2 + 1]);
  }
}

}  // namespace media
