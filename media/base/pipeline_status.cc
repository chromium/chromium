// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_status.h"

#include <ostream>

namespace media {

std::string PipelineStatusToString(const PipelineStatus& status) {
  return PipelineStatusTraits::ReadableCodeName(status.code());
}

std::ostream& operator<<(std::ostream& out, const PipelineStatus& status) {
  return out << PipelineStatusToString(status);
}

PipelineStatistics::PipelineStatistics() = default;
PipelineStatistics::PipelineStatistics(const PipelineStatistics& other) =
    default;
PipelineStatistics::~PipelineStatistics() = default;

bool operator==(const PipelineStatistics& first,
                const PipelineStatistics& second) {
  return first.audio_bytes_decoded == second.audio_bytes_decoded &&
         first.video_bytes_decoded == second.video_bytes_decoded &&
         first.video_frames_decoded == second.video_frames_decoded &&
         first.video_frames_dropped == second.video_frames_dropped &&
         first.video_frames_decoded_power_efficient ==
             second.video_frames_decoded_power_efficient &&
         first.audio_memory_usage == second.audio_memory_usage &&
         first.video_memory_usage == second.video_memory_usage &&
         first.video_keyframe_distance_average ==
             second.video_keyframe_distance_average &&
         first.video_frame_duration_average ==
             second.video_frame_duration_average &&
         first.video_pipeline_info == second.video_pipeline_info &&
         first.audio_pipeline_info == second.audio_pipeline_info;
}

bool operator!=(const PipelineStatistics& first,
                const PipelineStatistics& second) {
  return !(first == second);
}

}  // namespace media
