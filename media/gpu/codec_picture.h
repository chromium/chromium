// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CODEC_PICTURE_H_
#define MEDIA_GPU_CODEC_PICTURE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "media/base/decrypt_config.h"
#include "media/base/video_color_space.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

// Represents a picture encoded (or to be encoded) with a video codec, such as
// VP8. Users of this class do not require knowledge of the codec format, or any
// codec-specific details related to the picture, but may otherwise need to pass
// or keep references to the picture, e.g. to keep a list of reference pictures
// required for a codec task valid until it is finished. Also used for storing
// non-codec-specific metadata.
class MEDIA_GPU_EXPORT CodecPicture
    : public base::RefCountedThreadSafe<CodecPicture> {
 public:
  CodecPicture();

  CodecPicture(const CodecPicture&) = delete;
  CodecPicture& operator=(const CodecPicture&) = delete;

  int32_t bitstream_id() const { return bitstream_id_; }
  void set_bitstream_id(int32_t bitstream_id) { bitstream_id_ = bitstream_id; }

  const gfx::Rect visible_rect() const { return visible_rect_; }
  void set_visible_rect(const gfx::Rect& rect) { visible_rect_ = rect; }

  // DecryptConfig returned by this method describes the decryption
  // configuration of the input stream for this picture. Returns null if it is
  // not encrypted.
  const DecryptConfig* decrypt_config() const { return decrypt_config_.get(); }
  void set_decrypt_config(std::unique_ptr<DecryptConfig> config) {
    decrypt_config_ = std::move(config);
  }

  // Populate with an unspecified colorspace by default.
  const VideoColorSpace& get_colorspace() const { return colorspace_; }
  void set_colorspace(const VideoColorSpace& colorspace) {
    colorspace_ = colorspace;
  }

  const std::optional<gfx::HDRMetadata>& hdr_metadata() const {
    return hdr_metadata_;
  }
  void set_hdr_metadata(const std::optional<gfx::HDRMetadata>& hdr_metadata) {
    hdr_metadata_ = hdr_metadata;
  }

 protected:
  friend class base::RefCountedThreadSafe<CodecPicture>;
  virtual ~CodecPicture();

 private:
  int32_t bitstream_id_ = -1;
  gfx::Rect visible_rect_;
  std::unique_ptr<DecryptConfig> decrypt_config_;
  VideoColorSpace colorspace_;
  std::optional<gfx::HDRMetadata> hdr_metadata_;
};

}  // namespace media

#endif  // MEDIA_GPU_CODEC_PICTURE_H_
