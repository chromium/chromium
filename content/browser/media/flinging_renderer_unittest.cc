// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/flinging_renderer.h"

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/version.h"
#include "media/base/media_controller.h"
#include "media/base/mock_filters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;
using ::testing::NiceMock;

namespace content {

class MockMediaController : public media::MediaController {
 public:
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetMute, void(bool));
  MOCK_METHOD1(SetVolume, void(float));
  MOCK_METHOD1(Seek, void(base::TimeDelta));
};

class MockFlingingController : public media::FlingingController {
 public:
  explicit MockFlingingController(media::MediaController* media_controller)
      : media_controller_(media_controller) {}

  media::MediaController* GetMediaController() { return media_controller_; }

  MOCK_METHOD1(AddMediaStatusObserver, void(media::MediaStatusObserver*));
  MOCK_METHOD1(RemoveMediaStatusObserver, void(media::MediaStatusObserver*));
  MOCK_METHOD0(GetApproximateCurrentTime, base::TimeDelta());

 private:
  media::MediaController* media_controller_;
};

class FlingingRendererTest : public testing::Test {
 public:
  FlingingRendererTest()
      : media_controller_(new StrictMock<MockMediaController>()),
        flinging_controller_(
            new StrictMock<MockFlingingController>(media_controller_.get())) {
    EXPECT_CALL(*flinging_controller_, AddMediaStatusObserver(_));
    EXPECT_CALL(*flinging_controller_, RemoveMediaStatusObserver(_));

    renderer_ = base::WrapUnique(new FlingingRenderer(
        std::unique_ptr<media::FlingingController>(flinging_controller_),
        mojo::NullRemote()));

    renderer_->Initialize(nullptr, &renderer_client_, base::DoNothing());
  }

 protected:
  NiceMock<media::MockRendererClient> renderer_client_;
  std::unique_ptr<MockMediaController> media_controller_;
  StrictMock<MockFlingingController>* flinging_controller_;
  std::unique_ptr<FlingingRenderer> renderer_;
};

TEST_F(FlingingRendererTest, StartPlayingFromTime) {
  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(10);
  EXPECT_CALL(*media_controller_, Seek(seek_time));

  renderer_->StartPlayingFrom(seek_time);
}

TEST_F(FlingingRendererTest, StartPlayingFromBeginning) {
  EXPECT_CALL(*media_controller_, Seek(base::TimeDelta()));

  renderer_->StartPlayingFrom(base::TimeDelta());
}

TEST_F(FlingingRendererTest, SetPlaybackRate) {
  double playback_rate = 1.0;
  EXPECT_CALL(*media_controller_, Play());

  renderer_->SetPlaybackRate(playback_rate);
}

TEST_F(FlingingRendererTest, SetPlaybackRateToZero) {
  double playback_rate = 0.0;
  EXPECT_CALL(*media_controller_, Pause());

  renderer_->SetPlaybackRate(playback_rate);
}

// Setting the volume to a positive value should not change the mute state.
TEST_F(FlingingRendererTest, SetVolume) {
  float volume = 0.5;
  EXPECT_CALL(*media_controller_, SetVolume(volume));
  EXPECT_CALL(*media_controller_, SetMute(_)).Times(0);

  renderer_->SetVolume(volume);
}

// Setting the volume to 0 should not set the mute state.
TEST_F(FlingingRendererTest, SetVolumeToZero) {
  float volume = 0;
  EXPECT_CALL(*media_controller_, SetVolume(volume));
  EXPECT_CALL(*media_controller_, SetMute(_)).Times(0);

  renderer_->SetVolume(volume);
}

}  // namespace content
