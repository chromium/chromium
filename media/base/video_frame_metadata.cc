// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_metadata.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/util/values/values_util.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

VideoFrameMetadata::VideoFrameMetadata() = default;

VideoFrameMetadata::VideoFrameMetadata(const VideoFrameMetadata& other) =
    default;

#define MERGE_FIELD(a, source) \
  if (source.a)                \
  this->a = source.a

void VideoFrameMetadata::MergeMetadataFrom(
    const VideoFrameMetadata& metadata_source) {
  MERGE_FIELD(allow_overlay, metadata_source);
  MERGE_FIELD(capture_begin_time, metadata_source);
  MERGE_FIELD(capture_end_time, metadata_source);
  MERGE_FIELD(capture_counter, metadata_source);
  MERGE_FIELD(capture_update_rect, metadata_source);
  MERGE_FIELD(copy_mode, metadata_source);
  MERGE_FIELD(end_of_stream, metadata_source);
  MERGE_FIELD(frame_duration, metadata_source);
  MERGE_FIELD(frame_rate, metadata_source);
  MERGE_FIELD(interactive_content, metadata_source);
  MERGE_FIELD(reference_time, metadata_source);
  MERGE_FIELD(read_lock_fences_enabled, metadata_source);
  MERGE_FIELD(transformation, metadata_source);
  MERGE_FIELD(texture_owner, metadata_source);
  MERGE_FIELD(wants_promotion_hint, metadata_source);
  MERGE_FIELD(protected_video, metadata_source);
  MERGE_FIELD(hw_protected, metadata_source);
  MERGE_FIELD(overlay_plane_id, metadata_source);
  MERGE_FIELD(power_efficient, metadata_source);
  MERGE_FIELD(device_scale_factor, metadata_source);
  MERGE_FIELD(page_scale_factor, metadata_source);
  MERGE_FIELD(root_scroll_offset_x, metadata_source);
  MERGE_FIELD(root_scroll_offset_y, metadata_source);
  MERGE_FIELD(top_controls_visible_height, metadata_source);
  MERGE_FIELD(decode_begin_time, metadata_source);
  MERGE_FIELD(decode_end_time, metadata_source);
  MERGE_FIELD(processing_time, metadata_source);
  MERGE_FIELD(rtp_timestamp, metadata_source);
  MERGE_FIELD(receive_time, metadata_source);
  MERGE_FIELD(wallclock_frame_duration, metadata_source);
  MERGE_FIELD(maximum_composition_delay_in_frames, metadata_source);
  MERGE_FIELD(hw_protected_validation_id, metadata_source);
}

}  // namespace media
