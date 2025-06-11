// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_input_win.h"

#include <windows.h>

#include <mmsystem.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/check_deref.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_input_stream_data_interceptor.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/test_audio_thread.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/audio/win/test_support/fake_win_wasapi_environment.h"
#include "media/audio/win/test_support/wasapi_test_error_code.h"
#include "media/base/media_switches.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::NotNull;

namespace media {

namespace {

constexpr char kMockApplicationLoopbackDeviceId[] = "applicationLoopback:12345";
// When opening a WASAPIAudioInputStream for application loopback capture, it's
// necessary to wait for the activation to complete. This short timeout is used
// to avoid long waits in the timeout test cases.
constexpr base::TimeDelta kShortAsyncActivationTimeoutMs =
    base::Milliseconds(10);

void LogCallbackDummy(const std::string& /* message */) {}

}  // namespace

ACTION_P4(CheckCountAndPostQuitTask, count, limit, task_runner, quit_closure) {
  if (++*count >= limit)
    task_runner->PostTask(FROM_HERE, quit_closure);
}

void FlushTaskRunner(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!task_runner->BelongsToCurrentThread()) {
    base::RunLoop run_loop;
    task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                  run_loop.QuitClosure());
    run_loop.Run();
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

class FakeAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  FakeAudioInputCallback()
      : num_received_audio_frames_(0),
        data_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
        error_(false) {}

  FakeAudioInputCallback(const FakeAudioInputCallback&) = delete;
  FakeAudioInputCallback& operator=(const FakeAudioInputCallback&) = delete;

  bool error() const { return error_; }
  int num_callbacks() const { return num_callbacks_; }
  int num_received_audio_frames() const { return num_received_audio_frames_; }

  // Waits until OnData() is called on another thread.
  void WaitForData() { data_event_.Wait(); }

  // Waits for OnData() to be called on another thread.
  // Returns true if the event is signaled, false if it times out.
  bool WaitForDataWithTimeout(base::TimeDelta timeout) {
    return data_event_.TimedWait(timeout);
  }

  void OnData(const AudioBus* src,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    EXPECT_GE(capture_time, base::TimeTicks());
    num_callbacks_++;
    num_received_audio_frames_ += src->frames();
    data_event_.Signal();
  }

  void OnError() override { error_ = true; }

 private:
  int num_callbacks_ = 0;
  int num_received_audio_frames_;
  base::WaitableEvent data_event_;
  bool error_;
};

class FakeAudioOutputCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  FakeAudioOutputCallback()
      : num_rendered_audio_frames_(0),
        data_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
        error_(false) {}

  FakeAudioOutputCallback(const FakeAudioOutputCallback&) = delete;
  FakeAudioOutputCallback& operator=(const FakeAudioOutputCallback&) = delete;

  bool error() const { return error_; }
  int num_callbacks() const { return num_callbacks_; }
  int num_rendered_audio_frames() const { return num_rendered_audio_frames_; }

  // Waits until OnMoreData() is called on another thread.
  void WaitForMoreData() { data_event_.Wait(); }

  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const AudioGlitchInfo& glitch_info,
                 AudioBus* dest) override {
    num_callbacks_++;
    num_rendered_audio_frames_ += dest->frames();
    dest->Zero();
    data_event_.Signal();
    return dest->frames();
  }

  void OnError(ErrorType type) override { error_ = true; }

 private:
  int num_callbacks_ = 0;
  int num_rendered_audio_frames_;
  base::WaitableEvent data_event_;
  bool error_;
};

// This audio sink implementation should be used for manual tests only since
// the recorded data is stored on a raw binary data file.
class WriteToFileAudioSink : public AudioInputStream::AudioInputCallback {
 public:
  // Allocate space for ~10 seconds of data @ 48kHz in stereo:
  // 2 bytes per sample, 2 channels, 10ms @ 48kHz, 10 seconds <=> 1920000 bytes.
  static const size_t kMaxBufferSize = 2 * 2 * 480 * 100 * 10;

