// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ACCELERATED_VIDEO_DECODER_H_
#define MEDIA_GPU_ACCELERATED_VIDEO_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_types.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

// An AcceleratedVideoDecoder is a video decoder that requires support from an
// external accelerator (typically a hardware accelerator) to partially offload
// the decode process after parsing stream headers, and performing reference
// frame and state management.
class MEDIA_GPU_EXPORT AcceleratedVideoDecoder {
 public:
  AcceleratedVideoDecoder() = default;
  virtual ~AcceleratedVideoDecoder() = default;

  AcceleratedVideoDecoder(const AcceleratedVideoDecoder&) = delete;
  AcceleratedVideoDecoder& operator=(const AcceleratedVideoDecoder&) = delete;

  // Set the buffer owned by |decoder_buffer| as the current source of encoded
  // stream data. AcceleratedVideoDecoder doesn't have an ownership of the
  // buffer. |decoder_buffer| must be kept alive until Decode() returns
  // kRanOutOfStreamData. Pictures produced as a result of this call should be
  // assigned the passed stream |id|.
  virtual void SetStream(int32_t id, const DecoderBuffer& decoder_buffer) = 0;

  // Have the decoder flush its state and trigger output of all previously
  // decoded surfaces. Return false on failure.
  [[nodiscard]] virtual bool Flush() = 0;

  // Stop (pause) decoding, discarding all remaining inputs and outputs,
  // but do not flush decoder state, so that playback can be resumed later,
  // possibly from a different location.
  // To be called during decoding.
  virtual void Reset() = 0;

  enum DecodeResult {
    kDecodeError,  // Error while decoding.
    // TODO(posciak): unsupported streams are currently treated as error
    // in decoding; in future it could perhaps be possible to fall back
    // to software decoding instead.
    // kStreamError,  // Error in stream.
    kConfigChange,  // Stream configuration has changed. E.g., profile, coded
                    // size, bit depth, or color space. A client may need to
                    // reallocate resources to apply the new configuration
                    // properly. E.g. allocate buffers with the new resolution.
    kRanOutOfStreamData,  // Need more stream data to proceed.
    kRanOutOfSurfaces,    // Waiting for the client to free up output surfaces.
    kTryAgain,  // The accelerator needs additional data (independently
    // provided) in order to proceed. This may be a new key in order to decrypt
    // encrypted data, or existing hardware resources freed so that they can be
    // reused. Decoding can resume once the data has been provided.
  };

  // Try to decode more of the stream, returning decoded frames asynchronously.
  // Return when more stream is needed, when we run out of free surfaces, when
  // we need a new set of them, or when an error occurs.
  [[nodiscard]] virtual DecodeResult Decode() = 0;

  // Return dimensions/visible rectangle/profile/bit depth/chroma sampling
  // format/hdr metadata/required number of pictures that client should be
  // ready to provide for the decoder to function properly (of which up to
  // GetNumReferenceFrames() might be needed for internal decoding). To be used
  // after Decode() returns kConfigChange.
  virtual gfx::Size GetPicSize() const = 0;
  virtual gfx::Rect GetVisibleRect() const = 0;
  virtual VideoCodecProfile GetProfile() const = 0;
  virtual uint8_t GetBitDepth() const = 0;
  virtual VideoChromaSampling GetChromaSampling() const = 0;
  // Returns the video color space for the in-band metadata / stream
  // configuration. The returned color space may vary between in-band metadata
  // and stream config based on video decoder's internal
  // preferences.
  virtual VideoColorSpace GetVideoColorSpace() const = 0;
  // Returns in-band HDR metadata if it exists. Clients must prefer in-band
  // metadata over container metadata to support dynamic HDR metadata.
  virtual std::optional<gfx::HDRMetadata> GetHDRMetadata() const = 0;
  virtual size_t GetRequiredNumOfPictures() const = 0;
  virtual size_t GetNumReferenceFrames() const = 0;

  // About 3 secs for 30 fps video. When the new sized keyframe is missed, the
  // decoder cannot decode the frame. The number of frames are skipped until
  // getting new keyframe. If dropping more than the number of frames, the
  // decoder reports decode error, which may take longer time to recover it.
  // The number is the sweet spot which the decoder can tolerate to handle the
  // missing keyframe by itself. In addition, this situation is exceptional.
  static constexpr size_t kVPxMaxNumOfSizeChangeFailures = 75;
};

}  //  namespace media

#endif  // MEDIA_GPU_ACCELERATED_VIDEO_DECODER_H_
