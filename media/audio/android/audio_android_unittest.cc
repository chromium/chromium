// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/android/aaudio_stream_wrapper.h"
#include "media/audio/android/audio_device_type.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/decoder_buffer.h"
#include "media/base/seekable_buffer.h"
#include "media/base/test_data_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

namespace media {
namespace {

ACTION_P4(CheckCountAndPostQuitTask, count, limit, task_runner, quit_closure) {
  if (++*count >= limit) {
    task_runner->PostTask(FROM_HERE, quit_closure);
  }
}

constexpr float kCallbackTestTimeMs = 2000.0;
constexpr size_t kBytesPerSample = 2;
constexpr SampleFormat kSampleFormat = kSampleFormatS16;

// Converts AudioParameters::Format enumerator to readable string.
std::string FormatToString(AudioParameters::Format format) {
  switch (format) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      return std::string("AUDIO_PCM_LINEAR");
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      return std::string("AUDIO_PCM_LOW_LATENCY");
    case AudioParameters::AUDIO_FAKE:
      return std::string("AUDIO_FAKE");
    default:
      return std::string();
  }
}

// Converts ChannelLayout enumerator to readable string. Does not include
// multi-channel cases since these layouts are not supported on Android.
std::string LayoutToString(ChannelLayout channel_layout) {
  switch (channel_layout) {
    case CHANNEL_LAYOUT_NONE:
      return std::string("CHANNEL_LAYOUT_NONE");
    case CHANNEL_LAYOUT_MONO:
      return std::string("CHANNEL_LAYOUT_MONO");
    case CHANNEL_LAYOUT_STEREO:
      return std::string("CHANNEL_LAYOUT_STEREO");
    case CHANNEL_LAYOUT_UNSUPPORTED:
    default:
      return std::string("CHANNEL_LAYOUT_UNSUPPORTED");
  }
}

double ExpectedTimeBetweenCallbacks(AudioParameters params) {
  return (base::Microseconds(params.frames_per_buffer() *
                             base::Time::kMicrosecondsPerSecond /
                             static_cast<double>(params.sample_rate())))
      .InMillisecondsF();
}

// Helper method which verifies that the device list starts with a valid
// default device name followed by non-default device names.
void CheckDeviceDescriptions(
    const AudioDeviceDescriptions& device_descriptions) {
  DVLOG(2) << "Got " << device_descriptions.size() << " audio devices.";
  if (device_descriptions.empty()) {
    // Log a warning so we can see the status on the build bots.  No need to
    // break the test though since this does successfully test the code and
    // some failure cases.
    LOG(WARNING) << "No input devices detected";
    return;
  }

  AudioDeviceDescriptions::const_iterator it = device_descriptions.begin();

  // The first device in the list should always be the default device.
  EXPECT_EQ(std::string(AudioDeviceDescription::kDefaultDeviceId),
            it->unique_id);
  ++it;

  // Other devices should have non-empty name and id and should not contain
  // default name or id.
  while (it != device_descriptions.end()) {
    EXPECT_FALSE(it->device_name.empty());
    EXPECT_FALSE(it->unique_id.empty());
    EXPECT_FALSE(it->group_id.empty());
    DVLOG(2) << "Device ID(" << it->unique_id << "), label: " << it->device_name
             << " group: " << it->group_id;
    EXPECT_NE(AudioDeviceDescription::GetDefaultDeviceName(), it->device_name);
    EXPECT_NE(std::string(AudioDeviceDescription::kDefaultDeviceId),
              it->unique_id);
    ++it;
  }
}

// We clear the data bus to ensure that the test does not cause noise.
int RealOnMoreData(base::TimeDelta /* delay */,
                   base::TimeTicks /* delay_timestamp */,
                   const AudioGlitchInfo& /* glitch_info */,
                   AudioBus* dest) {
  dest->Zero();
  return dest->frames();
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const AudioParameters& params) {
  using std::endl;
  os << endl
     << "format: " << FormatToString(params.format()) << endl
     << "channel layout: " << LayoutToString(params.channel_layout()) << endl
     << "sample rate: " << params.sample_rate() << endl
     << "frames per buffer: " << params.frames_per_buffer() << endl
     << "channels: " << params.channels() << endl
     << "bytes per buffer: " << params.GetBytesPerBuffer(kSampleFormat) << endl
     << "bytes per second: "
     << params.sample_rate() * params.GetBytesPerFrame(kSampleFormat) << endl
     << "bytes per frame: " << params.GetBytesPerFrame(kSampleFormat) << endl
     << "chunk size in ms: " << ExpectedTimeBetweenCallbacks(params) << endl
     << "echo_canceller: "
     << (params.effects() & AudioParameters::ECHO_CANCELLER);
  return os;
}

enum class AudioApi {
  AAudioWithPerStreamDeviceSelection,
  AAudioWithCommunicationDevices,
  OpenSLES
};

std::ostream& operator<<(std::ostream& os, const AudioApi& audio_api) {
  switch (audio_api) {
    case AudioApi::AAudioWithPerStreamDeviceSelection:
      os << "AAudioWithPerStreamDeviceSelection";
      break;
    case AudioApi::AAudioWithCommunicationDevices:
      os << "AAudioWithCommunicationDevices";
      break;
    case AudioApi::OpenSLES:
      os << "OpenSLES";
      break;
  }
  return os;
}

// Gmock implementation of AudioInputStream::AudioInputCallback.
class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD4(OnData,
               void(const AudioBus* src,
                    base::TimeTicks capture_time,
                    double volume,
                    const AudioGlitchInfo& glitch_info));
  MOCK_METHOD0(OnError, void());
};

