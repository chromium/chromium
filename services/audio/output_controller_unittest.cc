// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_controller.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/environment.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_message_loop.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "services/audio/loopback_group_member.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

using media::AudioBus;
using media::AudioManager;
using media::AudioOutputStream;
using media::AudioParameters;

using base::test::RunClosure;
using base::test::RunOnceClosure;

namespace audio {
namespace {

constexpr int kSampleRate = AudioParameters::kAudioCDSampleRate;
constexpr media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
constexpr int kSamplesPerPacket = kSampleRate / 1000;
constexpr double kTestVolume = 0.25;
constexpr float kBufferNonZeroData = 1.0f;

AudioParameters GetTestParams() {
  // AudioManagerForControllerTest only creates FakeAudioOutputStreams
  // behind-the-scenes. So, the use of PCM_LOW_LATENCY won't actually result in
  // any real system audio output during these tests.
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, kChannelLayout,
                         kSampleRate, kSamplesPerPacket);
}

class MockOutputControllerEventHandler : public OutputController::EventHandler {
 public:
  MockOutputControllerEventHandler() = default;

  MOCK_METHOD0(OnControllerPlaying, void());
  MOCK_METHOD0(OnControllerPaused, void());
  MOCK_METHOD0(OnControllerError, void());
  void OnLog(base::StringPiece) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOutputControllerEventHandler);
};

class MockOutputControllerSyncReader : public OutputController::SyncReader {
 public:
  MockOutputControllerSyncReader() = default;

  MOCK_METHOD3(RequestMoreData,
               void(base::TimeDelta delay,
                    base::TimeTicks delay_timestamp,
                    int prior_frames_skipped));
  MOCK_METHOD1(Read, void(AudioBus* dest));
  MOCK_METHOD0(Close, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOutputControllerSyncReader);
};

class MockStreamMonitor : public StreamMonitor {
 public:
  MockStreamMonitor() = default;

  MOCK_METHOD1(OnStreamActive, void(Snoopable* snoopable));
  MOCK_METHOD1(OnStreamInactive, void(Snoopable* snoopable));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStreamMonitor);
};

// Wraps an AudioOutputStream instance, calling DidXYZ() mock methods for test
// verification of controller behavior. If a null AudioOutputStream pointer is
// provided to the constructor, a "data pump" thread will be run between the
// Start() and Stop() calls to simulate an AudioOutputStream not owned by the
// AudioManager.
class MockAudioOutputStream : public AudioOutputStream,
                              public AudioOutputStream::AudioSourceCallback {
 public:
  MockAudioOutputStream(AudioOutputStream* impl, AudioParameters::Format format)
      : impl_(impl), format_(format) {}

  AudioParameters::Format format() const { return format_; }

  void set_close_callback(base::OnceClosure callback) {
    close_callback_ = std::move(callback);
  }

  // We forward to a fake stream to get automatic OnMoreData callbacks,
  // required by some tests.
  MOCK_METHOD0(DidOpen, void());
  MOCK_METHOD0(DidStart, void());
  MOCK_METHOD0(DidStop, void());
  MOCK_METHOD0(DidClose, void());
  MOCK_METHOD1(DidSetVolume, void(double));
  MOCK_METHOD0(DidFlush, void());

  bool Open() override {
    if (impl_)
      impl_->Open();
    DidOpen();
    return true;
  }

  void Start(AudioOutputStream::AudioSourceCallback* cb) override {
    EXPECT_EQ(nullptr, callback_);
    callback_ = cb;
    if (impl_) {
      impl_->Start(this);
    } else {
      data_thread_ = std::make_unique<base::Thread>("AudioDataThread");
      CHECK(data_thread_->StartAndWaitForTesting());
      data_thread_->task_runner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&MockAudioOutputStream::RunDataLoop,
                         base::Unretained(this), data_thread_->task_runner()),
          GetTestParams().GetBufferDuration());
    }
    DidStart();
  }

  void Stop() override {
    if (impl_) {
      impl_->Stop();
    } else {
      data_thread_ = nullptr;  // Joins/Stops the thread cleanly.
    }
    callback_ = nullptr;
    DidStop();
  }

  void Close() override {
    if (impl_) {
      impl_->Close();
      impl_ = nullptr;
    }
    DidClose();
    if (close_callback_)
      std::move(close_callback_).Run();
    delete this;
  }

  void SetVolume(double volume) override {
    volume_ = volume;
    if (impl_)
      impl_->SetVolume(volume);
    DidSetVolume(volume);
  }

  void GetVolume(double* volume) override { *volume = volume_; }

  void Flush() override {
    if (impl_)
      impl_->Flush();
    DidFlush();
  }

 protected:
  ~MockAudioOutputStream() override = default;

 private:
  // Calls OnMoreData() and then posts a delayed task to call itself again soon.
  void RunDataLoop(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    auto bus = AudioBus::Create(GetTestParams());
    OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0, bus.get());
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MockAudioOutputStream::RunDataLoop,
                       base::Unretained(this), task_runner),
        GetTestParams().GetBufferDuration());
  }

  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 int prior_frames_skipped,
                 AudioBus* dest) override {
    int res = callback_->OnMoreData(delay, delay_timestamp,
                                    prior_frames_skipped, dest);
    EXPECT_EQ(dest->channel(0)[0], kBufferNonZeroData);
    return res;
  }

  void OnError() override {
    // Fake stream doesn't send errors.
    NOTREACHED();
  }

  AudioOutputStream* impl_;
  const AudioParameters::Format format_;
  base::OnceClosure close_callback_;
  AudioOutputStream::AudioSourceCallback* callback_ = nullptr;
  double volume_ = 1.0;
  std::unique_ptr<base::Thread> data_thread_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputStream);
};

