// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_renderer_adapter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "remoting/protocol/fake_video_renderer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/make_ref_counted.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/test/mock_media_stream_interface.h"
#include "third_party/webrtc/api/test/mock_video_track.h"

using testing::_;
using testing::Return;

namespace remoting::protocol {

class WebrtcVideoRendererAdapterTest : public testing::Test {
 public:
  WebrtcVideoRendererAdapterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeVideoRenderer video_renderer_;
};

TEST_F(WebrtcVideoRendererAdapterTest, DanglingSinkAfterTrackSwap) {
  std::string label = "test_stream";
  auto adapter =
      std::make_unique<WebrtcVideoRendererAdapter>(label, &video_renderer_);

  auto track0 = webrtc::MockVideoTrack::Create();
  auto track1 = webrtc::MockVideoTrack::Create();
  auto stream = webrtc::make_ref_counted<webrtc::MockMediaStream>();

  EXPECT_CALL(*stream, id()).WillRepeatedly(Return(label));

  // Initial tracks: [track0]
  EXPECT_CALL(*stream, GetVideoTracks())
      .WillRepeatedly(Return(
          std::vector<webrtc::scoped_refptr<webrtc::VideoTrackInterface>>{
              track0}));

  // Expect registration on track0
  EXPECT_CALL(*track0, AddOrUpdateSink(adapter.get(), _)).Times(1);

  adapter->SetMediaStream(stream);

  // Swap tracks: [track1]
  EXPECT_CALL(*stream, GetVideoTracks())
      .WillRepeatedly(Return(
          std::vector<webrtc::scoped_refptr<webrtc::VideoTrackInterface>>{
              track1}));

  // On destruction, the code should unregister from the ORIGINAL track
  // (track0).
  EXPECT_CALL(*track1, RemoveSink(adapter.get())).Times(0);
  EXPECT_CALL(*track0, RemoveSink(adapter.get())).Times(1);

  adapter.reset();
}

TEST_F(WebrtcVideoRendererAdapterTest, DanglingSinkAfterSetMediaStreamReplace) {
  std::string label = "test_stream";
  auto adapter =
      std::make_unique<WebrtcVideoRendererAdapter>(label, &video_renderer_);

  auto track0 = webrtc::MockVideoTrack::Create();
  auto stream0 = webrtc::make_ref_counted<webrtc::MockMediaStream>();
  EXPECT_CALL(*stream0, id()).WillRepeatedly(Return(label));
  EXPECT_CALL(*stream0, GetVideoTracks())
      .WillRepeatedly(Return(
          std::vector<webrtc::scoped_refptr<webrtc::VideoTrackInterface>>{
              track0}));

  auto track1 = webrtc::MockVideoTrack::Create();
  auto stream1 = webrtc::make_ref_counted<webrtc::MockMediaStream>();
  EXPECT_CALL(*stream1, id()).WillRepeatedly(Return(label));
  EXPECT_CALL(*stream1, GetVideoTracks())
      .WillRepeatedly(Return(
          std::vector<webrtc::scoped_refptr<webrtc::VideoTrackInterface>>{
              track1}));

  // First SetMediaStream
  EXPECT_CALL(*track0, AddOrUpdateSink(adapter.get(), _)).Times(1);
  adapter->SetMediaStream(stream0);

  // Second SetMediaStream. The code should unregister from track0 before
  // registering on track1.
  EXPECT_CALL(*track0, RemoveSink(adapter.get())).Times(1);
  EXPECT_CALL(*track1, AddOrUpdateSink(adapter.get(), _)).Times(1);
  adapter->SetMediaStream(stream1);

  // On destruction, it will unregister from track1.
  EXPECT_CALL(*track1, RemoveSink(adapter.get())).Times(1);
  adapter.reset();
}

}  // namespace remoting::protocol
