// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MOCK_CODEC_IMAGE_H_
#define MEDIA_GPU_ANDROID_MOCK_CODEC_IMAGE_H_

#include "base/macros.h"
#include "media/gpu/android/codec_image.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// CodecImage with a mocked ReleaseCodecBuffer.
class MockCodecImage : public CodecImage {
 public:
  MockCodecImage();

  MOCK_METHOD0(ReleaseCodecBuffer, void());

 protected:
  ~MockCodecImage() override;

  DISALLOW_COPY_AND_ASSIGN(MockCodecImage);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MOCK_CODEC_IMAGE_H_