// Implements AudioOutputStream::AudioSourceCallback and provides audio data
// by reading from a data file.
class FileAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit FileAudioSource(base::WaitableEvent* event, const std::string& name)
      : event_(event) {
    // Reads a test file from media/test/data directory and stores it in
    // a DecoderBuffer.
    file_ = ReadTestDataFile(name);

    // Log the name of the file which is used as input for this test.
    base::FilePath file_path = GetTestDataFilePath(name);
    DVLOG(0) << "Reading from file: " << file_path.value().c_str();
  }

  FileAudioSource(const FileAudioSource&) = delete;
  FileAudioSource& operator=(const FileAudioSource&) = delete;

  ~FileAudioSource() override {}

  // AudioOutputStream::AudioSourceCallback implementation.

  // Use samples read from a data file and fill up the audio buffer
  // provided to us in the callback.
  int OnMoreData(base::TimeDelta /* delay */,
                 base::TimeTicks /* delay_timestamp */,
                 const AudioGlitchInfo& /* glitch_info */,
                 AudioBus* dest) override {
    bool stop_playing = false;
    size_t max_size = dest->frames() * dest->channels() * kBytesPerSample;

    // Adjust data size and prepare for end signal if file has ended.
    if (pos_ + max_size > file_size()) {
      stop_playing = true;
      max_size = file_size() - pos_;
    }

    // File data is stored as interleaved 16-bit values. Copy data samples from
    // the file and deinterleave to match the audio bus format.
    // FromInterleaved() will zero out any unfilled frames when there is not
    // sufficient data remaining in the file to fill up the complete frame.
    size_t frames = max_size / (dest->channels() * kBytesPerSample);
    if (max_size) {
      auto* source = reinterpret_cast<const int16_t*>(
          base::span<const uint8_t>(*file_).subspan(pos_).data());
      dest->FromInterleaved<SignedInt16SampleTypeTraits>(
          source, base::checked_cast<int>(frames));
      pos_ += max_size;
    }

    // Set event to ensure that the test can stop when the file has ended.
    if (stop_playing) {
      event_->Signal();
    }

    return frames;
  }

  void OnError(ErrorType type) override {}

  size_t file_size() const { return file_->size(); }

 private:
  raw_ptr<base::WaitableEvent> event_;
  size_t pos_ = 0;
  scoped_refptr<DecoderBuffer> file_;
};

// Implements AudioInputStream::AudioInputCallback and writes the recorded
// audio data to a local output file. Note that this implementation should
// only be used for manually invoked and evaluated tests, hence the created
// file will not be destroyed after the test is done since the intention is
// that it shall be available for off-line analysis.
class FileAudioSink : public AudioInputStream::AudioInputCallback {
 public:
  explicit FileAudioSink(base::WaitableEvent* event,
                         const AudioParameters& params,
                         const std::string& file_name)
      : event_(event), params_(params) {
    // Allocate space for ~10 seconds of data.
    const int kMaxBufferSize =
        10 * params.sample_rate() * params.GetBytesPerFrame(kSampleFormat);
    buffer_ = std::make_unique<media::SeekableBuffer>(0, kMaxBufferSize);

    // Open up the binary file which will be written to in the destructor.
    base::FilePath file_path;
    EXPECT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path));
    file_path = file_path.AppendASCII(file_name.c_str());
    binary_file_ = base::OpenFile(file_path, "wb");
    DLOG_IF(ERROR, !binary_file_) << "Failed to open binary PCM data file.";
    DVLOG(0) << "Writing to file: " << file_path.value().c_str();
  }

  FileAudioSink(const FileAudioSink&) = delete;
  FileAudioSink& operator=(const FileAudioSink&) = delete;

  ~FileAudioSink() override {
    size_t bytes_written = 0;
    while (bytes_written < buffer_->forward_capacity()) {
      const base::span<const uint8_t> chunk = buffer_->GetCurrentChunk();
      // Stop writing if no more data is available.
      if (chunk.empty()) {
        break;
      }

      // Write recorded data chunk to the file and prepare for next chunk.
      // TODO(henrika): use file_util:: instead.
      UNSAFE_TODO(fwrite(chunk.data(), 1, chunk.size(), binary_file_));
      buffer_->Seek(chunk.size());
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
    if (!buffer_->Append(base::as_bytes(interleaved.as_span()))) {
      event_->Signal();
    }
  }

  void OnError() override {}

 private:
  raw_ptr<base::WaitableEvent> event_;
  AudioParameters params_;
  std::unique_ptr<media::SeekableBuffer> buffer_;
  raw_ptr<FILE> binary_file_;
};

