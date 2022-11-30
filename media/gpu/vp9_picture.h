// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP9_PICTURE_H_
#define MEDIA_GPU_VP9_PICTURE_H_

#include <memory>

#include "media/filters/vp9_parser.h"
#include "media/gpu/codec_picture.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // Create a duplicate instance and copy the data to it. It is used to support
  // VP9 show_existing_frame feature. Return the scoped_refptr pointing to the
  // duplicate instance, or nullptr on failure.
  scoped_refptr<VP9Picture> Duplicate();

  std::unique_ptr<Vp9FrameHeader> frame_hdr;

  absl::optional<Vp9Metadata> metadata_for_encoding;

 protected:
  ~VP9Picture() override;

 private:
  // Create a duplicate instance.
  virtual scoped_refptr<VP9Picture> CreateDuplicate();
};

}  // namespace media

#endif  // MEDIA_GPU_VP9_PICTURE_H_
