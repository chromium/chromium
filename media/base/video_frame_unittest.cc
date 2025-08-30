// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/video_frame.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <numeric>
#include <vector>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/color_plane_layout.h"
#include "media/base/limits.h"
#include "media/base/simple_sync_token_client.h"
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

// Returns a VideoFrameMetadata object with a value for each field.
media::VideoFrameMetadata GetFullVideoFrameMetadata() {
  // Assign a non-default, distinct (when possible), value to all fields, and
  // make sure values are preserved across serialization.
  media::VideoFrameMetadata metadata;

  // ints
  metadata.capture_counter = 123;

  // gfx::Rects
  metadata.capture_update_rect = gfx::Rect(12, 34, 360, 480);

  // media::VideoTransformation
  metadata.transformation = media::VIDEO_ROTATION_90;

  // bools
  metadata.allow_overlay = true;
  metadata.copy_required = true;
  metadata.end_of_stream = true;
  metadata.in_surface_view = true;
  metadata.wants_promotion_hint = true;
  metadata.protected_video = true;
  metadata.hw_protected = true;
  metadata.power_efficient = true;
  metadata.read_lock_fences_enabled = true;
  metadata.interactive_content = true;

  // base::UnguessableTokens
  metadata.tracking_token = base::UnguessableToken::Create();

  // doubles
  metadata.device_scale_factor = 2.0;
  metadata.page_scale_factor = 2.1;
  metadata.root_scroll_offset_x = 100.2;
  metadata.root_scroll_offset_y = 200.1;
  metadata.top_controls_visible_height = 25.5;
  metadata.frame_rate = 29.94;
  metadata.rtp_timestamp = 1.0;

  // base::TimeTicks
  base::TimeTicks now = base::TimeTicks::Now();
  metadata.receive_time = now + base::Milliseconds(10);
  metadata.capture_begin_time = now + base::Milliseconds(20);
  metadata.capture_end_time = now + base::Milliseconds(30);
  metadata.decode_begin_time = now + base::Milliseconds(40);
  metadata.decode_end_time = now + base::Milliseconds(50);
  metadata.reference_time = now + base::Milliseconds(60);

  // base::TimeDeltas
  metadata.processing_time = base::Milliseconds(500);
  metadata.frame_duration = base::Milliseconds(16);
  metadata.wallclock_frame_duration = base::Milliseconds(17);

  return metadata;
}

void VerifyVideoFrameMetadataEquality(const media::VideoFrameMetadata& a,
                                      const media::VideoFrameMetadata& b) {
  EXPECT_EQ(a.allow_overlay, b.allow_overlay);
  EXPECT_EQ(a.capture_begin_time, b.capture_begin_time);
  EXPECT_EQ(a.capture_end_time, b.capture_end_time);
  EXPECT_EQ(a.capture_counter, b.capture_counter);
  EXPECT_EQ(a.capture_update_rect, b.capture_update_rect);
  EXPECT_EQ(a.copy_required, b.copy_required);
  EXPECT_EQ(a.end_of_stream, b.end_of_stream);
  EXPECT_EQ(a.frame_duration, b.frame_duration);
  EXPECT_EQ(a.frame_rate, b.frame_rate);
  EXPECT_EQ(a.interactive_content, b.interactive_content);
  EXPECT_EQ(a.reference_time, b.reference_time);
  EXPECT_EQ(a.read_lock_fences_enabled, b.read_lock_fences_enabled);
  EXPECT_EQ(a.transformation, b.transformation);
  EXPECT_EQ(a.in_surface_view, b.in_surface_view);
  EXPECT_EQ(a.wants_promotion_hint, b.wants_promotion_hint);
  EXPECT_EQ(a.protected_video, b.protected_video);
  EXPECT_EQ(a.hw_protected, b.hw_protected);
  EXPECT_EQ(a.tracking_token, b.tracking_token);
  EXPECT_EQ(a.power_efficient, b.power_efficient);
  EXPECT_EQ(a.device_scale_factor, b.device_scale_factor);
  EXPECT_EQ(a.page_scale_factor, b.page_scale_factor);
  EXPECT_EQ(a.root_scroll_offset_x, b.root_scroll_offset_x);
  EXPECT_EQ(a.root_scroll_offset_y, b.root_scroll_offset_y);
  EXPECT_EQ(a.top_controls_visible_height, b.top_controls_visible_height);
  EXPECT_EQ(a.decode_begin_time, b.decode_begin_time);
  EXPECT_EQ(a.decode_end_time, b.decode_end_time);
  EXPECT_EQ(a.processing_time, b.processing_time);
  EXPECT_EQ(a.rtp_timestamp, b.rtp_timestamp);
  EXPECT_EQ(a.receive_time, b.receive_time);
  EXPECT_EQ(a.wallclock_frame_duration, b.wallclock_frame_duration);
}

}  // namespace

