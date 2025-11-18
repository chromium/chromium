// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/mojom/video_frame_mojom_traits.h"

#include <algorithm>
#include <array>

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/mojo/mojom/traits_test_service.test-mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <linux/kcmp.h>
#include <sys/syscall.h>

#include "base/posix/eintr_wrapper.h"
#include "base/process/process.h"
#include "media/mojo/mojom/buffer_handle_test_util.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace media {

namespace {

class VideoFrameStructTraitsTest : public testing::Test,
                                   public media::mojom::TraitsTestService {
 public:
  VideoFrameStructTraitsTest() = default;

  VideoFrameStructTraitsTest(const VideoFrameStructTraitsTest&) = delete;
  VideoFrameStructTraitsTest& operator=(const VideoFrameStructTraitsTest&) =
      delete;

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

  bool RoundTripFails(scoped_refptr<VideoFrame> frame) {
    // For a negative round trip test, the function will reduce the number of
    // error logs when compared to RoundTrip. This makes the test output read
    // cleaner.
    auto message = mojom::VideoFrame::SerializeAsMessage(&frame);

    // Required to pass base deserialize checks.
    mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
    message = mojo::Message::CreateFromMessageHandle(&handle);

    // Ensure deserialization fails instead of crashing.
    scoped_refptr<VideoFrame> new_frame;
    return false == mojom::VideoFrame::DeserializeFromMessage(
                        std::move(message), &new_frame);
  }

 private:
  void EchoVideoFrame(const scoped_refptr<VideoFrame>& f,
                      EchoVideoFrameCallback callback) override {
    // Touch all data in the received frame to ensure that it is valid.
    if (f && f->IsMappable()) {
      VideoFrame::HexHashOfFrameForTesting(*f);
    }

    std::move(callback).Run(f);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<TraitsTestService> traits_test_receivers_;
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
  EXPECT_TRUE(frame->metadata().end_of_stream);
}

TEST_F(VideoFrameStructTraitsTest, MappableVideoFrame) {
  constexpr VideoFrame::StorageType storage_types[] = {
      VideoFrame::STORAGE_SHMEM,
      VideoFrame::STORAGE_OWNED_MEMORY,
      VideoFrame::STORAGE_UNOWNED_MEMORY,
  };
  constexpr VideoPixelFormat formats[] = {
      PIXEL_FORMAT_I420, PIXEL_FORMAT_NV12, PIXEL_FORMAT_XRGB,
      PIXEL_FORMAT_ARGB, PIXEL_FORMAT_XBGR, PIXEL_FORMAT_ABGR,
  };
  constexpr gfx::Size kCodedSize(100, 100);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr double kFrameRate = 42.0;
  constexpr base::TimeDelta kTimestamp = base::Seconds(100);
  for (auto format : formats) {
    for (auto storage_type : storage_types) {
      scoped_refptr<media::VideoFrame> frame;
      base::MappedReadOnlyRegion region;
      if (storage_type == VideoFrame::STORAGE_OWNED_MEMORY) {
        frame = media::VideoFrame::CreateFrame(format, kCodedSize, kVisibleRect,
                                               kNaturalSize, kTimestamp);
      } else {
        std::vector<size_t> strides =
            VideoFrame::ComputeStrides(format, kCodedSize);
        size_t aggregate_size = 0;
        std::array<size_t, 3> sizes = {};
        for (size_t i = 0; i < strides.size(); ++i) {
          sizes[i] = media::VideoFrame::Rows(i, format, kCodedSize.height()) *
                     strides[i];
          aggregate_size += sizes[i];
        }
        region = base::ReadOnlySharedMemoryRegion::Create(aggregate_size);
        ASSERT_TRUE(region.IsValid());

        std::array<base::span<uint8_t>, 3> data = {};
        auto mapping_span = region.mapping.GetMemoryAsSpan<uint8_t>();
        size_t offset = 0;
        for (size_t i = 0; i < strides.size(); ++i) {
          data[i] = mapping_span.subspan(offset, sizes[i]);
          offset += sizes[i];
        }

        if (format == PIXEL_FORMAT_I420) {
          frame = media::VideoFrame::WrapExternalYuvData(
              format, kCodedSize, kVisibleRect, kNaturalSize, strides[0],
              strides[1], strides[2], data[0], data[1], data[2], kTimestamp);
        } else if (format == PIXEL_FORMAT_NV12) {
          frame = media::VideoFrame::WrapExternalYuvData(
              format, kCodedSize, kVisibleRect, kNaturalSize, strides[0],
              strides[1], data[0], data[1], kTimestamp);
        } else {
          ASSERT_TRUE(media::IsRGB(format));
          frame = media::VideoFrame::WrapExternalData(
              format, kCodedSize, kVisibleRect, kNaturalSize, data[0],
              kTimestamp);
        }
        if (storage_type == VideoFrame::STORAGE_SHMEM)
          frame->BackWithSharedMemory(&region.region);
      }

      ASSERT_TRUE(frame);
      frame->metadata().frame_rate = kFrameRate;
      ASSERT_EQ(frame->storage_type(), storage_type);
      ASSERT_TRUE(RoundTrip(&frame));
      ASSERT_TRUE(frame);
      EXPECT_FALSE(frame->metadata().end_of_stream);
      EXPECT_EQ(frame->format(), format);
      EXPECT_EQ(*frame->metadata().frame_rate, kFrameRate);
      EXPECT_EQ(frame->coded_size(), kCodedSize);
      EXPECT_EQ(frame->visible_rect(), kVisibleRect);
      EXPECT_EQ(frame->natural_size(), kNaturalSize);
      EXPECT_EQ(frame->timestamp(), kTimestamp);
      ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_SHMEM);
      EXPECT_TRUE(frame->shm_region()->IsValid());
    }
  }
}

TEST_F(VideoFrameStructTraitsTest, InterleavedPlanes) {
  constexpr VideoFrame::StorageType storage_type = VideoFrame::STORAGE_SHMEM;
  constexpr VideoPixelFormat format = PIXEL_FORMAT_I420;
  constexpr gfx::Size kCodedSize(100, 100);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr base::TimeDelta kTimestamp;

  scoped_refptr<media::VideoFrame> frame;

  std::vector<size_t> strides = VideoFrame::ComputeStrides(format, kCodedSize);
  ASSERT_EQ(strides[1], strides[2]);

  size_t aggregate_size = 0;
  std::array<size_t, 3> sizes = {};
  for (size_t i = 0; i < strides.size(); ++i) {
    sizes[i] =
        media::VideoFrame::Rows(i, format, kCodedSize.height()) * strides[i];
    aggregate_size += sizes[i];
  }
  auto region = base::WritableSharedMemoryRegion::Create(aggregate_size);
  ASSERT_TRUE(region.IsValid());
  auto mapping = region.MapAt(0, aggregate_size);

  auto [y_plane, uv_plane] =
      mapping.GetMemoryAsSpan<uint8_t>().split_at(sizes[0]);
  std::ranges::fill(y_plane, 1);

  // Setup memory layout where U and V planes occupy the same space, but have
  // interleaving U and V rows. This is achieved by doubling the stride.
  size_t normal_stride = strides[1];
  size_t uv_stride = normal_stride * 2;

  int yu_rows = media::VideoFrame::Rows(1, format, kCodedSize.height());
  auto uv_plane2 = uv_plane;  // Loop below is destructive.
  for (int i = 0; i < yu_rows; ++i) {
    const auto [u, v] = uv_plane2.take_first(uv_stride).split_at(normal_stride);
    std::ranges::fill(u, 2);
    std::ranges::fill(v, 3);
  }

  frame = media::VideoFrame::WrapExternalYuvData(
      format, kCodedSize, kVisibleRect, kNaturalSize, strides[0], uv_stride,
      uv_stride, y_plane, uv_plane, uv_plane.subspan(normal_stride),
      kTimestamp);
  auto ro_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
  frame->BackWithSharedMemory(&ro_region);

  EXPECT_TRUE(frame);
  EXPECT_EQ(frame->storage_type(), storage_type);
  EXPECT_TRUE(RoundTrip(&frame));
  EXPECT_TRUE(frame);
  EXPECT_EQ(frame->format(), format);
  EXPECT_EQ(frame->coded_size(), kCodedSize);

  auto plane_1 = frame->GetVisiblePlaneData(1);
  auto plane_2 = frame->GetVisiblePlaneData(2);
  // Bytes between the visible edge and the full stride are not considered part
  // of the visible plane, and may not be accessible through the above spans.
  const size_t row_bytes_1 =
      VideoFrame::RowBytes(1, format, kCodedSize.width());
  const size_t row_bytes_2 =
      VideoFrame::RowBytes(2, format, kCodedSize.width());
  for (int i = 0; i < yu_rows; ++i) {
    const auto [u, v] = uv_plane.take_first(uv_stride).split_at(normal_stride);
    EXPECT_EQ(plane_1.subspan(i * frame->stride(1), row_bytes_1),
              u.first(row_bytes_1));
    EXPECT_EQ(plane_2.subspan(i * frame->stride(2), row_bytes_2),
              v.first(row_bytes_2));
  }
}

TEST_F(VideoFrameStructTraitsTest, InvalidOffsets) {
  constexpr auto kFormat = PIXEL_FORMAT_I420;

  // This test works by patching the outgoing mojo message, so choose a size
  // that's two primes to try and maximize the uniqueness of the values we're
  // scanning for in the message.
  constexpr gfx::Size kSize(127, 149);

  auto strides = VideoFrame::ComputeStrides(kFormat, kSize);
  size_t aggregate_size = 0;
  std::array<size_t, 3> sizes = {};
  for (size_t i = 0; i < strides.size(); ++i) {
    sizes[i] = VideoFrame::Rows(i, kFormat, kSize.height()) * strides[i];
    aggregate_size += sizes[i];
  }

  auto region = base::ReadOnlySharedMemoryRegion::Create(aggregate_size);
  ASSERT_TRUE(region.IsValid());

  std::array<base::span<uint8_t>, 3> data = {};
  std::vector<size_t> offsets;
  auto mapping_span = region.mapping.GetMemoryAsSpan<uint8_t>();
  size_t offset = 0;
  for (size_t i = 0; i < strides.size(); ++i) {
    offsets.push_back(offset);
    data[i] = mapping_span.subspan(offset, sizes[i]);
    offset += sizes[i];
  }

  auto frame = VideoFrame::WrapExternalYuvData(
      kFormat, kSize, gfx::Rect(kSize), kSize, strides[0], strides[1],
      strides[2], data[0], data[1], data[2], base::Seconds(1));
  ASSERT_TRUE(frame);

  frame->BackWithSharedMemory(&region.region);

  auto message = mojom::VideoFrame::SerializeAsMessage(&frame);

  // Scan for the offsets array in the message body. It will start with an
  // array header and then have the three offsets matching our frame.
  base::span<uint32_t> body(
      reinterpret_cast<uint32_t*>(message.mutable_payload()),
      message.payload_num_bytes() / sizeof(uint32_t));

  bool patched_offsets = false;
  for (size_t i = 0; i + 3 < body.size(); ++i) {
    if (body[i] == offsets[0] && body[i + 1] == offsets[1] &&
        body[i + 2] == offsets[2]) {
      body[i + 1] = 0xc01db33f;
      patched_offsets = true;
      break;
    }
  }
  ASSERT_TRUE(patched_offsets);

  // Required to pass base deserialize checks.
  mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
  message = mojo::Message::CreateFromMessageHandle(&handle);

  // Ensure deserialization fails instead of crashing.
  scoped_refptr<VideoFrame> new_frame;
  EXPECT_FALSE(mojom::VideoFrame::DeserializeFromMessage(std::move(message),
                                                         &new_frame));
}

TEST_F(VideoFrameStructTraitsTest, HoleVideoFrame) {
  base::UnguessableToken overlay_plane_id = base::UnguessableToken::Create();
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateVideoHoleFrame(
      overlay_plane_id, gfx::Size(200, 100), base::Seconds(100));

  // Saves the VideoFrame metadata from the created frame. The test should not
  // assume these have any particular value.
  const VideoFrame::StorageType storage_type = frame->storage_type();
  const VideoPixelFormat format = frame->format();

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->storage_type(), storage_type);
  EXPECT_EQ(frame->format(), format);
  EXPECT_EQ(frame->natural_size(), gfx::Size(200, 100));
  EXPECT_EQ(frame->timestamp(), base::Seconds(100));
  ASSERT_TRUE(frame->metadata().tracking_token.has_value());
  ASSERT_EQ(*frame->metadata().tracking_token, overlay_plane_id);
}

