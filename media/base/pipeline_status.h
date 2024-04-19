// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_STATUS_H_
#define MEDIA_BASE_PIPELINE_STATUS_H_

#include <stdint.h>

#include <iosfwd>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/base/decoder.h"
#include "media/base/media_export.h"
#include "media/base/status.h"
#include "media/base/timestamp_constants.h"

namespace media {

// Status states for pipeline.  All codes except PIPELINE_OK indicate errors.
// Logged to UMA, so never reuse a value, always add new/greater ones!
// When adding a new one, also update enums.xml.
enum PipelineStatusCodes : StatusCodeType {
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

  // Used when hardware context is reset (e.g. OS sleep/resume), where we should
  // recreate the Renderer instead of failing the playback. See
  // https://crbug.com/1208618
  PIPELINE_ERROR_HARDWARE_CONTEXT_RESET = 23,

  // The remote media component was disconnected unexpectedly, e.g. crash.
  PIPELINE_ERROR_DISCONNECTED = 24,

  // Must be equal to the largest value ever logged.
  PIPELINE_STATUS_MAX = PIPELINE_ERROR_DISCONNECTED,
};

struct PipelineStatusTraits {
  using Codes = PipelineStatusCodes;

  static constexpr StatusGroupType Group() { return "PipelineStatus"; }
  static constexpr Codes OkEnumValue() { return PIPELINE_OK; }
};

using PipelineStatus = TypedStatus<PipelineStatusTraits>;

// Returns a string version of the status, unique to each PipelineStatus, and
// not including any ':'. This makes it suitable for usage in
// MediaError.message as the UA-specific-error-code.
MEDIA_EXPORT std::string PipelineStatusToString(const PipelineStatus& status);

MEDIA_EXPORT std::ostream& operator<<(std::ostream& out,
                                      const PipelineStatus& status);

// TODO(crbug.com/40649615): Delete PipelineStatusCB once all callbacks are
//                          converted to PipelineStatusCallback.
using PipelineStatusCB = base::RepeatingCallback<void(PipelineStatus)>;
using PipelineStatusCallback = base::OnceCallback<void(PipelineStatus)>;

// Information on how an audio/video stream is encrypted.
// Warning: Reported to UKM. Do not reuse or change existing values.
// Note: A stream can be marked as clear (unencrypted) or encrypted in the
// config. In a clear stream, all buffers must be clear. In an encrypted stream,
// buffers can be clear or encrypted. The term "clear lead" generally indicates
// the case where an encrypted stream starts with one or more clear buffers. In
// implementation, since a playback can start from the middle of a stream, the
// playback may not hit clear lead even if the stream has clear lead, so it'll
// be reported as `kEncrypted`, which is okay for metrics' purpose.
enum class EncryptionType {
  kNone = 0,                    // No corresponding audio/video stream
  kClear = 1,                   // Stream is clear (not encrypted)
  kEncrypted = 2,               // Stream is encrypted without clear lead
  kEncryptedWithClearLead = 3,  // Stream is encrypted but has clear lead
  kMaxValue = kEncryptedWithClearLead,
};

template <typename DecoderType>
struct PipelineInfo {
  bool is_platform_decoder = false;
  bool has_decrypting_demuxer_stream = false;
  DecoderType decoder_type = DecoderType::kUnknown;
  EncryptionType encryption_type = EncryptionType::kNone;
};

using AudioPipelineInfo = PipelineInfo<AudioDecoderType>;
using VideoPipelineInfo = PipelineInfo<VideoDecoderType>;

template <typename DecoderType>
MEDIA_EXPORT inline bool operator==(const PipelineInfo<DecoderType>& first,
                                    const PipelineInfo<DecoderType>& second) {
  return first.decoder_type == second.decoder_type &&
         first.is_platform_decoder == second.is_platform_decoder &&
         first.has_decrypting_demuxer_stream ==
             second.has_decrypting_demuxer_stream &&
         first.encryption_type == second.encryption_type;
}

template <typename DecoderType>
MEDIA_EXPORT inline bool operator!=(const PipelineInfo<DecoderType>& first,
                                    const PipelineInfo<DecoderType>& second) {
  return !(first == second);
}

template <typename DecoderType>
MEDIA_EXPORT inline std::ostream& operator<<(
    std::ostream& out,
    const PipelineInfo<DecoderType>& info) {
  return out << "{decoder_type:" << GetDecoderName(info.decoder_type) << ","
             << "is_platform_decoder:" << info.is_platform_decoder << ","
             << "has_decrypting_demuxer_stream:"
             << info.has_decrypting_demuxer_stream << ","
             << "encryption_type:" << static_cast<int>(info.encryption_type)
             << "}";
}

// Statistics for the media pipeline.
// Note: Different classes may have different interpretation on the fields.
// RendererClient.OnStatisticsUpdate() expects *_decoded*, *_dropped and
// *memory_usage to be the delta since the last OnStatisticsUpdate() call.
// WebMediaPlayerImpl expects them to be cumulation since playback start.
// TODO(crbug.com/40207229): Make the meaning consistent.
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
  AudioPipelineInfo audio_pipeline_info;
  VideoPipelineInfo video_pipeline_info;

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
