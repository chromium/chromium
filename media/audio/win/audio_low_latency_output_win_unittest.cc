// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_output_win.h"

#include <windows.h>

#include <mmsystem.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
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
#include "base/test/scoped_feature_list.h"
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
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
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
        text_file_(nullptr) {
    // Reads a test file from media/test/data directory.
    file_ = ReadTestDataFile(name);

    // Creates a vector that will store delta times between callbacks.
    // The content of this vector will be written to a text file at
    // destruction and can then be used for off-line analysis of the exact
    // timing of callbacks. The text file will be stored in media/test/data.
    delta_times_.reserve(kMaxDeltaSamples);
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

    for (auto delta : delta_times_) {
      fprintf(text_file_.get(), "%d\n", delta);
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
    if (delta_times_.size() < kMaxDeltaSamples) {
      delta_times_.push_back(diff);
    }

    size_t max_size = dest->frames() * dest->channels() * kBitsPerSample / 8;

    // Use samples read from a data file and fill up the audio buffer
    // provided to us in the callback.
    if (pos_ + max_size > file_size())
      max_size = file_size() - pos_;
    int frames = max_size / (dest->channels() * kBitsPerSample / 8);
    if (max_size) {
      static_assert(kBitsPerSample == 16, "FromInterleaved expects 2 bytes.");
      dest->FromInterleavedBytes<SignedInt16SampleTypeTraits>(
          base::span(*file_).subspan(pos_, max_size));
      pos_ += max_size;
    }
    return frames;
  }

  void OnError(ErrorType type) override {}

  size_t file_size() { return base::checked_cast<int>(file_->size()); }

 private:
  scoped_refptr<DecoderBuffer> file_;
  std::vector<int> delta_times_;
  size_t pos_;
  base::TimeTicks previous_call_time_;
  raw_ptr<FILE> text_file_;
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

// -----------------------------------------------------------------------------
// GlitchDetector Unit Tests
// -----------------------------------------------------------------------------
// These tests verify WASAPIAudioOutputStream::GlitchDetector in shared mode.
// They use synthetic sequences of playout position, QPC real time, and buffer
// padding without requiring physical audio hardware or active audio endpoints.
class GlitchDetectorTest : public ::testing::Test {
 public:
  GlitchDetectorTest() = default;
  ~GlitchDetectorTest() override = default;

 protected:
  static constexpr size_t kSampleRate = 48000;
  static constexpr size_t kPacketFrames = 480;  // 10ms at 48kHz
  static constexpr base::TimeDelta kBufferDuration =
      base::Seconds(static_cast<double>(kPacketFrames) / kSampleRate);

  // In Windows WASAPI, IAudioClock::GetFrequency() reports the device clock
  // frequency. In our real-world 48 kHz traces on Windows, GetFrequency()
  // reports 384,000 Hz (where position advances by 3,840 units every 10 ms).
  // AudioTimestampHelper::FramesToTime(delta_pos, kDeviceFrequency) accurately
  // resolves this to milliseconds.
  static constexpr UINT64 kDeviceFrequency = 384000;
  static constexpr UINT64 kInitialPosition = 384000;
  static constexpr UINT64 kInitialQpc = 117445800000;

  WASAPIAudioOutputStream::GlitchDetector detector_{kBufferDuration};
  UINT64 pos_ = kInitialPosition;
  UINT64 qpc_ = kInitialQpc;

  void SetUp() override { ResetPositions(); }

  void ResetPositions() {
    pos_ = kInitialPosition;
    qpc_ = kInitialQpc;
  }

  // Helper: advances state by one callback using real-world WASAPI units:
  // - `pos_duration` advances `pos_` at kDeviceFrequency (3,840 units per
  // 10ms).
  // - `qpc_duration` advances `qpc_` in standard 100ns units (100,000 ticks per
  // 10ms).
  // - `padding_frames` is the queued buffer frames reported by WASAPI.
  void StepCallback(base::TimeDelta pos_duration,
                    base::TimeDelta qpc_duration,
                    UINT32 padding_frames) {
    pos_ += (pos_duration.InMicroseconds() * kDeviceFrequency) /
            base::Time::kMicrosecondsPerSecond;
    // QPC ticks are in 100ns units (10,000 ticks per millisecond).
    qpc_ += static_cast<UINT64>(qpc_duration.InMicroseconds() * 10);

    detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency,
                                    padding_frames, kPacketFrames,
                                    /*is_shared_mode=*/true);
  }
};

