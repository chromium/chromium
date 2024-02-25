// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_frame_metadata_mojom_traits.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace mojo {

// Deserializes has_field and field into a std::optional.
#define DESERIALIZE_INTO_OPT(field) \
  if (input.has_##field())          \
    output->field = input.field()

#define READ_AND_ASSIGN_OPT(type, field, FieldInCamelCase) \
  std::optional<type> field;                               \
  if (!input.Read##FieldInCamelCase(&field))               \
    return false;                                          \
                                                           \
  output->field = field

// static
bool StructTraits<media::mojom::VideoFrameMetadataDataView,
                  media::VideoFrameMetadata>::
    Read(media::mojom::VideoFrameMetadataDataView input,
         media::VideoFrameMetadata* output) {
  // int.
  DESERIALIZE_INTO_OPT(capture_counter);
  output->sub_capture_target_version = input.sub_capture_target_version();
  output->frame_sequence = input.frame_sequence();

  // bool.
  output->allow_overlay = input.allow_overlay();
  output->copy_required = input.copy_required();
  output->end_of_stream = input.end_of_stream();
  output->texture_owner = input.texture_owner();
  output->wants_promotion_hint = input.wants_promotion_hint();
  output->protected_video = input.protected_video();
  output->hw_protected = input.hw_protected();
  output->needs_detiling = input.needs_detiling();
  output->is_webgpu_compatible = input.is_webgpu_compatible();
  output->power_efficient = input.power_efficient();
  output->read_lock_fences_enabled = input.read_lock_fences_enabled();
  output->interactive_content = input.interactive_content();
  output->texture_origin_is_top_left = input.texture_origin_is_top_left();

  // double.
  DESERIALIZE_INTO_OPT(device_scale_factor);
  DESERIALIZE_INTO_OPT(page_scale_factor);
  DESERIALIZE_INTO_OPT(root_scroll_offset_x);
  DESERIALIZE_INTO_OPT(root_scroll_offset_y);
  DESERIALIZE_INTO_OPT(top_controls_visible_height);
  DESERIALIZE_INTO_OPT(frame_rate);
  DESERIALIZE_INTO_OPT(rtp_timestamp);

  READ_AND_ASSIGN_OPT(media::VideoTransformation, transformation,
                      Transformation);

  READ_AND_ASSIGN_OPT(base::UnguessableToken, overlay_plane_id, OverlayPlaneId);

  READ_AND_ASSIGN_OPT(gfx::Size, source_size, SourceSize);
  READ_AND_ASSIGN_OPT(gfx::Rect, capture_update_rect, CaptureUpdateRect);
  READ_AND_ASSIGN_OPT(gfx::Rect, region_capture_rect, RegionCaptureRect);

  READ_AND_ASSIGN_OPT(base::TimeTicks, receive_time, ReceiveTime);
  READ_AND_ASSIGN_OPT(base::TimeTicks, capture_begin_time, CaptureBeginTime);
  READ_AND_ASSIGN_OPT(base::TimeTicks, capture_end_time, CaptureEndTime);
  READ_AND_ASSIGN_OPT(base::TimeTicks, decode_begin_time, DecodeBeginTime);
  READ_AND_ASSIGN_OPT(base::TimeTicks, decode_end_time, DecodeEndTime);
  READ_AND_ASSIGN_OPT(base::TimeTicks, reference_time, ReferenceTime);

  READ_AND_ASSIGN_OPT(base::TimeDelta, processing_time, ProcessingTime);
  READ_AND_ASSIGN_OPT(base::TimeDelta, frame_duration, FrameDuration);
  READ_AND_ASSIGN_OPT(base::TimeDelta, wallclock_frame_duration,
                      WallclockFrameDuration);

  output->frame_sequence = input.frame_sequence();

  return true;
}

}  // namespace mojo
