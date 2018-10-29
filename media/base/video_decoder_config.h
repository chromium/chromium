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
#include "media/base/video_rotation.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

MEDIA_EXPORT VideoCodec
VideoCodecProfileToVideoCodec(VideoCodecProfile profile);

class MEDIA_EXPORT VideoDecoderConfig {
 public:
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  VideoDecoderConfig();

  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  VideoDecoderConfig(VideoCodec codec,
                     VideoCodecProfile profile,
                     VideoPixelFormat format,
                     ColorSpace color_space,
                     VideoRotation rotation,
                     const gfx::Size& coded_size,
                     const gfx::Rect& visible_rect,
                     const gfx::Size& natural_size,
                     const std::vector<uint8_t>& extra_data,
                     const EncryptionScheme& encryption_scheme);

  VideoDecoderConfig(const VideoDecoderConfig& other);

  ~VideoDecoderConfig();

  // Resets the internal state of this object.
  void Initialize(VideoCodec codec,
                  VideoCodecProfile profile,
                  VideoPixelFormat format,
                  ColorSpace color_space,
                  VideoRotation rotation,
                  const gfx::Size& coded_size,
                  const gfx::Rect& visible_rect,
                  const gfx::Size& natural_size,
                  const std::vector<uint8_t>& extra_data,
                  const EncryptionScheme& encryption_scheme);

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

  // Video format used to determine YUV buffer sizes.
  VideoPixelFormat format() const { return format_; }

  // Default is VIDEO_ROTATION_0.
  VideoRotation video_rotation() const { return rotation_; }

  // Deprecated. TODO(wolenetz): Remove. See https://crbug.com/665539.
  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  const gfx::Size& coded_size() const { return coded_size_; }

  // Region of |coded_size_| that is visible.
  const gfx::Rect& visible_rect() const { return visible_rect_; }

  // Final visible width and height of a video frame with aspect ratio taken
  // into account.
  const gfx::Size& natural_size() const { return natural_size_; }

  // TODO(crbug.com/837337): This should be explicitly set (replacing
  // |natural_size|). It should also be possible to determine whether it was set
  // at all, since in-stream information may override it if it was not.
  double GetPixelAspectRatio() const;

  // Optional byte data required to initialize video decoders, such as H.264
  // AVCC data.
  void SetExtraData(const std::vector<uint8_t>& extra_data);
  const std::vector<uint8_t>& extra_data() const { return extra_data_; }

  // Whether the video stream is potentially encrypted.
  // Note that in a potentially encrypted video stream, individual buffers
  // can be encrypted or not encrypted.
  bool is_encrypted() const { return encryption_scheme_.is_encrypted(); }

  // Encryption scheme used for encrypted buffers.
  const EncryptionScheme& encryption_scheme() const {
    return encryption_scheme_;
  }

  void set_color_space_info(const VideoColorSpace& color_space_info);
  const VideoColorSpace& color_space_info() const;

  void set_hdr_metadata(const HDRMetadata& hdr_metadata);
  const base::Optional<HDRMetadata>& hdr_metadata() const;

  // Sets the config to be encrypted or not encrypted manually. This can be
  // useful for decryptors that decrypts an encrypted stream to a clear stream.
  void SetIsEncrypted(bool is_encrypted);

 private:
  VideoCodec codec_;
  VideoCodecProfile profile_;

  VideoPixelFormat format_;

  VideoRotation rotation_;

  // Deprecated. TODO(wolenetz): Remove. See https://crbug.com/665539.
  gfx::Size coded_size_;

  gfx::Rect visible_rect_;
  gfx::Size natural_size_;

  std::vector<uint8_t> extra_data_;

  EncryptionScheme encryption_scheme_;

  VideoColorSpace color_space_info_;
  base::Optional<HDRMetadata> hdr_metadata_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