// Healthy playout in steady state: 10ms callbacks arrive every 10ms with
// sufficient buffer padding. No glitches should be reported.
TEST_F(GlitchDetectorTest, HealthyPlayoutNoGlitches) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWasapiImproveGlitchDetection);

  // In real-world Windows 48 kHz traces (with a 1056-frame endpoint buffer
  // and 480-frame packet size), WASAPI maintains ~560 frames of padding.
  static constexpr UINT32 kHealthyPaddingFrames = 560;

  // First callback seeds baseline positions:
  detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency,
                                  kHealthyPaddingFrames, kPacketFrames, true);

  // 20 consecutive healthy callbacks:
  for (int i = 0; i < 20; ++i) {
    StepCallback(base::Milliseconds(10), base::Milliseconds(10),
                 kHealthyPaddingFrames);
  }

  AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
  EXPECT_EQ(info.count, 0u);
  EXPECT_EQ(info.duration, base::TimeDelta());

  SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
  EXPECT_EQ(stats.glitches_detected, 0);
  EXPECT_EQ(stats.total_glitch_duration, base::TimeDelta());
}

// Bluetooth Jitter Test:
// Common on Bluetooth headsets (e.g. 16-7-7ms cadence).
// The 16ms callback has a timing gap of 6ms (> buffer_duration / 2 = 5ms), but
// the hardware endpoint buffer has plenty of audio queued (padding = 560 frames
// > packet_size = 480 frames).
// - Legacy behavior (flag disabled): flags a false alarm glitch!
// - New behavior (flag enabled): sees buffer padding remains above packet size
// and reports 0 glitches.
TEST_F(GlitchDetectorTest, BluetoothSchedulerJitterDoesNotReportGlitch) {
  // Part A: New algorithm rejects false alarm when buffer padding is present.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(media::kWasapiImproveGlitchDetection);

    detector_.Reset();
    ResetPositions();

    // Seed baseline:
    detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 560,
                                    kPacketFrames, true);

    // Callback 1 (Delayed by 16ms, gap = 6ms > 5ms threshold, padding = 560):
    StepCallback(base::Milliseconds(10), base::Milliseconds(16), 560);

    // Callback 2 (Runs early 7ms later, padding = 560 frames):
    StepCallback(base::Milliseconds(10), base::Milliseconds(7), 560);

    // Callback 3 (Runs early 7ms later, padding = 560 frames):
    StepCallback(base::Milliseconds(10), base::Milliseconds(7), 560);

    AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
    EXPECT_EQ(info.count, 0u);
    EXPECT_EQ(info.duration, base::TimeDelta());

    SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
    EXPECT_EQ(stats.glitches_detected, 0);
  }

  // Part B: Legacy algorithm falsely flags a glitch on the exact same data.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(media::kWasapiImproveGlitchDetection);

    detector_.Reset();
    ResetPositions();

    // Seed baseline:
    detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 560,
                                    kPacketFrames, true);

    // Identical sequence as Part A:
    // Callback 1 (Delayed by 16ms, gap = 6ms > 5ms threshold, padding = 560):
    StepCallback(base::Milliseconds(10), base::Milliseconds(16), 560);

    // Callback 2 (Runs early 7ms later, padding = 560 frames):
    StepCallback(base::Milliseconds(10), base::Milliseconds(7), 560);

    // Callback 3 (Runs early 7ms later, padding = 560 frames):
    StepCallback(base::Milliseconds(10), base::Milliseconds(7), 560);

    AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
    // Legacy flags a false alarm because timing gap > 5ms, ignoring padding:
    // gap = qpc_duration - pos_duration = 16ms - 10ms = 6ms.
    // Legacy directly records this 6ms gap as glitch duration:
    EXPECT_EQ(info.count, 1u);
    EXPECT_EQ(info.duration, base::Milliseconds(6));

    SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
    EXPECT_EQ(stats.glitches_detected, 1);
    EXPECT_EQ(stats.total_glitch_duration, base::Milliseconds(6));
  }
}

