// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_controller.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/environment.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_source_diverter.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunClosure;
using ::base::test::RunOnceClosure;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Bool;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::TestWithParam;

namespace media {

static const int kSampleRate = AudioParameters::kAudioCDSampleRate;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kSamplesPerPacket = kSampleRate / 1000;
static const double kTestVolume = 0.25;
static const float kBufferNonZeroData = 1.0f;

AudioParameters AOCTestParams() {
  return AudioParameters(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kSamplesPerPacket);
}

class MockAudioOutputControllerEventHandler
    : public AudioOutputController::EventHandler {
 public:
  MockAudioOutputControllerEventHandler() = default;

  MOCK_METHOD0(OnControllerCreated, void());
  MOCK_METHOD0(OnControllerPlaying, void());
  MOCK_METHOD0(OnControllerPaused, void());
  MOCK_METHOD0(OnControllerError, void());
  void OnLog(base::StringPiece) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputControllerEventHandler);
};

class MockAudioOutputControllerSyncReader
    : public AudioOutputController::SyncReader {
 public:
  MockAudioOutputControllerSyncReader() = default;

  MOCK_METHOD3(RequestMoreData,
               void(base::TimeDelta delay,
                    base::TimeTicks delay_timestamp,
                    int prior_frames_skipped));
  MOCK_METHOD1(Read, void(AudioBus* dest));
  MOCK_METHOD0(Close, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputControllerSyncReader);
};

class MockAudioOutputStream : public AudioOutputStream,
                              public AudioOutputStream::AudioSourceCallback {
 public:
  explicit MockAudioOutputStream(AudioManager* audio_manager)
      : audio_manager_(audio_manager) {}

  explicit MockAudioOutputStream() : audio_manager_(nullptr) {}

  // We forward to a fake stream to get automatic OnMoreData callbacks,
  // required by some tests.
  MOCK_METHOD0(DidOpen, void());
  MOCK_METHOD0(DidStart, void());
  MOCK_METHOD0(DidStop, void());
  MOCK_METHOD0(DidClose, void());
  MOCK_METHOD0(DidFlush, void());
  MOCK_METHOD1(SetVolume, void(double));
  MOCK_METHOD1(GetVolume, void(double* volume));

  bool Open() override {
    EXPECT_EQ(nullptr, impl_);
    if (audio_manager_) {
      impl_ = audio_manager_->MakeAudioOutputStreamProxy(AOCTestParams(),
                                                         "default");
      impl_->Open();
    }
    DidOpen();
    return true;
  }

  void Start(AudioOutputStream::AudioSourceCallback* cb) override {
    EXPECT_EQ(nullptr, callback_);
    callback_ = cb;
    if (impl_) {
      impl_->Start(this);
    }
    DidStart();
  }

  void Stop() override {
    if (impl_) {
      impl_->Stop();
    }
    callback_ = nullptr;
    DidStop();
  }

  void Close() override {
    if (impl_) {
      impl_->Close();
    }
    impl_ = nullptr;
    DidClose();
  }

  void Flush() override {
    if (impl_) {
      impl_->Flush();
    }
    DidFlush();
  }

 private:
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

  AudioManager* audio_manager_;
  AudioOutputStream* impl_ = nullptr;
  AudioOutputStream::AudioSourceCallback* callback_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputStream);
};

class MockAudioPushSink : public AudioPushSink {
 public:
  MockAudioPushSink() = default;

  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(OnDataCheck, void(float));

  void OnData(std::unique_ptr<AudioBus> source,
              base::TimeTicks reference_time) override {
    OnDataCheck(source->channel(0)[0]);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioPushSink);
};

ACTION(PopulateBuffer) {
  arg0->Zero();
  // Note: To confirm the buffer will be populated in these tests, it's
  // sufficient that only the first float in channel 0 is set to the value.
  arg0->channel(0)[0] = kBufferNonZeroData;
}

class AudioOutputControllerTest : public TestWithParam<bool> {
 public:
  AudioOutputControllerTest()
      : synchronous_use_(GetParam()),
        audio_manager_(AudioManager::CreateForTesting(
            // Make sure that the audio manager thread == the main thread with
            // synchronous use of the controller.
            std::make_unique<TestAudioThread>(!synchronous_use_))),
        mock_stream_(audio_manager_.get()) {}

