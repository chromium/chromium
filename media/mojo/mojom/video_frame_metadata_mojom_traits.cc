// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_frame_metadata_mojom_traits.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace mojo {

// Deserializes has_field and field into a base::Optional.
#define DESERIALIZE_INTO_OPT(field) \
  if (input.has_##field())          \
    output->field = input.field()

#define READ_AND_ASSIGN_OPT(type, field, FieldInCamelCase) \
  base::Optional<type> field;                              \
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

  // bool.
  output->allow_overlay = input.allow_overlay();
  output->end_of_stream = input.end_of_stream();
  output->texture_owner = input.texture_owner();
  output->wants_promotion_hint = input.wants_promotion_hint();
  output->protected_video = input.protected_video();
  output->hw_protected = input.hw_protected();
  output->power_efficient = input.power_efficient();
  output->read_lock_fences_enabled = input.read_lock_fences_enabled();
  output->interactive_content = input.interactive_content();

  // double.
  DESERIALIZE_INTO_OPT(device_scale_factor);
  DESERIALIZE_INTO_OPT(page_scale_factor);
  DESERIALIZE_INTO_OPT(root_scroll_offset_x);
  DESERIALIZE_INTO_OPT(root_scroll_offset_y);
  DESERIALIZE_INTO_OPT(top_controls_visible_height);
  DESERIALIZE_INTO_OPT(frame_rate);
  DESERIALIZE_INTO_OPT(rtp_timestamp);

  // unsigned int.
  output->hw_protected_validation_id = input.hw_protected_validation_id();

  READ_AND_ASSIGN_OPT(media::VideoTransformation, transformation,
                      Transformation);

  if (input.has_copy_mode()) {
    media::VideoFrameMetadata::CopyMode copy_mode;
    if (!input.ReadCopyMode(&copy_mode))
      return false;
    output->copy_mode = copy_mode;
  }

  READ_AND_ASSIGN_OPT(base::UnguessableToken, overlay_plane_id, OverlayPlaneId);

  READ_AND_ASSIGN_OPT(gfx::Rect, capture_update_rect, CaptureUpdateRect);

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

  return true;
}

}  // namespace mojo