TEST_F(VideoFrameStructTraitsTest, TrackingTokenVideoFrame) {
  base::UnguessableToken tracking_token = base::UnguessableToken::Create();
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapTrackingToken(
      PIXEL_FORMAT_ARGB, tracking_token, gfx::Size(100, 100),
      gfx::Rect(10, 10, 80, 80), gfx::Size(200, 100), base::Seconds(100));

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->format(), PIXEL_FORMAT_ARGB);
  EXPECT_EQ(frame->coded_size(), gfx::Size(100, 100));
  EXPECT_EQ(frame->visible_rect(), gfx::Rect(10, 10, 80, 80));
  EXPECT_EQ(frame->natural_size(), gfx::Size(200, 100));
  EXPECT_EQ(frame->timestamp(), base::Seconds(100));
  ASSERT_TRUE(frame->metadata().tracking_token.has_value());
  ASSERT_EQ(*frame->metadata().tracking_token, tracking_token);
}

TEST_F(VideoFrameStructTraitsTest, SharedImageVideoFrame) {
  auto si_size = gfx::Size(100, 100);
  gpu::SharedImageMetadata metadata;
  metadata.format = viz::SinglePlaneFormat::kRGBA_8888;
  metadata.size = si_size;
  metadata.color_space = gfx::ColorSpace::CreateSRGB();
  metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
  metadata.alpha_type = kOpaque_SkAlphaType;
  metadata.usage = gpu::SharedImageUsageSet();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu::ClientSharedImage::CreateForTesting(metadata);
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapSharedImage(
      PIXEL_FORMAT_ARGB, shared_image, gpu::SyncToken(),
      VideoFrame::ReleaseMailboxCB(), si_size, gfx::Rect(10, 10, 80, 80),
      gfx::Size(200, 100), base::Seconds(100));
  frame->set_color_space(shared_image->color_space());
  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->format(), PIXEL_FORMAT_ARGB);
  EXPECT_EQ(frame->coded_size(), gfx::Size(100, 100));
  EXPECT_EQ(frame->visible_rect(), gfx::Rect(10, 10, 80, 80));
  EXPECT_EQ(frame->natural_size(), gfx::Size(200, 100));
  EXPECT_EQ(frame->timestamp(), base::Seconds(100));
  ASSERT_TRUE(frame->HasSharedImage());
  ASSERT_EQ(frame->shared_image()->mailbox(), shared_image->mailbox());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(VideoFrameStructTraitsTest, DmabufsVideoFrame) {
  constexpr gfx::Size kCodedSize = gfx::Size(256, 256);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr base::TimeDelta timestamp = base::Milliseconds(1);
  constexpr VideoPixelFormat kFormat = PIXEL_FORMAT_NV12;

  // Makes the "default" layout
  constexpr int kUvWidth = (kCodedSize.width() + 1) / 2;
  constexpr int kUvHeight = (kCodedSize.height() + 1) / 2;
  constexpr int kUvStride = kUvWidth * 2;
  constexpr int kUvSize = kUvStride * kUvHeight;
  std::vector<ColorPlaneLayout> planes = std::vector<ColorPlaneLayout>{
      ColorPlaneLayout(kCodedSize.width(), 0, kCodedSize.GetArea()),
      ColorPlaneLayout(kUvStride, kCodedSize.GetArea(), kUvSize),
  };
  std::optional<VideoFrameLayout> layout = VideoFrameLayout::CreateWithPlanes(
      kFormat, kCodedSize, std::move(planes));
  ASSERT_TRUE(layout.has_value());

  // Makes a single FD that is big enough to hold the layout.
  std::vector<base::ScopedFD> fds;
  fds.emplace_back(
      CreateValidLookingBufferHandle(kCodedSize.GetArea() + kUvSize));
  ASSERT_TRUE(fds.back().is_valid());
  // Mojo serialization can be destructive, so we dup() the FD before
  // serialization in order to use it later to compare it against the FD in the
  // deserialized message.
  std::vector<base::ScopedFD> duped_fds;
  duped_fds.emplace_back(HANDLE_EINTR(dup(fds.back().get())));
  ASSERT_TRUE(duped_fds.back().is_valid());

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, kVisibleRect, kNaturalSize, std::move(fds), timestamp);
  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_DMABUFS);
  EXPECT_TRUE(frame->HasDmaBufs());
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->format(), kFormat);
  EXPECT_EQ(frame->coded_size(), kCodedSize);
  EXPECT_EQ(frame->visible_rect(), kVisibleRect);
  EXPECT_EQ(frame->natural_size(), kNaturalSize);
  EXPECT_EQ(frame->timestamp(), timestamp);
  EXPECT_EQ(frame->layout().is_multi_planar(), layout->is_multi_planar());
  ASSERT_EQ(frame->layout().num_planes(), layout->num_planes());
  for (size_t i = 0, num_planes = layout->num_planes(); i < num_planes; ++i) {
    EXPECT_EQ(frame->layout().planes()[i].stride, layout->planes()[i].stride);
    EXPECT_EQ(frame->layout().planes()[i].offset, layout->planes()[i].offset);
    EXPECT_EQ(frame->layout().planes()[i].size, layout->planes()[i].size);
  }
  ASSERT_EQ(frame->NumDmabufFds(), duped_fds.size());
  for (size_t i = 0, num_fds = duped_fds.size(); i < num_fds; ++i) {
    const auto pid = base::Process::Current().Pid();
    EXPECT_EQ(syscall(SYS_kcmp, pid, pid, KCMP_FILE, duped_fds[i].get(),
                      frame->GetDmabufFd(i)),
              0);
  }
}

