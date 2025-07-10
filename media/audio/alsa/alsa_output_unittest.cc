// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "media/audio/alsa/alsa_output.h"

#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_message_loop.h"
#include "base/time/time.h"
#include "media/audio/alsa/alsa_wrapper.h"
#include "media/audio/alsa/audio_manager_alsa.h"
#include "media/audio/alsa/mock_alsa_wrapper.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/data_buffer.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::AtLeast;
using testing::DoAll;
using testing::Field;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::MockFunction;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::StrEq;
using testing::Unused;

namespace media {
namespace {

constexpr ChannelLayout kTestChannelLayout = CHANNEL_LAYOUT_STEREO;
constexpr int kTestSampleRate = AudioParameters::kAudioCDSampleRate;
constexpr size_t kTestBitsPerSample = 16;
constexpr AudioParameters::Format kTestFormat =
    AudioParameters::AUDIO_PCM_LINEAR;
constexpr char kTestDeviceName[] = "TestDevice";
constexpr char kDummyMessage[] = "dummy";
constexpr uint32_t kTestFramesPerPacket = 1000;
constexpr int kTestFailedErrno = -EACCES;

const size_t kTestBytesPerFrame =
    kTestBitsPerSample / 8 * ChannelLayoutToChannelCount(kTestChannelLayout);
const size_t kTestPacketSize = kTestFramesPerPacket * kTestBytesPerFrame;

// The APIs that use handles are not const-correct and require this annoying
// conversion.
snd_pcm_t* GetFakeHandle() {
  return const_cast<snd_pcm_t*>(reinterpret_cast<snd_pcm_t*>(1));
}

}  // namespace

class MockAudioManagerAlsa : public AudioManagerAlsa {
 public:
  MockAudioManagerAlsa()
      : AudioManagerAlsa(std::make_unique<TestAudioThread>(),
                         &fake_audio_log_factory_) {}
  MOCK_METHOD0(Init, void());
  MOCK_METHOD0(HasAudioOutputDevices, bool());
  MOCK_METHOD0(HasAudioInputDevices, bool());
  MOCK_METHOD2(MakeLinearOutputStream,
               AudioOutputStream*(const AudioParameters& params,
                                  const LogCallback& log_callback));
  MOCK_METHOD3(MakeLowLatencyOutputStream,
               AudioOutputStream*(const AudioParameters& params,
                                  const std::string& device_id,
                                  const LogCallback& log_callback));
  MOCK_METHOD3(MakeLowLatencyInputStream,
               AudioInputStream*(const AudioParameters& params,
                                 const std::string& device_id,
                                 const LogCallback& log_callback));

  // We need to override this function in order to skip the checking the number
  // of active output streams. It is because the number of active streams
  // is managed inside MakeAudioOutputStream, and we don't use
  // MakeAudioOutputStream to create the stream in the tests.
  void ReleaseOutputStream(AudioOutputStream* stream) override {
    DCHECK(stream);
    delete stream;
  }

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
};

class AlsaPcmOutputStreamTest : public testing::Test {
 public:
  AlsaPcmOutputStreamTest(const AlsaPcmOutputStreamTest&) = delete;
  AlsaPcmOutputStreamTest& operator=(const AlsaPcmOutputStreamTest&) = delete;

 protected:
  AlsaPcmOutputStreamTest() {
    mock_manager_ = std::make_unique<StrictMock<MockAudioManagerAlsa>>();
  }

  ~AlsaPcmOutputStreamTest() override { mock_manager_->Shutdown(); }

  AlsaPcmOutputStream* CreateStream(ChannelLayout layout) {
    return CreateStream(layout, kTestFramesPerPacket);
  }

  AlsaPcmOutputStream* CreateStream(ChannelLayout layout,
                                    int32_t samples_per_packet) {
    AudioParameters params(
        kTestFormat,
        ChannelLayoutConfig(layout, ChannelLayoutToChannelCount(layout)),
        kTestSampleRate, samples_per_packet);
    return new AlsaPcmOutputStream(kTestDeviceName,
                                   params,
                                   &mock_alsa_wrapper_,
                                   mock_manager_.get());
  }

