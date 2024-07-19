// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_controller.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
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
const media::ChannelLayoutConfig kChannelLayoutConfig =
    media::ChannelLayoutConfig::Stereo();
constexpr int kSamplesPerPacket = kSampleRate / 1000;
constexpr double kTestVolume = 0.25;
constexpr float kBufferNonZeroData = 1.0f;

AudioParameters GetTestParams() {
  // AudioManagerForControllerTest only creates FakeAudioOutputStreams
  // behind-the-scenes. So, the use of PCM_LOW_LATENCY won't actually result in
  // any real system audio output during these tests.
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         kChannelLayoutConfig, kSampleRate, kSamplesPerPacket);
}

class MockOutputControllerEventHandler : public OutputController::EventHandler {
 public:
  MockOutputControllerEventHandler() = default;

  MockOutputControllerEventHandler(const MockOutputControllerEventHandler&) =
      delete;
  MockOutputControllerEventHandler& operator=(
      const MockOutputControllerEventHandler&) = delete;

  MOCK_METHOD0(OnControllerPlaying, void());
  MOCK_METHOD0(OnControllerPaused, void());
  MOCK_METHOD0(OnControllerError, void());
  void OnLog(std::string_view) override {}
};

class MockOutputControllerSyncReader : public OutputController::SyncReader {
 public:
  MockOutputControllerSyncReader() = default;

  MockOutputControllerSyncReader(const MockOutputControllerSyncReader&) =
      delete;
  MockOutputControllerSyncReader& operator=(
      const MockOutputControllerSyncReader&) = delete;

  MOCK_METHOD3(RequestMoreData,
               void(base::TimeDelta delay,
                    base::TimeTicks delay_timestamp,
                    const media::AudioGlitchInfo& glitch_info));
  MOCK_METHOD2(Read, bool(AudioBus* dest, bool is_mixing));
  MOCK_METHOD0(Close, void());
};

class MockStreamFactory {
 public:
  MOCK_METHOD(media::AudioOutputStream*,
              CreateStream,
              (const std::string&,
               const media::AudioParameters&,
               base::OnceClosure on_device_change_callback));
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

  MockAudioOutputStream(const MockAudioOutputStream&) = delete;
  MockAudioOutputStream& operator=(const MockAudioOutputStream&) = delete;

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
    EXPECT_EQ(nullptr, callback_.get());
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
    OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {}, bus.get());
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MockAudioOutputStream::RunDataLoop,
                       base::Unretained(this), task_runner),
        GetTestParams().GetBufferDuration());
  }

  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 AudioBus* dest) override {
    int res = callback_->OnMoreData(delay, delay_timestamp, glitch_info, dest);
    EXPECT_EQ(dest->channel(0)[0], kBufferNonZeroData);
    return res;
  }

  void OnError(ErrorType type) override {
    // Fake stream doesn't send errors.
    NOTREACHED_IN_MIGRATION();
  }

  raw_ptr<AudioOutputStream, DanglingUntriaged> impl_;
  const AudioParameters::Format format_;
  base::OnceClosure close_callback_;
  raw_ptr<AudioOutputStream::AudioSourceCallback> callback_ = nullptr;
  double volume_ = 1.0;
  std::unique_ptr<base::Thread> data_thread_;
};

class MockSnooper : public Snoopable::Snooper {
 public:
  MockSnooper() = default;

  MockSnooper(const MockSnooper&) = delete;
  MockSnooper& operator=(const MockSnooper&) = delete;

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
};

// A FakeAudioManager that produces MockAudioOutputStreams, and tracks the last
// stream that was created and the last stream that was closed.
class AudioManagerForControllerTest final : public media::FakeAudioManager {
 public:
  AudioManagerForControllerTest()
      : media::FakeAudioManager(std::make_unique<media::TestAudioThread>(false),
                                &fake_audio_log_factory_) {}

  ~AudioManagerForControllerTest() override = default;

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

  void SimulateDeviceChange() { NotifyAllOutputDeviceChangeListeners(); }

 private:
  void SetLastClosedStream(MockAudioOutputStream* stream) {
    last_closed_stream_ = stream;
  }

  media::FakeAudioLogFactory fake_audio_log_factory_;
  raw_ptr<MockAudioOutputStream, DanglingUntriaged> last_created_stream_ =
      nullptr;
  raw_ptr<MockAudioOutputStream, DanglingUntriaged> last_closed_stream_ =
      nullptr;
};