namespace media {

// Helper function that initializes a YV12 frame with white and black scan
// lines based on the |white_to_black| parameter.  If 0, then the entire
// frame will be black, if 1 then the entire frame will be white.
void InitializeYV12Frame(VideoFrame* frame, double white_to_black) {
  EXPECT_EQ(PIXEL_FORMAT_YV12, frame->format());
  const int first_black_row =
      static_cast<int>(frame->coded_size().height() * white_to_black);
  uint8_t* y_plane = frame->writable_data(VideoFrame::Plane::kY);
  for (int row = 0; row < frame->coded_size().height(); ++row) {
    int color = (row < first_black_row) ? 0xFF : 0x00;
    memset(y_plane, color, frame->stride(VideoFrame::Plane::kY));
    y_plane += frame->stride(VideoFrame::Plane::kY);
  }
  uint8_t* u_plane = frame->writable_data(VideoFrame::Plane::kU);
  uint8_t* v_plane = frame->writable_data(VideoFrame::Plane::kV);
  for (int row = 0; row < frame->coded_size().height(); row += 2) {
    memset(u_plane, 0x80, frame->stride(VideoFrame::Plane::kU));
    memset(v_plane, 0x80, frame->stride(VideoFrame::Plane::kV));
    u_plane += frame->stride(VideoFrame::Plane::kU);
    v_plane += frame->stride(VideoFrame::Plane::kV);
  }
}

// Given a |yv12_frame| this method converts the YV12 frame to RGBA and
// makes sure that all the pixels of the RBG frame equal |expect_rgb_color|.
void ExpectFrameColor(VideoFrame* yv12_frame, uint32_t expect_rgb_color) {
  ASSERT_EQ(PIXEL_FORMAT_YV12, yv12_frame->format());
  ASSERT_EQ(yv12_frame->stride(VideoFrame::Plane::kU),
            yv12_frame->stride(VideoFrame::Plane::kV));
  ASSERT_EQ(
      yv12_frame->coded_size().width() & (VideoFrame::kFrameSizeAlignment - 1),
      0u);
  ASSERT_EQ(
      yv12_frame->coded_size().height() & (VideoFrame::kFrameSizeAlignment - 1),
      0u);

  size_t bytes_per_row = yv12_frame->coded_size().width() * 4u;
  uint8_t* rgb_data = reinterpret_cast<uint8_t*>(
      base::AlignedAlloc(bytes_per_row * yv12_frame->coded_size().height() +
                             VideoFrame::kFrameSizePadding,
                         VideoFrame::kFrameAddressAlignment));

  libyuv::I420ToARGB(yv12_frame->data(VideoFrame::Plane::kY),
                     yv12_frame->stride(VideoFrame::Plane::kY),
                     yv12_frame->data(VideoFrame::Plane::kU),
                     yv12_frame->stride(VideoFrame::Plane::kU),
                     yv12_frame->data(VideoFrame::Plane::kV),
                     yv12_frame->stride(VideoFrame::Plane::kV), rgb_data,
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
// zero values.  Additionally, for the first plane verify the rows, row_bytes,
// and columns values are correct.
void ExpectFrameExtents(VideoPixelFormat format, const char* expected_hash) {
  const unsigned char kFillByte = 0x80;
  const int kWidth = 61;
  const int kHeight = 31;
  const base::TimeDelta kTimestamp = base::Microseconds(1337);

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
    EXPECT_TRUE(frame->columns(plane));

    memset(frame->writable_data(plane), kFillByte,
           frame->stride(plane) * frame->rows(plane));
  }

  EXPECT_EQ(VideoFrame::HexHashOfFrameForTesting(*frame,
                                                 /*visible_data_only=*/false),
            expected_hash);
}

TEST(VideoFrame, CreateFrame) {
  const int kWidth = 64;
  const int kHeight = 48;
  const base::TimeDelta kTimestamp = base::Microseconds(1337);

  // Create a YV12 Video Frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      PIXEL_FORMAT_YV12, size, gfx::Rect(size), size, kTimestamp);
  ASSERT_TRUE(frame.get());

  // Test VideoFrame implementation.
  EXPECT_EQ(PIXEL_FORMAT_YV12, frame->format());
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame.get(), 0.0f);
    ExpectFrameColor(frame.get(), 0xFF000000);
  }
  EXPECT_EQ(VideoFrame::HexHashOfFrameForTesting(*frame.get()),
            "48a14002453cf6ff6719661fc0715cbf1978214c182d1b4bbb9afb934051d630");
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame.get(), 1.0f);
    ExpectFrameColor(frame.get(), 0xFFFFFFFF);
  }
  EXPECT_EQ(VideoFrame::HexHashOfFrameForTesting(*frame.get()),
            "a08db3e63e9b8ca723142d7fb734716a3a2af9f0e655271eb5acc9d2c2088dbb");

  // Test single planar frame.
  frame = VideoFrame::CreateFrame(PIXEL_FORMAT_ARGB, size, gfx::Rect(size),
                                  size, kTimestamp);
  EXPECT_EQ(PIXEL_FORMAT_ARGB, frame->format());
  EXPECT_GE(frame->stride(VideoFrame::Plane::kARGB),
            static_cast<size_t>(frame->coded_size().width()));

  // Test double planar frame.
  frame = VideoFrame::CreateFrame(PIXEL_FORMAT_NV12, size, gfx::Rect(size),
                                  size, kTimestamp);
  EXPECT_EQ(PIXEL_FORMAT_NV12, frame->format());

  // Test an empty frame.
  frame = VideoFrame::CreateEOSFrame();
  EXPECT_TRUE(frame->metadata().end_of_stream);

  // Test an video hole frame.
  frame = VideoFrame::CreateVideoHoleFrame(base::UnguessableToken::Create(),
                                           size, kTimestamp);
  ASSERT_TRUE(frame);
}

