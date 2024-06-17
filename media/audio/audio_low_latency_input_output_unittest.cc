// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Limits the number of delay measurements we can store in an array and
// then write to file at end of the WASAPIAudioInputOutputFullDuplex test.
static const size_t kMaxDelayMeasurements = 1000;

// Name of the output text file. The output file will be stored in the
// directory containing media_unittests.exe.
// Example: \src\build\Debug\audio_delay_values_ms.txt.
// See comments for the WASAPIAudioInputOutputFullDuplex test for more details
// about the file format.
static const char kDelayValuesFileName[] = "audio_delay_values_ms.txt";

// Contains delay values which are reported during the full-duplex test.
// Total delay = |buffer_delay_ms| + |input_delay_ms| + |output_delay_ms|.
struct AudioDelayState {
  AudioDelayState()
      : delta_time_ms(0),
        buffer_delay_ms(0),
        input_delay_ms(0),
        output_delay_ms(0) {
  }

  // Time in milliseconds since last delay report. Typical value is ~10 [ms].
  int delta_time_ms;

  // Size of internal sync buffer. Typical value is ~0 [ms].
  int buffer_delay_ms;

  // Reported capture/input delay. Typical value is ~10 [ms].
  int input_delay_ms;

  // Reported render/output delay. Typical value is ~40 [ms].
  int output_delay_ms;
};

void OnLogMessage(const std::string& message) {}

// Test fixture class.
class AudioLowLatencyInputOutputTest : public testing::Test {
 public:
  AudioLowLatencyInputOutputTest(const AudioLowLatencyInputOutputTest&) =
      delete;
  AudioLowLatencyInputOutputTest& operator=(
      const AudioLowLatencyInputOutputTest&) = delete;

 protected:
  AudioLowLatencyInputOutputTest() {
    audio_manager_ =
        AudioManager::CreateForTesting(std::make_unique<TestAudioThread>());
  }

  ~AudioLowLatencyInputOutputTest() override { audio_manager_->Shutdown(); }

  AudioManager* audio_manager() { return audio_manager_.get(); }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<AudioManager> audio_manager_;
};

