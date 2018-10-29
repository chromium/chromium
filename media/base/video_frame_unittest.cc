// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

namespace {
// Creates the backing storage for a frame suitable for WrapExternalData. Note
// that this is currently used only to test frame creation and tear-down, and so
// may not have alignment or other properties correct further video processing.
// |memory| must be at least 2 * coded_size.width() * coded_size.height() in
// bytes.
void CreateTestY16Frame(const gfx::Size& coded_size,
                        const gfx::Rect& visible_rect,
                        void* memory) {
  const int offset_x = visible_rect.x();
  const int offset_y = visible_rect.y();
  const int stride = coded_size.width();

  // In the visible rect, fill upper byte with [0-255] and lower with [255-0].
  uint16_t* data = static_cast<uint16_t*>(memory);
  for (int j = 0; j < visible_rect.height(); j++) {
    for (int i = 0; i < visible_rect.width(); i++) {
      const int value = i + j * visible_rect.width();
      data[(stride * (j + offset_y)) + i + offset_x] =
          ((value & 0xFF) << 8) | (~value & 0xFF);
    }
  }
}
}  // namespace

namespace media {

using base::MD5DigestToBase16;

// Helper function that initializes a YV12 frame with white and black scan
// lines based on the |white_to_black| parameter.  If 0, then the entire
// frame will be black, if 1 then the entire frame will be white.
void InitializeYV12Frame(VideoFrame* frame, double white_to_black) {
  EXPECT_EQ(PIXEL_FORMAT_YV12, frame->format());
  const int first_black_row =
      static_cast<int>(frame->coded_size().height() * white_to_black);
  uint8_t* y_plane = frame->data(VideoFrame::kYPlane);
  for (int row = 0; row < frame->coded_size().height(); ++row) {
    int color = (row < first_black_row) ? 0xFF : 0x00;
    memset(y_plane, color, frame->stride(VideoFrame::kYPlane));
    y_plane += frame->stride(VideoFrame::kYPlane);
  }
  uint8_t* u_plane = frame->data(VideoFrame::kUPlane);
  uint8_t* v_plane = frame->data(VideoFrame::kVPlane);
  for (int row = 0; row < frame->coded_size().height(); row += 2) {
    memset(u_plane, 0x80, frame->stride(VideoFrame::kUPlane));
    memset(v_plane, 0x80, frame->stride(VideoFrame::kVPlane));
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

// Given a |yv12_frame| this method converts the YV12 frame to RGBA and
// makes sure that all the pixels of the RBG frame equal |expect_rgb_color|.
void ExpectFrameColor(media::VideoFrame* yv12_frame,
                      uint32_t expect_rgb_color) {
  ASSERT_EQ(PIXEL_FORMAT_YV12, yv12_frame->format());
  ASSERT_EQ(yv12_frame->stride(VideoFrame::kUPlane),
            yv12_frame->stride(VideoFrame::kVPlane));
  ASSERT_EQ(
      yv12_frame->coded_size().width() & (VideoFrame::kFrameSizeAlignment - 1),
      0);
  ASSERT_EQ(
      yv12_frame->coded_size().height() & (VideoFrame::kFrameSizeAlignment - 1),
      0);

  size_t bytes_per_row = yv12_frame->coded_size().width() * 4u;
  uint8_t* rgb_data = reinterpret_cast<uint8_t*>(
      base::AlignedAlloc(bytes_per_row * yv12_frame->coded_size().height() +
                             VideoFrame::kFrameSizePadding,
                         VideoFrame::kFrameAddressAlignment));

  libyuv::I420ToARGB(yv12_frame->data(VideoFrame::kYPlane),
                     yv12_frame->stride(VideoFrame::kYPlane),
                     yv12_frame->data(VideoFrame::kUPlane),
                     yv12_frame->stride(VideoFrame::kUPlane),
                     yv12_frame->data(VideoFrame::kVPlane),
                     yv12_frame->stride(VideoFrame::kVPlane), rgb_data,
                     bytes_per_row, yv12_frame->coded_size().width(),
                     yv12_frame->coded_size().height());

  for (int row = 0; row < yv12_frame->coded_size().height(); ++row) {
    uint32_t* rgb_row_data =
        reinterpret_cast<uint32_t*>(rgb_data + (bytes_per_row * row));
    for (int col = 0; col < yv12_frame->coded_size().width(); ++col) {
      SCOPED_TRACE(base::StringPrintf("Checking (%d, %d)", row, col));
      EXPECT_EQ(expect_rgb_color, rgb_row_data[col]);
    }
  }

  base::AlignedFree(rgb_data);
}

// Fill each plane to its reported extents and verify accessors report non
// zero values.  Additionally, for the first plane verify the rows and
// row_bytes values are correct.
void ExpectFrameExtents(VideoPixelFormat format, const char* expected_hash) {
  const unsigned char kFillByte = 0x80;
  const int kWidth = 61;
  const int kHeight = 31;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  gfx::Size size(kWidth, kHeight);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      format, size, gfx::Rect(size), size, kTimestamp);
  ASSERT_TRUE(frame.get());

  int planes = VideoFrame::NumPlanes(format);
  for (int plane = 0; plane < planes; plane++) {
    SCOPED_TRACE(base::StringPrintf("Checking plane %d", plane));
    EXPECT_TRUE(frame->data(plane));
    EXPECT_TRUE(frame->stride(plane));
    EXPECT_TRUE(frame->rows(plane));
    EXPECT_TRUE(frame->row_bytes(plane));

    memset(frame->data(plane), kFillByte,
           frame->stride(plane) * frame->rows(plane));
  }

  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, frame);
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), expected_hash);
}