  explicit WriteToFileAudioSink(const char* file_name)
      : buffer_(0, kMaxBufferSize), bytes_to_write_(0) {
    base::FilePath file_path;
    EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &file_path));
    file_path = file_path.AppendASCII(file_name);
    binary_file_ = base::OpenFile(file_path, "wb");
    DLOG_IF(ERROR, !binary_file_) << "Failed to open binary PCM data file.";
    VLOG(0) << ">> Output file: " << file_path.value() << " has been created.";
  }

  ~WriteToFileAudioSink() override {
    size_t bytes_written = 0;
    while (bytes_written < bytes_to_write_) {
      // Stop writing if no more data is available.
      const base::span<const uint8_t> chunk = buffer_.GetCurrentChunk();
      if (chunk.empty()) {
        break;
      }

      // Write recorded data chunk to the file and prepare for next chunk.
      UNSAFE_TODO(fwrite(chunk.data(), 1, chunk.size(), binary_file_));
      buffer_.Seek(chunk.size());
      bytes_written += chunk.size();
    }
    base::CloseFile(binary_file_);
  }

  // AudioInputStream::AudioInputCallback implementation.
  void OnData(const AudioBus* src,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    const int num_samples = src->frames() * src->channels();
    auto interleaved = base::HeapArray<int16_t>::Uninit(num_samples);
    src->ToInterleaved<SignedInt16SampleTypeTraits>(src->frames(),
                                                    interleaved.data());

    // Store data data in a temporary buffer to avoid making blocking
    // fwrite() calls in the audio callback. The complete buffer will be
    // written to file in the destructor.
    const auto byte_span = base::as_bytes(interleaved.as_span());
    if (buffer_.Append(byte_span)) {
      bytes_to_write_ += byte_span.size();
    }
  }

  void OnError() override {}

 private:
  media::SeekableBuffer buffer_;
  raw_ptr<FILE> binary_file_;
  size_t bytes_to_write_;
};

static bool HasCoreAudioAndInputDevices(AudioManager* audio_man) {
  // The low-latency (WASAPI-based) version requires Windows Vista or higher.
  // TODO(henrika): note that we use Wave today to query the number of
  // existing input devices.
  return CoreAudioUtil::IsSupported() &&
         AudioDeviceInfoAccessorForTests(audio_man).HasAudioInputDevices();
}

// Convenience method which creates a default AudioInputStream object but
// also allows the user to modify the default settings.
class AudioInputStreamWrapper {
 public:
  explicit AudioInputStreamWrapper(AudioManager* audio_manager)
      : audio_man_(audio_manager) {
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(
        device_id_, false, &default_params_)));
    EXPECT_EQ(format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);
    frames_per_buffer_ = default_params_.frames_per_buffer();
  }

  AudioInputStreamWrapper(AudioManager* audio_manager,
                          const AudioParameters& default_params)
      : audio_man_(audio_manager), default_params_(default_params) {
    EXPECT_EQ(format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);
    frames_per_buffer_ = default_params_.frames_per_buffer();
  }

  AudioInputStreamWrapper(AudioManager* audio_manager,
                          const std::string& device_id)
      : audio_man_(audio_manager), device_id_(device_id) {
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(
        device_id_, false, &default_params_)));
    EXPECT_EQ(format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);
    frames_per_buffer_ = default_params_.frames_per_buffer();
  }

  ~AudioInputStreamWrapper() {}

  // Creates AudioInputStream object using default parameters.
  AudioInputStream* Create() { return CreateInputStream(); }

  // Creates AudioInputStream object using non-default parameters where the
  // frame size is modified.
  AudioInputStream* Create(int frames_per_buffer) {
    frames_per_buffer_ = frames_per_buffer;
    return CreateInputStream();
  }

  AudioParameters::Format format() const { return default_params_.format(); }
  int channels() const {
    return ChannelLayoutToChannelCount(default_params_.channel_layout());
  }
  int sample_rate() const { return default_params_.sample_rate(); }
  int frames_per_buffer() const { return frames_per_buffer_; }
  std::string device_id() const { return device_id_; }

 private:
  AudioInputStream* CreateInputStream() {
    AudioParameters params = default_params_;
    params.set_frames_per_buffer(frames_per_buffer_);
    AudioInputStream* ais = audio_man_->MakeAudioInputStream(
        params, device_id_, base::BindRepeating(&LogCallbackDummy));
    EXPECT_TRUE(ais);
    return ais;
  }

  raw_ptr<AudioManager> audio_man_;
  AudioParameters default_params_;
  std::string device_id_ = AudioDeviceDescription::kDefaultDeviceId;
  int frames_per_buffer_;
};

// Convenience method which creates a default AudioInputStream object.
static AudioInputStream* CreateDefaultAudioInputStream(
    AudioManager* audio_manager) {
  AudioInputStreamWrapper aisw(audio_manager);
  AudioInputStream* ais = aisw.Create();
  return ais;
}

class ScopedAudioInputStream {
 public:
  ScopedAudioInputStream() : stream_(nullptr) {}
  explicit ScopedAudioInputStream(AudioInputStream* stream) : stream_(stream) {}

  ScopedAudioInputStream(const ScopedAudioInputStream&) = delete;
  ScopedAudioInputStream& operator=(const ScopedAudioInputStream&) = delete;

  ~ScopedAudioInputStream() {
    if (stream_)
      stream_->Close();
  }

  void Close() {
    if (stream_)
      stream_->Close();
    stream_ = nullptr;
  }

  AudioInputStream* operator->() { return stream_; }

  AudioInputStream* get() const { return stream_.get(); }

  void Reset(AudioInputStream* new_stream) {
    Close();
    stream_ = new_stream;
  }

 private:
  // TODO(crbug.com/377749732): Fix dangling pointer when used with
  // `AudioInputStreamDataInterceptor`.
  raw_ptr<AudioInputStream, DanglingUntriaged> stream_;
};

