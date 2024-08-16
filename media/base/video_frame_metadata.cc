// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_metadata.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

VideoFrameMetadata::VideoFrameMetadata() = default;

VideoFrameMetadata::VideoFrameMetadata(const VideoFrameMetadata& other) =
    default;

void VideoFrameMetadata::MergeMetadataFrom(
    const VideoFrameMetadata& metadata_source) {
  static const VideoFrameMetadata kDefaultMetadata;

#define MERGE_OPTIONAL_FIELD(a, source) \
  if (source.a)                         \
  this->a = source.a

#define MERGE_VALUE_FIELD(a, source)  \
  if (source.a != kDefaultMetadata.a) \
  this->a = source.a

  MERGE_VALUE_FIELD(allow_overlay, metadata_source);
  MERGE_OPTIONAL_FIELD(capture_begin_time, metadata_source);
  MERGE_OPTIONAL_FIELD(capture_end_time, metadata_source);
  MERGE_OPTIONAL_FIELD(capture_counter, metadata_source);
  MERGE_OPTIONAL_FIELD(capture_update_rect, metadata_source);
  MERGE_OPTIONAL_FIELD(source_size, metadata_source);
  MERGE_OPTIONAL_FIELD(region_capture_rect, metadata_source);
  MERGE_VALUE_FIELD(sub_capture_target_version, metadata_source);
  MERGE_OPTIONAL_FIELD(copy_required, metadata_source);
  MERGE_VALUE_FIELD(end_of_stream, metadata_source);
  MERGE_OPTIONAL_FIELD(frame_duration, metadata_source);
  MERGE_OPTIONAL_FIELD(frame_rate, metadata_source);
  MERGE_VALUE_FIELD(interactive_content, metadata_source);
  MERGE_OPTIONAL_FIELD(reference_time, metadata_source);
  MERGE_VALUE_FIELD(read_lock_fences_enabled, metadata_source);
  MERGE_OPTIONAL_FIELD(transformation, metadata_source);
  MERGE_VALUE_FIELD(texture_owner, metadata_source);
  MERGE_VALUE_FIELD(wants_promotion_hint, metadata_source);
  MERGE_VALUE_FIELD(dcomp_surface, metadata_source);
  MERGE_VALUE_FIELD(protected_video, metadata_source);
  MERGE_VALUE_FIELD(hw_protected, metadata_source);
  MERGE_VALUE_FIELD(needs_detiling, metadata_source);
  MERGE_VALUE_FIELD(is_webgpu_compatible, metadata_source);
#if BUILDFLAG(USE_VAAPI)
  MERGE_OPTIONAL_FIELD(hw_va_protected_session_id, metadata_source);
#endif
  MERGE_OPTIONAL_FIELD(overlay_plane_id, metadata_source);
  MERGE_VALUE_FIELD(power_efficient, metadata_source);
  MERGE_VALUE_FIELD(texture_origin_is_top_left, metadata_source);
  MERGE_OPTIONAL_FIELD(device_scale_factor, metadata_source);
  MERGE_OPTIONAL_FIELD(page_scale_factor, metadata_source);
  MERGE_OPTIONAL_FIELD(root_scroll_offset_x, metadata_source);
  MERGE_OPTIONAL_FIELD(root_scroll_offset_y, metadata_source);
  MERGE_OPTIONAL_FIELD(top_controls_visible_height, metadata_source);
  MERGE_OPTIONAL_FIELD(decode_begin_time, metadata_source);
  MERGE_OPTIONAL_FIELD(decode_end_time, metadata_source);
  MERGE_OPTIONAL_FIELD(processing_time, metadata_source);
  MERGE_OPTIONAL_FIELD(rtp_timestamp, metadata_source);
  MERGE_OPTIONAL_FIELD(receive_time, metadata_source);
  MERGE_OPTIONAL_FIELD(wallclock_frame_duration, metadata_source);
  MERGE_OPTIONAL_FIELD(maximum_composition_delay_in_frames, metadata_source);
  MERGE_OPTIONAL_FIELD(frame_sequence, metadata_source);

#undef MERGE_VALUE_FIELD
#undef MERGE_OPTIONAL_FIELD
}

void VideoFrameMetadata::ClearTextureFrameMetadata() {
  is_webgpu_compatible = false;
  texture_origin_is_top_left = true;
  read_lock_fences_enabled = false;
}

}  // namespace media
