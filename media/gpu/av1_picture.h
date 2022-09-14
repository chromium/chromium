// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AV1_PICTURE_H_
#define MEDIA_GPU_AV1_PICTURE_H_

#include "media/gpu/codec_picture.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/libgav1/src/src/utils/types.h"

namespace media {

// AV1Picture carries the parsed frame header needed for decoding an AV1 frame.
// It also owns the decoded frame itself.
class MEDIA_GPU_EXPORT AV1Picture : public CodecPicture {
 public:
  AV1Picture();
  AV1Picture(const AV1Picture&) = delete;
  AV1Picture& operator=(const AV1Picture&) = delete;

  // Create a duplicate instance and copy the data to it. It is used to support
  // the AV1 show_existing_frame feature. Return the scoped_refptr pointing to
  // the duplicate instance, or nullptr on failure.
  scoped_refptr<AV1Picture> Duplicate();

  libgav1::ObuFrameHeader frame_header = {};

 protected:
  ~AV1Picture() override;

 private:
  // Create a duplicate instance.
  virtual scoped_refptr<AV1Picture> CreateDuplicate();
};
}  // namespace media
#endif  // MEDIA_GPU_AV1_PICTURE_H_
