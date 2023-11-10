// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/mock_device_info.h"

#include "base/android/build_info.h"

using ::testing::_;
using ::testing::Return;

namespace media {

MockDeviceInfo::MockDeviceInfo() {
  ON_CALL(*this, SdkVersion())
      .WillByDefault(Return(base::android::SDK_VERSION_MARSHMALLOW));
  ON_CALL(*this, IsVp8DecoderAvailable()).WillByDefault(Return(true));
  ON_CALL(*this, IsVp9DecoderAvailable()).WillByDefault(Return(true));
  ON_CALL(*this, IsAv1DecoderAvailable()).WillByDefault(Return(true));
  ON_CALL(*this, IsDecoderKnownUnaccelerated(_)).WillByDefault(Return(false));
  ON_CALL(*this, IsSetOutputSurfaceSupported()).WillByDefault(Return(true));
  ON_CALL(*this, SupportsOverlaySurfaces()).WillByDefault(Return(true));
  ON_CALL(*this, CodecNeedsFlushWorkaround(_)).WillByDefault(Return(false));
}

MockDeviceInfo::~MockDeviceInfo() = default;

}  // namespace media