class ScopedAudioOutputStream {
 public:
  ScopedAudioOutputStream() : stream_(nullptr) {}
  explicit ScopedAudioOutputStream(AudioOutputStream* stream)
      : stream_(stream) {}

  ScopedAudioOutputStream(const ScopedAudioOutputStream&) = delete;
  ScopedAudioOutputStream& operator=(const ScopedAudioOutputStream&) = delete;

  ~ScopedAudioOutputStream() {
    if (stream_) {
      stream_->Close();
    }
  }

  void Close() {
    if (stream_) {
      stream_->Close();
    }
    stream_ = nullptr;
  }

  AudioOutputStream* operator->() { return stream_; }

  AudioOutputStream* get() const { return stream_; }

  void Reset(AudioOutputStream* new_stream) {
    Close();
    stream_ = new_stream;
  }

 private:
  raw_ptr<AudioOutputStream> stream_;
};

class WinAudioInputTest : public ::testing::Test {
 public:
  WinAudioInputTest() {
    audio_manager_ =
        AudioManager::CreateForTesting(std::make_unique<TestAudioThread>());
    // Ensure that the AudioManager's thread (TestAudioThread) has processed
    // its initial tasks posted during AudioManager::CreateForTesting.
    FlushTaskRunner(audio_manager_->GetTaskRunner());
  }
  ~WinAudioInputTest() override { audio_manager_->Shutdown(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AudioManager> audio_manager_;
};

// Verify that we can retrieve the current hardware/mixing sample rate
// for all available input devices.
TEST_F(WinAudioInputTest, WASAPIAudioInputStreamHardwareSampleRate) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));

  // Retrieve a list of all available input devices.
  media::AudioDeviceDescriptions device_descriptions;
  AudioDeviceInfoAccessorForTests(audio_manager_.get())
      .GetAudioInputDeviceDescriptions(&device_descriptions);

  // Scan all available input devices and repeat the same test for all of them.
  for (const auto& device : device_descriptions) {
    // Retrieve the hardware sample rate given a specified audio input device.
    AudioParameters params;
    ASSERT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(
        device.unique_id, false, &params)));
    EXPECT_GE(params.sample_rate(), 0);
  }
}

// Test effects.
TEST_F(WinAudioInputTest, WASAPIAudioInputStreamEffects) {
  AudioDeviceInfoAccessorForTests device_info_accessor(audio_manager_.get());
  ABORT_AUDIO_TEST_IF_NOT(device_info_accessor.HasAudioInputDevices() &&
                          device_info_accessor.HasAudioOutputDevices() &&
                          CoreAudioUtil::IsSupported());

  // Retrieve a list of all available input devices.
  media::AudioDeviceDescriptions device_descriptions;
  device_info_accessor.GetAudioInputDeviceDescriptions(&device_descriptions);

  // No device should have any effects.
  for (const auto& device : device_descriptions) {
    AudioParameters params =
        device_info_accessor.GetInputStreamParameters(device.unique_id);
    EXPECT_EQ(params.effects(), AudioParameters::NO_EFFECTS);
  }

  // The two loopback devices are not included in the device description list
  // above. They should also have no effects.
  AudioParameters params = device_info_accessor.GetInputStreamParameters(
      AudioDeviceDescription::kLoopbackInputDeviceId);
  EXPECT_EQ(params.effects(), AudioParameters::NO_EFFECTS);

  params = device_info_accessor.GetInputStreamParameters(
      AudioDeviceDescription::kLoopbackWithMuteDeviceId);
  EXPECT_EQ(params.effects(), AudioParameters::NO_EFFECTS);
}

TEST_F(WinAudioInputTest,
       WASAPIAudioInputStreamLoopbackDevicesDoNotSupportSystemEffects) {
  AudioDeviceInfoAccessorForTests device_info_accessor(audio_manager_.get());
  ABORT_AUDIO_TEST_IF_NOT(device_info_accessor.HasAudioInputDevices() &&
                          CoreAudioUtil::IsSupported());

  base::HistogramTester histogram_tester;

  // Loopback devices do not support system effects when asked for its input
  // parameters.
  AudioParameters params = device_info_accessor.GetInputStreamParameters(
      AudioDeviceDescription::kLoopbackInputDeviceId);
  EXPECT_EQ(params.effects(), AudioParameters::NO_EFFECTS);
  histogram_tester.ExpectTotalCount(
      "Media.Audio.Capture.Win.VoiceProcessingEffects", 0);

  // Loopback devices do not support system effects when asked for its input
  // parameters even if we enable the system AEC flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kEnforceSystemEchoCancellation);
  params = device_info_accessor.GetInputStreamParameters(
      AudioDeviceDescription::kLoopbackInputDeviceId);
  EXPECT_EQ(params.effects(), AudioParameters::NO_EFFECTS);
  histogram_tester.ExpectTotalCount(
      "Media.Audio.Capture.Win.VoiceProcessingEffects", 0);

  // Loopback devices to not support system AEC when used as device for an
  // input stream even when the system AEC flag is enabled.
  ScopedAudioInputStream stream(audio_manager_->MakeAudioInputStream(
      params, AudioDeviceDescription::kLoopbackInputDeviceId,
      base::BindRepeating(&LogCallbackDummy)));
  EXPECT_EQ(stream->Open(), AudioInputStream::OpenOutcome::kSuccess);
  EXPECT_EQ(params.effects(), AudioParameters::NO_EFFECTS);
  histogram_tester.ExpectTotalCount(
      "Media.Audio.Capture.Win.VoiceProcessingEffects", 0);
}

