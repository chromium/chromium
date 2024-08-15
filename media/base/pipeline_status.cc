// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_status.h"

#include <ostream>

namespace media {

std::string PipelineStatusToString(const PipelineStatus& status) {
#define STRINGIFY_STATUS_CASE(status) \
  case status:                        \
    return #status

  switch (status.code()) {
    STRINGIFY_STATUS_CASE(PIPELINE_OK);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_NETWORK);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_DECODE);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_ABORT);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_INITIALIZATION_FAILED);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_COULD_NOT_RENDER);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_READ);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_INVALID_STATE);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_HARDWARE_CONTEXT_RESET);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_DISCONNECTED);
    STRINGIFY_STATUS_CASE(DEMUXER_ERROR_COULD_NOT_OPEN);
    STRINGIFY_STATUS_CASE(DEMUXER_ERROR_COULD_NOT_PARSE);
    STRINGIFY_STATUS_CASE(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    STRINGIFY_STATUS_CASE(DEMUXER_ERROR_DETECTED_HLS);
    STRINGIFY_STATUS_CASE(DECODER_ERROR_NOT_SUPPORTED);
    STRINGIFY_STATUS_CASE(CHUNK_DEMUXER_ERROR_APPEND_FAILED);
    STRINGIFY_STATUS_CASE(CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR);
    STRINGIFY_STATUS_CASE(CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR);
    STRINGIFY_STATUS_CASE(AUDIO_RENDERER_ERROR);
  }

#undef STRINGIFY_STATUS_CASE

  NOTREACHED();
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