TEST_F(VideoFrameStructTraitsTest, MultiplanarDmabufsVideoFrame) {
  constexpr gfx::Size kCodedSize = gfx::Size(256, 256);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr base::TimeDelta timestamp = base::Milliseconds(1);
  constexpr VideoPixelFormat kFormat = PIXEL_FORMAT_NV12;

  // Makes the "default" layout
  constexpr int kUvWidth = (kCodedSize.width() + 1) / 2;
  constexpr int kUvHeight = (kCodedSize.height() + 1) / 2;
  constexpr int kUvStride = kUvWidth * 2;
  constexpr int kUvSize = kUvStride * kUvHeight;
  std::vector<ColorPlaneLayout> planes = std::vector<ColorPlaneLayout>{
      ColorPlaneLayout(kCodedSize.width(), 1, kCodedSize.GetArea()),
      ColorPlaneLayout(kUvStride, 2, kUvSize),
  };
  std::optional<VideoFrameLayout> layout = VideoFrameLayout::CreateMultiPlanar(
      kFormat, kCodedSize, std::move(planes));
  ASSERT_TRUE(layout.has_value());

  // For each plane, makes an FD
  std::vector<base::ScopedFD> fds;
  std::vector<base::ScopedFD> duped_fds;
  for (const auto& plane : layout->planes()) {
    fds.emplace_back(CreateValidLookingBufferHandle(plane.offset + plane.size));
    ASSERT_TRUE(fds.back().is_valid());
    // Mojo serialization can be destructive, so we dup() the FD before
    // serialization in order to use it later to compare it against the FD in
    // the deserialized message.
    duped_fds.emplace_back(HANDLE_EINTR(dup(fds.back().get())));
    ASSERT_TRUE(duped_fds.back().is_valid());
  }

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, kVisibleRect, kNaturalSize, std::move(fds), timestamp);
  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_DMABUFS);
  EXPECT_TRUE(frame->HasDmaBufs());
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->format(), kFormat);
  EXPECT_EQ(frame->coded_size(), kCodedSize);
  EXPECT_EQ(frame->visible_rect(), kVisibleRect);
  EXPECT_EQ(frame->natural_size(), kNaturalSize);
  EXPECT_EQ(frame->timestamp(), timestamp);
  EXPECT_EQ(frame->layout().is_multi_planar(), layout->is_multi_planar());
  ASSERT_EQ(frame->layout().num_planes(), layout->num_planes());
  for (size_t i = 0, num_planes = layout->num_planes(); i < num_planes; ++i) {
    EXPECT_EQ(frame->layout().planes()[i].stride, layout->planes()[i].stride);
    EXPECT_EQ(frame->layout().planes()[i].offset, layout->planes()[i].offset);
    EXPECT_EQ(frame->layout().planes()[i].size, layout->planes()[i].size);
  }
  ASSERT_EQ(frame->NumDmabufFds(), duped_fds.size());
  for (size_t i = 0, num_fds = duped_fds.size(); i < num_fds; ++i) {
    const auto pid = base::Process::Current().Pid();
    EXPECT_EQ(syscall(SYS_kcmp, pid, pid, KCMP_FILE, duped_fds[i].get(),
                      frame->GetDmabufFd(i)),
              0);
  }
}

