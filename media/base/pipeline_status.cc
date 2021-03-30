// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_status.h"

#include <ostream>

namespace media {

base::Optional<PipelineStatus> StatusCodeToPipelineStatus(StatusCode status) {
  switch (status) {
    case StatusCode::kOk:
      return PIPELINE_OK;
    case StatusCode::kPipelineErrorNetwork:
      return PIPELINE_ERROR_NETWORK;
    case StatusCode::kPipelineErrorDecode:
      return PIPELINE_ERROR_DECODE;
    case StatusCode::kPipelineErrorAbort:
      return PIPELINE_ERROR_ABORT;
    case StatusCode::kPipelineErrorInitializationFailed:
      return PIPELINE_ERROR_INITIALIZATION_FAILED;
    case StatusCode::kPipelineErrorCouldNotRender:
      return PIPELINE_ERROR_COULD_NOT_RENDER;
    case StatusCode::kPipelineErrorRead:
      return PIPELINE_ERROR_READ;
    case StatusCode::kPipelineErrorInvalidState:
      return PIPELINE_ERROR_INVALID_STATE;
    case StatusCode::kPipelineErrorDemuxerErrorCouldNotOpen:
      return DEMUXER_ERROR_COULD_NOT_OPEN;
    case StatusCode::kPipelineErrorDemuxerErrorCouldNotParse:
      return DEMUXER_ERROR_COULD_NOT_PARSE;
    case StatusCode::kPipelineErrorDemuxerErrorNoSupportedStreams:
      return DEMUXER_ERROR_NO_SUPPORTED_STREAMS;
    case StatusCode::kPipelineErrorDecoderErrorNotSupported:
      return DECODER_ERROR_NOT_SUPPORTED;
    case StatusCode::kPipelineErrorChuckDemuxerErrorAppendFailed:
      return CHUNK_DEMUXER_ERROR_APPEND_FAILED;
    case StatusCode::kPipelineErrorChunkDemuxerErrorEosStatusDecodeError:
      return CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR;
    case StatusCode::kPipelineErrorChunkDemuxerErrorEosStatusNetworkError:
      return CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR;
    case StatusCode::kPipelineErrorAudioRendererError:
      return AUDIO_RENDERER_ERROR;
    case StatusCode::kPipelineErrorExternalRendererFailed:
      return PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED;
    case StatusCode::kPipelineErrorDemuxerErrorDetectedHLS:
      return DEMUXER_ERROR_DETECTED_HLS;
    default:
      NOTREACHED();
      return base::nullopt;
  }
}

StatusCode PipelineStatusToStatusCode(PipelineStatus status) {
  switch (status) {
    case PIPELINE_OK:
      return StatusCode::kOk;
    case PIPELINE_ERROR_NETWORK:
      return StatusCode::kPipelineErrorNetwork;
    case PIPELINE_ERROR_DECODE:
      return StatusCode::kPipelineErrorDecode;
    case PIPELINE_ERROR_ABORT:
      return StatusCode::kPipelineErrorAbort;
    case PIPELINE_ERROR_INITIALIZATION_FAILED:
      return StatusCode::kPipelineErrorInitializationFailed;
    case PIPELINE_ERROR_COULD_NOT_RENDER:
      return StatusCode::kPipelineErrorCouldNotRender;
    case PIPELINE_ERROR_READ:
      return StatusCode::kPipelineErrorRead;
    case PIPELINE_ERROR_INVALID_STATE:
      return StatusCode::kPipelineErrorInvalidState;
    case DEMUXER_ERROR_COULD_NOT_OPEN:
      return StatusCode::kPipelineErrorDemuxerErrorCouldNotOpen;
    case DEMUXER_ERROR_COULD_NOT_PARSE:
      return StatusCode::kPipelineErrorDemuxerErrorCouldNotParse;
    case DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
      return StatusCode::kPipelineErrorDemuxerErrorNoSupportedStreams;
    case DECODER_ERROR_NOT_SUPPORTED:
      return StatusCode::kPipelineErrorDecoderErrorNotSupported;
    case CHUNK_DEMUXER_ERROR_APPEND_FAILED:
      return StatusCode::kPipelineErrorChuckDemuxerErrorAppendFailed;
    case CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR:
      return StatusCode::kPipelineErrorChunkDemuxerErrorEosStatusDecodeError;
    case CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR:
      return StatusCode::kPipelineErrorChunkDemuxerErrorEosStatusNetworkError;
    case AUDIO_RENDERER_ERROR:
      return StatusCode::kPipelineErrorAudioRendererError;
    case PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED:
      return StatusCode::kPipelineErrorExternalRendererFailed;
    case DEMUXER_ERROR_DETECTED_HLS:
      return StatusCode::kPipelineErrorDemuxerErrorDetectedHLS;
  }

  NOTREACHED();
  // TODO(crbug.com/1153465): Log pipeline status that failed to convert.
  // Return a generic decode error.
  return StatusCode::kPipelineErrorDecode;
}

std::string PipelineStatusToString(PipelineStatus status) {
#define STRINGIFY_STATUS_CASE(status) \
  case status:                        \
    return #status

  switch (status) {
    STRINGIFY_STATUS_CASE(PIPELINE_OK);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_NETWORK);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_DECODE);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_ABORT);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_INITIALIZATION_FAILED);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_COULD_NOT_RENDER);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_READ);
    STRINGIFY_STATUS_CASE(PIPELINE_ERROR_INVALID_STATE);
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
  return "";
}

std::ostream& operator<<(std::ostream& out, PipelineStatus status) {
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
         first.video_decoder_info == second.video_decoder_info &&
         first.audio_decoder_info == second.audio_decoder_info;
}

bool operator!=(const PipelineStatistics& first,
                const PipelineStatistics& second) {
  return !(first == second);
}

}  // namespace media