// True Physical Underrun with Recovery Window:
// A real stall occurs where the thread is delayed AND the buffer empties to 0.
// When rendering resumes, the client writes audio to refill the buffer across
// consecutive callbacks while the playout position catches up.
// The recovery window groups the stall and refill phase into a SINGLE glitch
// capturing the full cumulative lost duration.
TEST_F(GlitchDetectorTest, PhysicalUnderrunFullDurationRecovery) {
  // Part A: New algorithm captures full cumulative recovery duration.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(media::kWasapiImproveGlitchDetection);

    detector_.Reset();
    ResetPositions();

    // Seed baseline:
    detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 480,
                                    kPacketFrames, true);

    // Callback 1: Real underrun. Playout stalled for 20ms.
    // Hardware played 0 frames, buffer emptied to 0 frames.
    // gap = 20ms, padding = 0 < packet_size.
    StepCallback(base::Milliseconds(0), base::Milliseconds(20), 0);

    // Callback 2: 10ms later. Client wrote 1 packet, so padding is now 480.
    // Playout resumed but clock is still catching up (hardware played 5ms of
    // audio). gap = 5ms > 0.
    StepCallback(base::Milliseconds(5), base::Milliseconds(10), 480);

    // Callback 3: 10ms later. Client writes another packet. Playout clock has
    // fully caught up (hardware played 10ms). gap = 0ms <= 0.
    StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);

    // Recovery window closes on Callback 3:
    AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
    // Expect exactly ONE unified glitch event:
    EXPECT_EQ(info.count, 1u);
    // Full accumulated lost duration is the sum of gaps across the recovery:
    // - Callback 1: gap = 20ms - 0ms = 20ms.
    // - Callback 2: gap = 10ms - 5ms = 5ms.
    // - Callback 3: gap = 10ms - 10ms = 0ms (ends recovery and commits total).
    // Total duration = 20ms + 5ms = 25ms.
    EXPECT_EQ(info.duration, base::Milliseconds(25));

    SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
    EXPECT_EQ(stats.glitches_detected, 1);
    EXPECT_EQ(stats.total_glitch_duration, base::Milliseconds(25));
  }

  // Part B: Legacy algorithm misses refill lag and underreports duration.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(media::kWasapiImproveGlitchDetection);

    detector_.Reset();
    ResetPositions();

    // Seed baseline:
    detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 480,
                                    kPacketFrames, true);

    // Identical sequence as Part A:
    // Callback 1 (Stall: gap = 20ms > 5ms threshold -> flags 20ms glitch):
    StepCallback(base::Milliseconds(0), base::Milliseconds(20), 0);

    // Callback 2 (Refill lag: gap = 5ms <= 5ms threshold -> ignored by
    // legacy!):
    StepCallback(base::Milliseconds(5), base::Milliseconds(10), 480);

    // Callback 3 (Playout caught up: gap = 0ms <= 5ms threshold):
    StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);

    AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
    EXPECT_EQ(info.count, 1u);
    // Legacy underreports duration: only sees the initial 20ms stall,
    // completely missing the 5ms refill lag on Callback 2:
    EXPECT_EQ(info.duration, base::Milliseconds(20));

    SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
    EXPECT_EQ(stats.glitches_detected, 1);
    EXPECT_EQ(stats.total_glitch_duration, base::Milliseconds(20));
  }
}