// Implements AudioInputCallback and AudioSourceCallback to support full
// duplex audio where captured samples are played out in loopback after
// reading from a temporary FIFO storage.
class FullDuplexAudioSinkSource
    : public AudioInputStream::AudioInputCallback,
      public AudioOutputStream::AudioSourceCallback {
 public:
  explicit FullDuplexAudioSinkSource(const AudioParameters& params)
      : params_(params), previous_time_(base::TimeTicks::Now()) {
    // Start with a reasonably small FIFO size. It will be increased
    // dynamically during the test if required.
    size_t buffer_size = params.GetBytesPerBuffer(kSampleFormat);
    fifo_ = std::make_unique<media::SeekableBuffer>(0, 2 * buffer_size);
    buffer_ = base::HeapArray<uint8_t>::Uninit(buffer_size);
  }

  FullDuplexAudioSinkSource(const FullDuplexAudioSinkSource&) = delete;
  FullDuplexAudioSinkSource& operator=(const FullDuplexAudioSinkSource&) =
      delete;

  ~FullDuplexAudioSinkSource() override {}

  // AudioInputStream::AudioInputCallback implementation
  void OnError() override {}
  void OnData(const AudioBus* src,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    const base::TimeTicks now_time = base::TimeTicks::Now();
    const int diff = (now_time - previous_time_).InMilliseconds();

    const int num_samples = src->frames() * src->channels();
    auto interleaved = base::HeapArray<int16_t>::Uninit(num_samples);
    src->ToInterleaved<SignedInt16SampleTypeTraits>(src->frames(),
                                                    interleaved.data());

    const auto byte_span = base::as_bytes(interleaved.as_span());
    base::AutoLock lock(lock_);
    if (diff > 1000) {
      started_ = true;
      previous_time_ = now_time;

      // Log out the extra delay added by the FIFO. This is a best effort
      // estimate. We might be +- 10ms off here.
      int extra_fifo_delay = static_cast<int>(
          BytesToMilliseconds(fifo_->forward_bytes() + byte_span.size()));
      DVLOG(1) << extra_fifo_delay;
    }

    // We add an initial delay of ~1 second before loopback starts to ensure
    // a stable callback sequence and to avoid initial bursts which might add
    // to the extra FIFO delay.
    if (!started_) {
      return;
    }

    // Append new data to the FIFO and extend the size if the max capacity
    // was exceeded. Flush the FIFO when extended just in case.
    if (!fifo_->Append(byte_span)) {
      fifo_->set_forward_capacity(2 * fifo_->forward_capacity());
      fifo_->Clear();
    }
  }

  // AudioOutputStream::AudioSourceCallback implementation
  void OnError(ErrorType type) override {}
  int OnMoreData(base::TimeDelta /* delay */,
                 base::TimeTicks /* delay_timestamp */,
                 const AudioGlitchInfo& /* glitch_info */,
                 AudioBus* dest) override {
    const size_t size_in_bytes =
        kBytesPerSample * dest->frames() * dest->channels();
    EXPECT_EQ(size_in_bytes,
              static_cast<size_t>(params_.GetBytesPerBuffer(kSampleFormat)));

    base::AutoLock lock(lock_);

    // We add an initial delay of ~1 second before loopback starts to ensure
    // a stable callback sequences and to avoid initial bursts which might add
    // to the extra FIFO delay.
    if (!started_) {
      dest->Zero();
      return dest->frames();
    }

    // Fill up destination with zeros if the FIFO does not contain enough
    // data to fulfill the request.
    if (fifo_->forward_bytes() < size_in_bytes) {
      dest->Zero();
    } else {
      fifo_->Read(buffer_.subspan(size_in_bytes));
      dest->FromInterleaved<SignedInt16SampleTypeTraits>(
          reinterpret_cast<int16_t*>(buffer_.data()), dest->frames());
    }

    return dest->frames();
  }

 private:
  // Converts from bytes to milliseconds given number of bytes and existing
  // audio parameters.
  double BytesToMilliseconds(int bytes) const {
    const int frames = bytes / params_.GetBytesPerFrame(kSampleFormat);
    return (base::Microseconds(frames * base::Time::kMicrosecondsPerSecond /
                               static_cast<double>(params_.sample_rate())))
        .InMillisecondsF();
  }

  AudioParameters params_;
  base::TimeTicks previous_time_;
  base::Lock lock_;
  std::unique_ptr<media::SeekableBuffer> fifo_;
  base::HeapArray<uint8_t> buffer_;
  bool started_ = false;
};

// Test fixture class for tests which only exercise the output path. It is
// value-parameterized to test against both the AAudio and OpenSLES paths.
class AudioAndroidOutputTest : public testing::TestWithParam<AudioApi> {
 public:
  AudioAndroidOutputTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        audio_manager_(AudioManager::CreateForTesting(
            std::make_unique<TestAudioThread>())),
        audio_manager_device_info_(audio_manager_.get()),
        audio_output_stream_(nullptr) {
    // Flush the message loop to ensure that AudioManager is fully initialized.
    base::RunLoop().RunUntilIdle();

    if (HasParam()) {
      InitFeatures(GetParam());
    }
  }

  AudioAndroidOutputTest(const AudioAndroidOutputTest&) = delete;
  AudioAndroidOutputTest& operator=(const AudioAndroidOutputTest&) = delete;