class WinAudioInputSystemEffectsTest : public WinAudioInputTest {
 public:
  using AP = AudioParameters;
  WinAudioInputSystemEffectsTest()
      : device_info_accessor_(audio_manager_.get()),
        params_(device_info_accessor_.GetInputStreamParameters(
            AudioDeviceDescription::kDefaultDeviceId)) {
    feature_list_.InitAndEnableFeature(media::kEnforceSystemEchoCancellation);
  }

 protected:
  AudioDeviceInfoAccessorForTests device_info_accessor_;
  AudioParameters params_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(WinAudioInputSystemEffectsTest,
       ParameterMustContainEchoCancellationToEnableSystemEffects) {
  ABORT_AUDIO_TEST_IF_NOT(device_info_accessor_.HasAudioInputDevices() &&
                          CoreAudioUtil::IsSupported());

  base::HistogramTester histogram_tester;

  static constexpr int kEffectsWithoutAEC[] = {
      AP::NO_EFFECTS, AP::NOISE_SUPPRESSION, AP::AUTOMATIC_GAIN_CONTROL,
      AP::NOISE_SUPPRESSION | AP::AUTOMATIC_GAIN_CONTROL};

  // Emulate that the enumeration found an effect mask *without* AEC and create
  // an input stream based on that. None of these should trigger a
  // VoiceProcessingEffects histogram after the stream has been opened and
  // closed.
  for (const int& effect : kEffectsWithoutAEC) {
    params_.set_effects(effect);
    {
      ScopedAudioInputStream stream(audio_manager_->MakeAudioInputStream(
          params_, AudioDeviceDescription::kDefaultDeviceId,
          base::BindRepeating(&LogCallbackDummy)));
      ASSERT_THAT(stream.get(), NotNull());
      ASSERT_THAT(stream->Open(), Eq(AudioInputStream::OpenOutcome::kSuccess));
    }
    histogram_tester.ExpectTotalCount(
        "Media.Audio.Capture.Win.VoiceProcessingEffects", 0);
  }
}

TEST_F(WinAudioInputSystemEffectsTest,
       ParameterWithEchoCancellationShouldEnableSystemEffects) {
  ABORT_AUDIO_TEST_IF_NOT(device_info_accessor_.HasAudioInputDevices() &&
                          CoreAudioUtil::IsSupported());

  static constexpr int kEffectsWithAEC[] = {
      AP::ECHO_CANCELLER,
      AP::ECHO_CANCELLER | AP::AUTOMATIC_GAIN_CONTROL,
      AP::ECHO_CANCELLER | AP::NOISE_SUPPRESSION,
      AP::ECHO_CANCELLER | AP::AUTOMATIC_GAIN_CONTROL | AP::NOISE_SUPPRESSION,
  };

  // Emulate that the enumeration found an effect mask *with* AEC and create
  // an input stream based on that. All of these effect masks should trigger a
  // VoiceProcessingEffects histogram after the stream has been opened and
  // closed. The exact count can't be predicted.
  for (const int& effect : kEffectsWithAEC) {
    base::HistogramTester histogram_tester;
    params_.set_effects(effect);
    {
      ScopedAudioInputStream stream(audio_manager_->MakeAudioInputStream(
          params_, AudioDeviceDescription::kDefaultDeviceId,
          base::BindRepeating(&LogCallbackDummy)));
      ASSERT_THAT(stream.get(), NotNull());
      ASSERT_THAT(stream->Open(), Eq(AudioInputStream::OpenOutcome::kSuccess));
    }
    EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                    "Media.Audio.Capture.Win.VoiceProcessingEffects"),
                ::testing::Contains(::testing::Pair(
                    "Media.Audio.Capture.Win.VoiceProcessingEffects",
                    ::testing::Gt(0))));
  }
}

// Test Create(), Close() calling sequence.
TEST_F(WinAudioInputTest, WASAPIAudioInputStreamCreateAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager_.get()));
  ais.Close();
}

// Test Open(), Close() calling sequence.
TEST_F(WinAudioInputTest, WASAPIAudioInputStreamOpenAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager_.get()));
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
  ais.Close();
}

