// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <windows.h>

#include <mmsystem.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/simple_sources.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/limits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

namespace media {

static int ClearData(base::TimeDelta /* delay */,
                     base::TimeTicks /* delay_timestamp */,
                     const AudioGlitchInfo& /* glitch_info */,
                     AudioBus* dest) {
  dest->Zero();
  return dest->frames();
}

// This class allows to find out if the callbacks are occurring as
// expected and if any error has been reported.
class TestSourceBasic : public AudioOutputStream::AudioSourceCallback {
 public:
  TestSourceBasic()
      : callback_count_(0),
        had_error_(0) {
  }
  // AudioSourceCallback::OnMoreData implementation:
  int OnMoreData(base::TimeDelta /* delay */,
                 base::TimeTicks /* delay_timestamp */,
                 const AudioGlitchInfo& /* glitch_info */,
                 AudioBus* dest) override {
    ++callback_count_;
    // Touch the channel memory value to make sure memory is good.
    dest->Zero();
    return dest->frames();
  }
  // AudioSourceCallback::OnError implementation:
  void OnError(ErrorType type) override { ++had_error_; }
  // Returns how many times OnMoreData() has been called.
  int callback_count() const {
    return callback_count_;
  }
  // Returns how many times the OnError callback was called.
  int had_error() const {
    return had_error_;
  }

  void set_error(bool error) {
    had_error_ += error ? 1 : 0;
  }

 private:
  int callback_count_;
  int had_error_;
};

const int kMaxNumBuffers = 3;
// Specializes TestSourceBasic to simulate a source that blocks for some time
// in the OnMoreData callback.
class TestSourceLaggy : public TestSourceBasic {
 public:
  explicit TestSourceLaggy(int lag_in_ms)
      : lag_in_ms_(lag_in_ms) {
  }
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const AudioGlitchInfo& glitch_info,
                 AudioBus* dest) override {
    // Call the base, which increments the callback_count_.
    TestSourceBasic::OnMoreData(delay, delay_timestamp, glitch_info, dest);
    if (callback_count() > kMaxNumBuffers) {
      ::Sleep(lag_in_ms_);
    }
    return dest->frames();
  }

 private:
  int lag_in_ms_;
};

// Helper class to memory map an entire file. The mapping is read-only. Don't
// use for gigabyte-sized files. Attempts to write to this memory generate
// memory access violations.
class ReadOnlyMappedFile {
 public:
  explicit ReadOnlyMappedFile(const wchar_t* file_name)
      : fmap_(NULL), start_(nullptr), size_(0) {
    HANDLE file = ::CreateFileW(file_name, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == file)
      return;
    fmap_ = ::CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    ::CloseHandle(file);
    if (!fmap_)
      return;
    start_ = reinterpret_cast<char*>(::MapViewOfFile(fmap_, FILE_MAP_READ,
                                                     0, 0, 0));
    if (!start_)
      return;
    MEMORY_BASIC_INFORMATION mbi = {0};
    ::VirtualQuery(start_, &mbi, sizeof(mbi));
    size_ = mbi.RegionSize;
  }
  ~ReadOnlyMappedFile() {
    if (start_) {
      ::UnmapViewOfFile(start_);
      ::CloseHandle(fmap_);
    }
  }
  // Returns true if the file was successfully mapped.
  bool is_valid() const { return (start_ && (size_ > 0)); }
  // Returns the size in bytes of the mapped memory.
  uint32_t size() const { return size_; }
  // Returns the memory backing the file.
  const void* GetChunkAt(uint32_t offset) { return &start_[offset]; }

 private:
  HANDLE fmap_;
  raw_ptr<char, AllowPtrArithmetic> start_;
  uint32_t size_;
};

