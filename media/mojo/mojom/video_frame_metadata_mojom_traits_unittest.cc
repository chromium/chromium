// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_frame_metadata_mojom_traits.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/mojo/mojom/traits_test_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

namespace {

class VideoFrameMetadataStructTraitsTest
    : public testing::Test,
      public mojom::VideoFrameMetadataTraitsTestService {
 public:
  VideoFrameMetadataStructTraitsTest() = default;

  VideoFrameMetadataStructTraitsTest(
      const VideoFrameMetadataStructTraitsTest&) = delete;
  VideoFrameMetadataStructTraitsTest& operator=(
      const VideoFrameMetadataStructTraitsTest&) = delete;

 protected:
  mojo::Remote<mojom::VideoFrameMetadataTraitsTestService>
  GetTraitsTestRemote() {
    mojo::Remote<mojom::VideoFrameMetadataTraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  bool RoundTrip(const VideoFrameMetadata& in, VideoFrameMetadata* out) {
    mojo::Remote<mojom::VideoFrameMetadataTraitsTestService> remote =
        GetTraitsTestRemote();
    return remote->EchoVideoFrameMetadata(in, out);
  }

 private:
  void EchoVideoFrameMetadata(
      const VideoFrameMetadata& vfm,
      EchoVideoFrameMetadataCallback callback) override {
    std::move(callback).Run(vfm);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<VideoFrameMetadataTraitsTestService> traits_test_receivers_;
};

}  // namespace

TEST_F(VideoFrameMetadataStructTraitsTest, EmptyMetadata) {
  VideoFrameMetadata metadata_in;
  VideoFrameMetadata metadata_out;

  ASSERT_TRUE(RoundTrip(metadata_in, &metadata_out));

  EXPECT_FALSE(metadata_out.capture_counter.has_value());
  EXPECT_FALSE(metadata_out.capture_update_rect.has_value());
  EXPECT_FALSE(metadata_out.transformation.has_value());
  EXPECT_FALSE(metadata_out.allow_overlay);
  EXPECT_FALSE(metadata_out.copy_required);
  EXPECT_FALSE(metadata_out.end_of_stream);
  EXPECT_FALSE(metadata_out.texture_owner);
  EXPECT_FALSE(metadata_out.wants_promotion_hint);
  EXPECT_FALSE(metadata_out.protected_video);
  EXPECT_FALSE(metadata_out.hw_protected);
  EXPECT_FALSE(metadata_out.is_webgpu_compatible);
  EXPECT_FALSE(metadata_out.power_efficient);
  EXPECT_FALSE(metadata_out.read_lock_fences_enabled);
  EXPECT_FALSE(metadata_out.interactive_content);
  EXPECT_FALSE(metadata_out.overlay_plane_id.has_value());
  EXPECT_FALSE(metadata_out.device_scale_factor.has_value());
  EXPECT_FALSE(metadata_out.page_scale_factor.has_value());
  EXPECT_FALSE(metadata_out.root_scroll_offset_x.has_value());
  EXPECT_FALSE(metadata_out.root_scroll_offset_y.has_value());
  EXPECT_FALSE(metadata_out.top_controls_visible_height.has_value());
  EXPECT_FALSE(metadata_out.frame_rate.has_value());
  EXPECT_FALSE(metadata_out.rtp_timestamp.has_value());
  EXPECT_FALSE(metadata_out.receive_time.has_value());
  EXPECT_FALSE(metadata_out.capture_begin_time.has_value());
  EXPECT_FALSE(metadata_out.capture_end_time.has_value());
  EXPECT_FALSE(metadata_out.decode_begin_time.has_value());
  EXPECT_FALSE(metadata_out.decode_end_time.has_value());
  EXPECT_FALSE(metadata_out.reference_time.has_value());
  EXPECT_FALSE(metadata_out.processing_time.has_value());
  EXPECT_FALSE(metadata_out.frame_duration.has_value());
  EXPECT_FALSE(metadata_out.wallclock_frame_duration.has_value());
  EXPECT_FALSE(metadata_out.frame_sequence.has_value());
}

TEST_F(VideoFrameMetadataStructTraitsTest, ValidMetadata) {
  // Assign a non-default, distinct (when possible), value to all fields, and
  // make sure values are preserved across serialization.
  VideoFrameMetadata metadata_in;

  // ints
  metadata_in.capture_counter = 123;
  metadata_in.frame_sequence = 456;

  // gfx::Rects
  metadata_in.capture_update_rect = gfx::Rect(12, 34, 360, 480);

  // VideoTransformation
  metadata_in.transformation = VideoTransformation(VIDEO_ROTATION_90, true);

  // bools
  metadata_in.allow_overlay = true;
  metadata_in.copy_required = true;
  metadata_in.end_of_stream = true;
  metadata_in.texture_owner = true;
  metadata_in.wants_promotion_hint = true;
  metadata_in.protected_video = true;
  metadata_in.hw_protected = true;
  metadata_in.is_webgpu_compatible = true;
  metadata_in.power_efficient = true;
  metadata_in.read_lock_fences_enabled = true;
  metadata_in.interactive_content = true;

  // base::UnguessableTokens
  metadata_in.overlay_plane_id = base::UnguessableToken::Create();

  // doubles
  metadata_in.device_scale_factor = 2.0;
  metadata_in.page_scale_factor = 2.1;
  metadata_in.root_scroll_offset_x = 100.2;
  metadata_in.root_scroll_offset_y = 200.1;
  metadata_in.top_controls_visible_height = 25.5;
  metadata_in.frame_rate = 29.94;
  metadata_in.rtp_timestamp = 1.0;

  // base::TimeTicks
  base::TimeTicks now = base::TimeTicks::Now();
  metadata_in.receive_time = now + base::Milliseconds(10);
  metadata_in.capture_begin_time = now + base::Milliseconds(20);
  metadata_in.capture_end_time = now + base::Milliseconds(30);
  metadata_in.decode_begin_time = now + base::Milliseconds(40);
  metadata_in.decode_end_time = now + base::Milliseconds(50);
  metadata_in.reference_time = now + base::Milliseconds(60);

  // base::TimeDeltas
  metadata_in.processing_time = base::Milliseconds(500);
  metadata_in.frame_duration = base::Milliseconds(16);
  metadata_in.wallclock_frame_duration = base::Milliseconds(17);

  VideoFrameMetadata metadata_out;

  ASSERT_TRUE(RoundTrip(metadata_in, &metadata_out));

  EXPECT_EQ(metadata_in.capture_counter, metadata_out.capture_counter);
  EXPECT_EQ(metadata_in.capture_update_rect, metadata_out.capture_update_rect);
  EXPECT_EQ(metadata_in.transformation, metadata_out.transformation);
  EXPECT_EQ(metadata_in.allow_overlay, metadata_out.allow_overlay);
  EXPECT_EQ(metadata_in.copy_required, metadata_out.copy_required);
  EXPECT_EQ(metadata_in.end_of_stream, metadata_out.end_of_stream);
  EXPECT_EQ(metadata_in.texture_owner, metadata_out.texture_owner);
  EXPECT_EQ(metadata_in.wants_promotion_hint,
            metadata_out.wants_promotion_hint);
  EXPECT_EQ(metadata_in.protected_video, metadata_out.protected_video);
  EXPECT_EQ(metadata_in.hw_protected, metadata_out.hw_protected);
  EXPECT_EQ(metadata_in.is_webgpu_compatible,
            metadata_out.is_webgpu_compatible);
  EXPECT_EQ(metadata_in.power_efficient, metadata_out.power_efficient);
  EXPECT_EQ(metadata_in.read_lock_fences_enabled,
            metadata_out.read_lock_fences_enabled);
  EXPECT_EQ(metadata_in.interactive_content, metadata_out.interactive_content);
  EXPECT_EQ(metadata_in.overlay_plane_id, metadata_out.overlay_plane_id);
  EXPECT_EQ(metadata_in.device_scale_factor, metadata_out.device_scale_factor);
  EXPECT_EQ(metadata_in.page_scale_factor, metadata_out.page_scale_factor);
  EXPECT_EQ(metadata_in.root_scroll_offset_x,
            metadata_out.root_scroll_offset_x);
  EXPECT_EQ(metadata_in.root_scroll_offset_y,
            metadata_out.root_scroll_offset_y);
  EXPECT_EQ(metadata_in.top_controls_visible_height,
            metadata_out.top_controls_visible_height);
  EXPECT_EQ(metadata_in.frame_rate, metadata_out.frame_rate);
  EXPECT_EQ(metadata_in.rtp_timestamp, metadata_out.rtp_timestamp);
  EXPECT_EQ(metadata_in.receive_time, metadata_out.receive_time);
  EXPECT_EQ(metadata_in.capture_begin_time, metadata_out.capture_begin_time);
  EXPECT_EQ(metadata_in.capture_end_time, metadata_out.capture_end_time);
  EXPECT_EQ(metadata_in.decode_begin_time, metadata_out.decode_begin_time);
  EXPECT_EQ(metadata_in.decode_end_time, metadata_out.decode_end_time);
  EXPECT_EQ(metadata_in.reference_time, metadata_out.reference_time);
  EXPECT_EQ(metadata_in.processing_time, metadata_out.processing_time);
  EXPECT_EQ(metadata_in.frame_duration, metadata_out.frame_duration);
  EXPECT_EQ(metadata_in.wallclock_frame_duration,
            metadata_out.wallclock_frame_duration);
  EXPECT_EQ(metadata_in.frame_sequence, metadata_out.frame_sequence);
}

}  // namespace media