ACTION(PopulateBuffer) {
  arg0->Zero();
  // Note: To confirm the buffer will be populated in these tests, it's
  // sufficient that only the first float in channel 0 is set to the value.
  arg0->channel(0)[0] = kBufferNonZeroData;
  return true;
}

class OutputControllerTest : public ::testing::Test {
 public:
  OutputControllerTest() : group_id_(base::UnguessableToken::Create()) {}

  OutputControllerTest(const OutputControllerTest&) = delete;
  OutputControllerTest& operator=(const OutputControllerTest&) = delete;

  ~OutputControllerTest() override { audio_manager_.Shutdown(); }

  void SetUp() override {
    controller_.emplace(&audio_manager_, &mock_event_handler_, GetTestParams(),
                        std::string(), &mock_sync_reader_);
    controller_->SetVolume(kTestVolume);
  }

  void TearDown() override { controller_ = std::nullopt; }

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
    EXPECT_CALL(mock_sync_reader_, Read(_, false))
        .WillOnce(Invoke([barrier](AudioBus* data, bool /*is_mixing*/) {
          data->Zero();
          data->channel(0)[0] = kBufferNonZeroData;
          barrier.Run();
          return true;
        }))
        .WillRepeatedly(PopulateBuffer());

    controller_->Play();
    Mock::VerifyAndClearExpectations(&mock_event_handler_);

