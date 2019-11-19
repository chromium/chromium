// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_session.h"

#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediasession/media_position_state.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using testing::_;

namespace {

class MockMediaSessionService : public mojom::blink::MediaSessionService {
 public:
  MockMediaSessionService() = default;

  mojo::Remote<mojom::blink::MediaSessionService> CreateRemoteAndBind() {
    return mojo::Remote<mojom::blink::MediaSessionService>(
        receiver_.BindNewPipeAndPassRemote());
  }

  void SetClient(
      mojo::PendingRemote<mojom::blink::MediaSessionClient> client) override {}
  void SetPlaybackState(
      mojom::blink::MediaSessionPlaybackState state) override {}
  MOCK_METHOD1(SetPositionState,
               void(media_session::mojom::blink::MediaPositionPtr));
  void SetMetadata(mojom::blink::SpecMediaMetadataPtr metadata) override {}
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

  void SetUp() override {
    PageTestBase::SetUp();

    mock_service_ = std::make_unique<MockMediaSessionService>();

    media_session_ = MakeGarbageCollected<MediaSession>(&GetDocument());
    media_session_->service_ = mock_service_->CreateRemoteAndBind();
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

  void ClearPositionState() {
    NonThrowableExceptionState exception_state;
    media_session_->setPositionState(MediaPositionState::Create(),
                                     exception_state);
  }

  void SetPlaybackState(const String& state) {
    media_session_->setPlaybackState(state);
  }

  MockMediaSessionService& service() { return *mock_service_.get(); }

 private:
  std::unique_ptr<MockMediaSessionService> mock_service_;

  Persistent<MediaSession> media_session_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionTest);
};

TEST_F(MediaSessionTest, PlaybackPositionState_None) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
        EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
        EXPECT_EQ(1.0, position_state->playback_rate);

        loop.Quit();
      }));

  SetPlaybackState("none");
  SetPositionState(10, 5, 1.0);
  loop.Run();
}

TEST_F(MediaSessionTest, PlaybackPositionState_Paused) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
        EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
        EXPECT_EQ(0.0, position_state->playback_rate);

        loop.Quit();
      }));

  SetPlaybackState("paused");
  SetPositionState(10, 5, 1.0);
  loop.Run();
}

TEST_F(MediaSessionTest, PlaybackPositionState_Playing) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
        EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
        EXPECT_EQ(1.0, position_state->playback_rate);

        loop.Quit();
      }));

  SetPlaybackState("playing");
  SetPositionState(10, 5, 1.0);
  loop.Run();
}

TEST_F(MediaSessionTest, PlaybackPositionState_Paused_Clear) {
  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
          EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
          EXPECT_EQ(0.0, position_state->playback_rate);

          loop.Quit();
        }));

    SetPlaybackState("paused");
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
        EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
        EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
        EXPECT_EQ(1.0, position_state->playback_rate);

        loop.Quit();
      }));

  SetPositionState(10, 5, 1.0);
  SetPlaybackState("none");
  loop.Run();
}

TEST_F(MediaSessionTest, PositionPlaybackState_Paused_None) {
  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
          EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
          EXPECT_EQ(1.0, position_state->playback_rate);

          loop.Quit();
        }));

    SetPositionState(10, 5, 1.0);
    loop.Run();
  }

  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
          EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
          EXPECT_EQ(0.0, position_state->playback_rate);

          loop.Quit();
        }));

    SetPlaybackState("paused");
    loop.Run();
  }

  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
          EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
          EXPECT_EQ(1.0, position_state->playback_rate);

          loop.Quit();
        }));

    SetPlaybackState("none");
    loop.Run();
  }
}

TEST_F(MediaSessionTest, PositionPlaybackState_Paused_Playing) {
  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
          EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
          EXPECT_EQ(1.0, position_state->playback_rate);

          loop.Quit();
        }));

    SetPositionState(10, 5, 1.0);
    loop.Run();
  }

  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
          EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
          EXPECT_EQ(0.0, position_state->playback_rate);

          loop.Quit();
        }));

    SetPlaybackState("paused");
    loop.Run();
  }

  {
    base::RunLoop loop;
    EXPECT_CALL(service(), SetPositionState(_))
        .WillOnce(testing::Invoke([&](auto position_state) {
          EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
          EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
          EXPECT_EQ(1.0, position_state->playback_rate);

          loop.Quit();
        }));

    SetPlaybackState("playing");
    loop.Run();
  }
}

TEST_F(MediaSessionTest, PositionPlaybackState_Playing) {
  base::RunLoop loop;
  EXPECT_CALL(service(), SetPositionState(_))
      .WillOnce(testing::Invoke([&](auto position_state) {
        EXPECT_EQ(base::TimeDelta::FromSeconds(10), position_state->duration);
        EXPECT_EQ(base::TimeDelta::FromSeconds(5), position_state->position);
        EXPECT_EQ(1.0, position_state->playback_rate);

        loop.Quit();
      }));

  SetPositionState(10, 5, 1.0);
  SetPlaybackState("playing");
  loop.Run();
}

}  // namespace blink
