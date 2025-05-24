// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_session.h"

#include "base/test/simple_test_tick_clock.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_position_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session_playback_state.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using testing::_;

namespace {

class MockMediaSessionService : public mojom::blink::MediaSessionService {
 public:
  MockMediaSessionService() = default;

  HeapMojoRemote<mojom::blink::MediaSessionService> CreateRemoteAndBind(
      ContextLifecycleNotifier* notifier,
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    HeapMojoRemote<mojom::blink::MediaSessionService> remote(notifier);
    remote.Bind(receiver_.BindNewPipeAndPassRemote(), task_runner);
    return remote;
  }

  void SetClient(
      mojo::PendingRemote<mojom::blink::MediaSessionClient> client) override {}
  void SetPlaybackState(
      mojom::blink::MediaSessionPlaybackState state) override {}
  MOCK_METHOD1(SetPositionState,
               void(media_session::mojom::blink::MediaPositionPtr));
  void SetMetadata(mojom::blink::SpecMediaMetadataPtr metadata) override {}
  void SetMicrophoneState(
      media_session::mojom::MicrophoneState microphone_state) override {}
  void SetCameraState(media_session::mojom::CameraState camera_state) override {
  }
  void EnableAction(
      media_session::mojom::blink::MediaSessionAction action) override {}
  void DisableAction(
      media_session::mojom::blink::MediaSessionAction action) override {}

 private:
  mojo::Receiver<mojom::blink::MediaSessionService> receiver_{this};
};

}  // namespace

class MediaSessionTest : public PageTestBase {
 public:
  MediaSessionTest() = default;

  MediaSessionTest(const MediaSessionTest&) = delete;
  MediaSessionTest& operator=(const MediaSessionTest&) = delete;

  void SetUp() override {
    PageTestBase::SetUp();

    mock_service_ = std::make_unique<MockMediaSessionService>();

    media_session_ =
        MediaSession::mediaSession(*GetFrame().DomWindow()->navigator());
    media_session_->service_ = mock_service_->CreateRemoteAndBind(
        GetFrame().DomWindow(),
        GetFrame().DomWindow()->GetTaskRunner(TaskType::kMiscPlatformAPI));
    media_session_->clock_ = &test_clock_;
  }

  void SetPositionState(double duration,
                        double position,
                        double playback_rate) {
    auto* position_state = MediaPositionState::Create();
    position_state->setDuration(duration);
    position_state->setPosition(position);
    position_state->setPlaybackRate(playback_rate);

    NonThrowableExceptionState exception_state;
    media_session_->setPositionState(position_state, exception_state);
  }

  void SetPositionStateThrowsException(double duration,
                                       double position,
                                       double playback_rate) {
    auto* position_state = MediaPositionState::Create();
    position_state->setDuration(duration);
    position_state->setPosition(position);
    position_state->setPlaybackRate(playback_rate);

    DummyExceptionStateForTesting exception_state;
    media_session_->setPositionState(position_state, exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  }

  void ClearPositionState() {
    NonThrowableExceptionState exception_state;
    media_session_->setPositionState(MediaPositionState::Create(),
                                     exception_state);
  }

  void SetPlaybackState(V8MediaSessionPlaybackState::Enum state) {
    media_session_->setPlaybackState(V8MediaSessionPlaybackState(state));
  }

  MockMediaSessionService& service() { return *mock_service_.get(); }

  base::SimpleTestTickClock& clock() { return test_clock_; }

 private:
  base::SimpleTestTickClock test_clock_;

  std::unique_ptr<MockMediaSessionService> mock_service_;

  Persistent<MediaSession> media_session_;
};

TEST_F(MediaSessionTest, PlaybackPositionState_None) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::Seconds(10), position_state->duration);
        EXPECT_EQ(base::Seconds(5), position_state->position);
        EXPECT_EQ(1.0, position_state->playback_rate);
        EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

        loop.Quit();
      }));

  SetPlaybackState(V8MediaSessionPlaybackState::Enum::kNone);
  SetPositionState(10, 5, 1.0);
  loop.Run();
}

TEST_F(MediaSessionTest, PlaybackPositionState_Paused) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::Seconds(10), position_state->duration);
        EXPECT_EQ(base::Seconds(5), position_state->position);
        EXPECT_EQ(0.0, position_state->playback_rate);
        EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

        loop.Quit();
      }));

  SetPlaybackState(V8MediaSessionPlaybackState::Enum::kPaused);
  SetPositionState(10, 5, 1.0);
  loop.Run();
}

TEST_F(MediaSessionTest, PlaybackPositionState_Playing) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::Seconds(10), position_state->duration);
        EXPECT_EQ(base::Seconds(5), position_state->position);
        EXPECT_EQ(1.0, position_state->playback_rate);
        EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

        loop.Quit();
      }));

  SetPlaybackState(V8MediaSessionPlaybackState::Enum::kPlaying);
  SetPositionState(10, 5, 1.0);
  loop.Run();
}