TEST(VideoFrame, CreateZeroInitializedFrame) {
  const int kWidth = 2;
  const int kHeight = 2;
  const base::TimeDelta kTimestamp = base::Microseconds(1337);

  // Create a YV12 Video Frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateZeroInitializedFrame(
      PIXEL_FORMAT_YV12, size, gfx::Rect(size), size, kTimestamp);
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

  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
  ASSERT_TRUE(frame.get());
  EXPECT_TRUE(frame->IsMappable());

  // Test basic properties.
  EXPECT_EQ(0, frame->timestamp().InMicroseconds());
  EXPECT_FALSE(frame->metadata().end_of_stream);

  // Test |frame| properties.
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(kWidth, frame->coded_size().width());
  EXPECT_EQ(kHeight, frame->coded_size().height());

  // Test frames themselves.
  uint8_t* y_plane = frame->writable_data(VideoFrame::Plane::kY);
  for (int y = 0; y < frame->coded_size().height(); ++y) {
    EXPECT_EQ(0, memcmp(kExpectedYRow, y_plane, std::size(kExpectedYRow)));
    y_plane += frame->stride(VideoFrame::Plane::kY);
  }

  uint8_t* u_plane = frame->writable_data(VideoFrame::Plane::kU);
  uint8_t* v_plane = frame->writable_data(VideoFrame::Plane::kV);
  for (int y = 0; y < frame->coded_size().height() / 2; ++y) {
    EXPECT_EQ(0, memcmp(kExpectedUVRow, u_plane, std::size(kExpectedUVRow)));
    EXPECT_EQ(0, memcmp(kExpectedUVRow, v_plane, std::size(kExpectedUVRow)));
    u_plane += frame->stride(VideoFrame::Plane::kU);
    v_plane += frame->stride(VideoFrame::Plane::kV);
  }
}

static void FrameNoLongerNeededCallback(bool* triggered) {
  *triggered = true;
}

TEST(VideoFrame, DestructChainOfWrappedVideoFrames) {
  constexpr int kWidth = 4;
  constexpr int kHeight = 4;
  constexpr int kFramesInChain = 50000;
  auto frame = VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
  bool base_frame_done_callback_was_run = false;
  frame->AddDestructionObserver(base::BindOnce(
      &FrameNoLongerNeededCallback, &base_frame_done_callback_was_run));
  std::array<bool, kFramesInChain> wrapped_frame_done_callback_was_run = {};
  std::vector<scoped_refptr<VideoFrame>> frames;

  for (int i = 0; i < kFramesInChain; i++) {
    frames.push_back(frame);
    frame = VideoFrame::WrapVideoFrame(
        frame, frame->format(), frame->visible_rect(), frame->natural_size());
    frame->AddDestructionObserver(base::BindOnce(
        &FrameNoLongerNeededCallback, &wrapped_frame_done_callback_was_run[i]));
  }
  frames.clear();

  EXPECT_FALSE(base_frame_done_callback_was_run);
  EXPECT_FALSE(std::accumulate(wrapped_frame_done_callback_was_run.begin(),
                               wrapped_frame_done_callback_was_run.end(), true,
                               std::logical_and<bool>()));

  frame.reset();
  EXPECT_TRUE(base_frame_done_callback_was_run);
  EXPECT_TRUE(std::accumulate(wrapped_frame_done_callback_was_run.begin(),
                              wrapped_frame_done_callback_was_run.end(), true,
                              std::logical_and<bool>()));
}

TEST(VideoFrame, WrapVideoFrame) {
  const int kWidth = 4;
  const int kHeight = 4;
  const base::TimeDelta kFrameDuration = base::Microseconds(42);

  scoped_refptr<VideoFrame> frame, frame2;
  bool base_frame_done_callback_was_run = false;
  bool wrapped_frame_done_callback_was_run = false;
  {
    auto base_frame = VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
    ASSERT_TRUE(base_frame);

    gfx::Rect visible_rect(0, 0, 2, 2);
    gfx::Size natural_size = visible_rect.size();
    base_frame->metadata().frame_duration = kFrameDuration;
    frame = VideoFrame::WrapVideoFrame(base_frame, base_frame->format(),
                                       visible_rect, natural_size);
    base_frame->AddDestructionObserver(base::BindOnce(
        &FrameNoLongerNeededCallback, &base_frame_done_callback_was_run));
    ASSERT_TRUE(frame);
    EXPECT_EQ(base_frame->coded_size(), frame->coded_size());
    EXPECT_EQ(base_frame->data(VideoFrame::Plane::kY),
              frame->data(VideoFrame::Plane::kY));
    EXPECT_NE(base_frame->visible_rect(), frame->visible_rect());
    EXPECT_EQ(visible_rect, frame->visible_rect());
    EXPECT_NE(base_frame->natural_size(), frame->natural_size());
    EXPECT_EQ(natural_size, frame->natural_size());

    // Verify metadata was copied to the wrapped frame.
    EXPECT_EQ(*frame->metadata().frame_duration, kFrameDuration);

    // Verify the metadata copy was a deep copy.
    base_frame->clear_metadata();
    EXPECT_NE(base_frame->metadata().frame_duration.has_value(),
              frame->metadata().frame_duration.has_value());

    frame->AddDestructionObserver(base::BindOnce(
        &FrameNoLongerNeededCallback, &wrapped_frame_done_callback_was_run));

    visible_rect = gfx::Rect(0, 0, 1, 1);
    natural_size = visible_rect.size();
    frame2 = VideoFrame::WrapVideoFrame(frame, frame->format(), visible_rect,
                                        natural_size);
    ASSERT_TRUE(frame2);
    EXPECT_EQ(base_frame->coded_size(), frame2->coded_size());
    EXPECT_EQ(base_frame->data(VideoFrame::Plane::kY),
              frame2->data(VideoFrame::Plane::kY));
    EXPECT_NE(base_frame->visible_rect(), frame2->visible_rect());
    EXPECT_EQ(visible_rect, frame2->visible_rect());
    EXPECT_NE(base_frame->natural_size(), frame2->natural_size());
    EXPECT_EQ(natural_size, frame2->natural_size());
  }

  {
    auto base_frame = VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
    ASSERT_TRUE(base_frame);
    // WrapVideoFrame is successful with the visible_rect that is not contained
    // by |base_frame|'s visible rectangle, but contained by |base_frame|'s
    // coded size area.
    const gfx::Rect larger_visible_rect(0, 0, 3, 3);
    auto frame3 = VideoFrame::WrapVideoFrame(base_frame, base_frame->format(),
                                             larger_visible_rect,
                                             larger_visible_rect.size());
    ASSERT_TRUE(frame3);
    EXPECT_EQ(base_frame->coded_size(), frame3->coded_size());
    EXPECT_EQ(base_frame->data(VideoFrame::Plane::kY),
              frame3->data(VideoFrame::Plane::kY));
    EXPECT_NE(base_frame->visible_rect(), frame3->visible_rect());
    EXPECT_EQ(larger_visible_rect, frame3->visible_rect());
    EXPECT_NE(base_frame->natural_size(), frame3->natural_size());
    EXPECT_EQ(larger_visible_rect.size(), frame3->natural_size());
    // WrapVideoFrame() fails if the new visible rect is larger than
    // |base_frame|'s coded size area.
    const gfx::Rect too_large_visible_rect(0, 0, 5, 5);
    EXPECT_FALSE(VideoFrame::WrapVideoFrame(base_frame, base_frame->format(),
                                            too_large_visible_rect,
                                            too_large_visible_rect.size()));
    // WrapVideoFrame() fails if the new visible rect is not contained by
    // |base_frame|'s coded size area.
    const gfx::Rect non_contained_visible_rect(3, 3, 2, 2);
    EXPECT_FALSE(VideoFrame::WrapVideoFrame(base_frame, base_frame->format(),
                                            non_contained_visible_rect,
                                            non_contained_visible_rect.size()));
  }

  // At this point |base_frame| is held by |frame|, |frame2|.
  EXPECT_FALSE(base_frame_done_callback_was_run);
  EXPECT_FALSE(wrapped_frame_done_callback_was_run);

  // At this point |base_frame| is held by |frame2|, which also holds |frame|.
  frame.reset();
  EXPECT_FALSE(base_frame_done_callback_was_run);
  EXPECT_FALSE(wrapped_frame_done_callback_was_run);

  // Now all |base_frame| references should be released.
  frame2.reset();
  EXPECT_TRUE(base_frame_done_callback_was_run);
}