// Recovery Window Delivery to Audio Source:
// Unlike PhysicalUnderrunFullDurationRecovery which only inspects glitch info
// at the very end of the stream, production RenderAudioFromSource() queries
// GetGlitchInfoAndReset() on every single render callback to deliver metrics to
// AudioSourceCallback::OnMoreData().
//
// Using the same 20ms stall + 5ms refill lag sequence as the test above, this
// test verifies that intermediate callbacks report 0 glitches to the client
// source while the recovery window is actively accumulating, and that the
// complete 25ms unified glitch is delivered once the playout clock catches up.
TEST_F(GlitchDetectorTest, RecoveryWindowSurvivesPerCallbackDraining) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWasapiImproveGlitchDetection);

  detector_.Reset();
  ResetPositions();

  // Seed baseline:
  detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 480,
                                  kPacketFrames, true);

  // Callback 1: Real underrun. gap = 20ms - 0ms = 20ms, padding = 0.
  // The recovery window opens and begins accumulating duration.
  // No glitch is reported to OnMoreData() yet because the refill is pending:
  StepCallback(base::Milliseconds(0), base::Milliseconds(20), 0);
  AudioGlitchInfo info_after_1 = detector_.GetGlitchInfoAndReset();
  EXPECT_EQ(info_after_1.count, 0u);
  EXPECT_EQ(info_after_1.duration, base::TimeDelta());

  // Callback 2: Client refills one packet (padding = 480). The playout clock
  // is still catching up (gap = 10ms - 5ms = 5ms). The recovery window
  // continues accumulating duration without reporting a partial glitch yet:
  StepCallback(base::Milliseconds(5), base::Milliseconds(10), 480);
  AudioGlitchInfo info_after_2 = detector_.GetGlitchInfoAndReset();
  EXPECT_EQ(info_after_2.count, 0u);
  EXPECT_EQ(info_after_2.duration, base::TimeDelta());

  // Callback 3: Playout has caught up: gap = 10ms - 10ms = 0ms.
  // The recovery window closes and delivers the complete lost duration:
  // 20ms (stall) + 5ms (refill lag) = 25ms, as a single glitch to OnMoreData():
  StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);
  AudioGlitchInfo info_after_3 = detector_.GetGlitchInfoAndReset();
  EXPECT_EQ(info_after_3.count, 1u);
  EXPECT_EQ(info_after_3.duration, base::Milliseconds(25));

  SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
  EXPECT_EQ(stats.glitches_detected, 1);
  EXPECT_EQ(stats.total_glitch_duration, base::Milliseconds(25));
}

// Recent Empty Buffer Glitch (Non-sleep withholding):
// In some audio driver configurations, buffer padding drops to 0 on callback N,
// but the hardware playout clock position gap registers on callback N+1 when
// the client writes a refill packet. The recent empty buffer countdown ensures
// this is captured across the boundary.
TEST_F(GlitchDetectorTest, RecentEmptyBufferCapturesRefillOffsetGlitch) {
  // Part A: New algorithm uses recent empty buffer countdown to capture offset
  // glitch.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(media::kWasapiImproveGlitchDetection);

    detector_.Reset();
    ResetPositions();

    // Seed baseline:
    detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 480,
                                    kPacketFrames, true);

    // Callback 1: Buffer empties to 0, but timing gap is 0 (instant sample):
    // gap = 10ms - 10ms = 0ms, but padding < packet_size marks empty buffer.
    StepCallback(base::Milliseconds(10), base::Milliseconds(10), 0);

    // Callback 2: Client writes 1 refill packet (padding = 480).
    // The playout clock gap hits now: gap = 10ms - 0ms = 10ms (> 5ms
    // threshold). Because buffer was empty on Callback 1, this triggers
    // recovery and accumulates 10ms.
    StepCallback(base::Milliseconds(0), base::Milliseconds(10), 480);

    // Callback 3: Playout catches back up: gap = 10ms - 10ms = 0ms <= 0ms.
    // Recovery closes and commits the accumulated glitch duration.
    StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);

    AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
    EXPECT_EQ(info.count, 1u);
    // Total duration = 10ms accumulated from Callback 2:
    EXPECT_EQ(info.duration, base::Milliseconds(10));

    SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
    EXPECT_EQ(stats.glitches_detected, 1);
    EXPECT_EQ(stats.total_glitch_duration, base::Milliseconds(10));
  }

  // Part B: Legacy algorithm on identical sequence.
  // Legacy does not inspect buffer padding, so on Callback 1 (gap = 0) it sees
  // nothing. On Callback 2 (gap = 10ms > 5ms) it flags a 10ms glitch.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(media::kWasapiImproveGlitchDetection);

    detector_.Reset();
    ResetPositions();

    // Seed baseline:
    detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 480,
                                    kPacketFrames, true);

    // Identical sequence as Part A:
    // Callback 1 (Buffer empty, gap = 0ms <= 5ms threshold):
    StepCallback(base::Milliseconds(10), base::Milliseconds(10), 0);

    // Callback 2 (Refill packet, gap = 10ms > 5ms threshold):
    StepCallback(base::Milliseconds(0), base::Milliseconds(10), 480);

    // Callback 3 (Playout caught up, gap = 0ms <= 5ms threshold):
    StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);

    AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
    EXPECT_EQ(info.count, 1u);
    EXPECT_EQ(info.duration, base::Milliseconds(10));

    SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
    EXPECT_EQ(stats.glitches_detected, 1);
    EXPECT_EQ(stats.total_glitch_duration, base::Milliseconds(10));
  }
}