// Test Open(), Close() calling sequences for all available devices.
TEST_F(WinAudioInputTest, WASAPIAudioInputStreamOpenAndCloseForAllDevices) {
  AudioDeviceInfoAccessorForTests device_info_accessor(audio_manager_.get());
  ABORT_AUDIO_TEST_IF_NOT(device_info_accessor.HasAudioInputDevices() &&
                          CoreAudioUtil::IsSupported());

  // Retrieve a list of all available input devices.
  media::AudioDeviceDescriptions device_descriptions;
  device_info_accessor.GetAudioInputDeviceDescriptions(&device_descriptions);

  // Open and close an audio input stream for all available devices.
  for (const auto& device : device_descriptions) {
    AudioInputStreamWrapper aisw(audio_manager_.get(), device.unique_id);
    {
      ScopedAudioInputStream ais(aisw.Create());
      EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
    }
  }
}

// Test Open(), Start(), Close() calling sequence.
TEST_F(WinAudioInputTest, WASAPIAudioInputStreamOpenStartAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager_.get()));
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais.Close();
}

// Test Open(), Start(), Stop(), Close() calling sequence.
TEST_F(WinAudioInputTest, WASAPIAudioInputStreamOpenStartStopAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager_.get()));
  EXPECT_TRUE(ais->SetAutomaticGainControl(true));
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais->Stop();
  ais.Close();
}

// Verify that histograms are created as expected. Only covers the latest
// histograms.
TEST_F(WinAudioInputTest, WASAPIAudioInputStreamHistograms) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));
  base::HistogramTester histogram_tester;
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager_.get()));
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
  FakeAudioInputCallback sink;
  ais->Start(&sink);
  sink.WaitForData();
  sink.WaitForData();
  ais->Stop();
  ais.Close();
  histogram_tester.ExpectTotalCount("Media.Audio.Capture.EarlyGlitchDetected",
                                    1);
}

// Test some additional calling sequences.
TEST_F(WinAudioInputTest, WASAPIAudioInputStreamMiscCallingSequences) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager_.get()));

  // Open(), Open() should fail the second time.
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kAlreadyOpen);

  FakeAudioInputCallback sink;

  // Start(), Start() is a valid calling sequence (second call does nothing).
  ais->Start(&sink);
  sink.WaitForData();
  ais->Start(&sink);
  // Ensure the stream is still started.
  sink.WaitForData();
  sink.WaitForData();

  // Stop(), Stop() is a valid calling sequence (second call does nothing).
  ais->Stop();
  ais->Stop();
  ais.Close();
}

TEST_F(WinAudioInputTest, WASAPIAudioInputStreamTestPacketSizes) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));

  int count = 0;

  // 10 ms packet size.

  // Create default WASAPI input stream which records in stereo using
  // the shared mixing rate. The default buffer size is 10ms.
  AudioInputStreamWrapper aisw(audio_manager_.get());
  ScopedAudioInputStream ais(aisw.Create());
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);

  MockAudioInputCallback sink;

  {
    // We use 10ms packets and will run the test until ten packets are received.
    // All should contain valid packets of the same size and a valid delay
    // estimate.
    base::RunLoop run_loop;
    EXPECT_CALL(sink, OnData(NotNull(), _, _, _))
        .Times(AtLeast(10))
        .WillRepeatedly(CheckCountAndPostQuitTask(
            &count, 10, task_environment_.GetMainThreadTaskRunner(),
            run_loop.QuitWhenIdleClosure()));
    ais->Start(&sink);
    run_loop.Run();
    ais->Stop();
  }

  // Store current packet size (to be used in the subsequent tests).
  int frames_per_buffer_10ms = aisw.frames_per_buffer();

  ais.Close();

  // 20 ms packet size.

  count = 0;
  ais.Reset(aisw.Create(2 * frames_per_buffer_10ms));
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(sink, OnData(NotNull(), _, _, _))
        .Times(AtLeast(10))
        .WillRepeatedly(CheckCountAndPostQuitTask(
            &count, 10, task_environment_.GetMainThreadTaskRunner(),
            run_loop.QuitWhenIdleClosure()));
    ais->Start(&sink);
    run_loop.Run();
    ais->Stop();
    ais.Close();
  }

  // 5 ms packet size.

  count = 0;
  ais.Reset(aisw.Create(frames_per_buffer_10ms / 2));
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(sink, OnData(NotNull(), _, _, _))
        .Times(AtLeast(10))
        .WillRepeatedly(CheckCountAndPostQuitTask(
            &count, 10, task_environment_.GetMainThreadTaskRunner(),
            run_loop.QuitWhenIdleClosure()));
    ais->Start(&sink);
    run_loop.Run();
    ais->Stop();
    ais.Close();
  }
}

class WinAudioInputLoopbackTest : public WinAudioInputTest {
 public:
  WinAudioInputLoopbackTest() : device_info_accessor_(audio_manager_.get()) {
    // Defer stream creation and parameter fetching to SetUp.
  }

