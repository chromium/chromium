// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_bridge.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

using testing::_;
using testing::StrictMock;

class MockMediaPlayerBridgeClient : public MediaPlayerBridge::Client {
 public:
  MOCK_METHOD0(GetMediaResourceGetter, MediaResourceGetter*());
  MOCK_METHOD0(GetMediaUrlInterceptor, MediaUrlInterceptor*());
  MOCK_METHOD1(OnMediaDurationChanged, void(base::TimeDelta duration));
  MOCK_METHOD0(OnPlaybackComplete, void());
  MOCK_METHOD1(OnError, void(int error));
  MOCK_METHOD2(OnVideoSizeChanged, void(int width, int height));
};

}  // anonymous namespace

class MediaPlayerBridgeTest : public testing::Test {
 public:
  MediaPlayerBridgeTest()
      : bridge_(GURL(),
                GURL(),
                url::Origin(),
                "",
                false,
                &client_,
                false,
                false) {}

 protected:
  void SimulateDurationChange(base::TimeDelta duration) {
    bridge_.PropagateDuration(duration);
  }

  void SimulateVideoSizeChanged(int width, int height) {
    bridge_.OnVideoSizeChanged(width, height);
  }

  void SimulateError(int error) { bridge_.OnMediaError(error); }

  void SimulatePlaybackCompleted() { bridge_.OnPlaybackComplete(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<MockMediaPlayerBridgeClient> client_;
  MediaPlayerBridge bridge_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerBridgeTest);
};

TEST_F(MediaPlayerBridgeTest, Client_OnMediaMetadataChanged) {
  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(20);

  EXPECT_CALL(client_, OnMediaDurationChanged(kDuration));

  SimulateDurationChange(kDuration);
}

TEST_F(MediaPlayerBridgeTest, Client_OnVideoSizeChanged) {
  const int kWidth = 1600;
  const int kHeight = 900;

  EXPECT_CALL(client_, OnVideoSizeChanged(kWidth, kHeight));

  SimulateVideoSizeChanged(kWidth, kHeight);
}

TEST_F(MediaPlayerBridgeTest, Client_OnPlaybackComplete) {
  EXPECT_CALL(client_, OnPlaybackComplete());

  SimulatePlaybackCompleted();
}

TEST_F(MediaPlayerBridgeTest, Client_OnError) {
  // MEDIA_ERROR_INVALID_CODE should still be propagated.
  EXPECT_CALL(client_, OnError(_)).Times(1);
  SimulateError(MediaPlayerBridge::MediaErrorType::MEDIA_ERROR_INVALID_CODE);

  EXPECT_CALL(client_, OnError(_)).Times(1);
  SimulateError(MediaPlayerBridge::MediaErrorType::MEDIA_ERROR_FORMAT);
}

}  // namespace media