TEST(VideoFrame, CreateFrame) {
  const int kWidth = 64;
  const int kHeight = 48;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  // Create a YV12 Video Frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<media::VideoFrame> frame = VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_YV12, size, gfx::Rect(size), size, kTimestamp);
  ASSERT_TRUE(frame.get());

  // Test VideoFrame implementation.
  EXPECT_EQ(media::PIXEL_FORMAT_YV12, frame->format());
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame.get(), 0.0f);
    ExpectFrameColor(frame.get(), 0xFF000000);
  }
  base::MD5Digest digest;
  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, frame);
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), "9065c841d9fca49186ef8b4ef547e79b");
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame.get(), 1.0f);
    ExpectFrameColor(frame.get(), 0xFFFFFFFF);
  }
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, frame);
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), "911991d51438ad2e1a40ed5f6fc7c796");

  // Test single planar frame.
  frame = VideoFrame::CreateFrame(media::PIXEL_FORMAT_ARGB, size,
                                  gfx::Rect(size), size, kTimestamp);
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, frame->format());
  EXPECT_GE(frame->stride(VideoFrame::kARGBPlane), frame->coded_size().width());

  // Test double planar frame.
  frame = VideoFrame::CreateFrame(media::PIXEL_FORMAT_NV12, size,
                                  gfx::Rect(size), size, kTimestamp);
  EXPECT_EQ(media::PIXEL_FORMAT_NV12, frame->format());

  // Test an empty frame.
  frame = VideoFrame::CreateEOSFrame();
  EXPECT_TRUE(
      frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
}

TEST(VideoFrame, CreateZeroInitializedFrame) {
  const int kWidth = 2;
  const int kHeight = 2;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  // Create a YV12 Video Frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<media::VideoFrame> frame =
      VideoFrame::CreateZeroInitializedFrame(media::PIXEL_FORMAT_YV12, size,
                                             gfx::Rect(size), size, kTimestamp);
  ASSERT_TRUE(frame.get());
  EXPECT_TRUE(frame->IsMappable());

  // Verify that frame is initialized with zeros.
  // TODO(emircan): Check all the contents when we know the exact size of the
  // allocated buffer.
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); ++i)
    EXPECT_EQ(0, frame->data(i)[0]);
}