  void SetUp() override {
    // Abort early if requirements are not met.
    bool prerequisites_met = device_info_accessor_.HasAudioOutputDevices() &&
                             CoreAudioUtil::IsSupported();
    if (!prerequisites_met) {
      GTEST_SKIP() << "Missing audio output devices or CoreAudio support";
    }

    CreateParameters();
    CreateStreams();
  }

  void CreateParameters() {
    params_ = device_info_accessor_.GetInputStreamParameters(
        AudioDeviceDescription::kLoopbackInputDeviceId);
    output_params_ =
        device_info_accessor_.GetOutputStreamParameters(std::string());
  }

  void CreateStreams() {
    stream_.Reset(audio_manager_->MakeAudioInputStream(
        params_, AudioDeviceDescription::kLoopbackInputDeviceId,
        base::BindRepeating(&LogCallbackDummy)));
    output_stream_.Reset(audio_manager_->MakeAudioOutputStream(
        output_params_, std::string(), base::BindRepeating(&LogCallbackDummy)));

    ASSERT_THAT(stream_.get(), NotNull());
    ASSERT_THAT(stream_->Open(), Eq(AudioInputStream::OpenOutcome::kSuccess));
    ASSERT_THAT(output_stream_.get(), NotNull());
    ASSERT_TRUE(output_stream_->Open());
  }

 protected:
  AudioDeviceInfoAccessorForTests device_info_accessor_;
  AudioParameters params_;
  AudioParameters output_params_;
  ScopedAudioInputStream stream_;
  ScopedAudioOutputStream output_stream_;
};

TEST_F(WinAudioInputLoopbackTest, ValidateMatchingInputOutputParameters) {
  // Input parameters should be the same as default output parameters in
  // loopback capturing mode.
  ASSERT_THAT(params_.sample_rate(), Eq(output_params_.sample_rate()));
  ASSERT_THAT(params_.channel_layout(), Eq(output_params_.channel_layout()));
}

TEST_F(WinAudioInputLoopbackTest,
       LoopbackEventsWhenDefaultOutputDeviceIsRenderingAudio) {
  // Start a silent output stream and ensure that rendering starts.
  FakeAudioOutputCallback source;
  output_stream_->Start(&source);
  output_stream_->SetVolume(0.0);
  source.WaitForMoreData();

  EXPECT_EQ(source.num_callbacks(), 1);
  EXPECT_GT(source.num_rendered_audio_frames(), 0);
  EXPECT_FALSE(source.error());

  // Start the loopback stream and verify that loopback events are now fired
  // since the default audio output device plays out audio.
  FakeAudioInputCallback sink;
  stream_->Start(&sink);
  ASSERT_FALSE(sink.error());
  sink.WaitForData();
  sink.WaitForData();
  stream_.Close();
  output_stream_.Close();

  EXPECT_EQ(sink.num_callbacks(), 2);
  EXPECT_GT(sink.num_received_audio_frames(), 0);
  EXPECT_FALSE(sink.error());
}

