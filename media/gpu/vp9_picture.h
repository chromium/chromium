// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP9_PICTURE_H_
#define MEDIA_GPU_VP9_PICTURE_H_

#include <memory>
#include <optional>

#include "media/gpu/codec_picture.h"
#include "media/parsers/vp9_parser.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class V4L2VP9Picture;
class VaapiVP9Picture;

class MEDIA_GPU_EXPORT VP9Picture : public CodecPicture {
 public:
  VP9Picture();

  VP9Picture(const VP9Picture&) = delete;
  VP9Picture& operator=(const VP9Picture&) = delete;

  // TODO(tmathmeyer) remove these and just use static casts everywhere.
  virtual V4L2VP9Picture* AsV4L2VP9Picture();
  virtual VaapiVP9Picture* AsVaapiVP9Picture();

  // Create a copy of this picture (used to implement show_existing_frame).
  scoped_refptr<VP9Picture> Duplicate();

  std::unique_ptr<Vp9FrameHeader> frame_hdr;

  std::optional<Vp9Metadata> metadata_for_encoding;

 protected:
  ~VP9Picture() override;

  // Create an instance of the same class, and copy any accelerator-specific
  // fields. Used by Duplicate() which handles copying CodecPicture and
  // VP9Picture fields.
  //
  // All subclasses should override this method.
  virtual scoped_refptr<VP9Picture> CreateDuplicate();
};

}  // namespace media

#endif  // MEDIA_GPU_VP9_PICTURE_H_