  // Helper function to malloc the string returned by DeviceNameHint for NAME.
  static char* EchoHint(const void* name, Unused) {
    return strdup(static_cast<const char*>(name));
  }

  // Helper function to malloc the string returned by DeviceNameHint for IOID.
  static char* OutputHint(Unused, Unused) {
    return strdup("Output");
  }

  // Helper function to initialize |test_stream->buffer_|. Must be called
  // in all tests that use buffer_ without opening the stream.
  void InitBuffer(AlsaPcmOutputStream* test_stream) {
    DCHECK(test_stream);
    packet_ = base::MakeRefCounted<DataBuffer>(kTestPacketSize);
    packet_->set_size(kTestPacketSize);
    test_stream->buffer_ = std::make_unique<SeekableBuffer>(0, kTestPacketSize);
    test_stream->buffer_->Append(packet_.get());
  }

  base::TestMessageLoop message_loop_;
  StrictMock<MockAlsaWrapper> mock_alsa_wrapper_;
  std::unique_ptr<StrictMock<MockAudioManagerAlsa>> mock_manager_;
  scoped_refptr<DataBuffer> packet_;
};

// Custom action to clear a memory buffer.
ACTION(ClearBuffer) {
  arg3->Zero();
}

TEST_F(AlsaPcmOutputStreamTest, ConstructedState) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  EXPECT_EQ(AlsaPcmOutputStream::kCreated, test_stream->state());
  test_stream->Close();

  // Should support mono.
  test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  EXPECT_EQ(AlsaPcmOutputStream::kCreated, test_stream->state());
  test_stream->Close();

  // Should support multi-channel.
  test_stream = CreateStream(CHANNEL_LAYOUT_SURROUND);
  EXPECT_EQ(AlsaPcmOutputStream::kCreated, test_stream->state());
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, LatencyFloor) {
  constexpr const double kMicrosPerFrame =
      static_cast<double>(1000000) / kTestSampleRate;
  const double kPacketFramesInMinLatency =
      AlsaPcmOutputStream::kMinLatencyMicros / kMicrosPerFrame / 2.0;

  // Test that packets which would cause a latency under less than
  // AlsaPcmOutputStream::kMinLatencyMicros will get clipped to
  // AlsaPcmOutputStream::kMinLatencyMicros,
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(GetFakeHandle()), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmSetParams(_, _, _, _, _, _,
                           AlsaPcmOutputStream::kMinLatencyMicros))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kTestFramesPerPacket),
                      SetArgPointee<2>(kTestFramesPerPacket / 2), Return(0)));

  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout,
                                                  kPacketFramesInMinLatency);
  ASSERT_TRUE(test_stream->Open());

  // Now close it and test that everything was released.
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(GetFakeHandle()))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(GetFakeHandle()))
      .WillOnce(Return(kTestDeviceName));
  test_stream->Close();

  Mock::VerifyAndClear(&mock_alsa_wrapper_);
  Mock::VerifyAndClear(mock_manager_.get());

  // Test that having more packets ends up with a latency based on packet size.
  const int kOverMinLatencyPacketSize = kPacketFramesInMinLatency + 1;
  int64_t expected_micros = AudioTimestampHelper::FramesToTime(
                                kOverMinLatencyPacketSize * 2, kTestSampleRate)
                                .InMicroseconds();

  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(GetFakeHandle()), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmSetParams(_, _, _, _, _, _, expected_micros))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kTestFramesPerPacket),
                      SetArgPointee<2>(kTestFramesPerPacket / 2), Return(0)));

  test_stream = CreateStream(kTestChannelLayout,
                             kOverMinLatencyPacketSize);
  ASSERT_TRUE(test_stream->Open());

  // Now close it and test that everything was released.
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(GetFakeHandle()))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(GetFakeHandle()))
      .WillOnce(Return(kTestDeviceName));
  test_stream->Close();

  Mock::VerifyAndClear(&mock_alsa_wrapper_);
  Mock::VerifyAndClear(mock_manager_.get());
}

