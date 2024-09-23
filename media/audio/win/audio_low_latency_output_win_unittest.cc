// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/win/audio_low_latency_output_win.h"

#include <windows.h>

#include <mmsystem.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/test_audio_thread.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/decoder_buffer.h"
#include "media/base/seekable_buffer.h"
#include "media/base/test_data_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;

namespace media {

static const int kBitsPerSample = 16;
static const size_t kMaxDeltaSamples = 1000;
static const char kDeltaTimeMsFileName[] = "delta_times_ms.txt";

MATCHER_P(HasValidDelay, value, "") {
  // It is difficult to come up with a perfect test condition for the delay
  // estimation. For now, verify that the produced output delay is always
  // larger than the selected buffer size.
  return arg >= value;
}

// This audio source implementation should be used for manual tests only since
// it takes about 20 seconds to play out a file.
class ReadFromFileAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit ReadFromFileAudioSource(const std::string& name)
      : pos_(0),
        previous_call_time_(base::TimeTicks::Now()),
        text_file_(nullptr),
        elements_to_write_(0) {
    // Reads a test file from media/test/data directory.
    file_ = ReadTestDataFile(name);

    // Creates an array that will store delta times between callbacks.
    // The content of this array will be written to a text file at
    // destruction and can then be used for off-line analysis of the exact
    // timing of callbacks. The text file will be stored in media/test/data.
    delta_times_.reset(new int[kMaxDeltaSamples]);
  }

  ~ReadFromFileAudioSource() override {
    // Get complete file path to output file in directory containing
    // media_unittests.exe.
    base::FilePath file_name;
    EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &file_name));
    file_name = file_name.AppendASCII(kDeltaTimeMsFileName);

    EXPECT_TRUE(!text_file_);
    text_file_ = base::OpenFile(file_name, "wt");
    DLOG_IF(ERROR, !text_file_) << "Failed to open log file.";

    // Write the array which contains delta times to a text file.
    size_t elements_written = 0;
    while (elements_written < elements_to_write_) {
      fprintf(text_file_.get(), "%d\n", delta_times_[elements_written]);
      ++elements_written;
    }

    base::CloseFile(text_file_);
  }

  // AudioOutputStream::AudioSourceCallback implementation.
  int OnMoreData(base::TimeDelta /* delay */,
                 base::TimeTicks /* delay_timestamp */,
                 const AudioGlitchInfo& /* glitch_info */,
                 AudioBus* dest) override {
    // Store time difference between two successive callbacks in an array.
    // These values will be written to a file in the destructor.
    const base::TimeTicks now_time = base::TimeTicks::Now();
    const int diff = (now_time - previous_call_time_).InMilliseconds();
    previous_call_time_ = now_time;
    if (elements_to_write_ < kMaxDeltaSamples) {
      delta_times_[elements_to_write_] = diff;
      ++elements_to_write_;
    }

    int max_size = dest->frames() * dest->channels() * kBitsPerSample / 8;

    // Use samples read from a data file and fill up the audio buffer
    // provided to us in the callback.
    if (pos_ + max_size > file_size())
      max_size = file_size() - pos_;
    int frames = max_size / (dest->channels() * kBitsPerSample / 8);
    if (max_size) {
      static_assert(kBitsPerSample == 16, "FromInterleaved expects 2 bytes.");
      dest->FromInterleaved<SignedInt16SampleTypeTraits>(
          reinterpret_cast<const int16_t*>(file_->data() + pos_), frames);
      pos_ += max_size;
    }
    return frames;
  }

  void OnError(ErrorType type) override {}

  int file_size() { return base::checked_cast<int>(file_->size()); }

 private:
  scoped_refptr<DecoderBuffer> file_;
  std::unique_ptr<int[]> delta_times_;
  int pos_;
  base::TimeTicks previous_call_time_;
  raw_ptr<FILE> text_file_;
  size_t elements_to_write_;
};

