// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
#define MEDIA_BASE_VIDEO_DECODER_CONFIG_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "media/base/encryption_scheme.h"
#include "media/base/hdr_metadata.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_transformation.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

MEDIA_EXPORT VideoCodec
VideoCodecProfileToVideoCodec(VideoCodecProfile profile);

// Describes the content of a video stream, as described by the media container
// (or otherwise determined by the demuxer).
class MEDIA_EXPORT VideoDecoderConfig {
 public:
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  VideoDecoderConfig();

  enum class AlphaMode { kHasAlpha, kIsOpaque };

  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  VideoDecoderConfig(VideoCodec codec,
                     VideoCodecProfile profile,
                     AlphaMode alpha_mode,
                     const VideoColorSpace& color_space,
                     VideoTransformation transformation,
                     const gfx::Size& coded_size,
                     const gfx::Rect& visible_rect,
                     const gfx::Size& natural_size,
                     const std::vector<uint8_t>& extra_data,
                     EncryptionScheme encryption_scheme);
  VideoDecoderConfig(const VideoDecoderConfig& other);

  ~VideoDecoderConfig();

  // Resets the internal state of this object.
  void Initialize(VideoCodec codec,
                  VideoCodecProfile profile,
                  AlphaMode alpha_mode,
                  const VideoColorSpace& color_space,
                  VideoTransformation transformation,
                  const gfx::Size& coded_size,
                  const gfx::Rect& visible_rect,
                  const gfx::Size& natural_size,
                  const std::vector<uint8_t>& extra_data,
                  EncryptionScheme encryption_scheme);

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  bool IsValidConfig() const;

  // Returns true if all fields in |config| match this config.
  // Note: The contents of |extra_data_| are compared not the raw pointers.
  bool Matches(const VideoDecoderConfig& config) const;

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString() const;

  std::string GetHumanReadableCodecName() const;

  static std::string GetHumanReadableProfile(VideoCodecProfile profile);

  VideoCodec codec() const { return codec_; }
  VideoCodecProfile profile() const { return profile_; }
  AlphaMode alpha_mode() const { return alpha_mode_; }

  // Difference between encoded and display orientation.
  //
  // Default is VIDEO_ROTATION_0. Note that rotation should be applied after
  // scaling to natural_size().
  //
  // TODO(sandersd): Which direction is orientation measured in?
  VideoTransformation video_transformation() const { return transformation_; }

  // Deprecated. TODO(wolenetz): Remove. See https://crbug.com/665539.
  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  const gfx::Size& coded_size() const { return coded_size_; }

  // Region of coded_size() that contains image data, also known as the clean
  // aperture. Usually, but not always, origin-aligned (top-left).
  const gfx::Rect& visible_rect() const { return visible_rect_; }

  // Final visible width and height of a video frame with aspect ratio taken
  // into account. Image data in the visible_rect() should be scaled to this
  // size for display.
  const gfx::Size& natural_size() const { return natural_size_; }

  // The shape of encoded pixels. Given visible_rect() and a pixel aspect ratio,
  // it is possible to compute natural_size() (see video_util.h).
  //
  // SUBTLE: "pixel aspect ratio" != "display aspect ratio". *Pixel* aspect
  // ratio describes the shape of a *pixel* as the ratio of its width to its
  // height (ex: anamorphic video may have rectangular pixels). *Display* aspect
  // ratio is natural_width / natural_height.
  //
  // CONTRACT: Dynamic changes to *pixel* aspect ratio are not supported unless
  // done with explicit signal (new init-segment in MSE). Streams may still
  // change their frame sizes dynamically, including their *display* aspect
  // ratio. But, at this time (2019) changes to pixel aspect ratio are not
  // surfaced by all platform decoders (ex: MediaCodec), so non-support is
  // chosen for cross platform consistency. Hence, natural size should always be
  // computed by scaling visbilte_size by the *pixel* aspect ratio from the
  // container metadata. See GetNaturalSize() in video_util.h.
  //
  // TODO(crbug.com/837337): This should be explicitly set (replacing
  // |natural_size|). Alternatively, this could be replaced by
  // GetNaturalSize(visible_rect), with pixel aspect ratio being an internal
  // detail of the config.
  double GetPixelAspectRatio() const;

  // Optional video decoder initialization data, such as H.264 AVCC.
  //
  // Note: FFmpegVideoDecoder assumes that H.264 is in AVC format if there is
  // |extra_data|, and in Annex B format if there is not. We should probably add
  // explicit signaling of encoded format.
  void SetExtraData(const std::vector<uint8_t>& extra_data);
  const std::vector<uint8_t>& extra_data() const { return extra_data_; }

  // Whether the video stream is potentially encrypted.
  // Note that in a potentially encrypted video stream, individual buffers
  // can be encrypted or not encrypted.
  bool is_encrypted() const {
    return encryption_scheme_ != EncryptionScheme::kUnencrypted;
  }

  // Encryption scheme used for encrypted buffers.
  EncryptionScheme encryption_scheme() const { return encryption_scheme_; }

  // Color space of the image data.
  void set_color_space_info(const VideoColorSpace& color_space);
  const VideoColorSpace& color_space_info() const;

  // Dynamic range of the image data.
  void set_hdr_metadata(const HDRMetadata& hdr_metadata);
  const base::Optional<HDRMetadata>& hdr_metadata() const;

  // Sets the config to be encrypted or not encrypted manually. This can be
  // useful for decryptors that decrypts an encrypted stream to a clear stream.
  void SetIsEncrypted(bool is_encrypted);

 private:
  VideoCodec codec_;
  VideoCodecProfile profile_;

  AlphaMode alpha_mode_;

  VideoTransformation transformation_;

  // Deprecated. TODO(wolenetz): Remove. See https://crbug.com/665539.
  gfx::Size coded_size_;

  gfx::Rect visible_rect_;
  gfx::Size natural_size_;

  std::vector<uint8_t> extra_data_;

  EncryptionScheme encryption_scheme_ = EncryptionScheme::kUnencrypted;

  VideoColorSpace color_space_info_;
  base::Optional<HDRMetadata> hdr_metadata_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