// Two Distinct Glitches Separated by Normal Playout:
// Verifies that after a recovery window closes, the glitch detector returns to
// normal steady-state monitoring and accurately detects subsequent glitches as
// separate, distinct events.
TEST_F(GlitchDetectorTest, TwoDistinctGlitchesSeparatedByNormalPlayout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWasapiImproveGlitchDetection);

  detector_.Reset();
  ResetPositions();

  // Seed baseline:
  detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 480,
                                  kPacketFrames, true);

  // First Glitch Event:
  // Callback 1 (Stall & Underrun): Playout stalls for 20ms, buffer empties.
  // gap = 20ms - 0ms = 20ms > 5ms, padding = 0 < 480. Opens recovery window.
  StepCallback(base::Milliseconds(0), base::Milliseconds(20), 0);

  // Callback 2 (Refill & Recover): Client writes refill packet, clock catches
  // up. gap = 10ms - 10ms = 0ms <= 0ms. Recovery closes and commits Glitch #1
  // (20ms).
  StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);

  // Normal steady-state playout between glitches:
  // 3 consecutive 10ms callbacks with sufficient buffer padding:
  for (int i = 0; i < 3; ++i) {
    StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);
  }

  // Second Glitch Event:
  // Callback 6 (Second Stall & Underrun): Playout stalls for 15ms, buffer
  // empties. gap = 15ms - 0ms = 15ms > 5ms, padding = 0 < 480. Opens new
  // recovery window.
  StepCallback(base::Milliseconds(0), base::Milliseconds(15), 0);

  // Callback 7 (Refill & Recover): Client writes refill packet, clock catches
  // up. gap = 10ms - 10ms = 0ms <= 0ms. Recovery closes and commits Glitch #2
  // (15ms).
  StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);

  // Verify that both glitches were counted as separate events and their
  // durations were summed correctly:
  // - Glitch 1: 20ms
  // - Glitch 2: 15ms
  // Total: count = 2, duration = 35ms.
  AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
  EXPECT_EQ(info.count, 2u);
  EXPECT_EQ(info.duration, base::Milliseconds(35));

  SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
  EXPECT_EQ(stats.glitches_detected, 2);
  EXPECT_EQ(stats.total_glitch_duration, base::Milliseconds(35));
}

// Goal: Verify that the recovery window times out after a bounded number of
// callbacks (`kRecoveryWindowCallbacks` = 10) if the hardware playout clock
// never fully resynchronizes to real time.
//
// Physical scenario: After a physical underrun, a clock drift or sample-rate
// mismatch causes the hardware playout position to continue lagging behind QPC
// by 1ms on every subsequent callback (`gap_duration > 0`). Without the timeout
// branch (`recovery_window_countdown_ <= 0`), the recovery window would stay
// open indefinitely and never commit the accumulated glitch duration.
TEST_F(GlitchDetectorTest, RecoveryWindowTimeoutCommitsAccumulatedGlitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWasapiImproveGlitchDetection);

  detector_.Reset();
  ResetPositions();

  // Seed baseline:
  detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 480,
                                  kPacketFrames, true);

  // Callback 1 (Initial Underrun): Playout stalls for 20ms, buffer empties.
  // Opens recovery window with `recovery_window_countdown_ = 10` and
  // initial accumulated duration = 20ms.
  StepCallback(base::Milliseconds(0), base::Milliseconds(20), 0);
  EXPECT_EQ(detector_.GetGlitchInfoAndReset().count, 0u);

  // Callbacks 2 through 10 (9 consecutive callbacks of 1ms drift):
  // On each callback, QPC advances 10ms while playout advances only 9ms
  // (`gap = 1ms > 0`). The recovery window remains active and accumulates
  // 1ms per callback without reporting prematurely.
  for (int i = 0; i < 9; ++i) {
    StepCallback(base::Milliseconds(9), base::Milliseconds(10), 480);
    EXPECT_EQ(detector_.GetGlitchInfoAndReset().count, 0u);
  }

  // Callback 11 (10th recovery callback):
  // `recovery_window_countdown_` reaches 0. Even though `gap = 1ms > 0`, the
  // recovery window times out and commits the entire accumulated duration:
  // 20ms (initial stall) + 10 * 1ms (drift) = 30ms.
  StepCallback(base::Milliseconds(9), base::Milliseconds(10), 480);

  AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
  EXPECT_EQ(info.count, 1u);
  EXPECT_EQ(info.duration, base::Milliseconds(30));

  SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
  EXPECT_EQ(stats.glitches_detected, 1);
  EXPECT_EQ(stats.total_glitch_duration, base::Milliseconds(30));
}