TEST(VideoFrame, CreateBlackFrame) {
  const int kWidth = 2;
  const int kHeight = 2;
  const uint8_t kExpectedYRow[] = {0, 0};
  const uint8_t kExpectedUVRow[] = {128};

  scoped_refptr<media::VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
  ASSERT_TRUE(frame.get());
  EXPECT_TRUE(frame->IsMappable());

  // Test basic properties.
  EXPECT_EQ(0, frame->timestamp().InMicroseconds());
  EXPECT_FALSE(
      frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));

  // Test |frame| properties.
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(kWidth, frame->coded_size().width());
  EXPECT_EQ(kHeight, frame->coded_size().height());

  // Test frames themselves.
  uint8_t* y_plane = frame->data(VideoFrame::kYPlane);
  for (int y = 0; y < frame->coded_size().height(); ++y) {
    EXPECT_EQ(0, memcmp(kExpectedYRow, y_plane, arraysize(kExpectedYRow)));
    y_plane += frame->stride(VideoFrame::kYPlane);
  }

  uint8_t* u_plane = frame->data(VideoFrame::kUPlane);
  uint8_t* v_plane = frame->data(VideoFrame::kVPlane);
  for (int y = 0; y < frame->coded_size().height() / 2; ++y) {
    EXPECT_EQ(0, memcmp(kExpectedUVRow, u_plane, arraysize(kExpectedUVRow)));
    EXPECT_EQ(0, memcmp(kExpectedUVRow, v_plane, arraysize(kExpectedUVRow)));
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

static void FrameNoLongerNeededCallback(
    const scoped_refptr<media::VideoFrame>& frame,
    bool* triggered) {
  *triggered = true;
}

TEST(VideoFrame, WrapVideoFrame) {
  const int kWidth = 4;
  const int kHeight = 4;
  const base::TimeDelta kFrameDuration = base::TimeDelta::FromMicroseconds(42);

  scoped_refptr<media::VideoFrame> frame;
  bool done_callback_was_run = false;
  {
    scoped_refptr<media::VideoFrame> wrapped_frame =
        VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
    ASSERT_TRUE(wrapped_frame.get());

    gfx::Rect visible_rect(1, 1, 1, 1);
    gfx::Size natural_size = visible_rect.size();
    wrapped_frame->metadata()->SetTimeDelta(
        media::VideoFrameMetadata::FRAME_DURATION, kFrameDuration);
    frame = media::VideoFrame::WrapVideoFrame(
        wrapped_frame, wrapped_frame->format(), visible_rect, natural_size);
    frame->AddDestructionObserver(base::Bind(
        &FrameNoLongerNeededCallback, wrapped_frame, &done_callback_was_run));
    EXPECT_EQ(wrapped_frame->coded_size(), frame->coded_size());
    EXPECT_EQ(wrapped_frame->data(media::VideoFrame::kYPlane),
              frame->data(media::VideoFrame::kYPlane));
    EXPECT_NE(wrapped_frame->visible_rect(), frame->visible_rect());
    EXPECT_EQ(visible_rect, frame->visible_rect());
    EXPECT_NE(wrapped_frame->natural_size(), frame->natural_size());
    EXPECT_EQ(natural_size, frame->natural_size());

    // Verify metadata was copied to the wrapped frame.
    base::TimeDelta frame_duration;
    ASSERT_TRUE(frame->metadata()->GetTimeDelta(
        media::VideoFrameMetadata::FRAME_DURATION, &frame_duration));

    EXPECT_EQ(frame_duration, kFrameDuration);

    // Verify the metadata copy was a deep copy.
    wrapped_frame->metadata()->Clear();
    EXPECT_NE(
        wrapped_frame->metadata()->HasKey(
            media::VideoFrameMetadata::FRAME_DURATION),
        frame->metadata()->HasKey(media::VideoFrameMetadata::FRAME_DURATION));
  }

  EXPECT_FALSE(done_callback_was_run);
  frame = NULL;
  EXPECT_TRUE(done_callback_was_run);
}

// Create a frame that wraps unowned memory.
TEST(VideoFrame, WrapExternalData) {
  uint8_t memory[2 * 256 * 256];
  gfx::Size coded_size(256, 256);
  gfx::Rect visible_rect(coded_size);
  CreateTestY16Frame(coded_size, visible_rect, memory);
  auto timestamp = base::TimeDelta::FromMilliseconds(1);
  auto frame = VideoFrame::WrapExternalData(media::PIXEL_FORMAT_Y16, coded_size,
                                            visible_rect, visible_rect.size(),
                                            memory, sizeof(memory), timestamp);

  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->timestamp(), timestamp);
  EXPECT_EQ(frame->data(media::VideoFrame::kYPlane)[0], 0xff);
}