  ~AudioAndroidOutputTest() override {
    audio_manager_->Shutdown();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  AudioManager* audio_manager() { return audio_manager_.get(); }
  AudioDeviceInfoAccessorForTests* audio_manager_device_info() {
    return &audio_manager_device_info_;
  }

  // Returns whether the test is running with a parameter, meaning GetParam()
  // has a valid value.
  bool HasParam() {
    return testing::UnitTest::GetInstance()
               ->current_test_info()
               ->value_param() != nullptr;
  }

  void InitFeatures(AudioApi audio_api) {
    bool enable_aaudio = false;
    bool enable_aaudio_per_stream_device_selection = false;
    switch (audio_api) {
      case AudioApi::AAudioWithPerStreamDeviceSelection:
        enable_aaudio = true;
        enable_aaudio_per_stream_device_selection = true;
        break;
      case AudioApi::AAudioWithCommunicationDevices:
        enable_aaudio = true;
        break;
      case AudioApi::OpenSLES:
        break;
    }

    if (enable_aaudio) {
      if (!__builtin_available(android AAUDIO_MIN_API, *)) {
        GTEST_SKIP() << "AAudio is not available.";
      }
    } else {
      // Use OpenSL ES fallback
#if !BUILDFLAG(USE_OPENSLES)
      GTEST_SKIP() << "OpenSLES is not available.";
#endif
    }

    base::flat_map<base::test::FeatureRef, bool> feature_states(
        {{features::kUseAAudioDriver, enable_aaudio},
         {features::kUseAAudioInput, enable_aaudio},
         {features::kAAudioPerStreamDeviceSelection,
          enable_aaudio_per_stream_device_selection}});
    feature_list_.InitWithFeatureStates(feature_states);
  }

  // Synchronously runs the provided callback/closure on the audio thread.
  void RunOnAudioThread(base::OnceClosure closure) {
    if (!audio_manager()->GetTaskRunner()->BelongsToCurrentThread()) {
      base::WaitableEvent event(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED);
      audio_manager()->GetTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&AudioAndroidOutputTest::RunOnAudioThreadImpl,
                         base::Unretained(this), std::move(closure), &event));
      event.Wait();
    } else {
      std::move(closure).Run();
    }
  }

  void RunOnAudioThreadImpl(base::OnceClosure closure,
                            base::WaitableEvent* event) {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    std::move(closure).Run();
    event->Signal();
  }

  AudioParameters GetDefaultOutputStreamParametersOnAudioThread() {
    RunOnAudioThread(base::BindOnce(
        [](AudioAndroidOutputTest* self) {
          std::string default_device_id =
              AudioDeviceDescription::kDefaultDeviceId;
          self->audio_output_parameters_ =
              self->audio_manager_device_info()->GetOutputStreamParameters(
                  default_device_id);
          EXPECT_TRUE(self->audio_output_parameters_.IsValid());
        },
        base::Unretained(this)));
    return audio_output_parameters_;
  }

  AudioDeviceDescriptions GetAudioOutputDeviceDescriptionsOnAudioThread() {
    AudioDeviceDescriptions devices;
    RunOnAudioThread(base::BindOnce(
        &AudioDeviceInfoAccessorForTests::GetAudioOutputDeviceDescriptions,
        base::Unretained(audio_manager_device_info()), &devices));
    return devices;
  }

  void MakeAudioOutputStreamOnAudioThread(
      const AudioParameters& params,
      const std::string& device_id = AudioDeviceDescription::kDefaultDeviceId) {
    RunOnAudioThread(base::BindOnce(&AudioAndroidOutputTest::MakeOutputStream,
                                    base::Unretained(this), params, device_id));
  }

  void CloseAudioOutputStreamOnAudioThread(raw_ptr<AudioOutputStream> stream) {
    RunOnAudioThread(
        base::BindOnce(&AudioOutputStream::Close, base::Unretained(stream)));
  }

  void OpenAudioOutputStreamOnAudioThread() {
    RunOnAudioThread(
        base::BindOnce(&AudioAndroidOutputTest::Open, base::Unretained(this)));
  }

  void OpenAndCloseAudioOutputStreamOnAudioThread() {
    RunOnAudioThread(base::BindOnce(&AudioAndroidOutputTest::OpenAndClose,
                                    base::Unretained(this)));
  }

  void OpenAndStartAudioOutputStreamOnAudioThread(
      AudioOutputStream::AudioSourceCallback* source) {
    RunOnAudioThread(base::BindOnce(&AudioAndroidOutputTest::OpenAndStart,
                                    base::Unretained(this), source));
  }

  void StopAndCloseAudioOutputStreamOnAudioThread() {
    RunOnAudioThread(base::BindOnce(&AudioAndroidOutputTest::StopAndClose,
                                    base::Unretained(this)));
  }

  double AverageTimeBetweenCallbacks(int num_callbacks) const {
    return ((end_time_ - start_time_) / static_cast<double>(num_callbacks - 1))
        .InMillisecondsF();
  }

  void StartOutputStreamCallbacks(const AudioParameters& params) {
    double expected_time_between_callbacks_ms =
        ExpectedTimeBetweenCallbacks(params);
    const int num_callbacks =
        (kCallbackTestTimeMs / expected_time_between_callbacks_ms);
    MakeAudioOutputStreamOnAudioThread(params);

    int count = 0;
    MockAudioSourceCallback source;

    base::RunLoop run_loop;
    EXPECT_CALL(source, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
        .Times(AtLeast(num_callbacks))
        .WillRepeatedly(
            DoAll(CheckCountAndPostQuitTask(
                      &count, num_callbacks,
                      base::SingleThreadTaskRunner::GetCurrentDefault(),
                      run_loop.QuitWhenIdleClosure()),
                  Invoke(RealOnMoreData)));
    EXPECT_CALL(source, OnError(_)).Times(0);

    OpenAndStartAudioOutputStreamOnAudioThread(&source);

    start_time_ = base::TimeTicks::Now();
    run_loop.Run();
    end_time_ = base::TimeTicks::Now();

    StopAndCloseAudioOutputStreamOnAudioThread();

    double average_time_between_callbacks_ms =
        AverageTimeBetweenCallbacks(num_callbacks);
    DVLOG(1) << "expected time between callbacks: "
             << expected_time_between_callbacks_ms << " ms";
    DVLOG(1) << "average time between callbacks: "
             << average_time_between_callbacks_ms << " ms";
    EXPECT_GE(average_time_between_callbacks_ms,
              0.70 * expected_time_between_callbacks_ms);
    EXPECT_LT(average_time_between_callbacks_ms,
              2 * expected_time_between_callbacks_ms);
  }

  std::optional<AudioDeviceDescription> GetFirstNonDefaultOutputDevice() {
    AudioDeviceDescriptions devices =
        GetAudioOutputDeviceDescriptionsOnAudioThread();
    return GetFirstNonDefaultDeviceFromDescriptions(devices);
  }

  void MakeOutputStream(const AudioParameters& params,
                        const std::string& device_id) {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    audio_output_stream_ = audio_manager()->MakeAudioOutputStream(
        params, device_id, AudioManager::LogCallback());
    EXPECT_TRUE(audio_output_stream_);
  }

  void Open() {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    EXPECT_TRUE(audio_output_stream_->Open());
  }

  void OpenAndClose() {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    Open();
    audio_output_stream_->Close();
    audio_output_stream_ = nullptr;
  }

  void OpenAndStart(AudioOutputStream::AudioSourceCallback* source) {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    Open();
    audio_output_stream_->Start(source);
  }

  void StopAndClose() {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    audio_output_stream_->Stop();
    audio_output_stream_->Close();
    audio_output_stream_ = nullptr;
  }

  std::optional<AudioDeviceDescription>
  GetFirstNonDefaultDeviceFromDescriptions(
      const AudioDeviceDescriptions& devices) {
    for (AudioDeviceDescription device : devices) {
      if (!AudioDeviceDescription::IsDefaultDevice(device.unique_id)) {
        return device;
      }
    }
    return std::nullopt;
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<AudioManager> audio_manager_;
  AudioDeviceInfoAccessorForTests audio_manager_device_info_;
  AudioParameters audio_output_parameters_;
  raw_ptr<AudioOutputStream> audio_output_stream_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
};

// Test fixture class for tests which exercise the input path, or both input and
// output paths. It is value-parameterized to test against both the AAudio and
// OpenSLES paths.
class AudioAndroidInputTest : public AudioAndroidOutputTest {
 public:
  AudioAndroidInputTest() : audio_input_stream_(nullptr) {}

  AudioAndroidInputTest(const AudioAndroidInputTest&) = delete;
  AudioAndroidInputTest& operator=(const AudioAndroidInputTest&) = delete;

 protected:
  AudioParameters GetDefaultInputStreamParametersOnAudioThread() {
    RunOnAudioThread(
        base::BindOnce(&AudioAndroidInputTest::GetDefaultInputStreamParameters,
                       base::Unretained(this)));
    return audio_input_parameters_;
  }

  AudioDeviceDescriptions GetAudioInputDeviceDescriptionsOnAudioThread() {
    AudioDeviceDescriptions devices;
    RunOnAudioThread(base::BindOnce(
        &AudioDeviceInfoAccessorForTests::GetAudioInputDeviceDescriptions,
        base::Unretained(audio_manager_device_info()), &devices));
    return devices;
  }

  void MakeAudioInputStreamOnAudioThread(
      const AudioParameters& params,
      const std::string& device_id = AudioDeviceDescription::kDefaultDeviceId) {
    RunOnAudioThread(base::BindOnce(&AudioAndroidInputTest::MakeInputStream,
                                    base::Unretained(this), params, device_id));
  }

  void CloseAudioInputStreamOnAudioThread(raw_ptr<AudioInputStream> stream) {
    RunOnAudioThread(
        base::BindOnce(&AudioInputStream::Close, base::Unretained(stream)));
  }

  void OpenAudioInputStreamOnAudioThread() {
    RunOnAudioThread(
        base::BindOnce(&AudioAndroidInputTest::Open, base::Unretained(this)));
  }

  void OpenAndCloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(base::BindOnce(&AudioAndroidInputTest::OpenAndClose,
                                    base::Unretained(this)));
  }

  void OpenAndStartAudioInputStreamOnAudioThread(
      AudioInputStream::AudioInputCallback* sink) {
    RunOnAudioThread(base::BindOnce(&AudioAndroidInputTest::OpenAndStart,
                                    base::Unretained(this), sink));
  }

  void StopAndCloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(base::BindOnce(&AudioAndroidInputTest::StopAndClose,
                                    base::Unretained(this)));
  }

  void StartInputStreamCallbacks(const AudioParameters& params) {
    double expected_time_between_callbacks_ms =
        ExpectedTimeBetweenCallbacks(params);
    const int num_callbacks =
        (kCallbackTestTimeMs / expected_time_between_callbacks_ms);

    MakeAudioInputStreamOnAudioThread(params);

    int count = 0;
    MockAudioInputCallback sink;

    base::RunLoop run_loop;
    EXPECT_CALL(sink, OnData(NotNull(), _, _, _))
        .Times(AtLeast(num_callbacks))
        .WillRepeatedly(CheckCountAndPostQuitTask(
            &count, num_callbacks,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            run_loop.QuitWhenIdleClosure()));
    EXPECT_CALL(sink, OnError()).Times(0);

    OpenAndStartAudioInputStreamOnAudioThread(&sink);

    start_time_ = base::TimeTicks::Now();
    run_loop.Run();
    end_time_ = base::TimeTicks::Now();

    StopAndCloseAudioInputStreamOnAudioThread();

    double average_time_between_callbacks_ms =
        AverageTimeBetweenCallbacks(num_callbacks);
    DVLOG(0) << "expected time between callbacks: "
             << expected_time_between_callbacks_ms << " ms";
    DVLOG(0) << "average time between callbacks: "
             << average_time_between_callbacks_ms << " ms";
    EXPECT_GE(average_time_between_callbacks_ms,
              0.70 * expected_time_between_callbacks_ms);
    EXPECT_LE(average_time_between_callbacks_ms,
              1.30 * expected_time_between_callbacks_ms);
  }

  void GetDefaultInputStreamParameters() {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    audio_input_parameters_ =
        audio_manager_device_info()->GetInputStreamParameters(
            AudioDeviceDescription::kDefaultDeviceId);
  }

  std::optional<AudioDeviceDescription> GetFirstNonDefaultInputDevice() {
    AudioDeviceDescriptions devices =
        GetAudioInputDeviceDescriptionsOnAudioThread();
    return GetFirstNonDefaultDeviceFromDescriptions(devices);
  }

  void MakeInputStream(const AudioParameters& params,
                       const std::string& device_id) {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    audio_input_stream_ = audio_manager()->MakeAudioInputStream(
        params, device_id, AudioManager::LogCallback());
    EXPECT_TRUE(audio_input_stream_);
  }

  void Open() {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    EXPECT_EQ(audio_input_stream_->Open(),
              AudioInputStream::OpenOutcome::kSuccess);
  }

  void OpenAndClose() {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    Open();
    audio_input_stream_->Close();
    audio_input_stream_ = nullptr;
  }

  void OpenAndStart(AudioInputStream::AudioInputCallback* sink) {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    Open();
    audio_input_stream_->Start(sink);
  }

  void StopAndClose() {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    audio_input_stream_->Stop();
    audio_input_stream_->Close();
    audio_input_stream_ = nullptr;
  }

  raw_ptr<AudioInputStream> audio_input_stream_;
  AudioParameters audio_input_parameters_;
};