TEST_F(AlsaPcmOutputStreamTest, OpenClose) {
  int64_t expected_micros = AudioTimestampHelper::FramesToTime(
                                2 * kTestFramesPerPacket, kTestSampleRate)
                                .InMicroseconds();

  // Open() call opens the playback device, sets the parameters, posts a task
  // with the resulting configuration data, and transitions the object state to
  // kIsOpened.
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmOpen(_, StrEq(kTestDeviceName), SND_PCM_STREAM_PLAYBACK,
                      SND_PCM_NONBLOCK))
      .WillOnce(DoAll(SetArgPointee<0>(GetFakeHandle()), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmSetParams(GetFakeHandle(), SND_PCM_FORMAT_S16_LE,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           ChannelLayoutToChannelCount(kTestChannelLayout),
                           kTestSampleRate, 1, expected_micros))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(GetFakeHandle(), _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kTestFramesPerPacket),
                      SetArgPointee<2>(kTestFramesPerPacket / 2), Return(0)));

  // Open the stream.
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  ASSERT_TRUE(test_stream->Open());

  EXPECT_EQ(AlsaPcmOutputStream::kIsOpened, test_stream->state());
  EXPECT_EQ(GetFakeHandle(), test_stream->playback_handle_);
  EXPECT_EQ(kTestFramesPerPacket, test_stream->frames_per_packet_);
  EXPECT_TRUE(test_stream->buffer_.get());
  EXPECT_FALSE(test_stream->stop_stream_);

  // Now close it and test that everything was released.
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(GetFakeHandle()))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(GetFakeHandle()))
      .WillOnce(Return(kTestDeviceName));
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, PcmOpenFailed) {
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));

  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  ASSERT_FALSE(test_stream->Open());
  ASSERT_EQ(AlsaPcmOutputStream::kInError, test_stream->state());

  // Ensure internal state is set for a no-op stream if PcmOpen() fails.
  EXPECT_TRUE(test_stream->stop_stream_);
  EXPECT_FALSE(test_stream->playback_handle_);
  EXPECT_FALSE(test_stream->buffer_.get());

  // Close the stream since we opened it to make destruction happy.
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, PcmSetParamsFailed) {
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(GetFakeHandle()), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmSetParams(_, _, _, _, _, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(GetFakeHandle()))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(GetFakeHandle()))
      .WillOnce(Return(kTestDeviceName));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));

  // If open fails, the stream stays in kCreated because it has effectively had
  // no changes.
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  ASSERT_FALSE(test_stream->Open());
  EXPECT_EQ(AlsaPcmOutputStream::kInError, test_stream->state());

  // Ensure internal state is set for a no-op stream if PcmSetParams() fails.
  EXPECT_TRUE(test_stream->stop_stream_);
  EXPECT_FALSE(test_stream->playback_handle_);
  EXPECT_FALSE(test_stream->buffer_.get());

  // Close the stream since we opened it to make destruction happy.
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, StartStop) {
  // Open() call opens the playback device, sets the parameters, posts a task
  // with the resulting configuration data, and transitions the object state to
  // kIsOpened.
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(GetFakeHandle()), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmSetParams(_, _, _, _, _, _, _))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kTestFramesPerPacket),
                      SetArgPointee<2>(kTestFramesPerPacket / 2), Return(0)));

  // Open the stream.
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  ASSERT_TRUE(test_stream->Open());
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  test_stream->SetTickClockForTesting(&tick_clock);

  // Expect Device setup.
  EXPECT_CALL(mock_alsa_wrapper_, PcmDrop(GetFakeHandle())).WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmPrepare(GetFakeHandle()))
      .WillOnce(Return(0));

  // Expect the pre-roll.
  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmState(GetFakeHandle()))
      .WillRepeatedly(Return(SND_PCM_STATE_RUNNING));
  EXPECT_CALL(mock_alsa_wrapper_, PcmDelay(GetFakeHandle(), _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return(0)));
  EXPECT_CALL(mock_callback,
              OnMoreData(base::TimeDelta(), tick_clock.NowTicks(),
                         AudioGlitchInfo(), _))
      .WillRepeatedly(DoAll(ClearBuffer(), Return(kTestFramesPerPacket)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(GetFakeHandle(), _, _))
      .WillRepeatedly(Return(kTestFramesPerPacket));

  // Expect scheduling.
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(GetFakeHandle()))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(kTestFramesPerPacket));

  test_stream->Start(&mock_callback);
  // Start() will issue a WriteTask() directly and then schedule the next one,
  // call Stop() immediately after to ensure we don't run the message loop
  // forever.
  test_stream->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(GetFakeHandle()))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(GetFakeHandle()))
      .WillOnce(Return(kTestDeviceName));
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_FinishedPacket) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  InitBuffer(test_stream);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsOpened);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsPlaying);

  // Nothing should happen.  Don't set any expectations and Our strict mocks
  // should verify most of this.

  // Test empty buffer.
  test_stream->buffer_->Clear();
  test_stream->WritePacket();
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_NormalPacket) {
  // We need to open the stream before writing data to ALSA.
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(GetFakeHandle()), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmSetParams(_, _, _, _, _, _, _))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kTestFramesPerPacket),
                      SetArgPointee<2>(kTestFramesPerPacket / 2), Return(0)));
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  ASSERT_TRUE(test_stream->Open());
  InitBuffer(test_stream);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsPlaying);

  // Write a little less than half the data.
  size_t written = packet_->size() / kTestBytesPerFrame / 2 - 1;
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(GetFakeHandle()))
      .WillOnce(Return(static_cast<snd_pcm_sframes_t>(written)));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmWritei(GetFakeHandle(), packet_->data().data(), _))
      .WillOnce(Return(static_cast<snd_pcm_sframes_t>(written)));

  test_stream->WritePacket();

  ASSERT_EQ(static_cast<size_t>(test_stream->buffer_->forward_bytes()),
            packet_->size() - written * kTestBytesPerFrame);

  // Write the rest.
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(GetFakeHandle()))
      .WillOnce(Return(
          static_cast<snd_pcm_sframes_t>(kTestFramesPerPacket - written)));
  EXPECT_CALL(
      mock_alsa_wrapper_,
      PcmWritei(GetFakeHandle(),
                packet_->data().subspan(written * kTestBytesPerFrame).data(),
                _))
      .WillOnce(Return(static_cast<snd_pcm_sframes_t>(
          packet_->size() / kTestBytesPerFrame - written)));
  test_stream->WritePacket();
  EXPECT_EQ(0u, test_stream->buffer_->forward_bytes());

  // Now close it and test that everything was released.
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(GetFakeHandle()))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(GetFakeHandle()))
      .WillOnce(Return(kTestDeviceName));
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_WriteFails) {
  // We need to open the stream before writing data to ALSA.
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(GetFakeHandle()), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmSetParams(_, _, _, _, _, _, _))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kTestFramesPerPacket),
                      SetArgPointee<2>(kTestFramesPerPacket / 2), Return(0)));
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  ASSERT_TRUE(test_stream->Open());
  InitBuffer(test_stream);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsPlaying);

  // Fail due to a recoverable error and see that PcmRecover code path
  // continues normally.
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(GetFakeHandle()))
      .WillOnce(Return(kTestFramesPerPacket));
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(GetFakeHandle(), _, _))
      .WillOnce(Return(-EINTR));
  EXPECT_CALL(mock_alsa_wrapper_, PcmRecover(GetFakeHandle(), _, _))
      .WillOnce(Return(0));

  test_stream->WritePacket();

  ASSERT_EQ(test_stream->buffer_->forward_bytes(), packet_->size());

  // Fail the next write, and see that stop_stream_ is set.
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(GetFakeHandle()))
      .WillOnce(Return(kTestFramesPerPacket));
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(GetFakeHandle(), _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmRecover(GetFakeHandle(), _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));
  test_stream->WritePacket();
  EXPECT_EQ(test_stream->buffer_->forward_bytes(), packet_->size());
  EXPECT_TRUE(test_stream->stop_stream_);

  // Now close it and test that everything was released.
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(GetFakeHandle()))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(GetFakeHandle()))
      .WillOnce(Return(kTestDeviceName));
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_StopStream) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  InitBuffer(test_stream);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsOpened);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsPlaying);

  // No expectations set on the strict mock because nothing should be called.
  test_stream->stop_stream_ = true;
  test_stream->WritePacket();
  EXPECT_EQ(0u, test_stream->buffer_->forward_bytes());
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  test_stream->SetTickClockForTesting(&tick_clock);
  InitBuffer(test_stream);
  test_stream->buffer_->Clear();

  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmState(_))
      .WillOnce(Return(SND_PCM_STATE_RUNNING));
  EXPECT_CALL(mock_alsa_wrapper_, PcmDelay(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(1), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(_))
      .WillRepeatedly(Return(0));  // Buffer is full.

  // Return a partially filled packet.
  EXPECT_CALL(mock_callback,
              OnMoreData(base::TimeDelta(), tick_clock.NowTicks(),
                         AudioGlitchInfo(), _))
      .WillOnce(DoAll(ClearBuffer(), Return(kTestFramesPerPacket / 2)));

  bool source_exhausted;
  test_stream->set_source_callback(&mock_callback);
  test_stream->packet_size_ = kTestPacketSize;
  test_stream->BufferPacket(&source_exhausted);

  EXPECT_EQ(kTestPacketSize / 2, test_stream->buffer_->forward_bytes());
  EXPECT_FALSE(source_exhausted);
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_Negative) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  test_stream->SetTickClockForTesting(&tick_clock);
  InitBuffer(test_stream);
  test_stream->buffer_->Clear();

  // Simulate where the underrun has occurred right after checking the delay.
  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmState(_))
      .WillOnce(Return(SND_PCM_STATE_RUNNING));
  EXPECT_CALL(mock_alsa_wrapper_, PcmDelay(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(-1), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(_))
      .WillRepeatedly(Return(0));  // Buffer is full.
  EXPECT_CALL(mock_callback,
              OnMoreData(base::TimeDelta(), tick_clock.NowTicks(),
                         AudioGlitchInfo(), _))
      .WillOnce(DoAll(ClearBuffer(), Return(kTestFramesPerPacket / 2)));

  bool source_exhausted;
  test_stream->set_source_callback(&mock_callback);
  test_stream->packet_size_ = kTestPacketSize;
  test_stream->BufferPacket(&source_exhausted);

  EXPECT_EQ(kTestPacketSize / 2, test_stream->buffer_->forward_bytes());
  EXPECT_FALSE(source_exhausted);
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_Underrun) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  test_stream->SetTickClockForTesting(&tick_clock);
  InitBuffer(test_stream);
  test_stream->buffer_->Clear();

  // If ALSA has underrun then we should assume a delay of zero.
  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmState(_))
      .WillOnce(Return(SND_PCM_STATE_XRUN));
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(_))
      .WillRepeatedly(Return(0));  // Buffer is full.
  EXPECT_CALL(mock_callback,
              OnMoreData(base::TimeDelta(), tick_clock.NowTicks(),
                         AudioGlitchInfo(), _))
      .WillOnce(DoAll(ClearBuffer(), Return(kTestFramesPerPacket / 2)));

  bool source_exhausted;
  test_stream->set_source_callback(&mock_callback);
  test_stream->packet_size_ = kTestPacketSize;
  test_stream->BufferPacket(&source_exhausted);

  EXPECT_EQ(kTestPacketSize / 2, test_stream->buffer_->forward_bytes());
  EXPECT_FALSE(source_exhausted);
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_FullBuffer) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  InitBuffer(test_stream);
  // No expectations set on the strict mock because nothing should be called.
  bool source_exhausted;
  test_stream->packet_size_ = kTestPacketSize;
  test_stream->BufferPacket(&source_exhausted);
  EXPECT_EQ(kTestPacketSize, test_stream->buffer_->forward_bytes());
  EXPECT_FALSE(source_exhausted);
  test_stream->Close();
}

