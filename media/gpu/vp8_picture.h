// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP8_PICTURE_H_
#define MEDIA_GPU_VP8_PICTURE_H_

#include "media/gpu/codec_picture.h"
#include "media/parsers/vp8_parser.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class V4L2VP8Picture;
class VaapiVP8Picture;

class VP8Picture : public CodecPicture {
 public:
  VP8Picture();

  VP8Picture(const VP8Picture&) = delete;
  VP8Picture& operator=(const VP8Picture&) = delete;

  virtual V4L2VP8Picture* AsV4L2VP8Picture();
  virtual VaapiVP8Picture* AsVaapiVP8Picture();

  std::unique_ptr<Vp8FrameHeader> frame_hdr;

  std::optional<Vp8Metadata> metadata_for_encoding;

 protected:
  ~VP8Picture() override;
};

}  // namespace media

#endif  // MEDIA_GPU_VP8_PICTURE_H_