  ~AudioOutputControllerTest() override { audio_manager_->Shutdown(); }

 protected:
  void Create() {
    EXPECT_CALL(mock_event_handler_, OnControllerCreated());

    controller_ = AudioOutputController::Create(
        audio_manager_.get(), &mock_event_handler_, AOCTestParams(),
        std::string(), base::UnguessableToken(), &mock_sync_reader_);
    EXPECT_NE(nullptr, controller_.get());
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
          data->channel(0)[0] = kBufferNonZeroData;
          barrier.Run();
        }))
        .WillRepeatedly(PopulateBuffer());
    controller_->Play();

    // Waits for all gmock expectations to be satisfied.
    loop.Run();
  }

  void PlayWhileDiverting() {
    base::RunLoop loop;
    // The barrier is used to wait for all of the expectations to be fulfilled.
    base::RepeatingClosure barrier =
        base::BarrierClosure(4, loop.QuitClosure());
    EXPECT_CALL(mock_stream_, DidStart()).WillOnce(RunClosure(barrier));
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying())
        .WillOnce(RunClosure(barrier));
    // The mock stream will start pulling data. We verify that the calls are
    // forwarded to SyncReader, and write some data to the buffer that we can
    // verify later.
    EXPECT_CALL(mock_sync_reader_, RequestMoreData(_, _, _))
        .WillOnce(RunClosure(barrier))
        .WillRepeatedly(Return());
    EXPECT_CALL(mock_sync_reader_, Read(_))
        .WillOnce(Invoke([barrier](AudioBus* data) {
          data->channel(0)[0] = kBufferNonZeroData;
          barrier.Run();
        }))
        .WillRepeatedly(PopulateBuffer());
    controller_->Play();

    // Waits for all gmock expectations to be satisfied.
    loop.Run();
    // At some point in the future, the stream must be stopped.
    EXPECT_CALL(mock_stream_, DidStop());
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

    // Simulate a device change event to AudioOutputController from the
    // AudioManager.
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioOutputController::OnDeviceChange, controller_));

    // Wait for device change to take effect.
    base::RunLoop loop;
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void Divert() {
    EXPECT_CALL(mock_stream_, DidOpen());
    EXPECT_CALL(mock_stream_, SetVolume(kTestVolume));

    controller_->StartDiverting(&mock_stream_);
    base::RunLoop loop;
    // Wait for controller to start diverting.
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void DivertWhilePlaying() {
    base::RunLoop loop;
    // The barrier is used to wait for all of the expectations to be fulfilled.
    base::RepeatingClosure barrier =
        base::BarrierClosure(4, loop.QuitClosure());
    // Expect mock streams to be initialized and started.
    EXPECT_CALL(mock_stream_, DidOpen()).WillOnce(RunClosure(barrier));
    EXPECT_CALL(mock_stream_, SetVolume(kTestVolume))
        .WillOnce(RunClosure(barrier));
    EXPECT_CALL(mock_stream_, DidStart()).WillOnce(RunClosure(barrier));
    // Expect event handler to be informed.
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying())
        .WillOnce(RunClosure(barrier));

    controller_->StartDiverting(&mock_stream_);
    // Wait until callbacks has started.
    loop.Run();
    // At some point in the future, the stream must be stopped.
    EXPECT_CALL(mock_stream_, DidStop());
  }

  void StartDuplicating(MockAudioPushSink* sink) {
    base::RunLoop loop;
    EXPECT_CALL(*sink, OnDataCheck(kBufferNonZeroData))
        .WillOnce(RunOnceClosure(loop.QuitClosure()))
        .WillRepeatedly(Return());
    controller_->StartDuplicating(sink);
    loop.Run();
  }

  void Revert(bool was_playing) {
    if (was_playing) {
      // Expect the handler to receive one OnControllerPlaying() call as a
      // result of the stream switching back.
      EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    }

    EXPECT_CALL(mock_stream_, DidClose());

    controller_->StopDiverting();
    base::RunLoop loop;
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void StopDuplicating(MockAudioPushSink* sink) {
    {
      // First, verify we're still getting callbacks. Must be done on the AM
      // task runner, since it may be a separate thread, and EXPECTing from the
      // main thread would be racy.
      base::RunLoop loop;
      audio_manager_->GetTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](MockAudioPushSink* sink, base::RepeatingClosure done_closure) {
                Mock::VerifyAndClear(sink);
                EXPECT_CALL(*sink, OnDataCheck(kBufferNonZeroData))
                    .WillOnce(RunClosure(done_closure))
                    .WillRepeatedly(Return());
              },
              sink, loop.QuitClosure()));
      loop.Run();
    }

    {
      EXPECT_CALL(*sink, Close());
      controller_->StopDuplicating(sink);
      base::RunLoop loop;
      audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
  }

  void Close() {
    EXPECT_CALL(mock_sync_reader_, Close());

    if (synchronous_use_) {
      controller_->Close(base::OnceClosure());
      return;
    }
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AudioOutputController::Close, controller_,
                                  run_loop.QuitClosure()));
    run_loop.Run();
  }

  void SimulateErrorThenDeviceChange() {
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioOutputControllerTest::TriggerErrorThenDeviceChange,
                       base::Unretained(this)));

    base::RunLoop loop;
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  // These help make test sequences more readable.
  void RevertWasNotPlaying() { Revert(false); }
  void RevertWhilePlaying() { Revert(true); }

  void TriggerErrorThenDeviceChange() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    // Errors should be deferred; the device change should ensure it's dropped.
    EXPECT_CALL(mock_event_handler_, OnControllerError()).Times(0);
    controller_->OnError();

    EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    EXPECT_CALL(mock_event_handler_, OnControllerPaused()).Times(0);
    controller_->OnDeviceChange();
  }

  base::TestMessageLoop message_loop_;
  const bool synchronous_use_;
  std::unique_ptr<AudioManager> audio_manager_;
  StrictMock<MockAudioOutputControllerEventHandler> mock_event_handler_;
  StrictMock<MockAudioOutputControllerSyncReader> mock_sync_reader_;
  StrictMock<MockAudioOutputStream> mock_stream_;
  scoped_refptr<AudioOutputController> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioOutputControllerTest);
};

