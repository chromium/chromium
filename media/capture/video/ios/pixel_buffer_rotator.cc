// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/ios/pixel_buffer_rotator.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"

namespace media {

PixelBufferRotator::PixelBufferRotator() {
  if (__builtin_available(iOS 16, *)) {
    OSStatus error =
        VTPixelRotationSessionCreate(nil, rotation_session_.InitializeInto());
    // There is no known way to make session creation fail, so we do not deal
    // with failures gracefully.
    CHECK(error == noErr) << "Creating a   VTPixelRotationSession failed: "
                          << error;
  }
}

bool PixelBufferRotator::Rotate(CVPixelBufferRef source,
                                CVPixelBufferRef destination,
                                int rotation) {
  DCHECK(source);
  DCHECK(destination);

  if (__builtin_available(iOS 16, *)) {
    CFStringRef rotation_angle = nullptr;
    switch (rotation) {
      case 0:
        rotation_angle = kVTRotation_0;
        break;
      case 90:
        rotation_angle = kVTRotation_CW90;
        break;
      case 180:
        rotation_angle = kVTRotation_180;
        break;
      case 270:
        rotation_angle = kVTRotation_CCW90;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    OSStatus error = VTSessionSetProperty(rotation_session_.get(),
                                          kVTPixelRotationPropertyKey_Rotation,
                                          rotation_angle);
    CHECK(error == noErr) << "Unexpected RotationPropertyKey error: " << error;
    error = VTPixelRotationSessionRotateImage(rotation_session_.get(), source,
                                              destination);
    if (error == kVTPixelRotationNotSupportedErr) {
      return false;
    }
    CHECK(error == noErr)
        << "Unexpected  VTPixelRotationSessionRotateImage error: " << error;
    return true;
  }
  return false;
}

PixelBufferRotator::~PixelBufferRotator() {
  if (__builtin_available(iOS 16, *)) {
    VTPixelRotationSessionInvalidate(rotation_session_.get());
  }
}

}  // namespace media