static bool ExclusiveModeIsEnabled() {
  return (WASAPIAudioOutputStream::GetShareMode() ==
          AUDCLNT_SHAREMODE_EXCLUSIVE);
}

static bool HasCoreAudioAndOutputDevices(AudioManager* audio_man) {
  // The low-latency (WASAPI-based) version requires Windows Vista or higher.
  // TODO(henrika): note that we use Wave today to query the number of
  // existing output devices.
  return CoreAudioUtil::IsSupported() &&
         AudioDeviceInfoAccessorForTests(audio_man).HasAudioOutputDevices();
}

// Convenience method which creates a default AudioOutputStream object but
// also allows the user to modify the default settings.
class AudioOutputStreamWrapper {
 public:
  explicit AudioOutputStreamWrapper(AudioManager* audio_manager)
      : audio_man_(audio_manager),
        format_(AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    AudioParameters preferred_params;
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(
        AudioDeviceDescription::kDefaultDeviceId, true, &preferred_params)));
    channels_ = preferred_params.channels();
    channel_layout_ = preferred_params.channel_layout();
    sample_rate_ = preferred_params.sample_rate();
    samples_per_packet_ = preferred_params.frames_per_buffer();
  }

  ~AudioOutputStreamWrapper() {}

  // Creates AudioOutputStream object using default parameters.
  AudioOutputStream* Create() { return CreateOutputStream(); }

  // Creates AudioOutputStream object using non-default parameters where the
  // frame size is modified.
  AudioOutputStream* Create(int samples_per_packet) {
    samples_per_packet_ = samples_per_packet;
    return CreateOutputStream();
  }

  // Creates AudioOutputStream object using non-default parameters where the
  // sample rate and frame size are modified.
  AudioOutputStream* Create(int sample_rate, int samples_per_packet) {
    sample_rate_ = sample_rate;
    samples_per_packet_ = samples_per_packet;
    return CreateOutputStream();
  }

  // Creates AudioOutputStream object using non-default parameters where the
  // sample rate, frame size and audio offload are modified.
  AudioOutputStream* Create(int sample_rate,
                            int samples_per_packet,
                            bool audio_offload) {
    sample_rate_ = sample_rate;
    samples_per_packet_ = samples_per_packet;
    AudioParameters::HardwareCapabilities hardware_cap(0, true);
    hardware_cap.require_audio_offload = true;
    hardware_capabilities_ = hardware_cap;

    return CreateOutputStream();
  }

  AudioParameters::Format format() const { return format_; }
  int channels() const { return channels_; }
  int sample_rate() const { return sample_rate_; }
  int samples_per_packet() const { return samples_per_packet_; }

 private:
  AudioOutputStream* CreateOutputStream() {
    AudioParameters params(format_, {channel_layout_, channels_}, sample_rate_,
                           samples_per_packet_);
    if (hardware_capabilities_) {
      params.set_hardware_capabilities(hardware_capabilities_.value());
    }
    DVLOG(1) << params.AsHumanReadableString();
    AudioOutputStream* aos = audio_man_->MakeAudioOutputStream(
        params, std::string(), AudioManager::LogCallback());
    EXPECT_TRUE(aos);
    return aos;
  }

  raw_ptr<AudioManager> audio_man_;
  AudioParameters::Format format_;
  int channels_;
  ChannelLayout channel_layout_;
  int sample_rate_;
  int samples_per_packet_;
  std::optional<AudioParameters::HardwareCapabilities> hardware_capabilities_;
};

// Convenience method which creates a default AudioOutputStream object.
static AudioOutputStream* CreateDefaultAudioOutputStream(
    AudioManager* audio_manager) {
  AudioOutputStreamWrapper aosw(audio_manager);
  AudioOutputStream* aos = aosw.Create();
  return aos;
}