// Create a frame that wraps read-only shared memory.
TEST(VideoFrame, WrapExternalReadOnlySharedMemory) {
  const size_t kDataSize = 2 * 256 * 256;
  auto mapped_region = base::ReadOnlySharedMemoryRegion::Create(kDataSize);
  gfx::Size coded_size(256, 256);
  gfx::Rect visible_rect(coded_size);
  CreateTestY16Frame(coded_size, visible_rect, mapped_region.mapping.memory());
  auto timestamp = base::TimeDelta::FromMilliseconds(1);
  auto frame = VideoFrame::WrapExternalReadOnlySharedMemory(
      media::PIXEL_FORMAT_Y16, coded_size, visible_rect, visible_rect.size(),
      static_cast<uint8_t*>(mapped_region.mapping.memory()), kDataSize,
      &mapped_region.region, 0, timestamp);

  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->timestamp(), timestamp);
  EXPECT_EQ(frame->data(media::VideoFrame::kYPlane)[0], 0xff);
}

// Create a frame that wraps unsafe shared memory.
TEST(VideoFrame, WrapExternalUnsafeSharedMemory) {
  const size_t kDataSize = 2 * 256 * 256;
  auto region = base::UnsafeSharedMemoryRegion::Create(kDataSize);
  auto mapping = region.Map();
  gfx::Size coded_size(256, 256);
  gfx::Rect visible_rect(coded_size);
  CreateTestY16Frame(coded_size, visible_rect, mapping.memory());
  auto timestamp = base::TimeDelta::FromMilliseconds(1);
  auto frame = VideoFrame::WrapExternalUnsafeSharedMemory(
      media::PIXEL_FORMAT_Y16, coded_size, visible_rect, visible_rect.size(),
      static_cast<uint8_t*>(mapping.memory()), kDataSize, &region, 0,
      timestamp);

  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->timestamp(), timestamp);
  EXPECT_EQ(frame->data(media::VideoFrame::kYPlane)[0], 0xff);
}

// Create a frame that wraps a legacy shared memory handle.
TEST(VideoFrame, WrapExternalSharedMemory) {
  const size_t kDataSize = 2 * 256 * 256;
  base::SharedMemory shm;
  ASSERT_TRUE(shm.CreateAndMapAnonymous(kDataSize));
  gfx::Size coded_size(256, 256);
  gfx::Rect visible_rect(coded_size);
  CreateTestY16Frame(coded_size, visible_rect, shm.memory());
  auto timestamp = base::TimeDelta::FromMilliseconds(1);
  auto frame = VideoFrame::WrapExternalSharedMemory(
      media::PIXEL_FORMAT_Y16, coded_size, visible_rect, visible_rect.size(),
      static_cast<uint8_t*>(shm.memory()), kDataSize, shm.handle(), 0,
      timestamp);

  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->timestamp(), timestamp);
  EXPECT_EQ(frame->data(media::VideoFrame::kYPlane)[0], 0xff);
}

#if defined(OS_LINUX)
TEST(VideoFrame, WrapExternalDmabufs) {
  gfx::Size coded_size = gfx::Size(256, 256);
  gfx::Rect visible_rect(coded_size);
  std::vector<int32_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 100, 200};
  std::vector<size_t> buffer_sizes = {73728, 18432, 18432};
  std::vector<VideoFrameLayout::Plane> planes(strides.size());

  for (size_t i = 0; i < planes.size(); i++) {
    planes[i].stride = strides[i];
    planes[i].offset = offsets[i];
  }
  auto timestamp = base::TimeDelta::FromMilliseconds(1);
  auto layout = VideoFrameLayout::CreateWithPlanes(
      PIXEL_FORMAT_I420, coded_size, planes, buffer_sizes);
  ASSERT_TRUE(layout);
  std::vector<base::ScopedFD> dmabuf_fds(3u);
  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, visible_rect, visible_rect.size(), std::move(dmabuf_fds),
      timestamp);

  EXPECT_EQ(frame->layout().format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(frame->layout().coded_size(), coded_size);
  EXPECT_EQ(frame->layout().num_planes(), 3u);
  EXPECT_EQ(frame->layout().num_buffers(), 3u);
  EXPECT_EQ(frame->layout().GetTotalBufferSize(), 110592u);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(frame->layout().planes()[i].stride, strides[i]);
    EXPECT_EQ(frame->layout().planes()[i].offset, offsets[i]);
    EXPECT_EQ(frame->layout().buffer_sizes()[i], buffer_sizes[i]);
  }
  EXPECT_TRUE(frame->HasDmaBufs());
  EXPECT_EQ(frame->DmabufFds().size(), 3u);
  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->timestamp(), timestamp);
}
#endif