// TODO(crbug.com/417671787): Add test doubles for the device list fetch path
// and enhance and expand device list-dependent tests.

// Get the default audio input parameters.
TEST_F(AudioAndroidInputTest, GetDefaultInputStreamParameters) {
  AudioParameters params = GetDefaultInputStreamParametersOnAudioThread();
  EXPECT_TRUE(params.IsValid());
}

// Get the default audio output parameters.
TEST_F(AudioAndroidOutputTest, GetDefaultOutputStreamParameters) {
  AudioParameters params = GetDefaultOutputStreamParametersOnAudioThread();
  EXPECT_TRUE(params.IsValid());
}

// Verify input device enumeration.
TEST_F(AudioAndroidInputTest, GetAudioInputDeviceDescriptions) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info()->HasAudioInputDevices());
  AudioDeviceDescriptions devices =
      GetAudioInputDeviceDescriptionsOnAudioThread();
  CheckDeviceDescriptions(devices);
}

// Verify output device enumeration.
TEST_F(AudioAndroidOutputTest, GetAudioOutputDeviceDescriptions) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info()->HasAudioOutputDevices());
  AudioDeviceDescriptions devices =
      GetAudioOutputDeviceDescriptionsOnAudioThread();
  CheckDeviceDescriptions(devices);
}

