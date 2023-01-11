// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_track_source.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using testing::Field;
using testing::InSequence;
using testing::Property;
using webrtc::BasicDesktopFrame;
using webrtc::DesktopSize;
using webrtc::VideoFrame;

namespace remoting::protocol {

namespace {

class MockVideoSink : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  ~MockVideoSink() override = default;

  MOCK_METHOD(void, OnFrame, (const VideoFrame& frame), (override));
};

}  // namespace

class WebrtcVideoTrackSourceTest : public testing::Test {
 public:
  WebrtcVideoTrackSourceTest() = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockVideoSink video_sink_;
  base::MockCallback<WebrtcVideoTrackSource::AddSinkCallback>
      add_sink_callback_;
};

TEST_F(WebrtcVideoTrackSourceTest, AddSinkTriggersCallback) {
  EXPECT_CALL(add_sink_callback_,
              Run(Field(&rtc::VideoSinkWants::max_framerate_fps, 123)));

  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source(
      new rtc::RefCountedObject<WebrtcVideoTrackSource>(
          add_sink_callback_.Get()));
  rtc::VideoSinkWants wants;
  wants.max_framerate_fps = 123;
  source->AddOrUpdateSink(&video_sink_, wants);

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(WebrtcVideoTrackSourceTest, CapturedFrameSentToAddedSink) {
  auto frame = std::make_unique<BasicDesktopFrame>(DesktopSize(123, 234));
  EXPECT_CALL(video_sink_, OnFrame(Property(&VideoFrame::width, 123)));

  rtc::scoped_refptr<WebrtcVideoTrackSource> source(
      new rtc::RefCountedObject<WebrtcVideoTrackSource>(
          add_sink_callback_.Get()));
  source->AddOrUpdateSink(&video_sink_, rtc::VideoSinkWants());
  source->SendCapturedFrame(std::move(frame), nullptr);

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(WebrtcVideoTrackSourceTest, FramesHaveIncrementingIds) {
  {
    InSequence s;
    EXPECT_CALL(video_sink_, OnFrame(Property(&VideoFrame::id, 0)));
    EXPECT_CALL(video_sink_, OnFrame(Property(&VideoFrame::id, 1)));
    EXPECT_CALL(video_sink_, OnFrame(Property(&VideoFrame::id, 2)));
  }

  rtc::scoped_refptr<WebrtcVideoTrackSource> source(
      new rtc::RefCountedObject<WebrtcVideoTrackSource>(
          add_sink_callback_.Get()));
  source->AddOrUpdateSink(&video_sink_, rtc::VideoSinkWants());
  source->SendCapturedFrame(
      std::make_unique<BasicDesktopFrame>(DesktopSize(100, 100)), nullptr);
  source->SendCapturedFrame(
      std::make_unique<BasicDesktopFrame>(DesktopSize(100, 100)), nullptr);
  source->SendCapturedFrame(
      std::make_unique<BasicDesktopFrame>(DesktopSize(100, 100)), nullptr);

  task_environment_.FastForwardUntilNoTasksRemain();
}

}  // namespace remoting::protocol