TEST_F(VideoFrameStructTraitsTest, DmabufsVideoInvalidPixelFormat) {
  constexpr gfx::Size kCodedSize = gfx::Size(256, 256);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr base::TimeDelta timestamp = base::Milliseconds(1);
  constexpr VideoPixelFormat kFormat = PIXEL_FORMAT_XRGB;

  // Makes the "default" layout
  std::vector<ColorPlaneLayout> planes = std::vector<ColorPlaneLayout>{
      ColorPlaneLayout(kCodedSize.width() * 4, 0, kCodedSize.GetArea() * 4),
  };
  std::optional<VideoFrameLayout> layout = VideoFrameLayout::CreateWithPlanes(
      kFormat, kCodedSize, std::move(planes));
  ASSERT_TRUE(layout.has_value());

  // Makes a single FD that is too small to hold the layout.
  std::vector<base::ScopedFD> fds;
  fds.emplace_back(CreateValidLookingBufferHandle(4 * kCodedSize.GetArea()));
  ASSERT_TRUE(fds.back().is_valid());

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, kVisibleRect, kNaturalSize, std::move(fds), timestamp);
  ASSERT_TRUE(frame);

  // Ensure deserialization fails instead of crashing.
  EXPECT_TRUE(RoundTripFails(std::move(frame)));
}