class WinAudioTest : public ::testing::Test {
 public:
  WinAudioTest() {
    audio_manager_ =
        AudioManager::CreateForTesting(std::make_unique<TestAudioThread>());
    audio_manager_device_info_ =
        std::make_unique<AudioDeviceInfoAccessorForTests>(audio_manager_.get());
    base::RunLoop().RunUntilIdle();
  }
  ~WinAudioTest() override { audio_manager_->Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<AudioManager> audio_manager_;
  std::unique_ptr<AudioDeviceInfoAccessorForTests> audio_manager_device_info_;
};

// ===========================================================================
// Validation of AudioManager::AUDIO_PCM_LINEAR
//
// NOTE:
// The tests can fail on the build bots when somebody connects to them via
// remote-desktop and the rdp client installs an audio device that fails to open
// at some point, possibly when the connection goes idle.

// Test that can it be created and closed.
TEST_F(WinAudioTest, PCMWaveStreamGetAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Stereo(), 8000, 256),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);
  oas->Close();
}

// Test that it can be opened and closed.
TEST_F(WinAudioTest, PCMWaveStreamOpenAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Stereo(), 8000, 256),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open());
  oas->Close();
}

// Test potential deadlock situation if the source is slow or blocks for some
// time. The actual EXPECT_GT are mostly meaningless and the real test is that
// the test completes in reasonable time.
TEST_F(WinAudioTest, PCMWaveSlowSource) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(), 16000, 256),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);
  TestSourceLaggy test_laggy(90);
  EXPECT_TRUE(oas->Open());
  // The test parameters cause a callback every 32 ms and the source is
  // sleeping for 90 ms, so it is guaranteed that we run out of ready buffers.
  oas->Start(&test_laggy);
  ::Sleep(500);
  EXPECT_GT(test_laggy.callback_count(), 2);
  EXPECT_FALSE(test_laggy.had_error());
  oas->Stop();
  ::Sleep(500);
  oas->Close();
}

// Test another potential deadlock situation if the thread that calls Start()
// gets paused. This test is best when run over RDP with audio enabled. See
// bug 19276 for more details.
TEST_F(WinAudioTest, PCMWaveStreamPlaySlowLoop) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  uint32_t samples_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(),
                      AudioParameters::kAudioCDSampleRate, samples_100_ms),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate);

  EXPECT_TRUE(oas->Open());
  oas->SetVolume(1.0);

  for (int ix = 0; ix != 5; ++ix) {
    oas->Start(&source);
    ::Sleep(10);
    oas->Stop();
  }
  oas->Close();
}


// This test produces actual audio for .5 seconds on the default wave
// device at 44.1K s/sec. Parameters have been chosen carefully so you should
// not hear pops or noises while the sound is playing.
TEST_F(WinAudioTest, PCMWaveStreamPlay200HzTone44Kss) {
  if (!audio_manager_device_info_->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  uint32_t samples_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(),
                      AudioParameters::kAudioCDSampleRate, samples_100_ms),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate);

  EXPECT_TRUE(oas->Open());
  oas->SetVolume(1.0);
  oas->Start(&source);
  ::Sleep(500);
  oas->Stop();
  oas->Close();
}

// This test produces actual audio for for .5 seconds on the default wave
// device at 22K s/sec. Parameters have been chosen carefully so you should
// not hear pops or noises while the sound is playing. The audio also should
// sound with a lower volume than PCMWaveStreamPlay200HzTone44Kss.
TEST_F(WinAudioTest, PCMWaveStreamPlay200HzTone22Kss) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  uint32_t samples_100_ms = AudioParameters::kAudioCDSampleRate / 20;
  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(),
                      AudioParameters::kAudioCDSampleRate / 2, samples_100_ms),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate/2);

  EXPECT_TRUE(oas->Open());

  oas->SetVolume(0.5);
  oas->Start(&source);
  ::Sleep(500);

  // Test that the volume is within the set limits.
  double volume = 0.0;
  oas->GetVolume(&volume);
  EXPECT_LT(volume, 0.51);
  EXPECT_GT(volume, 0.49);
  oas->Stop();
  oas->Close();
}