// Create a frame that wraps unowned memory.
TEST(VideoFrame, WrapExternalData) {
  std::array<uint8_t, 2 * 256 * 256> memory{};
  gfx::Size coded_size(256, 256);
  gfx::Rect visible_rect(coded_size);
  CreateTestY16Frame(coded_size, visible_rect, memory.data());
  auto timestamp = base::Milliseconds(1);
  auto frame =
      VideoFrame::WrapExternalData(PIXEL_FORMAT_Y16, coded_size, visible_rect,
                                   visible_rect.size(), memory, timestamp);

  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->timestamp(), timestamp);
  EXPECT_EQ(frame->data(VideoFrame::Plane::kY)[0], 0xff);
}

// Create a frame that wraps read-only shared memory.
TEST(VideoFrame, WrapSharedMemory) {
  const size_t kDataSize = 2 * 256 * 256;
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(kDataSize);
  ASSERT_TRUE(mapped_region.IsValid());
  gfx::Size coded_size(256, 256);
  gfx::Rect visible_rect(coded_size);
  CreateTestY16Frame(coded_size, visible_rect, mapped_region.mapping.memory());
  auto timestamp = base::Milliseconds(1);
  auto frame = VideoFrame::WrapExternalData(
      PIXEL_FORMAT_Y16, coded_size, visible_rect, visible_rect.size(),
      mapped_region.mapping.GetMemoryAsSpan<uint8_t>(), timestamp);
  EXPECT_EQ(frame->storage_type(), VideoFrame::STORAGE_UNOWNED_MEMORY);

  frame->BackWithSharedMemory(&mapped_region.region);
  EXPECT_EQ(frame->storage_type(), VideoFrame::STORAGE_SHMEM);
  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->timestamp(), timestamp);
  EXPECT_EQ(frame->data(VideoFrame::Plane::kY)[0], 0xff);
}

