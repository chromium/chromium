// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MOCK_DEVICE_INFO_H_
#define MEDIA_GPU_ANDROID_MOCK_DEVICE_INFO_H_

#include "media/gpu/android/device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MediaCodecBridge;

// A mock DeviceInfo with reasonable defaults.
class MockDeviceInfo : public DeviceInfo {
 public:
  MockDeviceInfo();
  virtual ~MockDeviceInfo();

  MOCK_METHOD0(SdkVersion, int());
  MOCK_METHOD0(IsVp8DecoderAvailable, bool());
  MOCK_METHOD0(IsVp9DecoderAvailable, bool());
  MOCK_METHOD0(IsAv1DecoderAvailable, bool());
  MOCK_METHOD1(IsDecoderKnownUnaccelerated, bool(VideoCodec codec));
  MOCK_METHOD0(IsSetOutputSurfaceSupported, bool());
  MOCK_METHOD0(SupportsOverlaySurfaces, bool());
  MOCK_METHOD1(CodecNeedsFlushWorkaround, bool(MediaCodecBridge* codec));
  MOCK_METHOD1(AddSupportedCodecProfileLevels,
               void(std::vector<CodecProfileLevel>*));
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MOCK_DEVICE_INFO_H_