class WASAPIAudioOutputStreamTest : public ::testing::Test {
 public:
  WASAPIAudioOutputStreamTest() {
    audio_manager_ =
        AudioManager::CreateForTesting(std::make_unique<TestAudioThread>());
    base::RunLoop().RunUntilIdle();
  }
  ~WASAPIAudioOutputStreamTest() override { audio_manager_->Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<AudioManager> audio_manager_;
};

// Test Create(), Close() calling sequence.
TEST_F(WASAPIAudioOutputStreamTest, CreateAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()));
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager_.get());
  aos->Close();
}

// Test Open(), Close() calling sequence.
TEST_F(WASAPIAudioOutputStreamTest, OpenAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()));
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager_.get());
  EXPECT_TRUE(aos->Open());
  aos->Close();
}

// Test Open(), Start(), Close() calling sequence.
TEST_F(WASAPIAudioOutputStreamTest, OpenStartAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()));
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager_.get());
  EXPECT_TRUE(aos->Open());
  MockAudioSourceCallback source;
  EXPECT_CALL(source, OnError(_)).Times(0);
  aos->Start(&source);
  aos->Close();
}

// Test Open(), Start(), Stop(), Close() calling sequence.
TEST_F(WASAPIAudioOutputStreamTest, OpenStartStopAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()));
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager_.get());
  EXPECT_TRUE(aos->Open());
  MockAudioSourceCallback source;
  EXPECT_CALL(source, OnError(_)).Times(0);
  aos->Start(&source);
  aos->Stop();
  aos->Close();
}

// Test SetVolume(), GetVolume()
TEST_F(WASAPIAudioOutputStreamTest, Volume) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()));
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager_.get());

  // Initial volume should be full volume (1.0).
  double volume = 0.0;
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  // Verify some valid volume settings.
  aos->SetVolume(0.0);
  aos->GetVolume(&volume);
  EXPECT_EQ(0.0, volume);

  aos->SetVolume(0.5);
  aos->GetVolume(&volume);
  EXPECT_EQ(0.5, volume);

  aos->SetVolume(1.0);
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  // Ensure that invalid volume setting have no effect.
  aos->SetVolume(1.5);
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  aos->SetVolume(-0.5);
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  aos->Close();
}

// Test some additional calling sequences.
TEST_F(WASAPIAudioOutputStreamTest, MiscCallingSequences) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()));

  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager_.get());
  WASAPIAudioOutputStream* waos = static_cast<WASAPIAudioOutputStream*>(aos);

  // Open(), Open() is a valid calling sequence (second call does nothing).
  EXPECT_TRUE(aos->Open());
  EXPECT_TRUE(aos->Open());

  MockAudioSourceCallback source;

  // Start(), Start() is a valid calling sequence (second call does nothing).
  aos->Start(&source);
  EXPECT_TRUE(waos->started());
  aos->Start(&source);
  EXPECT_TRUE(waos->started());

  // Stop(), Stop() is a valid calling sequence (second call does nothing).
  aos->Stop();
  EXPECT_FALSE(waos->started());
  aos->Stop();
  EXPECT_FALSE(waos->started());

  // Start(), Stop(), Start(), Stop().
  aos->Start(&source);
  EXPECT_TRUE(waos->started());
  aos->Stop();
  EXPECT_FALSE(waos->started());
  aos->Start(&source);
  EXPECT_TRUE(waos->started());
  aos->Stop();
  EXPECT_FALSE(waos->started());

  aos->Close();
}

