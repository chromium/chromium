// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
#define MEDIA_BASE_VIDEO_DECODER_CONFIG_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "media/base/encryption_scheme.h"
#include "media/base/media_export.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_transformation.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

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
  VideoDecoderConfig(VideoDecoderConfig&& other);
  VideoDecoderConfig& operator=(const VideoDecoderConfig& other);
  VideoDecoderConfig& operator=(VideoDecoderConfig&& other);

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

  VideoCodec codec() const { return codec_; }
  VideoCodecProfile profile() const { return profile_; }
  void set_profile(VideoCodecProfile profile) { profile_ = profile; }
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

  void set_coded_size(const gfx::Size& coded_size) { coded_size_ = coded_size; }

  // Region of coded_size() that contains image data, also known as the clean
  // aperture. Usually, but not always, origin-aligned (top-left).
  const gfx::Rect& visible_rect() const { return visible_rect_; }
  void set_visible_rect(const gfx::Rect& visible_rect) {
    visible_rect_ = visible_rect;
  }

  // DEPRECATED: Use aspect_ratio().GetNaturalSize().
  // TODO(crbug.com/40769111): Remove.
  // Final visible width and height of a video frame with aspect ratio taken
  // into account. Image data in the visible_rect() should be scaled to this
  // size for display.
  const gfx::Size& natural_size() const { return natural_size_; }
  void set_natural_size(const gfx::Size& natural_size) {
    natural_size_ = natural_size;
  }

  // The aspect ratio parsed from the container. Decoders may override using
  // in-band metadata only if !aspect_ratio().IsValid().
  const VideoAspectRatio& aspect_ratio() const { return aspect_ratio_; }

  void set_aspect_ratio(const VideoAspectRatio& aspect_ratio) {
    aspect_ratio_ = aspect_ratio;
  }

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
  void set_color_space_info(const VideoColorSpace& color_space) {
    color_space_info_ = color_space;
  }
  const VideoColorSpace& color_space_info() const { return color_space_info_; }

  // Dynamic range of the image data.
  void set_hdr_metadata(const gfx::HDRMetadata& hdr_metadata) {
    hdr_metadata_ = hdr_metadata;
  }
  const std::optional<gfx::HDRMetadata>& hdr_metadata() const {
    return hdr_metadata_;
  }

  // Codec level.
  void set_level(VideoCodecLevel level) { level_ = level; }
  VideoCodecLevel level() const { return level_; }

  // Sets the config to be encrypted or not encrypted manually. This can be
  // useful for decryptors that decrypts an encrypted stream to a clear stream.
  void SetIsEncrypted(bool is_encrypted);

 private:
  VideoCodec codec_ = VideoCodec::kUnknown;
  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;

  // Optional video codec level. kNoVideoCodecLevel means the field is not
  // available.
  VideoCodecLevel level_ = kNoVideoCodecLevel;

  AlphaMode alpha_mode_ = AlphaMode::kIsOpaque;

  VideoTransformation transformation_ = kNoTransformation;

  // Deprecated. TODO(wolenetz): Remove. See https://crbug.com/665539.
  gfx::Size coded_size_;
  gfx::Rect visible_rect_;
  gfx::Size natural_size_;

  VideoAspectRatio aspect_ratio_;

  std::vector<uint8_t> extra_data_;

  EncryptionScheme encryption_scheme_ = EncryptionScheme::kUnencrypted;

  VideoColorSpace color_space_info_;
  std::optional<gfx::HDRMetadata> hdr_metadata_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
