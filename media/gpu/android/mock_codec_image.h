// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MOCK_CODEC_IMAGE_H_
#define MEDIA_GPU_ANDROID_MOCK_CODEC_IMAGE_H_

#include "media/gpu/android/codec_image.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// CodecImage with a mocked ReleaseCodecBuffer.
class MockCodecImage : public CodecImage {
 public:
  MockCodecImage(const gfx::Size& coded_size);

  MockCodecImage(const MockCodecImage&) = delete;
  MockCodecImage& operator=(const MockCodecImage&) = delete;

  MOCK_METHOD0(ReleaseCodecBuffer, void());

 protected:
  ~MockCodecImage() override;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MOCK_CODEC_IMAGE_H_