// Ensure each frame is properly sized and allocated.  Will trigger OOB reads
// and writes as well as incorrect frame hashes otherwise.
TEST(VideoFrame, CheckFrameExtents) {
  // Each call consists of a Format and the expected hash of all
  // planes if filled with kFillByte (defined in ExpectFrameExtents).
  ExpectFrameExtents(PIXEL_FORMAT_YV12, "8e5d54cb23cd0edca111dd35ffb6ff05");
  ExpectFrameExtents(PIXEL_FORMAT_I422, "cce408a044b212db42a10dfec304b3ef");
}

static void TextureCallback(gpu::SyncToken* called_sync_token,
                            const gpu::SyncToken& release_sync_token) {
  *called_sync_token = release_sync_token;
}

// Verify the gpu::MailboxHolder::ReleaseCallback is called when VideoFrame is
// destroyed with the default release sync point.
TEST(VideoFrame, TextureNoLongerNeededCallbackIsCalled) {
  gpu::SyncToken called_sync_token(gpu::CommandBufferNamespace::GPU_IO,
                                   gpu::CommandBufferId::FromUnsafeValue(1), 1);

  {
    gpu::MailboxHolder holders[media::VideoFrame::kMaxPlanes] = {
        gpu::MailboxHolder(gpu::Mailbox::Generate(), gpu::SyncToken(), 5)};
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
        PIXEL_FORMAT_ARGB, holders,
        base::Bind(&TextureCallback, &called_sync_token),
        gfx::Size(10, 10),   // coded_size
        gfx::Rect(10, 10),   // visible_rect
        gfx::Size(10, 10),   // natural_size
        base::TimeDelta());  // timestamp
    EXPECT_EQ(PIXEL_FORMAT_ARGB, frame->format());
    EXPECT_EQ(VideoFrame::STORAGE_OPAQUE, frame->storage_type());
    EXPECT_TRUE(frame->HasTextures());
  }
  // Nobody set a sync point to |frame|, so |frame| set |called_sync_token|
  // cleared to default value.
  EXPECT_FALSE(called_sync_token.HasData());
}

namespace {

class SyncTokenClientImpl : public VideoFrame::SyncTokenClient {
 public:
  explicit SyncTokenClientImpl(const gpu::SyncToken& sync_token)
      : sync_token_(sync_token) {}
  ~SyncTokenClientImpl() override = default;
  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    *sync_token = sync_token_;
  }
  void WaitSyncToken(const gpu::SyncToken& sync_token) override {}

 private:
  gpu::SyncToken sync_token_;
};

}  // namespace

// Verify the gpu::MailboxHolder::ReleaseCallback is called when VideoFrame is
// destroyed with the release sync point, which was updated by clients.
// (i.e. the compositor, webgl).
TEST(VideoFrame,
     TexturesNoLongerNeededCallbackAfterTakingAndReleasingMailboxes) {
  const int kPlanesNum = 3;
  const gpu::CommandBufferNamespace kNamespace =
      gpu::CommandBufferNamespace::GPU_IO;
  const gpu::CommandBufferId kCommandBufferId =
      gpu::CommandBufferId::FromUnsafeValue(0x123);
  gpu::Mailbox mailbox[kPlanesNum];
  for (int i = 0; i < kPlanesNum; ++i) {
    mailbox[i].name[0] = 50 + 1;
  }

  gpu::SyncToken sync_token(kNamespace, kCommandBufferId, 7);
  sync_token.SetVerifyFlush();
  uint32_t target = 9;
  gpu::SyncToken release_sync_token(kNamespace, kCommandBufferId, 111);
  release_sync_token.SetVerifyFlush();

  gpu::SyncToken called_sync_token;
  {
    gpu::MailboxHolder holders[media::VideoFrame::kMaxPlanes] = {
        gpu::MailboxHolder(mailbox[VideoFrame::kYPlane], sync_token, target),
        gpu::MailboxHolder(mailbox[VideoFrame::kUPlane], sync_token, target),
        gpu::MailboxHolder(mailbox[VideoFrame::kVPlane], sync_token, target),
    };
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
        PIXEL_FORMAT_I420, holders,
        base::Bind(&TextureCallback, &called_sync_token),
        gfx::Size(10, 10),   // coded_size
        gfx::Rect(10, 10),   // visible_rect
        gfx::Size(10, 10),   // natural_size
        base::TimeDelta());  // timestamp

    EXPECT_EQ(VideoFrame::STORAGE_OPAQUE, frame->storage_type());
    EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
    EXPECT_EQ(3u, VideoFrame::NumPlanes(frame->format()));
    EXPECT_TRUE(frame->HasTextures());
    for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); ++i) {
      const gpu::MailboxHolder& mailbox_holder = frame->mailbox_holder(i);
      EXPECT_EQ(mailbox[i].name[0], mailbox_holder.mailbox.name[0]);
      EXPECT_EQ(target, mailbox_holder.texture_target);
      EXPECT_EQ(sync_token, mailbox_holder.sync_token);
    }

    SyncTokenClientImpl client(release_sync_token);
    frame->UpdateReleaseSyncToken(&client);
    EXPECT_EQ(sync_token,
              frame->mailbox_holder(VideoFrame::kYPlane).sync_token);
  }
  EXPECT_EQ(release_sync_token, called_sync_token);
}

