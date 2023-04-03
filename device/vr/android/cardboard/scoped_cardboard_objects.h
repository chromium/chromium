// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_SCOPED_CARDBOARD_OBJECTS_H_
#define DEVICE_VR_ANDROID_CARDBOARD_SCOPED_CARDBOARD_OBJECTS_H_

#include "base/scoped_generic.h"
#include "third_party/cardboard/src/sdk/include/cardboard.h"

namespace device::internal {
template <class T>
struct ScopedGenericCardboardObject {
  static T InvalidValue() { return nullptr; }

  // Implementation not provided by design - this will cause linker errors if a
  // ScopedCardboardObject<T> is used for T that does not have a template
  // specialization defining the Free() method.
  static void Free(T object);
};

template <>
void inline ScopedGenericCardboardObject<CardboardLensDistortion*>::Free(
    CardboardLensDistortion* lens_distortion) {
  CardboardLensDistortion_destroy(lens_distortion);
}

template <>
void inline ScopedGenericCardboardObject<CardboardDistortionRenderer*>::Free(
    CardboardDistortionRenderer* distortion_renderer) {
  CardboardDistortionRenderer_destroy(distortion_renderer);
}

template <>
void inline ScopedGenericCardboardObject<CardboardHeadTracker*>::Free(
    CardboardHeadTracker* head_tracker) {
  CardboardHeadTracker_destroy(head_tracker);
}

// The EncodedDeviceParams are a uint8_t*, when owned.
template <>
void inline ScopedGenericCardboardObject<uint8_t*>::Free(
    uint8_t* device_params) {
  CardboardQrCode_destroy(device_params);
}

template <class T>
using ScopedCardboardObject =
    base::ScopedGeneric<T, ScopedGenericCardboardObject<T>>;

}  // namespace device::internal

#endif  // DEVICE_VR_ANDROID_CARDBOARD_SCOPED_CARDBOARD_OBJECTS_H_