// Uses a restricted source to play ~2 seconds of audio for about 5 seconds. We
// try hard to generate situation where the two threads are accessing the
// object roughly at the same time.
TEST_F(WinAudioTest, PushSourceFile16KHz) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  static const int kSampleRate = 16000;
  SineWaveAudioSource source(1, 200.0, kSampleRate);
  // Compute buffer size for 100ms of audio.
  const uint32_t kSamples100ms = (kSampleRate / 1000) * 100;
  // Restrict SineWaveAudioSource to 100ms of samples.
  source.CapSamples(kSamples100ms);

  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(), kSampleRate, kSamples100ms),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);

  EXPECT_TRUE(oas->Open());

  oas->SetVolume(1.0);
  oas->Start(&source);

  // We buffer and play at the same time, buffering happens every ~10ms and the
  // consuming of the buffer happens every ~100ms. We do 100 buffers which
  // effectively wrap around the file more than once.
  for (uint32_t ix = 0; ix != 100; ++ix) {
    ::Sleep(10);
    source.Reset();
  }

  // Play a little bit more of the file.
  ::Sleep(500);

  oas->Stop();
  oas->Close();
}

// This test is to make sure an AudioOutputStream can be started after it was
// stopped. You will here two .5 seconds wave signal separated by 0.5 seconds
// of silence.
TEST_F(WinAudioTest, PCMWaveStreamPlayTwice200HzTone44Kss) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  uint32_t samples_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(),
                      AudioParameters::kAudioCDSampleRate, samples_100_ms),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate);
  EXPECT_TRUE(oas->Open());
  oas->SetVolume(1.0);

  // Play the wave for .5 seconds.
  oas->Start(&source);
  ::Sleep(500);
  oas->Stop();

  // Sleep to give silence after stopping the AudioOutputStream.
  ::Sleep(250);

  // Start again and play for .5 seconds.
  oas->Start(&source);
  ::Sleep(500);
  oas->Stop();

  oas->Close();
}

// With the low latency mode, WASAPI is utilized by default for Vista and
// higher and Wave is used for XP and lower. It is possible to utilize a
// smaller buffer size for WASAPI than for Wave.
TEST_F(WinAudioTest, PCMWaveStreamPlay200HzToneLowLatency) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  // Use 10 ms buffer size for WASAPI and 50 ms buffer size for Wave.
  // Take the existing native sample rate into account.
  std::string default_device_id =
      audio_manager_device_info_->GetDefaultOutputDeviceID();
  const AudioParameters params =
      audio_manager_device_info_->GetOutputStreamParameters(default_device_id);
  int sample_rate = params.sample_rate();
  uint32_t samples_10_ms = sample_rate / 100;
  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Mono(), sample_rate, samples_10_ms),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200, sample_rate);

  bool opened = oas->Open();
  if (!opened) {
    // It was not possible to open this audio device in mono.
    // No point in continuing the test so let's break here.
    LOG(WARNING) << "Mono is not supported. Skipping test.";
    oas->Close();
    return;
  }
  oas->SetVolume(1.0);

  // Play the wave for .8 seconds.
  oas->Start(&source);
  ::Sleep(800);
  oas->Stop();
  oas->Close();
}

// Check that the pending bytes value is correct what the stream starts.
TEST_F(WinAudioTest, PCMWaveStreamPendingBytes) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  uint32_t samples_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(),
                      AudioParameters::kAudioCDSampleRate, samples_100_ms),
      std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);

  NiceMock<MockAudioSourceCallback> source;
  EXPECT_TRUE(oas->Open());

  const base::TimeDelta delay_100_ms = base::Milliseconds(100);
  const base::TimeDelta delay_200_ms = base::Milliseconds(200);

  // Audio output stream has either a double or triple buffer scheme. We expect
  // the delay to reach up to 200 ms depending on the number of buffers used.
  // From that it would decrease as we are playing the data but not providing
  // new one. And then we will try to provide zero data so the amount of
  // pending bytes will go down and eventually read zero.
  InSequence s;

  EXPECT_CALL(source,
              OnMoreData(base::TimeDelta(), _, AudioGlitchInfo(), NotNull()))
      .WillOnce(Invoke(ClearData));

  // Note: If AudioManagerWin::NumberOfWaveOutBuffers() ever changes, or if this
  // test is run on Vista, these expectations will fail.
  EXPECT_CALL(source, OnMoreData(delay_100_ms, _, AudioGlitchInfo(), NotNull()))
      .WillOnce(Invoke(ClearData));
  EXPECT_CALL(source, OnMoreData(delay_200_ms, _, AudioGlitchInfo(), NotNull()))
      .WillOnce(Invoke(ClearData));
  EXPECT_CALL(source, OnMoreData(delay_200_ms, _, AudioGlitchInfo(), NotNull()))
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));
  EXPECT_CALL(source, OnMoreData(delay_100_ms, _, AudioGlitchInfo(), NotNull()))
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));
  EXPECT_CALL(source,
              OnMoreData(base::TimeDelta(), _, AudioGlitchInfo(), NotNull()))
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));

  oas->Start(&source);
  ::Sleep(500);
  oas->Stop();
  oas->Close();
}