TEST_F(VideoFrameStructTraitsTest, DmabufsVideoNondecreasingStrides) {
  constexpr gfx::Size kCodedSize = gfx::Size(256, 256);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr base::TimeDelta timestamp = base::Milliseconds(1);
  constexpr VideoPixelFormat kFormat = PIXEL_FORMAT_NV12;

  // Makes the "default" layout
  constexpr int kUvWidth = (kCodedSize.width() + 1) / 2;
  constexpr int kUvHeight = (kCodedSize.height() + 1) / 2;
  constexpr int kUvStride = kUvWidth * 2;
  constexpr int kUvSize = kUvStride * kUvHeight;
  // Mess up the firs plane width
  std::vector<ColorPlaneLayout> planes = std::vector<ColorPlaneLayout>{
      ColorPlaneLayout(1, 0, kCodedSize.GetArea()),
      ColorPlaneLayout(kUvStride, kCodedSize.GetArea(), kUvSize),
  };
  std::optional<VideoFrameLayout> layout = VideoFrameLayout::CreateWithPlanes(
      kFormat, kCodedSize, std::move(planes));
  ASSERT_TRUE(layout.has_value());

  // Makes a single FD that is too small to hold the layout.
  std::vector<base::ScopedFD> fds;
  fds.emplace_back(CreateValidLookingBufferHandle(kCodedSize.GetArea()));
  ASSERT_TRUE(fds.back().is_valid());

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, kVisibleRect, kNaturalSize, std::move(fds), timestamp);

  // Ensure deserialization fails instead of crashing.
  EXPECT_TRUE(RoundTripFails(std::move(frame)));
}