TEST(VideoFrame, IsValidConfig_OddCodedSize) {
  // Odd sizes are valid for all formats. Odd formats may be internally rounded
  // in VideoFrame::CreateFrame because VideoFrame owns the allocation and can
  // pad the requested coded_size to ensure the UV sample boundaries line up
  // with the Y plane after subsample scaling. See CreateFrame_OddWidth.
  gfx::Size odd_size(677, 288);

  // First choosing a format with sub-sampling for UV.
  EXPECT_TRUE(VideoFrame::IsValidConfig(
      PIXEL_FORMAT_I420, VideoFrame::STORAGE_OWNED_MEMORY, odd_size,
      gfx::Rect(odd_size), odd_size));

  // Next try a format with no sub-sampling for UV.
  EXPECT_TRUE(VideoFrame::IsValidConfig(
      PIXEL_FORMAT_I444, VideoFrame::STORAGE_OWNED_MEMORY, odd_size,
      gfx::Rect(odd_size), odd_size));
}

TEST(VideoFrame, CreateFrame_OddWidth) {
  // Odd sizes are non-standard for YUV formats that subsample the UV, but they
  // do exist in the wild and should be gracefully handled by VideoFrame in
  // situations where VideoFrame allocates the YUV memory. See discussion in
  // crrev.com/1240833003
  const gfx::Size odd_size(677, 288);
  const base::TimeDelta kTimestamp = base::TimeDelta();

  // First create a frame that sub-samples UV.
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420, odd_size, gfx::Rect(odd_size), odd_size, kTimestamp);
  ASSERT_TRUE(frame.get());
  // I420 aligns UV to every 2 Y pixels. Hence, 677 should be rounded to 678
  // which is the nearest value such that width % 2 == 0
  EXPECT_EQ(678, frame->coded_size().width());

  // Next create a frame that does not sub-sample UV.
  frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I444, odd_size,
                                  gfx::Rect(odd_size), odd_size, kTimestamp);
  ASSERT_TRUE(frame.get());
  // No sub-sampling for YV24 will mean odd width can remain odd since any pixel
  // in the Y plane has a a corresponding pixel in the UV planes at the same
  // index.
  EXPECT_EQ(677, frame->coded_size().width());
}