    // Waits for all gmock expectations to be satisfied.
    loop.Run();
  }

  void PlayWhilePlaying() { controller_->Play(); }

  void Pause() {
    base::RunLoop loop;
    EXPECT_CALL(mock_event_handler_, OnControllerPaused())
        .WillOnce(RunOnceClosure(loop.QuitClosure()));
    controller_->Pause();
    loop.Run();
    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void ChangeDevice(bool expect_play_event = true) {
    // If the stream was already playing before the device change, expect the
    // event handler to receive one OnControllerPaying() call.
    if (expect_play_event) {
      EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    } else {
      EXPECT_CALL(mock_event_handler_, OnControllerPlaying()).Times(0);
    }

    // Never expect a OnControllerPaused() call.
    EXPECT_CALL(mock_event_handler_, OnControllerPaused()).Times(0);

    // Simulate a device change event to OutputController from the AudioManager.
    audio_manager_.GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioManagerForControllerTest::SimulateDeviceChange,
                       base::Unretained(&audio_manager_)));

    // Wait for device change to take effect.
    base::RunLoop loop;
    audio_manager_.GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();

    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void SwitchDeviceId(bool expect_play_event = true,
                      const std::string& device_id = "deviceId_1") {
    if (expect_play_event) {
      EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    } else {
      EXPECT_CALL(mock_event_handler_, OnControllerPlaying()).Times(0);
    }

    EXPECT_CALL(mock_event_handler_, OnControllerPaused()).Times(0);

    // Never expect a OnControllerError() call.
    EXPECT_CALL(mock_event_handler_, OnControllerError()).Times(0);

    controller_->SwitchAudioOutputDeviceId(device_id);

    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void StartMutingBeforePlaying() { controller_->StartMuting(); }

  void StartMutingWhilePlaying() {
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    controller_->StartMuting();
    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void StopMutingBeforePlaying() { controller_->StopMuting(); }

  void StopMutingWhilePlaying() {
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

  void Close() {
    EXPECT_CALL(mock_sync_reader_, Close());
    controller_->Close();

    // Flush any pending tasks (that should have been canceled!).
    base::RunLoop loop;
    audio_manager_.GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void Flush() { controller_->Flush(); }

  StrictMock<MockOutputControllerEventHandler> mock_event_handler_;

 private:
  base::TestMessageLoop message_loop_;
  AudioManagerForControllerTest audio_manager_;
  base::UnguessableToken group_id_;
  StrictMock<MockOutputControllerSyncReader> mock_sync_reader_;
  std::optional<OutputController> controller_;
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

TEST_F(OutputControllerTest, PlayDeviceChangeDeviceChangeClose) {
  Create();
  Play();
  ChangeDevice();
  ChangeDevice();
  Close();
}

TEST_F(OutputControllerTest, PlayPauseDeviceChangeClose) {
  Create();
  Play();
  Pause();
  ChangeDevice(/*expect_play_event=*/false);
  Close();
}

TEST_F(OutputControllerTest, CreateDeviceChangeClose) {
  Create();
  ChangeDevice(/*expect_play_event=*/false);
  Close();
}

TEST_F(OutputControllerTest, SwitchDeviceClose) {
  Create();
  SwitchDeviceId(/*expect_play_event=*/false);
  Close();
}

TEST_F(OutputControllerTest, SwitchDevicePlayClose) {
  Create();
  SwitchDeviceId(/*expect_play_event=*/false);
  Play();
  Close();
}

TEST_F(OutputControllerTest, PlayPauseSwitchDevicePlayClose) {
  Create();
  Play();
  Pause();
  SwitchDeviceId(/*expect_play_event=*/false);
  Play();
  Close();
}

TEST_F(OutputControllerTest, PlaySwitchDeviceSwitchDevice2Close) {
  Create();
  Play();
  SwitchDeviceId();
  SwitchDeviceId(/*expect_play_event=*/true, /*device_id*/ "deviceId_2");
  Close();
}

TEST_F(OutputControllerTest, PlaySwitchDevicePausePlayPauseClose) {
  Create();
  Play();
  SwitchDeviceId();
  Pause();
  Play();
  Pause();
  Close();
}

TEST_F(OutputControllerTest, PlaySwitchDeviceSwitchDeviceClose) {
  Create();
  Play();
  SwitchDeviceId();
  // It does not expect Play event because the device is switched because
  // the new device is the same as the current device.
  SwitchDeviceId(/*expect_play_event=*/false);
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
TEST_F(OutputControllerTest, MutePlayUnmuteClose) {
  StartMutingBeforePlaying();
  Create();
  Play();
  MockAudioOutputStream* const mute_stream = last_created_stream();
  ASSERT_TRUE(mute_stream);
  EXPECT_EQ(nullptr, last_closed_stream());
  EXPECT_EQ(AudioParameters::AUDIO_FAKE, mute_stream->format());

  StopMutingWhilePlaying();
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

TEST_F(OutputControllerTest, FlushesAfterDeviceChange) {
  Create();
  ChangeDevice(/*expect_play_event=*/false);

  MockAudioOutputStream* const mock_stream = last_created_stream();
  EXPECT_CALL(*mock_stream, DidFlush()).Times(1);
  Flush();

  Close();
}

class MockAudioOutputStreamForMixing : public AudioOutputStream {
 public:
  MOCK_METHOD0(Open, bool());
  MOCK_METHOD0(DidStart, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Flush, void());

  void SetVolume(double volume) override {}
  void GetVolume(double* volume) override {}

  void Start(AudioSourceCallback* callback) override {
    callback_ = callback;
    DidStart();
  }

  void SimulateOnMoreDataCalled(const AudioParameters& params,
                                media::AudioGlitchInfo glitch_info,
                                bool is_mixing) {
    DCHECK(callback_);
    auto audio_bus = media::AudioBus::Create(params);
    callback_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(),
                          glitch_info, audio_bus.get(), is_mixing);
  }

 private:
  raw_ptr<AudioSourceCallback> callback_;
};

TEST(OutputControllerMixingTest,
     ControllerUsesProvidedCManagedDeviceOutputStreamCreateCallbackCorrectly) {
  base::TestMessageLoop message_loop_;
  // Controller creation parameters.
  AudioManagerForControllerTest audio_manager;
  NiceMock<MockOutputControllerEventHandler> mock_event_handler;
  NiceMock<MockOutputControllerSyncReader> mock_sync_reader;
  const std::string controller_device_id("device id");
  const AudioParameters controller_params(GetTestParams());

  // Callback that OutputController should provide when calling
  // ManagedDeviceOutputStreamCreateCallback to create a stream.
  base::OnceClosure provided_on_device_change_callback;

  // Mock outputstream which will be returned to the OutputController when it
  // calls ManagedDeviceOutputStreamCreateCallback.
  StrictMock<MockAudioOutputStreamForMixing> mock_output_stream;

  // Mock to be used for ManagedDeviceOutputStreamCreateCallback implementation.
  auto CreateStream =
      [](const std::string& controller_device_id,
         const AudioParameters& controller_params,
         MockAudioOutputStreamForMixing* const mock_output_stream,
         base::OnceClosure* provided_on_device_change_callback,
         const std::string& device_id, const AudioParameters& params,
         base::OnceClosure on_device_change_callback) -> AudioOutputStream* {
    EXPECT_TRUE(params.Equals(controller_params));
    EXPECT_EQ(device_id, controller_device_id);
    *provided_on_device_change_callback = std::move(on_device_change_callback);
    return mock_output_stream;
  };

  ON_CALL(mock_output_stream, Open()).WillByDefault(Return(true));

  OutputController::ManagedDeviceOutputStreamCreateCallback
      mock_create_stream_cb = base::BindRepeating(
          CreateStream, controller_device_id, controller_params,
          &mock_output_stream, &provided_on_device_change_callback);

  EXPECT_FALSE(provided_on_device_change_callback);

  // Check that controller called |mock_create_stream_cb| on its CreateStream(),
  // and uses the result AudioOutputStream.
  EXPECT_CALL(mock_output_stream, Open()).Times(1);

  OutputController controller(&audio_manager, &mock_event_handler,
                              controller_params, controller_device_id,
                              &mock_sync_reader,
                              std::move(mock_create_stream_cb));

  controller.CreateStream();

  // When invoking |mock_create_stream_cb|, OutputController
  // provided a callback to handle device changes.
  EXPECT_TRUE(provided_on_device_change_callback);
  Mock::VerifyAndClearExpectations(&mock_output_stream);

  {
    // When |provided_on_device_change_callback| is called, OutputController
    // must synchronously close the output stream, and then it will call
    // |mock_create_stream_cb| to create a new one.
    testing::InSequence s;
    EXPECT_CALL(mock_output_stream, Close()).Times(1);
    EXPECT_CALL(mock_output_stream, Open()).Times(1);
    EXPECT_TRUE(provided_on_device_change_callback);

    std::move(provided_on_device_change_callback).Run();
    Mock::VerifyAndClearExpectations(&mock_output_stream);
  }

  {
    // After the first device change, OutputController handles the next device
    // change correctly as well.
    testing::InSequence s;
    EXPECT_CALL(mock_output_stream, Close()).Times(1);
    EXPECT_CALL(mock_output_stream, Open()).Times(1);
    EXPECT_TRUE(provided_on_device_change_callback);

    std::move(provided_on_device_change_callback).Run();
    Mock::VerifyAndClearExpectations(&mock_output_stream);
  }

  EXPECT_CALL(mock_output_stream, Close()).Times(1);
  controller.Close();
  audio_manager.Shutdown();
}

TEST(OutputControllerMixingTest,
     ControllerForwardsMixingFlagAndGlitchesToSyncReader) {
  base::TestMessageLoop message_loop_;
  // Controller creation parameters.
  AudioManagerForControllerTest audio_manager;
  NiceMock<MockOutputControllerEventHandler> mock_event_handler;
  NiceMock<MockOutputControllerSyncReader> mock_sync_reader;
  const std::string controller_device_id("device id");
  const AudioParameters controller_params(GetTestParams());

  // Mock outputstream which will be returned to the OutputController when it
  // calls ManagedDeviceOutputStreamCreateCallback.
  NiceMock<MockAudioOutputStreamForMixing> mock_output_stream;

  // Mock to be used for ManagedDeviceOutputStreamCreateCallback implementation.
  auto CreateStream =
      [](const std::string& controller_device_id,
         const AudioParameters& controller_params,
         MockAudioOutputStreamForMixing* const mock_output_stream,
         const std::string& device_id, const AudioParameters& params,
         base::OnceClosure on_device_change_callback) -> AudioOutputStream* {
    EXPECT_TRUE(params.Equals(controller_params));
    EXPECT_EQ(device_id, controller_device_id);
    return mock_output_stream;
  };

  ON_CALL(mock_output_stream, Open()).WillByDefault(Return(true));

  OutputController::ManagedDeviceOutputStreamCreateCallback
      mock_create_stream_cb =
          base::BindRepeating(CreateStream, controller_device_id,
                              controller_params, &mock_output_stream);

  // Check that controller called |mock_create_stream_cb| on its CreateStream(),
  // and uses the result AudioOutputStream.
  EXPECT_CALL(mock_output_stream, Open()).Times(1);
  EXPECT_CALL(mock_output_stream, DidStart()).Times(1);

  OutputController controller(&audio_manager, &mock_event_handler,
                              controller_params, controller_device_id,
                              &mock_sync_reader,
                              std::move(mock_create_stream_cb));

  controller.CreateStream();
  controller.Play();

  // The stream should have been started.
  Mock::VerifyAndClearExpectations(&mock_output_stream);
  Mock::VerifyAndClearExpectations(&mock_sync_reader);

  // Verify OutputController forwards the mixing flag from OnMoreDataCalled when
  // it is true.
  EXPECT_CALL(mock_sync_reader, Read(_, /*is_mixing=*/true)).Times(1);
  mock_output_stream.SimulateOnMoreDataCalled(controller_params, {}, true);

  Mock::VerifyAndClearExpectations(&mock_sync_reader);

  // Verify OutputController forwards the mixing flag from OnMoreDataCalled when
  // it is false.
  EXPECT_CALL(mock_sync_reader, Read(_, /*is_mixing=*/false)).Times(1);
  mock_output_stream.SimulateOnMoreDataCalled(controller_params, {}, false);

  Mock::VerifyAndClearExpectations(&mock_sync_reader);

  // Verify OutputController forwards glitch info.
  media::AudioGlitchInfo glitch_info{.duration = base::Seconds(5),
                                     .count = 123};
  EXPECT_CALL(mock_sync_reader, Read(_, /*is_mixing=*/false)).Times(1);
  EXPECT_CALL(mock_sync_reader, RequestMoreData(_, _, glitch_info)).Times(1);
  mock_output_stream.SimulateOnMoreDataCalled(controller_params, glitch_info,
                                              false);

  Mock::VerifyAndClearExpectations(&mock_sync_reader);

  controller.Close();
  audio_manager.Shutdown();
}

TEST(OutputControllerMixingTest,
     SwitchDeviceIdManagedDeviceOutputStreamCreateCallbackCorrectly) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // Controller creation parameters.
  AudioManagerForControllerTest audio_manager;
  NiceMock<MockOutputControllerEventHandler> mock_event_handler;
  NiceMock<MockOutputControllerSyncReader> mock_sync_reader;
  const std::string controller_device_id("deviceId_1");
  const std::string switched_device_id("deviceId_2");
  const AudioParameters controller_params(GetTestParams());

  // Mock outputstream which will be returned to the OutputController when it
  // calls ManagedDeviceOutputStreamCreateCallback.
  StrictMock<MockAudioOutputStreamForMixing> mock_output_stream;

  MockStreamFactory mock_stream_factory;

  // Validates the called CreateStream has expected device id.
  EXPECT_CALL(mock_stream_factory, CreateStream(controller_device_id, _, _))
      .Times(1);
  EXPECT_CALL(mock_stream_factory, CreateStream(switched_device_id, _, _))
      .Times(1);
  ON_CALL(mock_stream_factory, CreateStream(_, _, _))
      .WillByDefault(Return(&mock_output_stream));

  ON_CALL(mock_output_stream, Open()).WillByDefault(Return(true));
  OutputController::ManagedDeviceOutputStreamCreateCallback
      mock_create_stream_cb =
          base::BindRepeating(&MockStreamFactory::CreateStream,
                              base::Unretained(&mock_stream_factory));

  // Check that controller called |mock_create_stream_cb| on its CreateStream(),
  // and uses the result AudioOutputStream.
  EXPECT_CALL(mock_output_stream, Open()).Times(1);

  OutputController controller(&audio_manager, &mock_event_handler,
                              controller_params, controller_device_id,
                              &mock_sync_reader,
                              std::move(mock_create_stream_cb));

  controller.CreateStream();

  Mock::VerifyAndClearExpectations(&mock_output_stream);

  {
    // When `SwitchAudioOutputDeviceId()` is called, it will call
    // `ManagedDeviceOutputStreamCreateCallback` callback, which make
    // OutputController to synchronously close the output stream, and then
    // it will call `mock_create_stream_cb` to create a new one.
    testing::InSequence s;
    EXPECT_CALL(mock_output_stream, Close()).Times(1);
    EXPECT_CALL(mock_output_stream, Open()).Times(1);

    controller.SwitchAudioOutputDeviceId(switched_device_id);
    Mock::VerifyAndClearExpectations(&mock_output_stream);
  }

  EXPECT_CALL(mock_output_stream, Close()).Times(1);
  controller.Close();
  audio_manager.Shutdown();
}

}  // namespace
}  // namespace audio