TEST_F(VideoFrameStructTraitsTest, DmabufsVideoInvalidStrides) {
  constexpr gfx::Size kCodedSize = gfx::Size(256, 256);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr base::TimeDelta timestamp = base::Milliseconds(1);
  constexpr VideoPixelFormat kFormat = PIXEL_FORMAT_NV12;

  // Makes the "default" layout
  constexpr int kUvWidth = (kCodedSize.width() + 1) / 2;
  constexpr int kUvHeight = (kCodedSize.height() + 1) / 2;
  constexpr int kUvStride = kUvWidth * 2;
  constexpr int kUvSize = kUvStride * kUvHeight;
  // Reverse the plane layout.
  std::vector<ColorPlaneLayout> planes = std::vector<ColorPlaneLayout>{
      ColorPlaneLayout(kUvStride, kCodedSize.GetArea(), kUvSize),
      ColorPlaneLayout(kCodedSize.width(), 0, kCodedSize.GetArea()),
  };
  std::optional<VideoFrameLayout> layout = VideoFrameLayout::CreateWithPlanes(
      kFormat, kCodedSize, std::move(planes));
  ASSERT_TRUE(layout.has_value());

  // Makes a single FD that is too small to hold the layout.
  std::vector<base::ScopedFD> fds;
  fds.emplace_back(CreateValidLookingBufferHandle(kCodedSize.GetArea()));
  ASSERT_TRUE(fds.back().is_valid());

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, kVisibleRect, kNaturalSize, std::move(fds), timestamp);

  // Ensure deserialization fails instead of crashing.
  EXPECT_TRUE(RoundTripFails(std::move(frame)));
}