class AudioOutputControllerMockTest : public TestWithParam<bool> {
 public:
  AudioOutputControllerMockTest()
      : audio_manager_(std::make_unique<media::TestAudioThread>(true)) {
    audio_manager_.SetMakeOutputStreamCB(
        base::BindRepeating([](media::AudioOutputStream* stream,
                               const media::AudioParameters& params,
                               const std::string& device_id) { return stream; },
                            &mock_stream_));
  }

  ~AudioOutputControllerMockTest() { audio_manager_.Shutdown(); }

 protected:
  void Create() {
    EXPECT_CALL(mock_event_handler_, OnControllerCreated());
    EXPECT_CALL(mock_stream_, DidOpen());
    EXPECT_CALL(mock_stream_, SetVolume(1));  // Default volume
    controller_ = AudioOutputController::Create(
        &audio_manager_, &mock_event_handler_, AOCTestParams(), std::string(),
        base::UnguessableToken(), &mock_sync_reader_);
    EXPECT_NE(nullptr, controller_.get());
    EXPECT_CALL(mock_stream_, SetVolume(kTestVolume));
    controller_->SetVolume(kTestVolume);
  }

  void Close() {
    EXPECT_CALL(mock_sync_reader_, Close());
    EXPECT_CALL(mock_stream_, DidClose());

    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AudioOutputController::Close, controller_,
                                  run_loop.QuitClosure()));
    run_loop.Run();
  }

  void Play() {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_stream_, DidStart())
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    EXPECT_CALL(mock_sync_reader_, RequestMoreData(_, _, _))
        .WillRepeatedly(Return());
    controller_->Play();
    run_loop.Run();
  }

  void Pause() {
    base::RunLoop loop;
    EXPECT_CALL(mock_stream_, DidStop());
    EXPECT_CALL(mock_event_handler_, OnControllerPaused())
        .WillOnce(RunOnceClosure(loop.QuitClosure()));
    controller_->Pause();
    loop.Run();
    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void Flush(bool is_playing) {
    base::RunLoop loop;
    if (is_playing) {
      EXPECT_CALL(mock_stream_, DidFlush())
          .WillOnce(RunOnceClosure(loop.QuitClosure()));
    } else {
      EXPECT_CALL(mock_event_handler_, OnControllerError())
          .Times(1)
          .WillOnce(RunOnceClosure(loop.QuitClosure()));
    }

    controller_->Flush();
    loop.Run();
  }

  StrictMock<MockAudioOutputControllerEventHandler> mock_event_handler_;

 private:
  base::TestMessageLoop message_loop_;
  MockAudioManager audio_manager_;
  StrictMock<MockAudioOutputControllerSyncReader> mock_sync_reader_;
  StrictMock<MockAudioOutputStream> mock_stream_;
  scoped_refptr<AudioOutputController> controller_;
};