// Use preferred packet size and verify that rendering starts.
TEST_F(WASAPIAudioOutputStreamTest, ValidPacketSize) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()));

  MockAudioSourceCallback source;
  // Create default WASAPI output stream which plays out in stereo using
  // the shared mixing rate. The default buffer size is 10ms.
  AudioOutputStreamWrapper aosw(audio_manager_.get());
  AudioOutputStream* aos = aosw.Create();
  EXPECT_TRUE(aos->Open());

  base::RunLoop loop;
  // Derive the expected duration of each packet.
  base::TimeDelta packet_duration = base::Seconds(
      static_cast<double>(aosw.samples_per_packet()) / aosw.sample_rate());

  // Wait for the first callback and verify its parameters.  Ignore any
  // subsequent callbacks that might arrive.
  EXPECT_CALL(source, OnMoreData(HasValidDelay(packet_duration), _,
                                 AudioGlitchInfo(), NotNull()))
      .WillOnce(DoAll(base::test::RunClosure(loop.QuitWhenIdleClosure()),
                      Return(aosw.samples_per_packet())))
      .WillRepeatedly(Return(0));

  aos->Start(&source);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitWhenIdleClosure(), TestTimeouts::action_timeout());
  loop.Run();
  aos->Stop();
  aos->Close();
}

// Verify that we are not allowed to open the output stream with audio offload
// enabled in exclusive mode.
TEST_F(WASAPIAudioOutputStreamTest, ExclusiveModeWithAudioOffload) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()) &&
                          ExclusiveModeIsEnabled());

  // Create exclusive-mode WASAPI output stream which plays out in stereo
  // using the minimum buffer size at 48kHz sample rate.
  AudioOutputStreamWrapper aosw(audio_manager_.get());

  // Open should fail with offload stream in exclusive mode.
  AudioOutputStream* aos = aosw.Create(48000, 160, true);
  EXPECT_FALSE(aos->Open());

  aos->Close();
}

// Verify that we can open the output stream in exclusive mode using a
// certain set of audio parameters and a sample rate of 48kHz.
// The expected outcomes of each setting in this test has been derived
// manually using log outputs (--v=1).
// It's disabled by default because a flag is required to enable exclusive mode.
TEST_F(WASAPIAudioOutputStreamTest, DISABLED_ExclusiveModeBufferSizesAt48kHz) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()) &&
                          ExclusiveModeIsEnabled());

  AudioOutputStreamWrapper aosw(audio_manager_.get());

  // 10ms @ 48kHz shall work.
  // Note that, this is the same size as we can use for shared-mode streaming
  // but here the endpoint buffer delay is only 10ms instead of 20ms.
  AudioOutputStream* aos = aosw.Create(48000, 480);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 5ms @ 48kHz does not work due to misalignment.
  // This test will propose an aligned buffer size of 5.3333ms.
  // Note that we must call Close() even is Open() fails since Close() also
  // deletes the object and we want to create a new object in the next test.
  aos = aosw.Create(48000, 240);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 5.3333ms @ 48kHz should work (see test above).
  aos = aosw.Create(48000, 256);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 2.6667ms is smaller than the minimum supported size (=3ms).
  aos = aosw.Create(48000, 128);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 3ms does not correspond to an aligned buffer size.
  // This test will propose an aligned buffer size of 3.3333ms.
  aos = aosw.Create(48000, 144);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 3.3333ms @ 48kHz <=> smallest possible buffer size we can use.
  aos = aosw.Create(48000, 160);
  EXPECT_TRUE(aos->Open());
  aos->Close();
}

// Verify that we can open the output stream in exclusive mode using a
// certain set of audio parameters and a sample rate of 44.1kHz.
// The expected outcomes of each setting in this test has been derived
// manually using log outputs (--v=1).
// It's disabled by default because a flag is required to enable exclusive mode.
TEST_F(WASAPIAudioOutputStreamTest, DISABLED_ExclusiveModeBufferSizesAt44kHz) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()) &&
                          ExclusiveModeIsEnabled());

  AudioOutputStreamWrapper aosw(audio_manager_.get());

  // 10ms @ 44.1kHz does not work due to misalignment.
  // This test will propose an aligned buffer size of 10.1587ms.
  AudioOutputStream* aos = aosw.Create(44100, 441);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 10.1587ms @ 44.1kHz shall work (see test above).
  aos = aosw.Create(44100, 448);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 5.8050ms @ 44.1 should work.
  aos = aosw.Create(44100, 256);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 4.9887ms @ 44.1kHz does not work to misalignment.
  // This test will propose an aligned buffer size of 5.0794ms.
  // Note that we must call Close() even is Open() fails since Close() also
  // deletes the object and we want to create a new object in the next test.
  aos = aosw.Create(44100, 220);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 5.0794ms @ 44.1kHz shall work (see test above).
  aos = aosw.Create(44100, 224);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 2.9025ms is smaller than the minimum supported size (=3ms).
  aos = aosw.Create(44100, 132);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 3.01587ms is larger than the minimum size but is not aligned.
  // This test will propose an aligned buffer size of 3.6281ms.
  aos = aosw.Create(44100, 133);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 3.6281ms @ 44.1kHz <=> smallest possible buffer size we can use.
  aos = aosw.Create(44100, 160);
  EXPECT_TRUE(aos->Open());
  aos->Close();
}