TEST_F(VideoFrameStructTraitsTest, DmabufsVideoFrameTooSmall) {
  constexpr gfx::Size kCodedSize = gfx::Size(256, 256);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr base::TimeDelta timestamp = base::Milliseconds(1);
  constexpr VideoPixelFormat kFormat = PIXEL_FORMAT_NV12;

  // Makes the "default" layout
  constexpr int kUvWidth = (kCodedSize.width() + 1) / 2;
  constexpr int kUvHeight = (kCodedSize.height() + 1) / 2;
  constexpr int kUvStride = kUvWidth * 2;
  constexpr int kUvSize = kUvStride * kUvHeight;
  std::vector<ColorPlaneLayout> planes = std::vector<ColorPlaneLayout>{
      ColorPlaneLayout(kCodedSize.width(), 0, kCodedSize.GetArea()),
      ColorPlaneLayout(kUvStride, kCodedSize.GetArea(), kUvSize),
  };
  std::optional<VideoFrameLayout> layout = VideoFrameLayout::CreateWithPlanes(
      kFormat, kCodedSize, std::move(planes));
  ASSERT_TRUE(layout.has_value());

  // Makes a single FD that is too small to hold the layout.
  std::vector<base::ScopedFD> fds;
  fds.emplace_back(CreateValidLookingBufferHandle(kCodedSize.GetArea()));
  ASSERT_TRUE(fds.back().is_valid());

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, kVisibleRect, kNaturalSize, std::move(fds), timestamp);
  ASSERT_TRUE(frame);

  // Ensure deserialization fails instead of crashing.
  EXPECT_TRUE(RoundTripFails(std::move(frame)));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

TEST_F(VideoFrameStructTraitsTest, MappableSharedImageVideoFrame) {
  auto test_sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();
  gfx::Size coded_size = gfx::Size(256, 256);
  gfx::Rect visible_rect(coded_size);
  auto timestamp = base::Milliseconds(1);
  auto si_format = viz::SinglePlaneFormat::kRGBA_8888;
  const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  auto shared_image = test_sii->CreateSharedImage(
      {si_format, coded_size, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(si_usage), "VideoFrameStructTraitsTest"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ);
  ASSERT_TRUE(shared_image);
  auto frame = VideoFrame::WrapMappableSharedImage(
      shared_image, test_sii->GenVerifiedSyncToken(), base::NullCallback(),
      visible_rect, visible_rect.size(), timestamp);
  ASSERT_TRUE(frame);
  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  EXPECT_TRUE(frame->HasMappableGpuBuffer());
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->format(), PIXEL_FORMAT_ABGR);
  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->natural_size(), visible_rect.size());
  EXPECT_EQ(frame->timestamp(), timestamp);
  ASSERT_TRUE(frame->HasSharedImage());
  ASSERT_EQ(frame->shared_image()->mailbox(), shared_image->mailbox());
}

}  // namespace media