TEST_P(AudioOutputControllerTest, CreateAndClose) {
  Create();
  Close();
}

TEST_P(AudioOutputControllerTest, PlayAndClose) {
  Create();
  Play();
  Close();
}

TEST_P(AudioOutputControllerTest, PlayPauseClose) {
  Create();
  Play();
  Pause();
  Close();
}

TEST_P(AudioOutputControllerTest, PlayPausePlayClose) {
  Create();
  Play();
  Pause();
  Play();
  Close();
}

TEST_P(AudioOutputControllerTest, PlayDeviceChangeClose) {
  Create();
  Play();
  ChangeDevice();
  Close();
}

TEST_P(AudioOutputControllerTest, PlayDeviceChangeError) {
  Create();
  Play();
  SimulateErrorThenDeviceChange();
  Close();
}

TEST_P(AudioOutputControllerTest, PlayDivertRevertClose) {
  Create();
  Play();
  DivertWhilePlaying();
  RevertWhilePlaying();
  Close();
}

TEST_P(AudioOutputControllerTest, PlayDivertRevertDivertRevertClose) {
  Create();
  Play();
  DivertWhilePlaying();
  RevertWhilePlaying();
  DivertWhilePlaying();
  RevertWhilePlaying();
  Close();
}

TEST_P(AudioOutputControllerTest, DivertPlayPausePlayRevertClose) {
  Create();
  Divert();
  PlayWhileDiverting();
  Pause();
  PlayWhileDiverting();
  RevertWhilePlaying();
  Close();
}

TEST_P(AudioOutputControllerTest, DivertRevertClose) {
  Create();
  Divert();
  RevertWasNotPlaying();
  Close();
}

TEST_P(AudioOutputControllerTest, PlayDuplicateStopClose) {
  Create();
  MockAudioPushSink mock_sink;
  Play();
  StartDuplicating(&mock_sink);
  StopDuplicating(&mock_sink);
  Close();
}

TEST_P(AudioOutputControllerTest, TwoDuplicates) {
  Create();
  MockAudioPushSink mock_sink_1;
  MockAudioPushSink mock_sink_2;
  Play();
  StartDuplicating(&mock_sink_1);
  StartDuplicating(&mock_sink_2);
  StopDuplicating(&mock_sink_1);
  StopDuplicating(&mock_sink_2);
  Close();
}

TEST_P(AudioOutputControllerTest, DuplicateDivertInteract) {
  Create();
  MockAudioPushSink mock_sink;
  Play();
  StartDuplicating(&mock_sink);
  DivertWhilePlaying();
  StopDuplicating(&mock_sink);
  RevertWhilePlaying();
  Close();
}

TEST_F(AudioOutputControllerMockTest, FlushWhenStreamIsPaused) {
  Create();
  Play();
  Pause();
  Flush(true);
  Close();
}

TEST_F(AudioOutputControllerMockTest, FlushWhenStreamIsPlayingTriggersError) {
  Create();
  Play();
  Flush(false);
  Pause();
  Close();
}

INSTANTIATE_TEST_SUITE_P(AOC, AudioOutputControllerTest, Bool());

}  // namespace media