TEST(VideoFrame, AllocationSize_OddSize) {
  const gfx::Size size(3, 5);

  for (unsigned int i = 1u; i <= PIXEL_FORMAT_MAX; ++i) {
    const VideoPixelFormat format = static_cast<VideoPixelFormat>(i);
    switch (format) {
      case PIXEL_FORMAT_YUV444P9:
      case PIXEL_FORMAT_YUV444P10:
      case PIXEL_FORMAT_YUV444P12:
        EXPECT_EQ(144u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_YUV422P9:
      case PIXEL_FORMAT_YUV422P10:
      case PIXEL_FORMAT_YUV422P12:
        EXPECT_EQ(96u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_I444:
      case PIXEL_FORMAT_YUV420P9:
      case PIXEL_FORMAT_YUV420P10:
      case PIXEL_FORMAT_YUV420P12:
        EXPECT_EQ(72u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_UYVY:
      case PIXEL_FORMAT_YUY2:
      case PIXEL_FORMAT_I422:
        EXPECT_EQ(48u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_YV12:
      case PIXEL_FORMAT_I420:
      case PIXEL_FORMAT_NV12:
      case PIXEL_FORMAT_NV21:
      case PIXEL_FORMAT_MT21:
        EXPECT_EQ(36u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_ARGB:
      case PIXEL_FORMAT_XRGB:
      case PIXEL_FORMAT_I420A:
      case PIXEL_FORMAT_RGB32:
      case PIXEL_FORMAT_ABGR:
      case PIXEL_FORMAT_XBGR:
        EXPECT_EQ(60u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_RGB24:
        EXPECT_EQ(45u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_Y16:
        EXPECT_EQ(30u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_MJPEG:
      case PIXEL_FORMAT_UNKNOWN:
        continue;
    }
  }
}

TEST(VideoFrameMetadata, SetAndThenGetAllKeysForAllTypes) {
  VideoFrameMetadata metadata;

  for (int i = 0; i < VideoFrameMetadata::NUM_KEYS; ++i) {
    const VideoFrameMetadata::Key key = static_cast<VideoFrameMetadata::Key>(i);

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetBoolean(key, true);
    EXPECT_TRUE(metadata.HasKey(key));
    bool bool_value = false;
    EXPECT_TRUE(metadata.GetBoolean(key, &bool_value));
    EXPECT_EQ(true, bool_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetInteger(key, i);
    EXPECT_TRUE(metadata.HasKey(key));
    int int_value = -999;
    EXPECT_TRUE(metadata.GetInteger(key, &int_value));
    EXPECT_EQ(i, int_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetDouble(key, 3.14 * i);
    EXPECT_TRUE(metadata.HasKey(key));
    double double_value = -999.99;
    EXPECT_TRUE(metadata.GetDouble(key, &double_value));
    EXPECT_EQ(3.14 * i, double_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetString(key, base::StringPrintf("\xfe%d\xff", i));
    EXPECT_TRUE(metadata.HasKey(key));
    std::string string_value;
    EXPECT_TRUE(metadata.GetString(key, &string_value));
    EXPECT_EQ(base::StringPrintf("\xfe%d\xff", i), string_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetTimeDelta(key, base::TimeDelta::FromInternalValue(42 + i));
    EXPECT_TRUE(metadata.HasKey(key));
    base::TimeDelta delta_value;
    EXPECT_TRUE(metadata.GetTimeDelta(key, &delta_value));
    EXPECT_EQ(base::TimeDelta::FromInternalValue(42 + i), delta_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetTimeTicks(key, base::TimeTicks::FromInternalValue(~(0LL) + i));
    EXPECT_TRUE(metadata.HasKey(key));
    base::TimeTicks ticks_value;
    EXPECT_TRUE(metadata.GetTimeTicks(key, &ticks_value));
    EXPECT_EQ(base::TimeTicks::FromInternalValue(~(0LL) + i), ticks_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetValue(key, std::make_unique<base::Value>());
    EXPECT_TRUE(metadata.HasKey(key));
    const base::Value* const null_value = metadata.GetValue(key);
    EXPECT_TRUE(null_value);
    EXPECT_EQ(base::Value::Type::NONE, null_value->type());
    metadata.Clear();
  }
}

TEST(VideoFrameMetadata, PassMetadataViaIntermediary) {
  VideoFrameMetadata expected;
  for (int i = 0; i < VideoFrameMetadata::NUM_KEYS; ++i) {
    const VideoFrameMetadata::Key key = static_cast<VideoFrameMetadata::Key>(i);
    expected.SetInteger(key, i);
  }

  VideoFrameMetadata result;
  result.MergeMetadataFrom(&expected);

  for (int i = 0; i < VideoFrameMetadata::NUM_KEYS; ++i) {
    const VideoFrameMetadata::Key key = static_cast<VideoFrameMetadata::Key>(i);
    int value = -1;
    EXPECT_TRUE(result.GetInteger(key, &value));
    EXPECT_EQ(i, value);
  }
}

}  // namespace media
