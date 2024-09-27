// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MUXER_H_
#define MEDIA_MUXERS_MUXER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "media/base/audio_encoder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class AudioParameters;

// Interface for muxers that take encoded audio and/or video data and mux it
// into a container. The format of the container is implementation specific, as
// is how the output is delivered.
class MEDIA_EXPORT Muxer {
 public:
  // Defines the type of a callback to be called when a derived muxer (e.g.
  // WebmMuxer or Mp4Muxer) is ready to write a chunk of data.
  using WriteDataCB = base::RepeatingCallback<void(base::span<const uint8_t>)>;

  // Container for the parameters that muxer uses that is extracted from
  // VideoFrame.
  struct MEDIA_EXPORT VideoParameters {
    explicit VideoParameters(const VideoFrame& frame);
    VideoParameters(gfx::Size visible_rect_size,
                    double frame_rate,
                    VideoCodec codec,
                    std::optional<gfx::ColorSpace> color_space);
    VideoParameters(const VideoParameters&);
    ~VideoParameters();
    // Returns a human-readable string describing `*this`.
    // For debugging & test output only.
    std::string AsHumanReadableString() const;

    gfx::Size visible_rect_size;
    double frame_rate;
    VideoCodec codec;
    std::optional<gfx::ColorSpace> color_space;
  };

  // Structure for passing encoded Audio and Video frames.
  struct MEDIA_EXPORT EncodedFrame {
    EncodedFrame();
    EncodedFrame(
        absl::variant<AudioParameters, VideoParameters> params,
        std::optional<media::AudioEncoder::CodecDescription> codec_description,
        scoped_refptr<DecoderBuffer> data);
    EncodedFrame(EncodedFrame&&);
    EncodedFrame(const EncodedFrame&) = delete;
    EncodedFrame& operator=(const EncodedFrame&) = delete;
    ~EncodedFrame();
    // Parameters for frame. Presence of either indicates the type of data
    // below.
    absl::variant<AudioParameters, VideoParameters> params;
    // Codec description for data.
    std::optional<media::AudioEncoder::CodecDescription> codec_description;
    // Audio or Video frame data.
    scoped_refptr<DecoderBuffer> data;
  };

  // The holder of muxer need to ensure Flush() is invoked before the muxer is
  // destroyed.
  virtual ~Muxer() = default;

  // Drains and writes out all buffered frames and finalizes the output.
  // Returns true on success, false otherwise.
  virtual bool Flush() = 0;

  // Emits a frame to the muxer. The caller guarantees `relative_timestamp` is
  // monotonically increasing over consecutive calls to PutFrame.
  // The held variant in params indicates audio or video.
  virtual bool PutFrame(EncodedFrame frame,
                        base::TimeDelta relative_timestamp) = 0;
};

static_assert(std::is_same<media::AudioEncoder::CodecDescription,
                           media::VideoEncoder::CodecDescription>::value,
              "media::AudioEncoder::CodecDescription and "
              "media::VideoEncoder::CodecDescription must currently be the "
              "same type. Adjust Muxer::EncodedFrame to allow it.");

}  // namespace media

#endif  // MEDIA_MUXERS_MUXER_H_