constexpr const char kSurround40[] = "surround40:CARD=foo,DEV=0";
constexpr const char kSurround41[] = "surround41:CARD=foo,DEV=0";
constexpr const char kSurround50[] = "surround50:CARD=foo,DEV=0";
constexpr const char kSurround51[] = "surround51:CARD=foo,DEV=0";
constexpr const char kSurround70[] = "surround70:CARD=foo,DEV=0";
constexpr const char kSurround71[] = "surround71:CARD=foo,DEV=0";
constexpr const char kGenericSurround50[] = "surround50";

void** GetFakeHints() {
  static const char* kFakeHints[] = {kSurround40, kSurround41, kSurround50,
                                     kSurround51, kSurround70, kSurround71,
                                     nullptr};
  return const_cast<void**>(
      reinterpret_cast<const void** const>(std::data(kFakeHints)));
}

TEST_F(AlsaPcmOutputStreamTest, AutoSelectDevice_DeviceSelect) {
  // Try channels from 1 -> 9. and see that we get the more specific surroundXX
  // device opened for channels 4-8.  For all other channels, the device should
  // default to |AlsaPcmOutputStream::kDefaultDevice|.  We should also not
  // downmix any channel in this case because downmixing is only defined for
  // channels 4-8, which we are guaranteeing to work.
  //
  // Note that the loop starts at "1", so the first parameter is ignored in
  // these arrays.
  constexpr std::array<const char*, 9> kExpectedDeviceName{
      {nullptr, AlsaPcmOutputStream::kDefaultDevice,
       AlsaPcmOutputStream::kDefaultDevice, AlsaPcmOutputStream::kDefaultDevice,
       kSurround40, kSurround50, kSurround51, kSurround70, kSurround71}};
  constexpr std::array<ChannelLayout, 9> kExpectedLayouts{
      {CHANNEL_LAYOUT_NONE, CHANNEL_LAYOUT_MONO, CHANNEL_LAYOUT_STEREO,
       CHANNEL_LAYOUT_SURROUND, CHANNEL_LAYOUT_4_0, CHANNEL_LAYOUT_5_0,
       CHANNEL_LAYOUT_5_1, CHANNEL_LAYOUT_7_0, CHANNEL_LAYOUT_7_1}};

  for (int i = 1; i < 9; ++i) {
    if (i == 3 || i == 4 || i == 5)  // invalid number of channels
      continue;
    SCOPED_TRACE(base::StringPrintf("Attempting %d Channel", i));

    // Hints will only be grabbed for channel numbers that have non-default
    // devices associated with them.
    if (kExpectedDeviceName[i] != AlsaPcmOutputStream::kDefaultDevice) {
      // The DeviceNameHint and DeviceNameFreeHint need to be paired to avoid a
      // memory leak.
      EXPECT_CALL(mock_alsa_wrapper_, DeviceNameHint(_, _, _))
          .WillOnce(DoAll(SetArgPointee<2>(GetFakeHints()), Return(0)));
      EXPECT_CALL(mock_alsa_wrapper_, DeviceNameFreeHint(GetFakeHints()))
          .Times(1);
    }

    EXPECT_CALL(mock_alsa_wrapper_,
                PcmOpen(_, StrEq(kExpectedDeviceName[i]), _, _))
        .WillOnce(DoAll(SetArgPointee<0>(GetFakeHandle()), Return(0)));
    EXPECT_CALL(mock_alsa_wrapper_,
                PcmSetParams(GetFakeHandle(), _, _, i, _, _, _))
        .WillOnce(Return(0));

    // The parameters are specified by ALSA documentation, and are in constants
    // in the implementation files.
    EXPECT_CALL(mock_alsa_wrapper_, DeviceNameGetHint(_, StrEq("IOID")))
        .WillRepeatedly(Invoke(OutputHint));
    EXPECT_CALL(mock_alsa_wrapper_, DeviceNameGetHint(_, StrEq("NAME")))
        .WillRepeatedly(Invoke(EchoHint));

    AlsaPcmOutputStream* test_stream = CreateStream(kExpectedLayouts[i]);
    EXPECT_TRUE(test_stream->AutoSelectDevice(i));

    // Expected downmix is only true for the fifth iteration.
    EXPECT_EQ(i == 5, static_cast<bool>(test_stream->channel_mixer_));

    Mock::VerifyAndClearExpectations(&mock_alsa_wrapper_);
    Mock::VerifyAndClearExpectations(mock_manager_.get());
    test_stream->Close();
  }
}

