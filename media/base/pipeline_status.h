// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_STATUS_H_
#define MEDIA_BASE_PIPELINE_STATUS_H_

#include <stdint.h>
#include <iosfwd>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "media/base/decoder.h"
#include "media/base/media_export.h"
#include "media/base/status.h"
#include "media/base/timestamp_constants.h"

namespace media {

// Status states for pipeline.  All codes except PIPELINE_OK indicate errors.
// Logged to UMA, so never reuse a value, always add new/greater ones!
// When adding a new one, also update enums.xml.
enum PipelineStatus {
  PIPELINE_OK = 0,
  // Deprecated: PIPELINE_ERROR_URL_NOT_FOUND = 1,
  PIPELINE_ERROR_NETWORK = 2,
  PIPELINE_ERROR_DECODE = 3,
  // Deprecated: PIPELINE_ERROR_DECRYPT = 4,
  PIPELINE_ERROR_ABORT = 5,
  PIPELINE_ERROR_INITIALIZATION_FAILED = 6,
  PIPELINE_ERROR_COULD_NOT_RENDER = 8,
  PIPELINE_ERROR_READ = 9,
  // Deprecated: PIPELINE_ERROR_OPERATION_PENDING = 10,
  PIPELINE_ERROR_INVALID_STATE = 11,

  // Demuxer related errors.
  DEMUXER_ERROR_COULD_NOT_OPEN = 12,
  DEMUXER_ERROR_COULD_NOT_PARSE = 13,
  DEMUXER_ERROR_NO_SUPPORTED_STREAMS = 14,

  // Decoder related errors.
  DECODER_ERROR_NOT_SUPPORTED = 15,

  // ChunkDemuxer related errors.
  CHUNK_DEMUXER_ERROR_APPEND_FAILED = 16,
  CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR = 17,
  CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR = 18,

  // Audio rendering errors.
  AUDIO_RENDERER_ERROR = 19,

  // Deprecated: AUDIO_RENDERER_ERROR_SPLICE_FAILED = 20,
  PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED = 21,

  // Android only. Used as a signal to fallback MediaPlayerRenderer, and thus
  // not exactly an 'error' per say.
  DEMUXER_ERROR_DETECTED_HLS = 22,

  // Must be equal to the largest value ever logged.
  PIPELINE_STATUS_MAX = DEMUXER_ERROR_DETECTED_HLS,
};

MEDIA_EXPORT base::Optional<PipelineStatus> StatusCodeToPipelineStatus(
    StatusCode status);
MEDIA_EXPORT StatusCode PipelineStatusToStatusCode(PipelineStatus status);

// Returns a string version of the status, unique to each PipelineStatus, and
// not including any ':'. This makes it suitable for usage in
// MediaError.message as the UA-specific-error-code.
MEDIA_EXPORT std::string PipelineStatusToString(PipelineStatus status);

MEDIA_EXPORT std::ostream& operator<<(std::ostream& out, PipelineStatus status);

// TODO(crbug.com/1007799): Delete PipelineStatusCB once all callbacks are
//                          converted to PipelineStatusCallback.
using PipelineStatusCB = base::RepeatingCallback<void(PipelineStatus)>;
using PipelineStatusCallback = base::OnceCallback<void(PipelineStatus)>;

template <typename DecoderTypeId>
struct PipelineDecoderInfo {
  bool is_platform_decoder = false;
  bool has_decrypting_demuxer_stream = false;
  DecoderTypeId decoder_type = DecoderTypeId::kUnknown;
};

using AudioDecoderInfo = PipelineDecoderInfo<AudioDecoderType>;
using VideoDecoderInfo = PipelineDecoderInfo<VideoDecoderType>;

template <typename DecoderTypeId>
MEDIA_EXPORT inline bool operator==(
    const PipelineDecoderInfo<DecoderTypeId>& first,
    const PipelineDecoderInfo<DecoderTypeId>& second) {
  return first.decoder_type == second.decoder_type &&
         first.is_platform_decoder == second.is_platform_decoder &&
         first.has_decrypting_demuxer_stream ==
             second.has_decrypting_demuxer_stream;
}

template <typename DecoderTypeId>
MEDIA_EXPORT inline bool operator!=(
    const PipelineDecoderInfo<DecoderTypeId>& first,
    const PipelineDecoderInfo<DecoderTypeId>& second) {
  return !(first == second);
}

template <typename DecoderTypeId>
MEDIA_EXPORT inline std::ostream& operator<<(
    std::ostream& out,
    const PipelineDecoderInfo<DecoderTypeId>& info) {
  // TODO(IN THIS CL DON'T FORGET) make a converter to print name.
  return out << "{decoder_type:" << static_cast<int64_t>(info.decoder_type)
             << ","
             << "is_platform_decoder:" << info.is_platform_decoder << ","
             << "has_decrypting_demuxer_stream:"
             << info.has_decrypting_demuxer_stream << "}";
}

struct MEDIA_EXPORT PipelineStatistics {
  PipelineStatistics();
  PipelineStatistics(const PipelineStatistics& other);
  ~PipelineStatistics();

  uint64_t audio_bytes_decoded = 0u;
  uint64_t video_bytes_decoded = 0u;
  uint32_t video_frames_decoded = 0u;
  uint32_t video_frames_dropped = 0u;
  uint32_t video_frames_decoded_power_efficient = 0u;

  int64_t audio_memory_usage = 0;
  int64_t video_memory_usage = 0;

  base::TimeDelta video_keyframe_distance_average = kNoTimestamp;

  // NOTE: frame duration should reflect changes to playback rate.
  base::TimeDelta video_frame_duration_average = kNoTimestamp;

  // Note: Keep these fields at the end of the structure, if you move them you
  // need to also update the test ProtoUtilsTest::PipelineStatisticsConversion.
  AudioDecoderInfo audio_decoder_info;
  VideoDecoderInfo video_decoder_info;

  // NOTE: always update operator== implementation in pipeline_status.cc when
  // adding a field to this struct. Leave this comment at the end.
};

MEDIA_EXPORT bool operator==(const PipelineStatistics& first,
                             const PipelineStatistics& second);
MEDIA_EXPORT bool operator!=(const PipelineStatistics& first,
                             const PipelineStatistics& second);

// Used for updating pipeline statistics; the passed value should be a delta
// of all attributes since the last update.
using StatisticsCB = base::RepeatingCallback<void(const PipelineStatistics&)>;

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_STATUS_H_
