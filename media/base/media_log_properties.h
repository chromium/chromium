// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_PROPERTIES_H_
#define MEDIA_BASE_MEDIA_LOG_PROPERTIES_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_config.h"
#include "media/base/media_export.h"
#include "media/base/media_log_type_enforcement.h"
#include "media/base/renderer_factory_selector.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// A list of all properties that can be set by a MediaLog. To add a new
// property, it must be added in this enum and have it's type defined below
// using MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(<name>, <type>) or with a custom
// specializer. See MEDIA_LOG_PROEPRTY_SUPPORTS_GFX_SIZE as an example.
// You may need to add a serializer for the `type` in media_serializer.h.
enum class MediaLogProperty {
  // Video resolution.
  kResolution,

  // Size of the media content in bytes.
  kTotalBytes,

  // How many bits-per-second this media uses.
  kBitrate,

  // The maximum duration of the media in seconds.
  kMaxDuration,

  // The time at which media starts, in seconds.
  kStartTime,

  // The Content Decryption Module (CDM) to be attached to the player, with the
  // CdmConfig.
  kSetCdm,

  // Whether a Content Decryption Module (CDM) has been successfully attached
  // to the player.
  kIsCdmAttached,

  // Represents whether the media source supports range requests. A truthful
  // value here means that range requests aren't supported and seeking probably
  // won't be supported.
  kIsStreaming,

  // The url and title of the frame containing the document that this media
  // player is loaded into.
  kFrameUrl,
  kFrameTitle,

  // Whether the media content coming from the same origin as the frame in which
  // the player is loaded.
  kIsSingleOrigin,

  // Can the url loading the data support the range http header, allowing chunks
  // to be sent rather than entire file.
  kIsRangeHeaderSupported,

  // The name of media::Renderer currently being used to play the media stream.
  kRendererName,

  // The name of the decoder implementation currently being used to play the
  // media stream. All audio/video decoders have id numbers defined in
  // decoder.h.
  kVideoDecoderName,
  kAudioDecoderName,

  // Whether this decoder is using hardware accelerated decoding.
  kIsPlatformVideoDecoder,
  kIsPlatformAudioDecoder,

  // Webcodecs supports encoding video streams.
  kVideoEncoderName,
  kIsPlatformVideoEncoder,

  // Whether this media player is using a decrypting demuxer for the given
  // audio or video stream.
  kIsVideoDecryptingDemuxerStream,
  kIsAudioDecryptingDemuxerStream,

  // Track metadata.
  kAudioTracks,
  kVideoTracks,

  // Effective video playback frame rate adjusted for the playback speed.
  // Updated along with kVideoPlaybackRoughness (i.e. not very often)
  kFramerate,

  // A playback quality metric calculated by VideoPlaybackRoughnessReporter
  kVideoPlaybackRoughness,

  // A playback quality metric that tries to account for large pauses and/or
  // discontinuities during playback.
  kVideoPlaybackFreezing,
};

MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kResolution, gfx::Size);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kTotalBytes, int64_t);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kBitrate, int);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kMaxDuration, float);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kStartTime, float);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kSetCdm, CdmConfig);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kIsCdmAttached, bool);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kIsStreaming, bool);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kFrameUrl, std::string);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kFrameTitle, std::string);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kIsSingleOrigin, bool);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kRendererName, RendererType);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kVideoDecoderName, VideoDecoderType);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kIsPlatformVideoDecoder, bool);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kIsRangeHeaderSupported, bool);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kIsVideoDecryptingDemuxerStream, bool);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kAudioDecoderName, AudioDecoderType);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kIsPlatformAudioDecoder, bool);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kVideoEncoderName, std::string);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kIsPlatformVideoEncoder, bool);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kIsAudioDecryptingDemuxerStream, bool);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kAudioTracks, std::vector<AudioDecoderConfig>);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kVideoTracks, std::vector<VideoDecoderConfig>);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kFramerate, double);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kVideoPlaybackRoughness, double);
MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(kVideoPlaybackFreezing, base::TimeDelta);

// Convert the enum to a string (used for the front-end enum matching).
MEDIA_EXPORT std::string MediaLogPropertyKeyToString(MediaLogProperty property);

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_PROPERTIES_H_