// Simple source that uses a SyncSocket to retrieve the audio data
// from a potentially remote thread.
class SyncSocketSource : public AudioOutputStream::AudioSourceCallback {
 public:
  SyncSocketSource(base::SyncSocket* socket,
                   const AudioParameters& params,
                   int expected_packet_count)
      : socket_(socket),
        params_(params),
        expected_packet_count_(expected_packet_count) {
    // Setup AudioBus wrapping data we'll receive over the sync socket.
    packet_size_ = AudioBus::CalculateMemorySize(params);
    data_.reset(static_cast<float*>(
        base::AlignedAlloc(packet_size_ + sizeof(AudioOutputBufferParameters),
                           AudioBus::kChannelAlignment)));
    audio_bus_ = AudioBus::WrapMemory(params, output_buffer()->audio);
  }
  ~SyncSocketSource() override {}

  // AudioSourceCallback::OnMoreData implementation:
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const AudioGlitchInfo& glitch_info,
                 AudioBus* dest) override {
    // If we ask for more data once the producer has shutdown, we will hang
    // on |socket_->Receive()|.
    if (current_packet_count_ < expected_packet_count_) {
      uint32_t control_signal = 0;
      socket_->Send(base::byte_span_from_ref(control_signal));
      output_buffer()->params.delay_us = delay.InMicroseconds();
      output_buffer()->params.delay_timestamp_us =
          (delay_timestamp - base::TimeTicks()).InMicroseconds();
      const size_t span_size =
          static_cast<size_t>(packet_size_) / sizeof(decltype(*data_.get()));
      uint32_t size = socket_->Receive(
          base::as_writable_bytes(base::make_span(data_.get(), span_size)));
      ++current_packet_count_;

      DCHECK_EQ(static_cast<size_t>(size) % sizeof(*audio_bus_->channel(0)),
                0U);
      audio_bus_->CopyTo(dest);
      return audio_bus_->frames();
    }

    return 0;
  }

  int packet_size() const { return packet_size_; }

  AudioOutputBuffer* output_buffer() const {
    return reinterpret_cast<AudioOutputBuffer*>(data_.get());
  }

  // AudioSourceCallback::OnError implementation:
  void OnError(ErrorType type) override {}

 private:
  raw_ptr<base::SyncSocket> socket_;
  const AudioParameters params_;
  int packet_size_;
  std::unique_ptr<float, base::AlignedFreeDeleter> data_;
  std::unique_ptr<AudioBus> audio_bus_;

  // This test produces a fixed number of packets, we need these so we know
  // when to stop listening.
  const int expected_packet_count_;
  int current_packet_count_ = 0;
};

struct SyncThreadContext {
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #reinterpret-cast-trivial-type
  RAW_PTR_EXCLUSION base::SyncSocket* socket;
  int sample_rate;
  int channels;
  int frames;
  double sine_freq;
  uint32_t packet_size_bytes;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #reinterpret-cast-trivial-type
  RAW_PTR_EXCLUSION AudioOutputBuffer* buffer;
  int total_packets;
};