class MockSnooper : public Snoopable::Snooper {
 public:
  MockSnooper() = default;
  ~MockSnooper() override = default;

  MOCK_METHOD0(DidProvideData, void());

  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks reference_time,
              double volume) final {
    // Is the AudioBus populated?
    EXPECT_EQ(kBufferNonZeroData, audio_bus.channel(0)[0]);

    // Are reference timestamps monotonically increasing?
    if (!last_reference_time_.is_null()) {
      EXPECT_LT(last_reference_time_, reference_time);
    }
    last_reference_time_ = reference_time;

    // Is the correct volume being provided?
    EXPECT_EQ(kTestVolume, volume);

    DidProvideData();
  }

 private:
  base::TimeTicks last_reference_time_;

  DISALLOW_COPY_AND_ASSIGN(MockSnooper);
};

// A FakeAudioManager that produces MockAudioOutputStreams, and tracks the last
// stream that was created and the last stream that was closed.
class AudioManagerForControllerTest : public media::FakeAudioManager {
 public:
  AudioManagerForControllerTest()
      : media::FakeAudioManager(std::make_unique<media::TestAudioThread>(false),
                                &fake_audio_log_factory_) {}

  ~AudioManagerForControllerTest() final = default;

  MockAudioOutputStream* last_created_stream() const {
    return last_created_stream_;
  }
  MockAudioOutputStream* last_closed_stream() const {
    return last_closed_stream_;
  }

  AudioOutputStream* MakeAudioOutputStream(const AudioParameters& params,
                                           const std::string& device_id,
                                           const LogCallback& cb) final {
    last_created_stream_ = new NiceMock<MockAudioOutputStream>(
        media::FakeAudioManager::MakeAudioOutputStream(params, device_id, cb),
        params.format());
    last_created_stream_->set_close_callback(
        base::BindOnce(&AudioManagerForControllerTest::SetLastClosedStream,
                       base::Unretained(this), last_created_stream_));
    return last_created_stream_;
  }

  AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params,
      const std::string& device_id) final {
    last_created_stream_ = new NiceMock<MockAudioOutputStream>(
        media::FakeAudioManager::MakeAudioOutputStream(params, device_id,
                                                       base::DoNothing()),
        params.format());
    last_created_stream_->set_close_callback(
        base::BindOnce(&AudioManagerForControllerTest::SetLastClosedStream,
                       base::Unretained(this), last_created_stream_));
    return last_created_stream_;
  }

 private:
  void SetLastClosedStream(MockAudioOutputStream* stream) {
    last_closed_stream_ = stream;
  }

  media::FakeAudioLogFactory fake_audio_log_factory_;
  MockAudioOutputStream* last_created_stream_ = nullptr;
  MockAudioOutputStream* last_closed_stream_ = nullptr;
};

ACTION(PopulateBuffer) {
  arg0->Zero();
  // Note: To confirm the buffer will be populated in these tests, it's
  // sufficient that only the first float in channel 0 is set to the value.
  arg0->channel(0)[0] = kBufferNonZeroData;
}

class OutputControllerTest : public ::testing::Test {
 public:
  OutputControllerTest()
      : group_id_(base::UnguessableToken::Create()),
        processing_id_(base::UnguessableToken::Create()) {}

  ~OutputControllerTest() override { audio_manager_.Shutdown(); }

  void SetUp() override {
    controller_.emplace(&audio_manager_, &mock_event_handler_, GetTestParams(),
                        std::string(), &mock_sync_reader_,
                        &stream_monitor_coordinator_, processing_id_);
    controller_->SetVolume(kTestVolume);
  }

  void TearDown() override { controller_ = base::nullopt; }

 protected:
  // Returns the last-created or last-closed AudioOuptutStream.
  MockAudioOutputStream* last_created_stream() const {
    return audio_manager_.last_created_stream();
  }
  MockAudioOutputStream* last_closed_stream() const {
    return audio_manager_.last_closed_stream();
  }

  void Create() {
    controller_->CreateStream();
    controller_->SetVolume(kTestVolume);
  }

  void Play() {
    base::RunLoop loop;
    // The barrier is used to wait for all of the expectations to be fulfilled.
    base::RepeatingClosure barrier =
        base::BarrierClosure(3, loop.QuitClosure());
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying())
        .WillOnce(RunClosure(barrier));
    EXPECT_CALL(mock_sync_reader_, RequestMoreData(_, _, _))
        .WillOnce(RunClosure(barrier))
        .WillRepeatedly(Return());
    EXPECT_CALL(mock_sync_reader_, Read(_))
        .WillOnce(Invoke([barrier](AudioBus* data) {
          data->Zero();
          data->channel(0)[0] = kBufferNonZeroData;
          barrier.Run();
        }))
        .WillRepeatedly(PopulateBuffer());

    controller_->Play();
    Mock::VerifyAndClearExpectations(&mock_event_handler_);