// Ensure that a default input stream can be created and closed.
TEST_P(AudioAndroidInputTest, CreateAndCloseInputStream) {
  AudioParameters params = GetDefaultInputStreamParametersOnAudioThread();
  MakeAudioInputStreamOnAudioThread(params);
  CloseAudioInputStreamOnAudioThread(audio_input_stream_);
}

// Ensure that an input stream with a non-default device can be created and
// closed.
TEST_P(AudioAndroidInputTest, CreateAndCloseInputStreamWithDevice) {
  std::optional<AudioDeviceDescription> device =
      GetFirstNonDefaultInputDevice();
  if (!device.has_value()) {
    GTEST_SKIP() << "Missing non-default input device.";
  }
  AudioParameters params = GetDefaultInputStreamParametersOnAudioThread();
  MakeAudioInputStreamOnAudioThread(params, device->unique_id);
  CloseAudioInputStreamOnAudioThread(audio_input_stream_);
}

// Ensure that a default output stream can be created and closed.
// TODO(henrika): should we also verify that this API changes the audio mode
// to communication mode, and calls RegisterHeadsetReceiver, the first time
// it is called?
TEST_P(AudioAndroidOutputTest, CreateAndCloseOutputStream) {
  AudioParameters params = GetDefaultOutputStreamParametersOnAudioThread();
  MakeAudioOutputStreamOnAudioThread(params);
  CloseAudioOutputStreamOnAudioThread(audio_output_stream_);
}

// Ensure that an output stream with a non-default device can be created and
// closed. This test is only relevant for AAudioWithPerStreamDeviceSelection.
TEST_F(AudioAndroidOutputTest, CreateAndCloseOutputStreamWithDevice) {
  InitFeatures(AudioApi::AAudioWithPerStreamDeviceSelection);
  std::optional<AudioDeviceDescription> device =
      GetFirstNonDefaultOutputDevice();
  if (!device.has_value()) {
    GTEST_SKIP() << "Missing non-default output device.";
  }
  AudioParameters params = GetDefaultOutputStreamParametersOnAudioThread();
  MakeAudioOutputStreamOnAudioThread(params, device->unique_id);
  CloseAudioOutputStreamOnAudioThread(audio_output_stream_);
}

// Ensure that a default input stream can be opened and closed.
TEST_P(AudioAndroidInputTest, OpenAndCloseInputStream) {
  AudioParameters params = GetDefaultInputStreamParametersOnAudioThread();
  MakeAudioInputStreamOnAudioThread(params);
  OpenAndCloseAudioInputStreamOnAudioThread();
}

