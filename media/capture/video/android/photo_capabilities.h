// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_ANDROID_PHOTO_CAPABILITIES_H_
#define MEDIA_CAPTURE_VIDEO_ANDROID_PHOTO_CAPABILITIES_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"

namespace media {

class PhotoCapabilities {
 public:
  // Metering modes from Java side, equivalent to media.mojom::MeteringMode,
  // except NOT_SET, which is used to signify absence of setting configuration.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum class AndroidMeteringMode {
    NOT_SET,  // Update Java code if this value is not equal 0.
    NONE,
    FIXED,
    SINGLE_SHOT,
    CONTINUOUS,

    NUM_ENTRIES
  };

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum class MeteringModeType {
    FOCUS,
    EXPOSURE,
    WHITE_BALANCE,

    NUM_ENTRIES
  };

  // Fill light modes from Java side, equivalent to media.mojom::FillLightMode,
  // except NOT_SET, which is used to signify absence of setting configuration.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum class AndroidFillLightMode {
    NOT_SET,  // Update Java code when this value is not equal 0.
    OFF,
    AUTO,
    FLASH,

    NUM_ENTRIES
  };

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum class PhotoCapabilityBool {
    SUPPORTS_TORCH,
    TORCH,
    RED_EYE_REDUCTION,

    NUM_ENTRIES
  };

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum class PhotoCapabilityDouble {
    MIN_ZOOM,
    MAX_ZOOM,
    CURRENT_ZOOM,
    STEP_ZOOM,

    MIN_FOCUS_DISTANCE,
    MAX_FOCUS_DISTANCE,
    CURRENT_FOCUS_DISTANCE,
    STEP_FOCUS_DISTANCE,

    MIN_EXPOSURE_COMPENSATION,
    MAX_EXPOSURE_COMPENSATION,
    CURRENT_EXPOSURE_COMPENSATION,
    STEP_EXPOSURE_COMPENSATION,

    MIN_EXPOSURE_TIME,
    MAX_EXPOSURE_TIME,
    CURRENT_EXPOSURE_TIME,
    STEP_EXPOSURE_TIME,

    NUM_ENTRIES
  };

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum class PhotoCapabilityInt {
    MIN_ISO,
    MAX_ISO,
    CURRENT_ISO,
    STEP_ISO,

    MIN_HEIGHT,
    MAX_HEIGHT,
    CURRENT_HEIGHT,
    STEP_HEIGHT,

    MIN_WIDTH,
    MAX_WIDTH,
    CURRENT_WIDTH,
    STEP_WIDTH,

    MIN_COLOR_TEMPERATURE,
    MAX_COLOR_TEMPERATURE,
    CURRENT_COLOR_TEMPERATURE,
    STEP_COLOR_TEMPERATURE,

    NUM_ENTRIES
  };

  explicit PhotoCapabilities(base::android::ScopedJavaLocalRef<jobject> object);
  ~PhotoCapabilities();

  int getInt(PhotoCapabilityInt capability) const;
  double getDouble(PhotoCapabilityDouble capability) const;
  bool getBool(PhotoCapabilityBool capability) const;
  std::vector<AndroidFillLightMode> getFillLightModeArray() const;
  AndroidMeteringMode getMeteringMode(MeteringModeType type) const;
  std::vector<AndroidMeteringMode> getMeteringModeArray(
      MeteringModeType type) const;

 private:
  const base::android::ScopedJavaLocalRef<jobject> object_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_ANDROID_PHOTO_CAPABILITIES_H_