class WinAudioProcessLoopbackTest
    : public ::testing::TestWithParam<std::string> {
 public:
  WinAudioProcessLoopbackTest()
      : audio_manager_(AudioManager::CreateForTesting(
            std::make_unique<TestAudioThread>())),
        device_info_accessor_(audio_manager_.get()) {
    // Ensure that the AudioManager's thread (TestAudioThread) has processed
    // its initial tasks posted during AudioManager::CreateForTesting.
    FlushTaskRunner(audio_manager_->GetTaskRunner());
    // Defer stream creation and parameter fetching to SetUp.
  }

  ~WinAudioProcessLoopbackTest() override {
    CHECK_DEREF(audio_manager_.get()).Shutdown();
  }

  void SetUp() override {
    // Abort early if requirements are not met.
    bool prerequisites_met = CoreAudioUtil::IsSupported();
    if (!prerequisites_met) {
      GTEST_SKIP() << "Missing audio output devices or CoreAudio support";
    }

    CreateParameters();
    CreateStream();
  }

  void CreateParameters() {
    params_ = device_info_accessor_.GetInputStreamParameters(GetParam());
  }

  void CreateStream() {
    std::string device_id =
        GetParam() == AudioDeviceDescription::kApplicationLoopbackDeviceId
            ? kMockApplicationLoopbackDeviceId
            : GetParam();
    stream_.Reset(audio_manager_->MakeAudioInputStream(
        params_, device_id, base::BindRepeating(&LogCallbackDummy)));
    EXPECT_THAT(stream_.get(), NotNull());
  }

  void OverrideAsyncActivationTimeout(base::TimeDelta timeout_ms) {
    AudioInputStreamDataInterceptor* audio_input_stream_data_interceptor =
        static_cast<AudioInputStreamDataInterceptor*>(stream_.get());
    static_cast<WASAPIAudioInputStream*>(
        audio_input_stream_data_interceptor->GetUnderlyingStreamForTesting())
        ->OverrideAsyncActivationTimeoutForTesting(timeout_ms);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AudioManager> audio_manager_;
  AudioDeviceInfoAccessorForTests device_info_accessor_;
  AudioParameters params_;
  ScopedAudioInputStream stream_;
  FakeWinWASAPIEnvironment fake_wasapi_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_P(WinAudioProcessLoopbackTest, OpenStreamSuccess) {
  ASSERT_THAT(stream_->Open(), Eq(AudioInputStream::OpenOutcome::kSuccess));
  histogram_tester_.ExpectTotalCount(
      "Media.Audio.Capture.Win.TimeToGetAudioClient", 1);
  histogram_tester_.ExpectBucketCount(
      "Media.Audio.Capture.Win.GetAudioClientTimedOut", false, 1);
}

TEST_P(WinAudioProcessLoopbackTest,
       OpenStreamActivateAudioInterfaceAsyncFailed) {
  fake_wasapi_environment_.SimulateError(
      WASAPITestErrorCode::kActivateAudioInterfaceAsyncFailed);
  EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kFailed);
  histogram_tester_.ExpectTotalCount(
      "Media.Audio.Capture.Win.TimeToGetAudioClient", 0);
  histogram_tester_.ExpectTotalCount(
      "Media.Audio.Capture.Win.GetAudioClientTimedOut", 0);
}

TEST_P(WinAudioProcessLoopbackTest,
       OpenInputStreamActivateAudioInterfaceAsyncOperationTimedOut) {
  fake_wasapi_environment_.SimulateError(
      WASAPITestErrorCode::kAudioClientActivationTimeout);
  // Override the default timeout so that this test can run quickly. The default
  // timeout is 10 seconds.
  OverrideAsyncActivationTimeout(kShortAsyncActivationTimeoutMs);
  EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kFailed);
  histogram_tester_.ExpectTotalCount(
      "Media.Audio.Capture.Win.TimeToGetAudioClient", 0);
  histogram_tester_.ExpectBucketCount(
      "Media.Audio.Capture.Win.GetAudioClientTimedOut", true, 1);
}

TEST_P(WinAudioProcessLoopbackTest,
       OpenStreamAudioClientActivationAsyncOperationFailed) {
  fake_wasapi_environment_.SimulateError(
      WASAPITestErrorCode::kAudioClientActivationAsyncOperationFailed);
  // Override the default timeout so that this test can run quickly. The default
  // timeout is 10 seconds.
  OverrideAsyncActivationTimeout(kShortAsyncActivationTimeoutMs);
  EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kFailed);
  histogram_tester_.ExpectTotalCount(
      "Media.Audio.Capture.Win.TimeToGetAudioClient", 0);
  histogram_tester_.ExpectBucketCount(
      "Media.Audio.Capture.Win.GetAudioClientTimedOut", true, 1);
}

TEST_P(WinAudioProcessLoopbackTest, OpenStreamAudioClientActivationFailed) {
  fake_wasapi_environment_.SimulateError(
      WASAPITestErrorCode::kAudioClientActivationFailed);
  EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kFailed);
  histogram_tester_.ExpectTotalCount(
      "Media.Audio.Capture.Win.TimeToGetAudioClient", 1);
  histogram_tester_.ExpectBucketCount(
      "Media.Audio.Capture.Win.GetAudioClientTimedOut", false, 1);
}

TEST_P(WinAudioProcessLoopbackTest, SuccessfulCapture) {
  ASSERT_THAT(stream_->Open(), Eq(AudioInputStream::OpenOutcome::kSuccess));

  FakeAudioInputCallback sink;
  stream_->Start(&sink);
  ASSERT_FALSE(sink.error());
  sink.WaitForData();
  sink.WaitForData();
  stream_.Close();

  EXPECT_EQ(sink.num_callbacks(), 2);
  EXPECT_GT(sink.num_received_audio_frames(), 0);
  EXPECT_FALSE(sink.error());
  histogram_tester_.ExpectTotalCount(
      "Media.Audio.Capture.Win.TimeToGetAudioClient", 1);
  histogram_tester_.ExpectBucketCount(
      "Media.Audio.Capture.Win.GetAudioClientTimedOut", false, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ProcessLoopbackDevices,
    WinAudioProcessLoopbackTest,
    ::testing::Values(AudioDeviceDescription::kApplicationLoopbackDeviceId,
                      AudioDeviceDescription::kLoopbackWithoutChromeId,
                      AudioDeviceDescription::kLoopbackAllDevicesId),
    [](const testing::TestParamInfo<WinAudioProcessLoopbackTest::ParamType>&
           info) {
      return info.param == AudioDeviceDescription::kApplicationLoopbackDeviceId
                 ? "ApplicationLoopback"
                 : (info.param ==
                            AudioDeviceDescription::kLoopbackWithoutChromeId
                        ? "LoopbackWithoutChromeId"
                        : "LoopbackAllDevices");
    });

// This test is intended for manual tests and should only be enabled
// when it is required to store the captured data on a local file.
// By default, GTest will print out YOU HAVE 1 DISABLED TEST.
// To include disabled tests in test execution, just invoke the test program
// with --gtest_also_run_disabled_tests or set the GTEST_ALSO_RUN_DISABLED_TESTS
// environment variable to a value greater than 0.
TEST_F(WinAudioInputTest, DISABLED_WASAPIAudioInputStreamRecordToFile) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));

  // Name of the output PCM file containing captured data. The output file
  // will be stored in the directory containing 'media_unittests.exe'.
  // Example of full name: \src\build\Debug\out_stereo_10sec.pcm.
  const char* file_name = "out_10sec.pcm";

  AudioInputStreamWrapper aisw(audio_manager_.get());
  ScopedAudioInputStream ais(aisw.Create());
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);

  VLOG(0) << ">> Sample rate: " << aisw.sample_rate() << " [Hz]";
  WriteToFileAudioSink file_sink(file_name);
  VLOG(0) << ">> Speak into the default microphone while recording.";
  ais->Start(&file_sink);
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());
  ais->Stop();
  VLOG(0) << ">> Recording has stopped.";
  ais.Close();
}