// This thread provides the data that the SyncSocketSource above needs
// using the other end of a SyncSocket. The protocol is as follows:
//
// SyncSocketSource ---send 4 bytes ------------> SyncSocketThread
//                  <--- audio packet ----------
//
DWORD __stdcall SyncSocketThread(void* context) {
  SyncThreadContext& ctx = *(reinterpret_cast<SyncThreadContext*>(context));

  // Setup AudioBus wrapping data we'll pass over the sync socket.
  std::unique_ptr<float, base::AlignedFreeDeleter> data(static_cast<float*>(
      base::AlignedAlloc(ctx.packet_size_bytes, AudioBus::kChannelAlignment)));
  std::unique_ptr<AudioBus> audio_bus =
      AudioBus::WrapMemory(ctx.channels, ctx.frames, data.get());

  SineWaveAudioSource sine(1, ctx.sine_freq, ctx.sample_rate);

  uint32_t control_signal = 0;
  for (int ix = 0; ix < ctx.total_packets; ++ix) {
    // Listen for a signal from the Audio Stream that it wants data. This is a
    // blocking call and will not proceed until we receive the signal.
    if (ctx.socket->Receive(base::byte_span_from_ref(control_signal)) == 0) {
      break;
    }
    base::TimeDelta delay = base::Microseconds(ctx.buffer->params.delay_us);
    base::TimeTicks delay_timestamp =
        base::TimeTicks() +
        base::Microseconds(ctx.buffer->params.delay_timestamp_us);
    sine.OnMoreData(delay, delay_timestamp, {}, audio_bus.get());

    // Send the audio data to the Audio Stream.
    // SAFETY: `data`'s allocation has size `ctx.packet_size_bytes`.
    ctx.socket->Send(UNSAFE_BUFFERS(base::make_span(
        reinterpret_cast<uint8_t*>(data.get()), ctx.packet_size_bytes)));
  }

  return 0;
}

// Test the basic operation of AudioOutputStream used with a SyncSocket.
// The emphasis is to verify that it is possible to feed data to the audio
// layer using a source based on SyncSocket. In a real situation we would
// go for the low-latency version in combination with SyncSocket, but to keep
// the test more simple, AUDIO_PCM_LINEAR is utilized instead. The main
// principle of the test still remains and we avoid the additional complexity
// related to the two different audio-layers for AUDIO_PCM_LOW_LATENCY.
// In this test you should hear a continuous 200Hz tone for 2 seconds.
TEST_F(WinAudioTest, SyncSocketBasic) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  static const int sample_rate = AudioParameters::kAudioCDSampleRate;
  static const uint32_t kSamples20ms = sample_rate / 50;
  // We want 2 seconds of audio, which means we need 100 packets as each packet
  // contains 20ms worth of audio samples.
  static const int kPackets2s = 100;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Mono(), sample_rate,
                         kSamples20ms);

  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      params, std::string(), AudioManager::LogCallback());
  ASSERT_TRUE(NULL != oas);

  ASSERT_TRUE(oas->Open());

  // Create two sockets and connect them with a named pipe.
  base::SyncSocket sockets[2];
  ASSERT_TRUE(base::SyncSocket::CreatePair(&sockets[0], &sockets[1]));

  // Give one socket to the source, which receives requests from the
  // AudioOutputStream for more data. On such a request, it will send a control
  // signal to the SyncThreadContext, then it will receive
  // an audio packet back which it will give to the AudioOutputStream.
  SyncSocketSource source(&sockets[0], params, kPackets2s);

  // Give the other socket to the thread. This thread runs a loop that will
  // generate enough audio packets for 2 seconds worth of audio. It will listen
  // for a control signal and when it gets one it will write one audio packet
  // to the pipe that connects the two sockets.
  SyncThreadContext thread_context;
  thread_context.sample_rate = params.sample_rate();
  thread_context.sine_freq = 200.0;
  thread_context.packet_size_bytes = source.packet_size();
  thread_context.frames = params.frames_per_buffer();
  thread_context.channels = params.channels();
  thread_context.socket = &sockets[1];
  thread_context.buffer = source.output_buffer();
  thread_context.total_packets = kPackets2s;

  HANDLE thread = ::CreateThread(NULL, 0, SyncSocketThread,
                                 &thread_context, 0, NULL);

  // Start the AudioOutputStream, which will request data via
  // SyncSocketSource::OnMoreData until the SyncThreadContext has run out of
  // data to give.
  oas->Start(&source);

  // Wait for the SyncThreadContext to finish its loop, should take 2 seconds.
  // During this time it is providing audio data as described above.
  ::WaitForSingleObject(thread, INFINITE);
  ::CloseHandle(thread);

  // Once no more data is being sent, we can stop and close the stream.
  oas->Stop();
  oas->Close();
}

}  // namespace media