TEST_F(MediaSessionTest, PlaybackPositionState_InfiniteDuration) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::TimeDelta::Max(), position_state->duration);
        EXPECT_EQ(base::Seconds(5), position_state->position);
        EXPECT_EQ(1.0, position_state->playback_rate);
        EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

        loop.Quit();
      }));

  SetPlaybackState(V8MediaSessionPlaybackState::Enum::kNone);
  SetPositionState(std::numeric_limits<double>::infinity(), 5, 1.0);
  loop.Run();
}

TEST_F(MediaSessionTest, PlaybackPositionState_NaNDuration) {
  SetPlaybackState(V8MediaSessionPlaybackState::Enum::kNone);
  SetPositionStateThrowsException(std::nan("10"), 5, 1.0);
}

TEST_F(MediaSessionTest, PlaybackPositionState_Paused_Clear) {
  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::Seconds(10), position_state->duration);
          EXPECT_EQ(base::Seconds(5), position_state->position);
          EXPECT_EQ(0.0, position_state->playback_rate);
          EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

          loop.Quit();
        }));

    SetPlaybackState(V8MediaSessionPlaybackState::Enum::kPaused);
    SetPositionState(10, 5, 1.0);
    loop.Run();
  }

  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_FALSE(position_state);
          loop.Quit();
        }));

    ClearPositionState();
    loop.Run();
  }
}

TEST_F(MediaSessionTest, PositionPlaybackState_None) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::Seconds(10), position_state->duration);
        EXPECT_EQ(base::Seconds(5), position_state->position);
        EXPECT_EQ(1.0, position_state->playback_rate);
        EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

        loop.Quit();
      }));

  SetPositionState(10, 5, 1.0);
  SetPlaybackState(V8MediaSessionPlaybackState::Enum::kNone);
  loop.Run();
}

TEST_F(MediaSessionTest, PositionPlaybackState_Paused_None) {
  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::Minutes(10), position_state->duration);
          EXPECT_EQ(base::Minutes(1), position_state->position);
          EXPECT_EQ(1.0, position_state->playback_rate);
          EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

          loop.Quit();
        }));

    SetPositionState(600, 60, 1.0);
    loop.Run();
  }

  clock().Advance(base::Minutes(1));

  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::Minutes(10), position_state->duration);
          EXPECT_EQ(base::Minutes(2), position_state->position);
          EXPECT_EQ(0.0, position_state->playback_rate);
          EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

          loop.Quit();
        }));

    SetPlaybackState(V8MediaSessionPlaybackState::Enum::kPaused);
    loop.Run();
  }

  clock().Advance(base::Minutes(1));

  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::Minutes(10), position_state->duration);
          EXPECT_EQ(base::Minutes(2), position_state->position);
          EXPECT_EQ(1.0, position_state->playback_rate);
          EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

          loop.Quit();
        }));

    SetPlaybackState(V8MediaSessionPlaybackState::Enum::kNone);
    loop.Run();
  }
}

TEST_F(MediaSessionTest, PositionPlaybackState_Paused_Playing) {
  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::Minutes(10), position_state->duration);
          EXPECT_EQ(base::Minutes(1), position_state->position);
          EXPECT_EQ(1.0, position_state->playback_rate);
          EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

          loop.Quit();
        }));

    SetPositionState(600, 60, 1.0);
    loop.Run();
  }

  clock().Advance(base::Minutes(1));

  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::Minutes(10), position_state->duration);
          EXPECT_EQ(base::Minutes(2), position_state->position);
          EXPECT_EQ(0.0, position_state->playback_rate);
          EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

          loop.Quit();
        }));

    SetPlaybackState(V8MediaSessionPlaybackState::Enum::kPaused);
    loop.Run();
  }

  clock().Advance(base::Minutes(1));

  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::Minutes(10), position_state->duration);
          EXPECT_EQ(base::Minutes(2), position_state->position);
          EXPECT_EQ(1.0, position_state->playback_rate);
          EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

          loop.Quit();
        }));

    SetPlaybackState(V8MediaSessionPlaybackState::Enum::kPlaying);
    loop.Run();
  }
}

TEST_F(MediaSessionTest, PositionPlaybackState_Playing) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::Seconds(10), position_state->duration);
        EXPECT_EQ(base::Seconds(5), position_state->position);
        EXPECT_EQ(1.0, position_state->playback_rate);
        EXPECT_EQ(clock().NowTicks(), position_state->last_updated_time);

        loop.Quit();
      }));

  SetPositionState(10, 5, 1.0);
  SetPlaybackState(V8MediaSessionPlaybackState::Enum::kPlaying);
  loop.Run();
}

}  // namespace blink