TEST_F(AlsaPcmOutputStreamTest, AutoSelectDevice_FallbackDevices) {
  // If there are problems opening a multi-channel device, it the fallbacks
  // operations should be as follows.  Assume the multi-channel device name is
  // surround50:
  //
  //   1) Try open "surround50:CARD=foo,DEV=0"
  //   2) Try open "plug:surround50:CARD=foo,DEV=0".
  //   3) Try open "plug:surround50".
  //   4) Try open "default".
  //   5) Try open "plug:default".
  //   6) Give up trying to open.
  //
  const std::array<std::string, 5> kTries{
      {kSurround50, std::string(AlsaPcmOutputStream::kPlugPrefix) + kSurround50,
       std::string(AlsaPcmOutputStream::kPlugPrefix) + kGenericSurround50,
       AlsaPcmOutputStream::kDefaultDevice,
       std::string(AlsaPcmOutputStream::kPlugPrefix) +
           AlsaPcmOutputStream::kDefaultDevice}};

  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameHint(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(GetFakeHints()), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameFreeHint(GetFakeHints())).Times(1);
  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameGetHint(_, StrEq("IOID")))
      .WillRepeatedly(Invoke(OutputHint));
  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameGetHint(_, StrEq("NAME")))
      .WillRepeatedly(Invoke(EchoHint));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillRepeatedly(Return(kDummyMessage));

  InSequence s;
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, StrEq(kTries[0].c_str()), _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, StrEq(kTries[1].c_str()), _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, StrEq(kTries[2].c_str()), _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, StrEq(kTries[3].c_str()), _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, StrEq(kTries[4].c_str()), _, _))
      .WillOnce(Return(kTestFailedErrno));

  AlsaPcmOutputStream* test_stream = CreateStream(CHANNEL_LAYOUT_5_0);
  EXPECT_FALSE(test_stream->AutoSelectDevice(5));
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, AutoSelectDevice_HintFail) {
  // Should get |kDefaultDevice|, and force a 2-channel downmix on a failure to
  // enumerate devices.
  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameHint(_, _, _))
      .WillRepeatedly(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmOpen(_, StrEq(AlsaPcmOutputStream::kDefaultDevice), _, _))
      .WillOnce(DoAll(SetArgPointee<0>(GetFakeHandle()), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmSetParams(GetFakeHandle(), _, _, 2, _, _, _))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));

  AlsaPcmOutputStream* test_stream = CreateStream(CHANNEL_LAYOUT_5_0);
  EXPECT_TRUE(test_stream->AutoSelectDevice(5));
  EXPECT_TRUE(test_stream->channel_mixer_);
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_StopStream) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  InitBuffer(test_stream);
  test_stream->stop_stream_ = true;
  bool source_exhausted;
  test_stream->BufferPacket(&source_exhausted);
  EXPECT_EQ(0u, test_stream->buffer_->forward_bytes());
  EXPECT_TRUE(source_exhausted);
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, ScheduleNextWrite) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsOpened);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsPlaying);
  InitBuffer(test_stream);
  DVLOG(1) << test_stream->state();
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(_))
      .WillOnce(Return(10));
  test_stream->ScheduleNextWrite(false);
  DVLOG(1) << test_stream->state();
  // TODO(sergeyu): Figure out how to check that the task has been added to the
  // message loop.

  // Cleanup the message queue. Currently ~MessageQueue() doesn't free pending
  // tasks unless running on valgrind. The code below is needed to keep
  // heapcheck happy.

  test_stream->stop_stream_ = true;
  DVLOG(1) << test_stream->state();
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsClosed);
  DVLOG(1) << test_stream->state();
  test_stream->Close();
}

TEST_F(AlsaPcmOutputStreamTest, ScheduleNextWrite_StopStream) {
  AlsaPcmOutputStream* test_stream = CreateStream(kTestChannelLayout);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsOpened);
  test_stream->TransitionTo(AlsaPcmOutputStream::kIsPlaying);

  InitBuffer(test_stream);

  test_stream->stop_stream_ = true;
  test_stream->ScheduleNextWrite(true);

  // TODO(ajwong): Find a way to test whether or not another task has been
  // posted so we can verify that the Alsa code will indeed break the task
  // posting loop.

  test_stream->TransitionTo(AlsaPcmOutputStream::kIsClosed);
  test_stream->Close();
}

}  // namespace media
