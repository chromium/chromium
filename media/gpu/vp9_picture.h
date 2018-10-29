// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP9_PICTURE_H_
#define MEDIA_GPU_VP9_PICTURE_H_

#include <memory>

#include "base/macros.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/codec_picture.h"

namespace media {

class V4L2VP9Picture;
class VaapiVP9Picture;

class VP9Picture : public CodecPicture {
 public:
  VP9Picture();

  // TODO(tmathmeyer) remove these and just use static casts everywhere.
  virtual V4L2VP9Picture* AsV4L2VP9Picture();
  virtual VaapiVP9Picture* AsVaapiVP9Picture();

  // Create a duplicate instance and copy the data to it. It is used to support
  // VP9 show_existing_frame feature. Return the scoped_refptr pointing to the
  // duplicate instance, or nullptr on failure.
  scoped_refptr<VP9Picture> Duplicate();

  std::unique_ptr<Vp9FrameHeader> frame_hdr;

 protected:
  ~VP9Picture() override;

 private:
  // Create a duplicate instance.
  virtual scoped_refptr<VP9Picture> CreateDuplicate();

  DISALLOW_COPY_AND_ASSIGN(VP9Picture);
};

}  // namespace media

#endif  // MEDIA_GPU_VP9_PICTURE_H_