// Ensure that an input stream with a non-default device can be opened and
// closed, emitting a histogram value for successfully setting the
// device ID if AAudioWithPerStreamDeviceSelection is enabled.
TEST_P(AudioAndroidInputTest, OpenAndCloseInputStreamWithDevice) {
  std::optional<AudioDeviceDescription> device =
      GetFirstNonDefaultInputDevice();
  if (!device.has_value()) {
    GTEST_SKIP() << "Missing non-default input device.";
  }
  AudioParameters params = GetDefaultInputStreamParametersOnAudioThread();
  MakeAudioInputStreamOnAudioThread(params, device->unique_id);

  base::HistogramTester histogram_tester;
  OpenAudioInputStreamOnAudioThread();

  if (GetParam() == AudioApi::AAudioWithPerStreamDeviceSelection) {
    constexpr std::string_view kHistogramPrefix =
        "Media.Audio.Android.AAudioSetDeviceId.Input.";
    const std::string kSuccessHistogram =
        base::StrCat({kHistogramPrefix, "Success"});
    const std::string kFailureHistogram =
        base::StrCat({kHistogramPrefix, "Failure"});
    // Emitted a success with a known device type.
    histogram_tester.ExpectTotalCount(kSuccessHistogram, 1);
    histogram_tester.ExpectBucketCount(kSuccessHistogram,
                                       android::AudioDeviceType::kUnknown, 0);
    histogram_tester.ExpectTotalCount(kFailureHistogram, 0);
  }

  CloseAudioInputStreamOnAudioThread(audio_input_stream_);
}

// Ensure that a default output stream can be opened and closed.
TEST_P(AudioAndroidOutputTest, OpenAndCloseOutputStream) {
  AudioParameters params = GetDefaultOutputStreamParametersOnAudioThread();
  MakeAudioOutputStreamOnAudioThread(params);
  OpenAndCloseAudioOutputStreamOnAudioThread();
}

// Ensure that an output stream with a non-default device can be successfully
// opened and closed, emitting a histogram value for successfully setting the
// device ID. This test is only relevant for AAudioWithPerStreamDeviceSelection.
TEST_F(AudioAndroidOutputTest, OpenAndCloseOutputStreamWithDevice) {
  InitFeatures(AudioApi::AAudioWithPerStreamDeviceSelection);
  std::optional<AudioDeviceDescription> device =
      GetFirstNonDefaultOutputDevice();
  if (!device.has_value()) {
    GTEST_SKIP() << "Missing non-default output device.";
  }
  AudioParameters params = GetDefaultOutputStreamParametersOnAudioThread();
  MakeAudioOutputStreamOnAudioThread(params, device->unique_id);

  base::HistogramTester histogram_tester;
  OpenAudioOutputStreamOnAudioThread();

  constexpr std::string_view kHistogramPrefix =
      "Media.Audio.Android.AAudioSetDeviceId.Output.";
  const std::string kSuccessHistogram =
      base::StrCat({kHistogramPrefix, "Success"});
  const std::string kFailureHistogram =
      base::StrCat({kHistogramPrefix, "Failure"});
  // Emitted a success with a known device type.
  histogram_tester.ExpectTotalCount(kSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(kSuccessHistogram,
                                     android::AudioDeviceType::kUnknown, 0);
  histogram_tester.ExpectTotalCount(base::StrCat({kHistogramPrefix, "Failure"}),
                                    0);

  CloseAudioOutputStreamOnAudioThread(audio_output_stream_);
}

// Ensure that a default input stream can be opened, started, and closed without
// explicitly being stopped.
TEST_P(AudioAndroidInputTest, OpenStartAndCloseInputStream) {
  NiceMock<MockAudioInputCallback> callback;
  EXPECT_CALL(callback, OnError()).Times(0);

  AudioParameters params = GetDefaultInputStreamParametersOnAudioThread();
  MakeAudioInputStreamOnAudioThread(params);
  OpenAndStartAudioInputStreamOnAudioThread(&callback);
  CloseAudioInputStreamOnAudioThread(audio_input_stream_);
}

// Ensure that a default output stream can be opened, started, and closed
// without explicitly being stopped.
TEST_P(AudioAndroidOutputTest, OpenStartAndCloseOutputStream) {
  NiceMock<MockAudioSourceCallback> callback;
  EXPECT_CALL(callback, OnError(_)).Times(0);

  AudioParameters params = GetDefaultOutputStreamParametersOnAudioThread();
  MakeAudioOutputStreamOnAudioThread(params);
  OpenAndStartAudioOutputStreamOnAudioThread(&callback);
  CloseAudioOutputStreamOnAudioThread(audio_output_stream_);
}

// Start input streaming using default input parameters and ensure that the
// callback sequence is sane.
TEST_P(AudioAndroidInputTest, StartInputStreamCallbacks) {
  AudioParameters native_params =
      GetDefaultInputStreamParametersOnAudioThread();
  StartInputStreamCallbacks(native_params);
}

// Start input streaming using non default input parameters and ensure that
// the callback sequence is sane. The only change we make in this test is to
// select a 10ms buffer size instead of the default size.
// Flaky, see crbug.com/683408.
TEST_P(AudioAndroidInputTest, StartInputStreamCallbacksNonDefaultParameters) {
  AudioParameters params = GetDefaultInputStreamParametersOnAudioThread();
  params.set_frames_per_buffer(params.sample_rate() / 100);
  StartInputStreamCallbacks(params);
}

// Start output streaming using default output parameters and ensure that the
// callback sequence is sane.
TEST_P(AudioAndroidOutputTest, StartOutputStreamCallbacks) {
  AudioParameters params = GetDefaultOutputStreamParametersOnAudioThread();
  StartOutputStreamCallbacks(params);
}

// Start output streaming using non default output parameters and ensure that
// the callback sequence is sane. The only change we make in this test is to
// select a 10ms buffer size instead of the default size and to open up the
// device in mono.
// TODO(henrika): possibly add support for more variations.
TEST_P(AudioAndroidOutputTest, StartOutputStreamCallbacksNonDefaultParameters) {
  AudioParameters default_params =
      GetDefaultOutputStreamParametersOnAudioThread();
  AudioParameters params(default_params.format(), ChannelLayoutConfig::Mono(),
                         default_params.sample_rate(),
                         default_params.sample_rate() / 100);
  StartOutputStreamCallbacks(params);
}

// Start input streaming and run it for ten seconds while recording to a
// local audio file.
// NOTE: this test requires user interaction and is not designed to run as an
// automatized test on bots.
TEST_P(AudioAndroidInputTest, DISABLED_RunSimplexInputStreamWithFileAsSink) {
  AudioParameters params = GetDefaultInputStreamParametersOnAudioThread();
  DVLOG(1) << params;
  MakeAudioInputStreamOnAudioThread(params);

  std::string file_name =
      base::StringPrintf("out_simplex_%d_%d_%d.pcm", params.sample_rate(),
                         params.frames_per_buffer(), params.channels());

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  FileAudioSink sink(&event, params, file_name);

  OpenAndStartAudioInputStreamOnAudioThread(&sink);
  DVLOG(0) << ">> Speak into the microphone to record audio...";
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_max_timeout()));
  StopAndCloseAudioInputStreamOnAudioThread();
}

