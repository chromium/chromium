// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_frame_mojom_traits.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/mojom/traits_test_service.mojom.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_LINUX)
#include <fcntl.h>
#include <sys/stat.h>
#endif

namespace media {

namespace {

class VideoFrameStructTraitsTest : public testing::Test,
                                   public media::mojom::TraitsTestService {
 public:
  VideoFrameStructTraitsTest() = default;

 protected:
  mojo::Remote<mojom::TraitsTestService> GetTraitsTestRemote() {
    mojo::Remote<mojom::TraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  bool RoundTrip(scoped_refptr<VideoFrame>* frame) {
    scoped_refptr<VideoFrame> input = std::move(*frame);
    mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
    return remote->EchoVideoFrame(std::move(input), frame);
  }

 private:
  void EchoVideoFrame(const scoped_refptr<VideoFrame>& f,
                      EchoVideoFrameCallback callback) override {
    std::move(callback).Run(f);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<TraitsTestService> traits_test_receivers_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameStructTraitsTest);
};

}  // namespace

TEST_F(VideoFrameStructTraitsTest, Null) {
  scoped_refptr<VideoFrame> frame;

  ASSERT_TRUE(RoundTrip(&frame));
  EXPECT_FALSE(frame);
}

TEST_F(VideoFrameStructTraitsTest, EOS) {
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateEOSFrame();

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_TRUE(frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
}

TEST_F(VideoFrameStructTraitsTest, MojoSharedBufferVideoFrame) {
  scoped_refptr<VideoFrame> frame =
      MojoSharedBufferVideoFrame::CreateDefaultI420ForTesting(
          gfx::Size(100, 100), base::TimeDelta::FromSeconds(100));
  frame->metadata()->SetDouble(VideoFrameMetadata::FRAME_RATE, 42.0);

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_FALSE(frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
  double frame_rate = 0.0;
  EXPECT_TRUE(frame->metadata()->GetDouble(VideoFrameMetadata::FRAME_RATE,
                                           &frame_rate));
  EXPECT_EQ(frame_rate, 42.0);
  EXPECT_EQ(frame->coded_size(), gfx::Size(100, 100));
  EXPECT_EQ(frame->timestamp(), base::TimeDelta::FromSeconds(100));

  ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_MOJO_SHARED_BUFFER);
  MojoSharedBufferVideoFrame* mojo_shared_buffer_frame =
      static_cast<MojoSharedBufferVideoFrame*>(frame.get());
  EXPECT_TRUE(mojo_shared_buffer_frame->Handle().is_valid());
}

#if defined(OS_LINUX)
TEST_F(VideoFrameStructTraitsTest, DmabufVideoFrame) {
  const size_t num_planes = media::VideoFrame::NumPlanes(PIXEL_FORMAT_NV12);
  std::vector<int> strides = {1280, 1280};
  std::vector<size_t> sizes = {1280 * 720, 1280 * 720 / 2};
  auto layout = media::VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_NV12, gfx::Size(1280, 720),
      {media::ColorPlaneLayout(strides[0], 0, sizes[0]),
       media::ColorPlaneLayout(strides[1], 0, sizes[1])});

  // DMABUF needs device to create, use file fd instead.
  std::vector<int> fake_fds = {open("/dev/null", O_RDWR),
                               open("/dev/zero", O_RDWR)};
  std::vector<base::ScopedFD> dmabuf_fds;
  dmabuf_fds.reserve(num_planes);
  for (size_t i = 0; i < num_planes; i++)
    dmabuf_fds.emplace_back(fake_fds[i]);

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalDmabufs(
      *layout, gfx::Rect(0, 0, 1280, 720), gfx::Size(1280, 720),
      std::move(dmabuf_fds), base::TimeDelta::FromSeconds(100));

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_FALSE(frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
  EXPECT_EQ(frame->format(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(frame->coded_size(), gfx::Size(1280, 720));
  EXPECT_EQ(frame->visible_rect(), gfx::Rect(0, 0, 1280, 720));
  EXPECT_EQ(frame->natural_size(), gfx::Size(1280, 720));
  EXPECT_EQ(frame->timestamp(), base::TimeDelta::FromSeconds(100));
  ASSERT_TRUE(frame->HasDmaBufs());
  ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_DMABUFS);
}
#endif

TEST_F(VideoFrameStructTraitsTest, MailboxVideoFrame) {
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  gpu::MailboxHolder mailbox_holder[VideoFrame::kMaxPlanes];
  mailbox_holder[0] = gpu::MailboxHolder(mailbox, gpu::SyncToken(), 0);
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_ARGB, mailbox_holder, VideoFrame::ReleaseMailboxCB(),
      gfx::Size(100, 100), gfx::Rect(10, 10, 80, 80), gfx::Size(200, 100),
      base::TimeDelta::FromSeconds(100));

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_FALSE(frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
  EXPECT_EQ(frame->format(), PIXEL_FORMAT_ARGB);
  EXPECT_EQ(frame->coded_size(), gfx::Size(100, 100));
  EXPECT_EQ(frame->visible_rect(), gfx::Rect(10, 10, 80, 80));
  EXPECT_EQ(frame->natural_size(), gfx::Size(200, 100));
  EXPECT_EQ(frame->timestamp(), base::TimeDelta::FromSeconds(100));
  ASSERT_TRUE(frame->HasTextures());
  ASSERT_EQ(frame->mailbox_holder(0).mailbox, mailbox);
}

// defined(OS_LINUX) because media::FakeGpuMemoryBuffer supports
// NativePixmapHandle backed GpuMemoryBufferHandle only.
// !defined(USE_OZONE) so as to force GpuMemoryBufferSupport to select
// gfx::ClientNativePixmapFactoryDmabuf for gfx::ClientNativePixmapFactory.
#if defined(OS_LINUX) && !defined(USE_OZONE)
TEST_F(VideoFrameStructTraitsTest, GpuMemoryBufferVideoFrame) {
  gfx::Size coded_size = gfx::Size(256, 256);
  gfx::Rect visible_rect(coded_size);
  auto timestamp = base::TimeDelta::FromMilliseconds(1);
  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      std::make_unique<FakeGpuMemoryBuffer>(
          coded_size, gfx::BufferFormat::YUV_420_BIPLANAR);
  gfx::BufferFormat expected_gmb_format = gmb->GetFormat();
  gfx::Size expected_gmb_size = gmb->GetSize();
  gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes] = {
      gpu::MailboxHolder(gpu::Mailbox::Generate(), gpu::SyncToken(), 5),
      gpu::MailboxHolder(gpu::Mailbox::Generate(), gpu::SyncToken(), 10)};
  auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      visible_rect, visible_rect.size(), std::move(gmb), mailbox_holders,
      base::DoNothing::Once<const gpu::SyncToken&>(), timestamp);
  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  EXPECT_TRUE(frame->HasGpuMemoryBuffer());
  EXPECT_FALSE(frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
  EXPECT_EQ(frame->format(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->natural_size(), visible_rect.size());
  EXPECT_EQ(frame->timestamp(), timestamp);
  ASSERT_TRUE(frame->HasTextures());
  EXPECT_EQ(frame->mailbox_holder(0).mailbox, mailbox_holders[0].mailbox);
  EXPECT_EQ(frame->mailbox_holder(1).mailbox, mailbox_holders[1].mailbox);
  EXPECT_EQ(frame->GetGpuMemoryBuffer()->GetFormat(), expected_gmb_format);
  EXPECT_EQ(frame->GetGpuMemoryBuffer()->GetSize(), expected_gmb_size);
}
#endif  // defined(OS_LINUX) && !defined(USE_OZONE)
}  // namespace media