TEST(VideoFrame, WrapMappableSharedImage) {
  gfx::Size coded_size = gfx::Size(256, 256);
  gfx::Rect visible_rect(coded_size);
  auto timestamp = base::Milliseconds(1);
  scoped_refptr<gpu::TestSharedImageInterface> test_sii =
      base::MakeRefCounted<gpu::TestSharedImageInterface>();

  // Setting some default usage in order to get a mappable shared image.
  const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto shared_image = test_sii->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, coded_size, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(si_usage), "VideoFrameTest"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ);
  auto mailbox = shared_image->mailbox();
  auto frame = VideoFrame::WrapMappableSharedImage(
      std::move(shared_image), test_sii->GenVerifiedSyncToken(),
      base::DoNothing(), visible_rect, coded_size, timestamp);

  EXPECT_EQ(frame->layout().format(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(frame->layout().coded_size(), coded_size);
  EXPECT_EQ(frame->layout().num_planes(), 2u);
  EXPECT_EQ(frame->layout().is_multi_planar(), false);
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_EQ(frame->layout().planes()[i].stride,
              static_cast<size_t>(coded_size.width()));
  }
  EXPECT_EQ(frame->storage_type(), VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->timestamp(), timestamp);
  EXPECT_EQ(frame->HasSharedImage(), true);
  EXPECT_EQ(frame->HasReleaseMailboxCB(), true);
  EXPECT_EQ(frame->shared_image()->mailbox(), mailbox);
  EXPECT_TRUE(frame->is_mappable_si_enabled());

  // Wrapped MappableSI frames must propagate the information of the wrappee.
  auto wrapped_frame = VideoFrame::WrapVideoFrame(
      frame, frame->format(), visible_rect, visible_rect.size());
  ASSERT_NE(wrapped_frame, nullptr);
  EXPECT_EQ(wrapped_frame->storage_type(),
            VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  EXPECT_EQ(wrapped_frame->coded_size(), coded_size);
  EXPECT_EQ(wrapped_frame->visible_rect(), visible_rect);
  EXPECT_EQ(wrapped_frame->timestamp(), timestamp);
  EXPECT_EQ(wrapped_frame->HasSharedImage(), true);
  EXPECT_EQ(wrapped_frame->HasReleaseMailboxCB(), true);
  EXPECT_EQ(wrapped_frame->shared_image()->mailbox(), mailbox);
  EXPECT_TRUE(wrapped_frame->is_mappable_si_enabled());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST(VideoFrame, WrapExternalDmabufs) {
  gfx::Size coded_size = gfx::Size(256, 256);
  gfx::Rect visible_rect(coded_size);
  std::vector<size_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 100, 200};
  std::vector<size_t> sizes = {100, 50, 50};
  std::vector<ColorPlaneLayout> planes(strides.size());

  for (size_t i = 0; i < planes.size(); i++) {
    planes[i].stride = strides[i];
    planes[i].offset = offsets[i];
    planes[i].size = sizes[i];
  }
  auto timestamp = base::Milliseconds(1);
  auto layout =
      VideoFrameLayout::CreateWithPlanes(PIXEL_FORMAT_I420, coded_size, planes);
  ASSERT_TRUE(layout);
  std::vector<base::ScopedFD> dmabuf_fds(3u);
  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, visible_rect, visible_rect.size(), std::move(dmabuf_fds),
      timestamp);

  EXPECT_EQ(frame->layout().format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(frame->layout().coded_size(), coded_size);
  EXPECT_EQ(frame->layout().num_planes(), 3u);
  EXPECT_EQ(frame->layout().is_multi_planar(), false);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(frame->layout().planes()[i].stride, strides[i]);
    EXPECT_EQ(frame->layout().planes()[i].offset, offsets[i]);
    EXPECT_EQ(frame->layout().planes()[i].size, sizes[i]);
  }
  EXPECT_TRUE(frame->HasDmaBufs());
  EXPECT_EQ(frame->NumDmabufFds(), 3u);
  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->timestamp(), timestamp);

  // Wrapped DMABUF frames must share the same memory as their wrappee.
  auto wrapped_frame = VideoFrame::WrapVideoFrame(
      frame, frame->format(), visible_rect, visible_rect.size());
  ASSERT_NE(wrapped_frame, nullptr);
  ASSERT_EQ(frame->NumDmabufFds(), wrapped_frame->NumDmabufFds());
  for (size_t i = 0; i < frame->NumDmabufFds(); ++i) {
    ASSERT_EQ(frame->GetDmabufFd(i), wrapped_frame->GetDmabufFd(i));
  }

  // Multi-level wrapping should share same memory as well.
  auto wrapped_frame2 = VideoFrame::WrapVideoFrame(
      wrapped_frame, frame->format(), visible_rect, visible_rect.size());
  ASSERT_NE(wrapped_frame2, nullptr);
  ASSERT_EQ(frame->NumDmabufFds(), wrapped_frame2->NumDmabufFds());
  for (size_t i = 0; i < frame->NumDmabufFds(); ++i) {
    ASSERT_EQ(frame->GetDmabufFd(i), wrapped_frame2->GetDmabufFd(i));
  }
  ASSERT_EQ(wrapped_frame->NumDmabufFds(), wrapped_frame2->NumDmabufFds());
  for (size_t i = 0; i < wrapped_frame2->NumDmabufFds(); ++i) {
    ASSERT_EQ(wrapped_frame->GetDmabufFd(i), wrapped_frame2->GetDmabufFd(i));
  }
}
#endif

// Ensure each frame is properly sized and allocated.  Will trigger OOB reads
// and writes as well as incorrect frame hashes otherwise.
TEST(VideoFrame, CheckFrameExtents) {
  // Each call consists of a Format and the expected hash of all
  // planes if filled with kFillByte (defined in ExpectFrameExtents).
  ExpectFrameExtents(
      PIXEL_FORMAT_YV12,
      "cdf392577e7dced37c10e986b82be9aaabdfe32a3e8c1e132c9986a533447740");
  ExpectFrameExtents(
      PIXEL_FORMAT_I422,
      "df513a840bbb43915da7b3d00c1191ce3f46d6e657db5ab7f65e3f879c6eded0");
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
    auto si_size = gfx::Size(10, 10);
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
        base::BindOnce(&TextureCallback, &called_sync_token),
        si_size,             // coded_size
        gfx::Rect(si_size),  // visible_rect
        si_size,             // natural_size
        base::TimeDelta());  // timestamp
    EXPECT_EQ(PIXEL_FORMAT_ARGB, frame->format());
    EXPECT_EQ(VideoFrame::STORAGE_OPAQUE, frame->storage_type());
    EXPECT_TRUE(frame->HasSharedImage());
  }
  // Nobody set a sync point to |frame|, so |frame| set |called_sync_token|
  // cleared to default value.
  EXPECT_FALSE(called_sync_token.HasData());
}

