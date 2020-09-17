// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_SCOPED_ARCORE_OBJECTS_H_
#define DEVICE_VR_ANDROID_ARCORE_SCOPED_ARCORE_OBJECTS_H_

#include "base/scoped_generic.h"
#include "device/vr/android/arcore/arcore_sdk.h"

namespace device {
namespace internal {

template <class T>
struct ScopedGenericArObject {
  static T InvalidValue() { return nullptr; }
  static void Free(T object) {}
};

template <>
void inline ScopedGenericArObject<ArSession*>::Free(ArSession* ar_session) {
  ArSession_destroy(ar_session);
}

template <>
void inline ScopedGenericArObject<ArFrame*>::Free(ArFrame* ar_frame) {
  ArFrame_destroy(ar_frame);
}

template <>
void inline ScopedGenericArObject<ArConfig*>::Free(ArConfig* ar_config) {
  ArConfig_destroy(ar_config);
}

template <>
void inline ScopedGenericArObject<ArCameraConfig*>::Free(
    ArCameraConfig* ar_camera_config) {
  ArCameraConfig_destroy(ar_camera_config);
}

template <>
void inline ScopedGenericArObject<ArCameraConfigFilter*>::Free(
    ArCameraConfigFilter* ar_camera_config_filter) {
  ArCameraConfigFilter_destroy(ar_camera_config_filter);
}

template <>
void inline ScopedGenericArObject<ArCameraConfigList*>::Free(
    ArCameraConfigList* ar_camera_config_list) {
  ArCameraConfigList_destroy(ar_camera_config_list);
}

template <>
void inline ScopedGenericArObject<ArPose*>::Free(ArPose* ar_pose) {
  ArPose_destroy(ar_pose);
}

template <>
void inline ScopedGenericArObject<ArTrackable*>::Free(
    ArTrackable* ar_trackable) {
  ArTrackable_release(ar_trackable);
}

template <>
void inline ScopedGenericArObject<ArPlane*>::Free(ArPlane* ar_plane) {
  // ArPlane itself doesn't have a method to decrease refcount, but it is an
  // instance of ArTrackable & we have to use ArTrackable_release.
  ArTrackable_release(ArAsTrackable(ar_plane));
}

template <>
void inline ScopedGenericArObject<ArImage*>::Free(ArImage* ar_image) {
  ArImage_release(ar_image);
}

template <>
void inline ScopedGenericArObject<ArAnchor*>::Free(ArAnchor* ar_anchor) {
  ArAnchor_release(ar_anchor);
}

template <>
void inline ScopedGenericArObject<ArTrackableList*>::Free(
    ArTrackableList* ar_trackable_list) {
  ArTrackableList_destroy(ar_trackable_list);
}

template <>
void inline ScopedGenericArObject<ArCamera*>::Free(ArCamera* ar_camera) {
  // Do nothing - ArCamera has no destroy method and is managed by ArCore.
}

template <>
void inline ScopedGenericArObject<ArHitResultList*>::Free(
    ArHitResultList* ar_hit_result_list) {
  ArHitResultList_destroy(ar_hit_result_list);
}

template <>
void inline ScopedGenericArObject<ArHitResult*>::Free(
    ArHitResult* ar_hit_result) {
  ArHitResult_destroy(ar_hit_result);
}

template <class T>
using ScopedArCoreObject = base::ScopedGeneric<T, ScopedGenericArObject<T>>;

}  // namespace internal
}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_SCOPED_ARCORE_OBJECTS_H_