// Verify that we can open and start the output stream in exclusive mode at
// the lowest possible delay at 48kHz.
// It's disabled by default because a flag is required to enable exclusive mode.
TEST_F(WASAPIAudioOutputStreamTest,
       DISABLED_ExclusiveModeMinBufferSizeAt48kHz) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndOutputDevices(audio_manager_.get()) &&
                          ExclusiveModeIsEnabled());

  MockAudioSourceCallback source;
  // Create exclusive-mode WASAPI output stream which plays out in stereo
  // using the minimum buffer size at 48kHz sample rate.
  AudioOutputStreamWrapper aosw(audio_manager_.get());
  AudioOutputStream* aos = aosw.Create(48000, 160);
  EXPECT_TRUE(aos->Open());

  base::RunLoop loop;
  // Derive the expected size in bytes of each packet.
  base::TimeDelta packet_duration = base::Seconds(
      static_cast<double>(aosw.samples_per_packet()) / aosw.sample_rate());

  // Wait for the first callback and verify its parameters.
  EXPECT_CALL(source, OnMoreData(HasValidDelay(packet_duration), _,
                                 AudioGlitchInfo(), NotNull()))
      .WillOnce(DoAll(base::test::RunClosure(loop.QuitWhenIdleClosure()),
                      Return(aosw.samples_per_packet())))
      .WillRepeatedly(Return(aosw.samples_per_packet()));

  aos->Start(&source);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitWhenIdleClosure(), TestTimeouts::action_timeout());
  loop.Run();
  aos->Stop();
  aos->Close();
}

// Verify that we can open and start the output stream in exclusive mode at
// the lowest possible delay at 44.1kHz.
// It's disabled by default because a flag is required to enable exclusive mode.
TEST_F(WASAPIAudioOutputStreamTest,
       DISABLED_ExclusiveModeMinBufferSizeAt44kHz) {
  ABORT_AUDIO_TEST_IF_NOT(ExclusiveModeIsEnabled());

  MockAudioSourceCallback source;
  // Create exclusive-mode WASAPI output stream which plays out in stereo
  // using the minimum buffer size at 44.1kHz sample rate.
  AudioOutputStreamWrapper aosw(audio_manager_.get());
  AudioOutputStream* aos = aosw.Create(44100, 160);
  EXPECT_TRUE(aos->Open());

  base::RunLoop loop;
  // Derive the expected size in bytes of each packet.
  base::TimeDelta packet_duration = base::Seconds(
      static_cast<double>(aosw.samples_per_packet()) / aosw.sample_rate());

  // Wait for the first callback and verify its parameters.
  EXPECT_CALL(source, OnMoreData(HasValidDelay(packet_duration), _,
                                 AudioGlitchInfo(), NotNull()))
      .WillOnce(DoAll(base::test::RunClosure(loop.QuitWhenIdleClosure()),
                      Return(aosw.samples_per_packet())))
      .WillRepeatedly(Return(aosw.samples_per_packet()));

  aos->Start(&source);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitWhenIdleClosure(), TestTimeouts::action_timeout());
  loop.Run();
  aos->Stop();
  aos->Close();
}

}  // namespace media