// Verify the gpu::MailboxHolder::ReleaseCallback is called when VideoFrame is
// destroyed with the release sync point, which was updated by clients.
// (i.e. the compositor, webgl).
TEST(VideoFrame,
     TexturesNoLongerNeededCallbackAfterTakingAndReleasingMailboxes) {
  const gpu::CommandBufferNamespace kNamespace =
      gpu::CommandBufferNamespace::GPU_IO;
  const gpu::CommandBufferId kCommandBufferId =
      gpu::CommandBufferId::FromUnsafeValue(0x123);
  auto si_size = gfx::Size(10, 10);
  gpu::SharedImageMetadata metadata;
  metadata.format = viz::SinglePlaneFormat::kRGBA_8888;
  metadata.size = si_size;
  metadata.color_space = gfx::ColorSpace::CreateSRGB();
  metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
  metadata.alpha_type = kOpaque_SkAlphaType;
  metadata.usage = gpu::SharedImageUsageSet();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu::ClientSharedImage::CreateForTesting(metadata);

  gpu::SyncToken sync_token(kNamespace, kCommandBufferId, 7);
  sync_token.SetVerifyFlush();
  uint32_t target = shared_image->GetTextureTarget();
  gpu::SyncToken release_sync_token(kNamespace, kCommandBufferId, 111);
  release_sync_token.SetVerifyFlush();

  gpu::SyncToken called_sync_token;
  {
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapSharedImage(
        PIXEL_FORMAT_I420, shared_image, sync_token,
        base::BindOnce(&TextureCallback, &called_sync_token),
        si_size,             // coded_size
        gfx::Rect(si_size),  // visible_rect
        si_size,             // natural_size
        base::TimeDelta());  // timestamp

    EXPECT_EQ(VideoFrame::STORAGE_OPAQUE, frame->storage_type());
    EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
    EXPECT_EQ(3u, VideoFrame::NumPlanes(frame->format()));
    EXPECT_TRUE(frame->HasSharedImage());
    EXPECT_EQ(shared_image->mailbox().name[0],
              frame->shared_image()->mailbox().name[0]);
    EXPECT_EQ(target, frame->shared_image()->GetTextureTarget());
    EXPECT_EQ(sync_token, frame->acquire_sync_token());

    SimpleSyncTokenClient client(release_sync_token);
    frame->UpdateReleaseSyncToken(&client);
    EXPECT_EQ(sync_token, frame->acquire_sync_token());
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

TEST(VideoFrame, CreateFrame_OddSize) {
  constexpr gfx::Size odd_size(677, 287);
  constexpr auto kTimestamp = base::TimeDelta();

  // I420A frames should have the Y, A planes aligned upward to an even size,
  // since it's required by the subsampled U,V planes.
  auto frame = VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420A, odd_size, gfx::Rect(odd_size), odd_size, kTimestamp);
  ASSERT_TRUE(frame);
  EXPECT_EQ(gfx::Size(678, 288), frame->coded_size());
  for (int plane = 0; plane < 4; plane++) {
    // CreateFrame() will always allocate to an even coded size for non-sampled
    // planes, so the visible row data should be less than the coded row data.
    if (plane == VideoFrame::Plane::kY || plane == VideoFrame::Plane::kA) {
      EXPECT_LT(frame->GetVisibleRowBytes(plane), frame->row_bytes(plane));
      EXPECT_LT(frame->GetVisibleRows(plane), frame->rows(plane));
    } else {
      EXPECT_EQ(frame->GetVisibleRowBytes(plane), frame->row_bytes(plane));
      EXPECT_EQ(frame->GetVisibleRows(plane), frame->rows(plane));
    }
  }

  // I444A frames don't have subsampling, so no adjustment is needed.
  frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I444A, odd_size,
                                  gfx::Rect(odd_size), odd_size, kTimestamp);
  ASSERT_TRUE(frame);
  EXPECT_EQ(odd_size, frame->coded_size());
  for (int plane = 0; plane < 4; plane++) {
    EXPECT_EQ(frame->GetVisibleRowBytes(plane), frame->row_bytes(plane));
    EXPECT_EQ(frame->GetVisibleRows(plane), frame->rows(plane));
  }
}

TEST(VideoFrame, RowBytes) {
  constexpr gfx::Size kCodedSize(16, 14);
  constexpr gfx::Rect kVisibleRect(4, 4, 8, 8);

  // Just spot test I420A for simplicity.
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateFrame(PIXEL_FORMAT_I420A, kCodedSize, kVisibleRect,
                              kVisibleRect.size(), base::TimeDelta());
  ASSERT_TRUE(frame);
  ASSERT_EQ(frame->row_bytes(VideoFrame::Plane::kY), kCodedSize.width());
  ASSERT_EQ(frame->row_bytes(VideoFrame::Plane::kU), kCodedSize.width() / 2);
  ASSERT_EQ(frame->row_bytes(VideoFrame::Plane::kV), kCodedSize.width() / 2);
  ASSERT_EQ(frame->row_bytes(VideoFrame::Plane::kA), kCodedSize.width());
  ASSERT_EQ(frame->rows(VideoFrame::Plane::kY), kCodedSize.height());
  ASSERT_EQ(frame->rows(VideoFrame::Plane::kU), kCodedSize.height() / 2);
  ASSERT_EQ(frame->rows(VideoFrame::Plane::kV), kCodedSize.height() / 2);
  ASSERT_EQ(frame->rows(VideoFrame::Plane::kA), kCodedSize.height());

  ASSERT_EQ(frame->GetVisibleRowBytes(VideoFrame::Plane::kY),
            kVisibleRect.width());
  ASSERT_EQ(frame->GetVisibleRowBytes(VideoFrame::Plane::kU),
            kVisibleRect.width() / 2);
  ASSERT_EQ(frame->GetVisibleRowBytes(VideoFrame::Plane::kV),
            kVisibleRect.width() / 2);
  ASSERT_EQ(frame->GetVisibleRowBytes(VideoFrame::Plane::kA),
            kVisibleRect.width());
  ASSERT_EQ(frame->GetVisibleRows(VideoFrame::Plane::kY),
            kVisibleRect.height());
  ASSERT_EQ(frame->GetVisibleRows(VideoFrame::Plane::kU),
            kVisibleRect.height() / 2);
  ASSERT_EQ(frame->GetVisibleRows(VideoFrame::Plane::kV),
            kVisibleRect.height() / 2);
  ASSERT_EQ(frame->GetVisibleRows(VideoFrame::Plane::kA),
            kVisibleRect.height());
}