    // Waits for all gmock expectations to be satisfied.
    loop.Run();
  }

  void Pause() {
    base::RunLoop loop;
    EXPECT_CALL(mock_event_handler_, OnControllerPaused())
        .WillOnce(RunOnceClosure(loop.QuitClosure()));
    controller_->Pause();
    loop.Run();
    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void ChangeDevice() {
    // Expect the event handler to receive one OnControllerPaying() call and no
    // OnControllerPaused() call.
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    EXPECT_CALL(mock_event_handler_, OnControllerPaused()).Times(0);

    // Simulate a device change event to OutputController from the AudioManager.
    audio_manager_.GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&OutputController::OnDeviceChange,
                                  base::Unretained(&(*controller_))));

    // Wait for device change to take effect.
    base::RunLoop loop;
    audio_manager_.GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();

    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void StartMutingBeforePlaying() { controller_->StartMuting(); }

  void StartMutingWhilePlaying() {
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    controller_->StartMuting();
    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void StopMuting() {
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    controller_->StopMuting();
    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void StartSnooping(MockSnooper* snooper) {
    controller_->StartSnooping(snooper);
  }

  void WaitForSnoopedData(MockSnooper* snooper) {
    base::RunLoop loop;
    EXPECT_CALL(*snooper, DidProvideData())
        .WillOnce(RunOnceClosure(loop.QuitClosure()))
        .WillRepeatedly(Return());
    loop.Run();
    Mock::VerifyAndClearExpectations(snooper);
  }

  void StopSnooping(MockSnooper* snooper) {
    controller_->StopSnooping(snooper);
  }

  Snoopable* GetSnoopable() { return &(*controller_); }

  void JoinProcessingGroup(StreamMonitor* monitor) {
    stream_monitor_coordinator_.RegisterMember(processing_id_, monitor);
  }

  void LeaveProcessingGroup(StreamMonitor* monitor) {
    stream_monitor_coordinator_.UnregisterMember(processing_id_, monitor);
  }

  void Close() {
    EXPECT_CALL(mock_sync_reader_, Close());
    controller_->Close();

    // Flush any pending tasks (that should have been canceled!).
    base::RunLoop loop;
    audio_manager_.GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void Flush() { controller_->Flush(); }

  void SimulateErrorThenDeviceChange() {
    audio_manager_.GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&OutputControllerTest::TriggerErrorThenDeviceChange,
                       base::Unretained(this)));

    base::RunLoop loop;
    audio_manager_.GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void TriggerErrorThenDeviceChange() {
    DCHECK(audio_manager_.GetTaskRunner()->BelongsToCurrentThread());

    // Errors should be deferred; the device change should ensure it's dropped.
    EXPECT_CALL(mock_event_handler_, OnControllerError()).Times(0);
    controller_->OnError();

    EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    EXPECT_CALL(mock_event_handler_, OnControllerPaused()).Times(0);
    controller_->OnDeviceChange();

    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  StrictMock<MockOutputControllerEventHandler> mock_event_handler_;

 private:
  base::TestMessageLoop message_loop_;
  AudioManagerForControllerTest audio_manager_;
  base::UnguessableToken group_id_;
  base::UnguessableToken processing_id_;
  StrictMock<MockOutputControllerSyncReader> mock_sync_reader_;
  base::Optional<OutputController> controller_;
  StreamMonitorCoordinator stream_monitor_coordinator_;

  DISALLOW_COPY_AND_ASSIGN(OutputControllerTest);
};

TEST_F(OutputControllerTest, CreateAndClose) {
  Create();
  Close();
}

TEST_F(OutputControllerTest, PlayAndClose) {
  Create();
  Play();
  Close();
}

TEST_F(OutputControllerTest, PlayPauseClose) {
  Create();
  Play();
  Pause();
  Close();
}

TEST_F(OutputControllerTest, PlayPausePlayClose) {
  Create();
  Play();
  Pause();
  Play();
  Close();
}

TEST_F(OutputControllerTest, PlayDeviceChangeClose) {
  Create();
  Play();
  ChangeDevice();
  Close();
}

TEST_F(OutputControllerTest, PlayDeviceChangeError) {
  Create();
  Play();
  SimulateErrorThenDeviceChange();
  Close();
}

// Syntactic convenience.
double GetStreamVolume(AudioOutputStream* stream) {
  double result = NAN;
  stream->GetVolume(&result);
  return result;
}

// Tests that muting before the stream is created will result in only the
// "muting stream" being created, and not any local playout streams (that might
// possibly cause an audible blip).
TEST_F(OutputControllerTest, MuteCreatePlayClose) {
  StartMutingBeforePlaying();
  EXPECT_EQ(nullptr, last_created_stream());  // No stream yet.
  EXPECT_EQ(nullptr, last_closed_stream());   // No stream yet.

  Create();
  MockAudioOutputStream* const mute_stream = last_created_stream();
  ASSERT_TRUE(mute_stream);
  EXPECT_EQ(nullptr, last_closed_stream());
  EXPECT_EQ(AudioParameters::AUDIO_FAKE, mute_stream->format());

  Play();
  ASSERT_EQ(mute_stream, last_created_stream());
  EXPECT_EQ(nullptr, last_closed_stream());
  EXPECT_EQ(AudioParameters::AUDIO_FAKE, mute_stream->format());

  Close();
  EXPECT_EQ(mute_stream, last_created_stream());
  EXPECT_EQ(mute_stream, last_closed_stream());
}

// Tests that a local playout stream is shut-down and replaced with a "muting
// stream" if StartMuting() is called after playback begins.
TEST_F(OutputControllerTest, CreatePlayMuteClose) {
  Create();
  MockAudioOutputStream* const playout_stream = last_created_stream();
  ASSERT_TRUE(playout_stream);
  EXPECT_EQ(nullptr, last_closed_stream());

  Play();
  ASSERT_EQ(playout_stream, last_created_stream());
  EXPECT_EQ(nullptr, last_closed_stream());
  EXPECT_EQ(GetTestParams().format(), playout_stream->format());
  EXPECT_EQ(kTestVolume, GetStreamVolume(playout_stream));

  StartMutingWhilePlaying();
  MockAudioOutputStream* const mute_stream = last_created_stream();
  ASSERT_TRUE(mute_stream);
  EXPECT_EQ(playout_stream, last_closed_stream());
  EXPECT_EQ(AudioParameters::AUDIO_FAKE, mute_stream->format());

  Close();
  EXPECT_EQ(mute_stream, last_created_stream());
  EXPECT_EQ(mute_stream, last_closed_stream());
}

// Tests that the "muting stream" is shut down and replaced with the normal
// playout stream after StopMuting() is called.
TEST_F(OutputControllerTest, PlayMuteUnmuteClose) {
  StartMutingBeforePlaying();
  Create();
  Play();
  MockAudioOutputStream* const mute_stream = last_created_stream();
  ASSERT_TRUE(mute_stream);
  EXPECT_EQ(nullptr, last_closed_stream());
  EXPECT_EQ(AudioParameters::AUDIO_FAKE, mute_stream->format());

  StopMuting();
  MockAudioOutputStream* const playout_stream = last_created_stream();
  ASSERT_TRUE(playout_stream);
  EXPECT_EQ(mute_stream, last_closed_stream());
  EXPECT_EQ(GetTestParams().format(), playout_stream->format());
  EXPECT_EQ(kTestVolume, GetStreamVolume(playout_stream));

  Close();
  EXPECT_EQ(playout_stream, last_created_stream());
  EXPECT_EQ(playout_stream, last_closed_stream());
}

TEST_F(OutputControllerTest, SnoopCreatePlayStopClose) {
  NiceMock<MockSnooper> snooper;
  StartSnooping(&snooper);
  Create();
  Play();
  WaitForSnoopedData(&snooper);
  StopSnooping(&snooper);
  Close();
}

TEST_F(OutputControllerTest, CreatePlaySnoopStopClose) {
  NiceMock<MockSnooper> snooper;
  Create();
  Play();
  StartSnooping(&snooper);
  WaitForSnoopedData(&snooper);
  StopSnooping(&snooper);
  Close();
}

TEST_F(OutputControllerTest, CreatePlaySnoopCloseStop) {
  NiceMock<MockSnooper> snooper;
  Create();
  Play();
  StartSnooping(&snooper);
  WaitForSnoopedData(&snooper);
  Close();
  StopSnooping(&snooper);
}

TEST_F(OutputControllerTest, TwoSnoopers_StartAtDifferentTimes) {
  NiceMock<MockSnooper> snooper1;
  NiceMock<MockSnooper> snooper2;
  StartSnooping(&snooper1);
  Create();
  Play();
  WaitForSnoopedData(&snooper1);
  StartSnooping(&snooper2);
  WaitForSnoopedData(&snooper2);
  WaitForSnoopedData(&snooper1);
  WaitForSnoopedData(&snooper2);
  Close();
  StopSnooping(&snooper1);
  StopSnooping(&snooper2);
}

TEST_F(OutputControllerTest, TwoSnoopers_StopAtDifferentTimes) {
  NiceMock<MockSnooper> snooper1;
  NiceMock<MockSnooper> snooper2;
  Create();
  Play();
  StartSnooping(&snooper1);
  WaitForSnoopedData(&snooper1);
  StartSnooping(&snooper2);
  WaitForSnoopedData(&snooper2);
  StopSnooping(&snooper1);
  WaitForSnoopedData(&snooper2);
  Close();
  StopSnooping(&snooper2);
}

TEST_F(OutputControllerTest, SnoopWhileMuting) {
  NiceMock<MockSnooper> snooper;

  StartMutingBeforePlaying();
  EXPECT_EQ(nullptr, last_created_stream());  // No stream yet.
  EXPECT_EQ(nullptr, last_closed_stream());   // No stream yet.

  Create();
  MockAudioOutputStream* const mute_stream = last_created_stream();
  ASSERT_TRUE(mute_stream);
  EXPECT_EQ(nullptr, last_closed_stream());

  Play();
  ASSERT_EQ(mute_stream, last_created_stream());
  EXPECT_EQ(nullptr, last_closed_stream());
  EXPECT_EQ(AudioParameters::AUDIO_FAKE, mute_stream->format());

  StartSnooping(&snooper);
  ASSERT_EQ(mute_stream, last_created_stream());
  EXPECT_EQ(nullptr, last_closed_stream());
  EXPECT_EQ(AudioParameters::AUDIO_FAKE, mute_stream->format());
  WaitForSnoopedData(&snooper);

  StopSnooping(&snooper);
  ASSERT_EQ(mute_stream, last_created_stream());
  EXPECT_EQ(nullptr, last_closed_stream());
  EXPECT_EQ(AudioParameters::AUDIO_FAKE, mute_stream->format());

  Close();
  EXPECT_EQ(mute_stream, last_created_stream());
  EXPECT_EQ(mute_stream, last_closed_stream());
}

TEST_F(OutputControllerTest, InformsStreamMonitorsAlreadyInGroup) {
  MockStreamMonitor monitor;
  EXPECT_CALL(monitor, OnStreamActive(GetSnoopable()));
  EXPECT_CALL(monitor, OnStreamInactive(GetSnoopable()));
  JoinProcessingGroup(&monitor);
  Create();
  Play();
  Close();
  LeaveProcessingGroup(&monitor);
}

TEST_F(OutputControllerTest, InformsStreamMonitorsJoiningInGroup) {
  MockStreamMonitor monitor;
  EXPECT_CALL(monitor, OnStreamActive(GetSnoopable()));
  EXPECT_CALL(monitor, OnStreamInactive(GetSnoopable()));
  Create();
  Play();
  JoinProcessingGroup(&monitor);
  Close();
  LeaveProcessingGroup(&monitor);
}

TEST_F(OutputControllerTest,
       DoesNotInformStreamMonitorsJoiningInGroupAfterClose) {
  MockStreamMonitor monitor;
  EXPECT_CALL(monitor, OnStreamActive(GetSnoopable())).Times(0);
  EXPECT_CALL(monitor, OnStreamInactive(GetSnoopable())).Times(0);
  Create();
  Play();
  Close();
  JoinProcessingGroup(&monitor);
  LeaveProcessingGroup(&monitor);
}

TEST_F(OutputControllerTest, FlushWhenStreamIsPlayingTriggersError) {
  Create();
  Play();

  MockAudioOutputStream* const mock_stream = last_created_stream();
  EXPECT_CALL(*mock_stream, DidFlush()).Times(0);
  EXPECT_CALL(mock_event_handler_, OnControllerError()).Times(1);
  Flush();

  Close();
}

TEST_F(OutputControllerTest, FlushesWhenStreamIsNotPlaying) {
  Create();
  Play();
  Pause();

  MockAudioOutputStream* const mock_stream = last_created_stream();
  EXPECT_CALL(*mock_stream, DidFlush()).Times(1);
  Flush();

  Close();
}

}  // namespace
}  // namespace audio