// This audio source/sink implementation should be used for manual tests
// only since delay measurements are stored on an output text file.
// All incoming/recorded audio packets are stored in an intermediate media
// buffer which the renderer reads from when it needs audio for playout.
// The total effect is that recorded audio is played out in loop back using
// a sync buffer as temporary storage.
class FullDuplexAudioSinkSource
    : public AudioInputStream::AudioInputCallback,
      public AudioOutputStream::AudioSourceCallback {
 public:
  FullDuplexAudioSinkSource(int sample_rate,
                            int samples_per_packet,
                            int channels)
    : sample_rate_(sample_rate),
      samples_per_packet_(samples_per_packet),
      channels_(channels),
      input_elements_to_write_(0),
      output_elements_to_write_(0),
      previous_write_time_(base::TimeTicks::Now()) {
    // Size in bytes of each audio frame (4 bytes for 16-bit stereo PCM).
    frame_size_ = (16 / 8) * channels_;

    // Start with the smallest possible buffer size. It will be increased
    // dynamically during the test if required.
    buffer_ = std::make_unique<media::SeekableBuffer>(
        0, samples_per_packet_ * frame_size_);

    frames_to_ms_ = static_cast<double>(1000.0 / sample_rate_);
    delay_states_ = std::make_unique<AudioDelayState[]>(kMaxDelayMeasurements);
  }

  ~FullDuplexAudioSinkSource() override {
    // Get complete file path to output file in the directory containing
    // media_unittests.exe. Example: src/build/Debug/audio_delay_values_ms.txt.
    base::FilePath file_name;
    EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &file_name));
    file_name = file_name.AppendASCII(kDelayValuesFileName);

    FILE* text_file = base::OpenFile(file_name, "wt");
    DLOG_IF(ERROR, !text_file) << "Failed to open log file.";
    VLOG(0) << ">> Output file " << file_name.value() << " has been created.";

    // Write the array which contains time-stamps, buffer size and
    // audio delays values to a text file.
    size_t elements_written = 0;
    while (elements_written <
        std::min(input_elements_to_write_, output_elements_to_write_)) {
      const AudioDelayState state = delay_states_[elements_written];
      fprintf(text_file, "%d %d %d %d\n",
              state.delta_time_ms,
              state.buffer_delay_ms,
              state.input_delay_ms,
              state.output_delay_ms);
      ++elements_written;
    }

    base::CloseFile(text_file);
  }

  // AudioInputStream::AudioInputCallback.
  void OnError() override {}
  void OnData(const AudioBus* src,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    base::AutoLock lock(lock_);

    // Update three components in the AudioDelayState for this recorded
    // audio packet.
    const base::TimeTicks now_time = base::TimeTicks::Now();
    const int diff = (now_time - previous_write_time_).InMilliseconds();
    previous_write_time_ = now_time;
    if (input_elements_to_write_ < kMaxDelayMeasurements) {
      delay_states_[input_elements_to_write_].delta_time_ms = diff;
      delay_states_[input_elements_to_write_].buffer_delay_ms =
          BytesToMilliseconds(buffer_->forward_bytes());
      delay_states_[input_elements_to_write_].input_delay_ms =
          (base::TimeTicks::Now() - capture_time).InMilliseconds();
      ++input_elements_to_write_;
    }

    // TODO(henrika): fix this and use AudioFifo instead.
    // Store the captured audio packet in a seekable media buffer.
    // if (!buffer_->Append(src, size)) {
    // An attempt to write outside the buffer limits has been made.
    // Double the buffer capacity to ensure that we have a buffer large
    // enough to handle the current sample test scenario.
    //   buffer_->set_forward_capacity(2 * buffer_->forward_capacity());
    //   buffer_->Clear();
    // }
  }

  // AudioOutputStream::AudioSourceCallback.
  void OnError(ErrorType type) override {}
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks /* delay_timestamp */,
                 const AudioGlitchInfo& /* glitch_info */,
                 AudioBus* dest) override {
    base::AutoLock lock(lock_);

    // Update one component in the AudioDelayState for the packet
    // which is about to be played out.
    if (output_elements_to_write_ < kMaxDelayMeasurements) {
      delay_states_[output_elements_to_write_].output_delay_ms =
          delay.InMilliseconds();
      ++output_elements_to_write_;
    }

    int size;
    const uint8_t* source;
    // Read the data from the seekable media buffer which contains
    // captured data at the same size and sample rate as the output side.
    if (buffer_->GetCurrentChunk(&source, &size) && size > 0) {
      EXPECT_EQ(channels_, dest->channels());
      size = std::min(dest->frames() * frame_size_, size);
      EXPECT_EQ(static_cast<size_t>(size) % sizeof(*dest->channel(0)), 0U);

      // We should only have 16 bits per sample.
      DCHECK_EQ(frame_size_ / channels_, 2);
      dest->FromInterleaved<SignedInt16SampleTypeTraits>(
          reinterpret_cast<const int16_t*>(source), size / channels_);

      buffer_->Seek(size);
      return size / frame_size_;
    }

    return 0;
  }

 protected:
  // Converts from bytes to milliseconds taking the sample rate and size
  // of an audio frame into account.
  int BytesToMilliseconds(uint32_t delay_bytes) const {
    return static_cast<int>((delay_bytes / frame_size_) * frames_to_ms_ + 0.5);
  }

 private:
  base::Lock lock_;
  std::unique_ptr<media::SeekableBuffer> buffer_;
  int sample_rate_;
  int samples_per_packet_;
  int channels_;
  int frame_size_;
  double frames_to_ms_;
  std::unique_ptr<AudioDelayState[]> delay_states_;
  size_t input_elements_to_write_;
  size_t output_elements_to_write_;
  base::TimeTicks previous_write_time_;
};

class AudioInputStreamTraits {
 public:
  typedef AudioInputStream StreamType;

  static AudioParameters GetDefaultAudioStreamParameters(
      AudioManager* audio_manager) {
    return AudioDeviceInfoAccessorForTests(audio_manager)
        .GetInputStreamParameters(AudioDeviceDescription::kDefaultDeviceId);
  }

  static StreamType* CreateStream(AudioManager* audio_manager,
      const AudioParameters& params) {
    return audio_manager->MakeAudioInputStream(
        params, AudioDeviceDescription::kDefaultDeviceId,
        base::BindRepeating(&OnLogMessage));
  }
};

class AudioOutputStreamTraits {
 public:
  typedef AudioOutputStream StreamType;

  static AudioParameters GetDefaultAudioStreamParameters(
      AudioManager* audio_manager) {
    std::string default_device_id =
        AudioDeviceInfoAccessorForTests(audio_manager)
            .GetDefaultOutputDeviceID();
    return AudioDeviceInfoAccessorForTests(audio_manager)
        .GetOutputStreamParameters(default_device_id);
  }

  static StreamType* CreateStream(AudioManager* audio_manager,
      const AudioParameters& params) {
    return audio_manager->MakeAudioOutputStream(
        params, std::string(), base::BindRepeating(&OnLogMessage));
  }
};

// Traits template holding a trait of StreamType. It encapsulates
// AudioInputStream and AudioOutputStream stream types.
template <typename StreamTraits>
class StreamWrapper {
 public:
  typedef typename StreamTraits::StreamType StreamType;