TEST(VideoFrame, AllocationSize_OddSize) {
  const gfx::Size size(3, 5);

  for (unsigned int i = 1u; i <= PIXEL_FORMAT_MAX; ++i) {
    const VideoPixelFormat format = static_cast<VideoPixelFormat>(i);
    switch (format) {
      case PIXEL_FORMAT_YUV444P10:
      case PIXEL_FORMAT_YUV444P12:
      case PIXEL_FORMAT_P410LE:
        EXPECT_EQ(90u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_YUV422AP10:
        EXPECT_EQ(100u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_RGBAF16:
      case PIXEL_FORMAT_YUV444AP10:
        EXPECT_EQ(120u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_YUV420AP10:
        EXPECT_EQ(84u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_YUV422P10:
      case PIXEL_FORMAT_YUV422P12:
      case PIXEL_FORMAT_P210LE:
        EXPECT_EQ(70u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_I444:
      case PIXEL_FORMAT_NV24:
      case PIXEL_FORMAT_RGB24:
        EXPECT_EQ(45u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_YUV420P10:
      case PIXEL_FORMAT_YUV420P12:
      case PIXEL_FORMAT_P010LE:
        EXPECT_EQ(54u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_Y16:
      case PIXEL_FORMAT_UYVY:
      case PIXEL_FORMAT_YUY2:
        EXPECT_EQ(30u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_NV16:
      case PIXEL_FORMAT_I422:
        EXPECT_EQ(35u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_YV12:
      case PIXEL_FORMAT_I420:
      case PIXEL_FORMAT_NV12:
      case PIXEL_FORMAT_NV21:
        EXPECT_EQ(27u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_I444A:
      case PIXEL_FORMAT_ARGB:
      case PIXEL_FORMAT_BGRA:
      case PIXEL_FORMAT_XRGB:
      case PIXEL_FORMAT_ABGR:
      case PIXEL_FORMAT_XBGR:
      case PIXEL_FORMAT_XR30:
      case PIXEL_FORMAT_XB30:
        EXPECT_EQ(60u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_NV12A:
      case PIXEL_FORMAT_I420A:
        EXPECT_EQ(42u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_I422A:
        EXPECT_EQ(50u, VideoFrame::AllocationSize(format, size))
            << VideoPixelFormatToString(format);
        break;
      case PIXEL_FORMAT_MJPEG:
      case PIXEL_FORMAT_UNKNOWN:
        continue;
    }
  }
}

// Test ensures we don't overflow on 32-bit platforms.
TEST(VideoFrame, NoFrameSizeExceedsUint32) {
  const int max_dimension = std::sqrt(limits::kMaxCanvas);
  const auto max_size = gfx::Size(max_dimension, max_dimension);
  for (unsigned int i = 1u; i <= PIXEL_FORMAT_MAX; ++i) {
    // Deprecated pixel formats.
    if (i == 13 || i == 15 || i == 16 || i == 18 || i == 20 || i == 25) {
      continue;
    }

    const auto format = static_cast<VideoPixelFormat>(i);

    ASSERT_TRUE(
        VideoFrame::IsValidConfig(format, VideoFrame::STORAGE_UNOWNED_MEMORY,
                                  max_size, gfx::Rect(max_size), max_size));

    base::CheckedNumeric<uint32_t> allocation_size =
        VideoFrame::AllocationSize(format, max_size);
    ASSERT_TRUE(allocation_size.IsValid());
  }
}

TEST(VideoFrame, WrapExternalDataWithInvalidLayout) {
  auto coded_size = gfx::Size(320, 180);

  std::vector<size_t> strides = {384, 192, 192};
  std::vector<size_t> offsets = {0, 200, 300};
  std::vector<size_t> sizes = {200, 100, 100};
  std::vector<ColorPlaneLayout> planes(strides.size());
  for (size_t i = 0; i < strides.size(); i++) {
    planes[i].stride = strides[i];
    planes[i].offset = offsets[i];
    planes[i].size = sizes[i];
  }

  auto layout =
      VideoFrameLayout::CreateWithPlanes(PIXEL_FORMAT_I420, coded_size, planes);
  ASSERT_TRUE(layout.has_value());

  // Validate single plane size exceeds data size.
  std::vector<uint8_t> small_data(1);
  auto frame = VideoFrame::WrapExternalDataWithLayout(
      *layout, gfx::Rect(coded_size), coded_size, small_data,
      base::TimeDelta());
  ASSERT_FALSE(frame);

  // Validate sum of planes exceeds data size.
  std::vector<uint8_t> medium_data(sizes[0] + sizes[1]);
  frame = VideoFrame::WrapExternalDataWithLayout(*layout, gfx::Rect(coded_size),
                                                 coded_size, medium_data,
                                                 base::TimeDelta());
  ASSERT_FALSE(frame);

  // Validate offset exceeds plane size.
  planes[0].offset = 201;
  layout =
      VideoFrameLayout::CreateWithPlanes(PIXEL_FORMAT_I420, coded_size, planes);
  ASSERT_TRUE(layout.has_value());
  std::vector<uint8_t> other_data(sizes[0]);
  frame = VideoFrame::WrapExternalDataWithLayout(*layout, gfx::Rect(coded_size),
                                                 coded_size, other_data,
                                                 base::TimeDelta());
  ASSERT_FALSE(frame);
}

TEST(VideoFrameMetadata, MergeMetadata) {
  VideoFrameMetadata reference_metadata = GetFullVideoFrameMetadata();
  VideoFrameMetadata full_metadata = reference_metadata;
  VideoFrameMetadata empty_metadata;

  // Merging empty metadata into full metadata should be a no-op.
  full_metadata.MergeMetadataFrom(empty_metadata);
  VerifyVideoFrameMetadataEquality(full_metadata, reference_metadata);

  // Merging full metadata into empty metadata should fill it up.
  empty_metadata.MergeMetadataFrom(full_metadata);
  VerifyVideoFrameMetadataEquality(empty_metadata, reference_metadata);
}

TEST(VideoFrameMetadata, ClearTextureMetadata) {
  VideoFrameMetadata reference_md = GetFullVideoFrameMetadata();
  reference_md.is_webgpu_compatible = true;
  reference_md.read_lock_fences_enabled = true;

  VideoFrameMetadata copy_md;
  copy_md.MergeMetadataFrom(reference_md);

  copy_md.ClearTextureFrameMetadata();
  EXPECT_FALSE(copy_md.is_webgpu_compatible);
  EXPECT_FALSE(copy_md.read_lock_fences_enabled);

  reference_md.is_webgpu_compatible = false;
  reference_md.read_lock_fences_enabled = false;
  VerifyVideoFrameMetadataEquality(copy_md, reference_md);
}

TEST(VideoFrameMetadata, PartialMergeMetadata) {
  VideoFrameMetadata full_metadata = GetFullVideoFrameMetadata();

  const gfx::Rect kTempRect{100, 200, 300, 400};
  const base::TimeTicks kTempTicks = base::TimeTicks::Now() + base::Seconds(2);
  const base::TimeDelta kTempDelta = base::Milliseconds(31415);

  VideoFrameMetadata partial_metadata;
  partial_metadata.capture_update_rect = kTempRect;
  partial_metadata.reference_time = kTempTicks;
  partial_metadata.processing_time = kTempDelta;
  partial_metadata.allow_overlay = false;

  // Merging partial metadata into full metadata partially override it.
  full_metadata.MergeMetadataFrom(partial_metadata);

  EXPECT_EQ(partial_metadata.capture_update_rect, kTempRect);
  EXPECT_EQ(partial_metadata.reference_time, kTempTicks);
  EXPECT_EQ(partial_metadata.processing_time, kTempDelta);
  EXPECT_EQ(partial_metadata.allow_overlay, false);
}

TEST(VideoFrame, AccessPlaneDataSpans) {
  for (auto format :
       {PIXEL_FORMAT_XRGB, PIXEL_FORMAT_I420, PIXEL_FORMAT_NV12}) {
    gfx::Size coded_size(100, 100);
    gfx::Rect visible_rect(10, 10, 60, 20);
    std::vector<uint8_t> pixels;
    pixels.resize(coded_size.GetArea() * 4);

    auto timestamp = base::Milliseconds(0);
    auto frame =
        VideoFrame::WrapExternalData(format, coded_size, visible_rect,
                                     visible_rect.size(), pixels, timestamp);

    int plane_offset = 0;
    for (size_t plane = 0; plane < VideoFrame::NumPlanes(format); ++plane) {
      auto sample_size = VideoFrame::SampleSize(format, plane);
      size_t bytes_per_pixel = VideoFrame::BytesPerElement(format, plane);
      auto plane_span = frame->GetVisiblePlaneData(plane);
      auto writable_plane_span = frame->GetWritableVisiblePlaneData(plane);
      EXPECT_EQ(
          plane_span.data(),
          pixels.data() + plane_offset +
              visible_rect.y() / sample_size.height() * frame->stride(plane) +
              visible_rect.x() / sample_size.width() * bytes_per_pixel)
          << " format: " << format << " plane: " << plane;
      EXPECT_GE(
          static_cast<int>(plane_span.size()),
          VideoFrame::PlaneSize(format, plane, visible_rect.size()).GetArea())
          << " format: " << format << " plane: " << plane;
      EXPECT_EQ(plane_span.data(), writable_plane_span.data());
      EXPECT_EQ(writable_plane_span.size(), plane_span.size());

      plane_offset +=
          VideoFrame::PlaneSize(format, plane, coded_size).GetArea();
    }
  }
}

TEST(VideoFrame, WrappedPlaneDataAccess) {
  VideoPixelFormat format = PIXEL_FORMAT_I420;
  gfx::Size coded_size(100, 100);
  gfx::Rect visible_rect(10, 10, 60, 20);
  const size_t data_size = coded_size.GetArea();
  std::vector<uint8_t> y_pixels(data_size);
  std::vector<uint8_t> u_pixels(data_size);
  std::vector<uint8_t> v_pixels;

  auto timestamp = base::Milliseconds(0);
  auto frame = VideoFrame::WrapExternalYuvData(
      format, coded_size, visible_rect, visible_rect.size(),
      /* stride Y */ 100,
      /* stride U */ 100,
      /* stride V */ 100,
      /* Y plane */ y_pixels,
      /* U plane */ base::span(u_pixels.data(), 0u),
      /* V plane */ v_pixels, timestamp);

  EXPECT_EQ(frame->data(VideoFrame::Plane::kY), y_pixels.data());
  EXPECT_EQ(frame->data_span(VideoFrame::Plane::kY).data(), y_pixels.data());
  EXPECT_EQ(frame->data_span(VideoFrame::Plane::kY).size(), y_pixels.size());
  EXPECT_EQ(frame->data(VideoFrame::Plane::kU), nullptr);
  EXPECT_TRUE(frame->data_span(VideoFrame::Plane::kU).empty());
  EXPECT_EQ(frame->data(VideoFrame::Plane::kV), nullptr);
  EXPECT_TRUE(frame->data_span(VideoFrame::Plane::kV).empty());
}
}  // namespace media
