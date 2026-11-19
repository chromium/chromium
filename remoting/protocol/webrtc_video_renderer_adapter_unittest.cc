// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_renderer_adapter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "remoting/codec/utils.h"
#include "remoting/protocol/fake_video_renderer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/webrtc/api/make_ref_counted.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/test/mock_media_stream_interface.h"
#include "third_party/webrtc/api/test/mock_video_track.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/api/video/video_common.h"
#include "third_party/webrtc/api/video/video_frame.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

using testing::_;
using testing::Return;

namespace remoting::protocol {

namespace {

class NullI420VideoFrameBuffer : public webrtc::VideoFrameBuffer {
 public:
  NullI420VideoFrameBuffer(int width,
                           int height,
                           base::OnceClosure on_to_i420_called)
      : width_(width),
        height_(height),
        on_to_i420_called_(std::move(on_to_i420_called)) {}

  Type type() const override { return Type::kNative; }
  int width() const override { return width_; }
  int height() const override { return height_; }
  webrtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
    if (on_to_i420_called_) {
      std::move(on_to_i420_called_).Run();
    }
    return nullptr;
  }

 private:
  const int width_;
  const int height_;
  base::OnceClosure on_to_i420_called_;
};

webrtc::scoped_refptr<webrtc::I420Buffer> CreatePatternI420Buffer(int width,
                                                                  int height) {
  const int uv_width = (width + 1) / 2;
  const int uv_height = (height + 1) / 2;
  std::vector<uint8_t> y_plane(static_cast<size_t>(width) * height);
  std::vector<uint8_t> u_plane(static_cast<size_t>(uv_width) * uv_height);
  std::vector<uint8_t> v_plane(static_cast<size_t>(uv_width) * uv_height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      y_plane[static_cast<size_t>(y * width + x)] =
          static_cast<uint8_t>(16 + ((x * 5 + y * 9) % 210));
    }
  }
  for (int y = 0; y < uv_height; ++y) {
    for (int x = 0; x < uv_width; ++x) {
      u_plane[static_cast<size_t>(y * uv_width + x)] =
          static_cast<uint8_t>(32 + ((x * 11 + y * 7) % 192));
      v_plane[static_cast<size_t>(y * uv_width + x)] =
          static_cast<uint8_t>(32 + ((x * 13 + y * 3) % 192));
    }
  }
  return webrtc::I420Buffer::Copy(width, height, y_plane.data(), width,
                                  u_plane.data(), uv_width, v_plane.data(),
                                  uv_width);
}

bool AreDesktopFramesEqual(const webrtc::DesktopFrame& a,
                           const webrtc::DesktopFrame& b) {
  if (!a.size().equals(b.size())) {
    return false;
  }
  const size_t row_bytes = static_cast<size_t>(a.size().width()) *
                           webrtc::DesktopFrame::kBytesPerPixel;
  for (int y = 0; y < a.size().height(); ++y) {
    const webrtc::DesktopVector row_pos(0, y);
    // SAFETY: `DesktopFrame` guarantees each row `[0, height)` has at least
    // `width * kBytesPerPixel` readable bytes starting at `GetFrameDataAtPos`.
    const auto row_a =
        UNSAFE_BUFFERS(base::span(a.GetFrameDataAtPos(row_pos), row_bytes));
    const auto row_b =
        UNSAFE_BUFFERS(base::span(b.GetFrameDataAtPos(row_pos), row_bytes));
    if (row_a != row_b) {
      return false;
    }
  }
  return true;
}

}  // namespace

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

TEST_F(WebrtcVideoRendererAdapterTest,
       OnFrame_ConvertsBelowThresholdI420ToRgba) {
  auto adapter = std::make_unique<WebrtcVideoRendererAdapter>("test_stream",
                                                              &video_renderer_);

  video_renderer_.GetFrameConsumer()->set_pixel_format(
      FrameConsumer::FORMAT_RGBA);
  webrtc::scoped_refptr<webrtc::I420Buffer> small_i420 =
      CreatePatternI420Buffer(32, 24);
  base::RunLoop run_loop;
  video_renderer_.GetFrameConsumer()->set_on_frame_callback(
      run_loop.QuitClosure());
  adapter->OnFrame(webrtc::VideoFrame::Builder()
                       .set_video_frame_buffer(small_i420)
                       .set_timestamp_us(0)
                       .set_rtp_timestamp(1000)
                       .build());
  run_loop.Run();

  ASSERT_EQ(video_renderer_.GetFrameConsumer()->received_frames().size(), 1u);
  const webrtc::DesktopFrame& rgba_frame =
      *video_renderer_.GetFrameConsumer()->received_frames().back();
  webrtc::BasicDesktopFrame expected_rgba(webrtc::DesktopSize(32, 24),
                                          webrtc::FOURCC_ARGB);
  libyuv::I420ToABGR(small_i420->DataY(), small_i420->StrideY(),
                     small_i420->DataU(), small_i420->StrideU(),
                     small_i420->DataV(), small_i420->StrideV(),
                     expected_rgba.data(), expected_rgba.stride(), 32, 24);
  EXPECT_TRUE(AreDesktopFramesEqual(rgba_frame, expected_rgba));
  EXPECT_TRUE(rgba_frame.updated_region().Equals(
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeWH(32, 24))));
}