// Same test as RunSimplexInputStreamWithFileAsSink but this time output
// streaming is active as well (reads zeros only).
// NOTE: this test requires user interaction and is not designed to run as an
// automatized test on bots.
TEST_P(AudioAndroidInputTest, DISABLED_RunDuplexInputStreamWithFileAsSink) {
  AudioParameters in_params = GetDefaultInputStreamParametersOnAudioThread();
  DVLOG(1) << in_params;
  MakeAudioInputStreamOnAudioThread(in_params);

  AudioParameters out_params = GetDefaultOutputStreamParametersOnAudioThread();
  DVLOG(1) << out_params;
  MakeAudioOutputStreamOnAudioThread(out_params);

  std::string file_name =
      base::StringPrintf("out_duplex_%d_%d_%d.pcm", in_params.sample_rate(),
                         in_params.frames_per_buffer(), in_params.channels());

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  FileAudioSink sink(&event, in_params, file_name);
  MockAudioSourceCallback source;

  EXPECT_CALL(source, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .WillRepeatedly(Invoke(RealOnMoreData));
  EXPECT_CALL(source, OnError(_)).Times(0);

  OpenAndStartAudioInputStreamOnAudioThread(&sink);
  OpenAndStartAudioOutputStreamOnAudioThread(&source);
  DVLOG(0) << ">> Speak into the microphone to record audio";
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_max_timeout()));
  StopAndCloseAudioOutputStreamOnAudioThread();
  StopAndCloseAudioInputStreamOnAudioThread();
}

// Start audio in both directions while feeding captured data into a FIFO so
// it can be read directly (in loopback) by the render side. A small extra
// delay will be added by the FIFO and an estimate of this delay will be
// printed out during the test.
// NOTE: this test requires user interaction and is not designed to run as an
// automatized test on bots.
TEST_P(AudioAndroidInputTest,
       DISABLED_RunSymmetricInputAndOutputStreamsInFullDuplex) {
  // Get native audio parameters for the input side.
  AudioParameters default_input_params =
      GetDefaultInputStreamParametersOnAudioThread();

  // Modify the parameters so that both input and output can use the same
  // parameters by selecting 10ms as buffer size. This will also ensure that
  // the output stream will be a mono stream since mono is default for input
  // audio on Android.
  AudioParameters io_params = default_input_params;
  default_input_params.set_frames_per_buffer(io_params.sample_rate() / 100);
  DVLOG(1) << io_params;

  // Create input and output streams using the common audio parameters.
  MakeAudioInputStreamOnAudioThread(io_params);
  MakeAudioOutputStreamOnAudioThread(io_params);

  FullDuplexAudioSinkSource full_duplex(io_params);

  // Start a full duplex audio session and print out estimates of the extra
  // delay we should expect from the FIFO. If real-time delay measurements are
  // performed, the result should be reduced by this extra delay since it is
  // something that has been added by the test.
  OpenAndStartAudioInputStreamOnAudioThread(&full_duplex);
  OpenAndStartAudioOutputStreamOnAudioThread(&full_duplex);
  DVLOG(1) << "HINT: an estimate of the extra FIFO delay will be updated "
           << "once per second during this test.";
  DVLOG(0) << ">> Speak into the mic and listen to the audio in loopback...";
  fflush(stdout);
  base::PlatformThread::Sleep(base::Seconds(20));
  printf("\n");
  StopAndCloseAudioOutputStreamOnAudioThread();
  StopAndCloseAudioInputStreamOnAudioThread();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AudioAndroidOutputTest,
    testing::Values(AudioApi::AAudioWithPerStreamDeviceSelection,
                    AudioApi::AAudioWithCommunicationDevices,
                    AudioApi::OpenSLES),
    testing::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(
    ,
    AudioAndroidInputTest,
    testing::Values(AudioApi::AAudioWithPerStreamDeviceSelection,
                    AudioApi::AAudioWithCommunicationDevices,
                    AudioApi::OpenSLES),
    testing::PrintToStringParamName());

}  // namespace media
