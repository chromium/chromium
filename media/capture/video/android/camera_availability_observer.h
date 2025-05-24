// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_ANDROID_CAMERA_AVAILABILITY_OBSERVER_H_
#define MEDIA_CAPTURE_VIDEO_ANDROID_CAMERA_AVAILABILITY_OBSERVER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "media/capture/capture_export.h"

namespace media {

// This class owns a CameraAvailabilityObserver on the Java side which observes
// camera availability changes through CameraManager.AvailabilityCallback. The
// Java CameraAvailabilityObserver holds a pointer to this class. Once any
// camera availability changes happen, it will call
// OnCameraAvailabilityChanged() through JNI to notify this class, which then
// notifies the system monitor about the change.
class CAPTURE_EXPORT CameraAvailabilityObserver {
 public:
  CameraAvailabilityObserver();
  ~CameraAvailabilityObserver();

  // Implement
  // org.chromium.media.CameraAvailabilityObserver.nativeOnCameraAvailabilityChanged.
  void OnCameraAvailabilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  base::android::ScopedJavaLocalRef<jobject> j_camera_availability_observer_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_ANDROID_CAMERA_AVAILABILITY_OBSERVER_H_