TEST_F(WebrtcVideoRendererAdapterTest, OnFrame_ConvertsParallelI420ToBgra) {
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);
  auto adapter = std::make_unique<WebrtcVideoRendererAdapter>("test_stream",
                                                              &video_renderer_);

  video_renderer_.GetFrameConsumer()->set_pixel_format(
      FrameConsumer::FORMAT_BGRA);
  webrtc::scoped_refptr<webrtc::I420Buffer> large_i420 =
      CreatePatternI420Buffer(1024, 542);
  base::RunLoop run_loop;
  video_renderer_.GetFrameConsumer()->set_on_frame_callback(
      run_loop.QuitClosure());
  adapter->OnFrame(webrtc::VideoFrame::Builder()
                       .set_video_frame_buffer(large_i420)
                       .set_timestamp_us(0)
                       .set_rtp_timestamp(2000)
                       .build());
  run_loop.Run();

  ASSERT_EQ(video_renderer_.GetFrameConsumer()->received_frames().size(), 1u);
  const webrtc::DesktopFrame& bgra_frame =
      *video_renderer_.GetFrameConsumer()->received_frames().back();
  webrtc::BasicDesktopFrame expected_bgra(webrtc::DesktopSize(1024, 542),
                                          webrtc::FOURCC_ARGB);
  libyuv::I420ToARGB(large_i420->DataY(), large_i420->StrideY(),
                     large_i420->DataU(), large_i420->StrideU(),
                     large_i420->DataV(), large_i420->StrideV(),
                     expected_bgra.data(), expected_bgra.stride(), 1024, 542);
  EXPECT_TRUE(AreDesktopFramesEqual(bgra_frame, expected_bgra));
  EXPECT_TRUE(bgra_frame.updated_region().Equals(
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeWH(1024, 542))));
}

TEST_F(WebrtcVideoRendererAdapterTest,
       OnFrame_PreservesOrderWhenLargeFramePrecedesSmallFrame) {
  ScopedParallelConversionCpuCapForTesting force_parallel(/*max_workers=*/4);
  auto adapter = std::make_unique<WebrtcVideoRendererAdapter>("test_stream",
                                                              &video_renderer_);

  webrtc::scoped_refptr<webrtc::I420Buffer> large_i420 =
      CreatePatternI420Buffer(1024, 542);
  webrtc::scoped_refptr<webrtc::I420Buffer> small_i420 =
      CreatePatternI420Buffer(32, 24);

  base::RunLoop run_loop;
  video_renderer_.GetFrameConsumer()->set_on_frame_callback(
      base::BarrierClosure(2, run_loop.QuitClosure()));
  adapter->OnFrame(webrtc::VideoFrame::Builder()
                       .set_video_frame_buffer(large_i420)
                       .set_timestamp_us(0)
                       .set_rtp_timestamp(2000)
                       .build());
  adapter->OnFrame(webrtc::VideoFrame::Builder()
                       .set_video_frame_buffer(small_i420)
                       .set_timestamp_us(0)
                       .set_rtp_timestamp(2001)
                       .build());
  run_loop.Run();

  const auto& received = video_renderer_.GetFrameConsumer()->received_frames();
  ASSERT_EQ(received.size(), 2u);
  EXPECT_TRUE(received.front()->size().equals(webrtc::DesktopSize(1024, 542)));
  EXPECT_TRUE(received.back()->size().equals(webrtc::DesktopSize(32, 24)));
}

TEST_F(WebrtcVideoRendererAdapterTest,
       OnFrame_DropsFrameWhenToI420ReturnsNull) {
  auto adapter = std::make_unique<WebrtcVideoRendererAdapter>("test_stream",
                                                              &video_renderer_);

  base::RunLoop to_i420_loop;
  auto null_buffer = webrtc::make_ref_counted<NullI420VideoFrameBuffer>(
      32, 24, to_i420_loop.QuitClosure());
  adapter->OnFrame(webrtc::VideoFrame::Builder()
                       .set_video_frame_buffer(null_buffer)
                       .set_timestamp_us(0)
                       .set_rtp_timestamp(4000)
                       .build());
  to_i420_loop.Run();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  base::RunLoop reply_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, reply_loop.QuitClosure());
  reply_loop.Run();

  EXPECT_TRUE(video_renderer_.GetFrameConsumer()->received_frames().empty());
}

}  // namespace remoting::protocol