// Goal: Verify that the `recent_empty_buffer_countdown_` carryover state
// expires back to 0 if no timing gap occurs within
// `kRecentEmptyBufferCallbacks` (2 callbacks), preventing stale empty-buffer
// states from combining with later OS scheduler jitter.
//
// Physical scenario: The endpoint buffer momentarily drains to 0 frames on
// Callback 1 right as the client writes a refill packet on time (`gap = 0ms`).
// Playout continues smoothly on Callback 2 (`gap = 0ms`, `padding = 480`),
// expiring the countdown. When OS scheduler jitter delays Callback 3 by 8ms
// (`gap = 8ms > 5ms`, `padding = 480`), no glitch should be reported.
TEST_F(GlitchDetectorTest, RecentEmptyBufferExpiresIfNoTimingGapFollows) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWasapiImproveGlitchDetection);

  detector_.Reset();
  ResetPositions();

  // Seed baseline:
  detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 480,
                                  kPacketFrames, true);

  // Callback 1: Buffer momentarily hits 0 padding, but playout is on time
  // (`gap = 0ms`). Sets countdown to 2, then decrements to 1 at end of tick.
  StepCallback(base::Milliseconds(10), base::Milliseconds(10), 0);

  // Callback 2: Client refills buffer (`padding = 480`), playout remains on
  // time (`gap = 0ms`). Countdown decrements from 1 to 0 (expired).
  StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);

  // Callback 3: OS scheduler jitter delays callback by 8ms (`gap = 8ms > 5ms`
  // threshold), while buffer has sufficient audio (`padding = 480`). Because
  // the empty buffer state from Callback 1 expired on Callback 2, no glitch
  // should be reported.
  StepCallback(base::Milliseconds(10), base::Milliseconds(18), 480);

  AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
  EXPECT_EQ(info.count, 0u);
  EXPECT_EQ(info.duration, base::TimeDelta());

  SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
  EXPECT_EQ(stats.glitches_detected, 0);
  EXPECT_EQ(stats.total_glitch_duration, base::TimeDelta());
}

// Goal: Verify that non-monotonic backward jumps in `device_position`
// (`device_position < last_device_position_`) are safely clamped to zero
// elapsed playout time rather than underflowing unsigned UINT64 subtraction.
//
// Physical scenario: Certain buggy Windows audio drivers occasionally report a
// `device_position` slightly smaller than on the previous callback during
// stream state transitions. Without the underflow guard, unsigned subtraction
// (`device_position - last_device_position_`) wraps around to `UINT64_MAX`,
// corrupting `position_time_increase` and producing a nonsensical glitch
// duration.
TEST_F(GlitchDetectorTest, DriverClockBackwardJumpDoesNotUnderflow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWasapiImproveGlitchDetection);

  detector_.Reset();
  ResetPositions();

  // Seed baseline:
  detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency, 480,
                                  kPacketFrames, true);

  // Simulate a backward jump in `device_position` (-10ms / -3840 units) while
  // QPC advances normally by +10ms and the buffer is empty (`padding = 0`).
  pos_ -= 3840;
  qpc_ += 100000;
  detector_.ProcessRenderCallback(pos_, qpc_, kDeviceFrequency,
                                  /*current_padding_frames=*/0, kPacketFrames,
                                  /*is_shared_mode=*/true);

  // Playout resumes and catches up on the next callback:
  StepCallback(base::Milliseconds(10), base::Milliseconds(10), 480);

  // Because the backward position jump was clamped to 0ms increase, the
  // resulting gap equals the 10ms QPC elapsed time (rather than UINT64
  // underflow).
  AudioGlitchInfo info = detector_.GetGlitchInfoAndReset();
  EXPECT_EQ(info.count, 1u);
  EXPECT_EQ(info.duration, base::Milliseconds(10));

  SystemGlitchReporter::Stats stats = detector_.GetLongTermStatsAndReset();
  EXPECT_EQ(stats.glitches_detected, 1);
  EXPECT_EQ(stats.total_glitch_duration, base::Milliseconds(10));
}

}  // namespace media