  explicit StreamWrapper(AudioManager* audio_manager)
      : audio_manager_(audio_manager),
        format_(AudioParameters::AUDIO_PCM_LOW_LATENCY),
#if BUILDFLAG(IS_ANDROID)
        channel_layout_(CHANNEL_LAYOUT_MONO)
#else
        channel_layout_(CHANNEL_LAYOUT_STEREO)
#endif
  {
    // Use the preferred sample rate.
    const AudioParameters& params =
        StreamTraits::GetDefaultAudioStreamParameters(audio_manager_);
    sample_rate_ = params.sample_rate();

    // Use the preferred buffer size. Note that the input side uses the same
    // size as the output side in this implementation.
    samples_per_packet_ = params.frames_per_buffer();
  }

  virtual ~StreamWrapper() = default;

  // Creates an Audio[Input|Output]Stream stream object using default
  // parameters.
  StreamType* Create() {
    return CreateStream();
  }

  int channels() const {
    return ChannelLayoutToChannelCount(channel_layout_);
  }
  int sample_rate() const { return sample_rate_; }
  int samples_per_packet() const { return samples_per_packet_; }

 private:
  StreamType* CreateStream() {
    StreamType* stream = StreamTraits::CreateStream(
        audio_manager_,
        AudioParameters(format_,
                        ChannelLayoutConfig(channel_layout_, channels()),
                        sample_rate_, samples_per_packet_));
    EXPECT_TRUE(stream);
    return stream;
  }

  raw_ptr<AudioManager> audio_manager_;
  AudioParameters::Format format_;
  ChannelLayout channel_layout_;
  int sample_rate_;
  int samples_per_packet_;
};

typedef StreamWrapper<AudioInputStreamTraits> AudioInputStreamWrapper;
typedef StreamWrapper<AudioOutputStreamTraits> AudioOutputStreamWrapper;

// This test is intended for manual tests and should only be enabled
// when it is required to make a real-time test of audio in full duplex and
// at the same time create a text file which contains measured delay values.
// The file can later be analyzed off line using e.g. MATLAB.
// MATLAB example:
//   D=load('audio_delay_values_ms.txt');
//   x=cumsum(D(:,1));
//   plot(x, D(:,2), x, D(:,3), x, D(:,4), x, D(:,2)+D(:,3)+D(:,4));
//   axis([0, max(x), 0, max(D(:,2)+D(:,3)+D(:,4))+10]);
//   legend('buffer delay','input delay','output delay','total delay');
//   xlabel('time [msec]')
//   ylabel('delay [msec]')
//   title('Full-duplex audio delay measurement');
TEST_F(AudioLowLatencyInputOutputTest, DISABLED_FullDuplexDelayMeasurement) {
  AudioDeviceInfoAccessorForTests device_info_accessor(audio_manager());
  ABORT_AUDIO_TEST_IF_NOT(device_info_accessor.HasAudioInputDevices() &&
                          device_info_accessor.HasAudioOutputDevices());

  AudioInputStreamWrapper aisw(audio_manager());
  AudioInputStream* ais = aisw.Create();
  EXPECT_TRUE(ais);

  AudioOutputStreamWrapper aosw(audio_manager());
  AudioOutputStream* aos = aosw.Create();
  EXPECT_TRUE(aos);

  // This test only supports identical parameters in both directions.
  // TODO(henrika): it is possible to cut delay here by using different
  // buffer sizes for input and output.
  if (aisw.sample_rate() != aosw.sample_rate() ||
      aisw.samples_per_packet() != aosw.samples_per_packet() ||
      aisw.channels() != aosw.channels()) {
    LOG(ERROR) << "This test requires symmetric input and output parameters. "
        "Ensure that sample rate and number of channels are identical in "
        "both directions";
    aos->Close();
    ais->Close();
    return;
  }

  EXPECT_EQ(ais->Open(), AudioInputStream::OpenOutcome::kSuccess);
  EXPECT_TRUE(aos->Open());

  FullDuplexAudioSinkSource full_duplex(
      aisw.sample_rate(), aisw.samples_per_packet(), aisw.channels());

  VLOG(0) << ">> You should now be able to hear yourself in loopback...";
  DVLOG(0) << "   sample_rate       : " << aisw.sample_rate();
  DVLOG(0) << "   samples_per_packet: " << aisw.samples_per_packet();
  DVLOG(0) << "   channels          : " << aisw.channels();

  ais->Start(&full_duplex);
  aos->Start(&full_duplex);

  // Wait for approximately 10 seconds. The user will hear their own voice
  // in loop back during this time. At the same time, delay recordings are
  // performed and stored in the output text file.
  base::RunLoop run_loop;
  task_runner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
  run_loop.Run();

  aos->Stop();
  ais->Stop();

  // All Close() operations that run on the mocked audio thread,
  // should be synchronous and not post additional close tasks to
  // mocked the audio thread. Hence, there is no need to call
  // message_loop()->RunUntilIdle() after the Close() methods.
  aos->Close();
  ais->Close();
}

}  // namespace

}  // namespace media