// As above, intended for manual testing only but this time using the raw
// capture mode.
TEST_F(WinAudioInputTest, DISABLED_WASAPIAudioInputStreamRecordToFileRAW) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));

  // Name of the output PCM file containing captured data. The output file
  // will be stored in the directory containing 'media_unittests.exe'.
  // Example of full name: \src\build\Debug\out_stereo_10sec_raw.pcm.
  const char* file_name = "out_10sec_raw.pcm";

  AudioInputStreamWrapper aisw(audio_manager_.get());
  ScopedAudioInputStream ais(aisw.Create());
  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);

  VLOG(0) << ">> Sample rate: " << aisw.sample_rate() << " [Hz]";
  WriteToFileAudioSink file_sink(file_name);
  VLOG(0) << ">> Speak into the default microphone while recording.";
  ais->Start(&file_sink);
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());
  ais->Stop();
  VLOG(0) << ">> Recording has stopped.";
  ais.Close();
}

TEST_F(WinAudioInputTest, DISABLED_WASAPIAudioInputStreamResampleToFile) {
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));

  // This is basically the same test as WASAPIAudioInputStreamRecordToFile
  // except it forces use of a different sample rate than is preferred by
  // the hardware.  This functionality is offered while we still have code
  // that doesn't ask the lower levels for what the preferred audio parameters
  // are (and previously depended on the old Wave API to do this automatically).

  struct TestData {
    const int rate;
    const int frames;
    ChannelLayoutConfig layout;
  } tests[] = {
      {8000, 80, media::ChannelLayoutConfig::Mono()},
      {8000, 80, media::ChannelLayoutConfig::Stereo()},
      {44100, 441, media::ChannelLayoutConfig::Mono()},
      {44100, 1024, media::ChannelLayoutConfig::Stereo()},
  };

  for (const auto& test : tests) {
    AudioParameters params;
    ASSERT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(
        AudioDeviceDescription::kDefaultDeviceId, false, &params)));

    VLOG(0) << ">> Hardware sample rate: " << params.sample_rate() << " [Hz]";
    VLOG(0) << ">> Hardware channel layout: "
            << ChannelLayoutToString(params.channel_layout());

    // Pick a somewhat difficult sample rate to convert too.
    // If the sample rate is 8kHz, 16kHz, 32kHz, 48kHz etc, we convert to
    // 44.1kHz.
    // Otherwise (e.g. 44.1kHz, 22.05kHz etc) we convert to 48kHz.
    const int hw_sample_rate = params.sample_rate();
    params.Reset(params.format(), test.layout, test.rate, test.frames);

    std::string file_name(base::StringPrintf(
        "resampled_10sec_%i_to_%i_%s.pcm", hw_sample_rate, params.sample_rate(),
        ChannelLayoutToString(params.channel_layout())));

    AudioInputStreamWrapper aisw(audio_manager_.get(), params);
    ScopedAudioInputStream ais(aisw.Create());
    EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);

    VLOG(0) << ">> Resampled rate will be: " << aisw.sample_rate() << " [Hz]";
    VLOG(0) << ">> New layout will be: "
            << ChannelLayoutToString(params.channel_layout());
    WriteToFileAudioSink file_sink(file_name.c_str());
    VLOG(0) << ">> Speak into the default microphone while recording.";
    ais->Start(&file_sink);
    base::PlatformThread::Sleep(TestTimeouts::action_timeout());
    ais->Stop();
    VLOG(0) << ">> Recording has stopped.";
    ais.Close();
  }
}

}  // namespace media
