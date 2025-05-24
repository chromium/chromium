// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/camera_availability_observer.h"

#include "base/system/system_monitor.h"
#include "media/capture/video/android/capture_jni_headers/CameraAvailabilityObserver_jni.h"

using jni_zero::AttachCurrentThread;

namespace {

void notifyVideoCaptureDeviceChanged() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor) {
    monitor->ProcessDevicesChanged(
        base::SystemMonitor::DeviceType::DEVTYPE_VIDEO_CAPTURE);
  }
}

}  // namespace

namespace media {

CameraAvailabilityObserver::CameraAvailabilityObserver() {
  JNIEnv* env = AttachCurrentThread();
  j_camera_availability_observer_.Reset(
      Java_CameraAvailabilityObserver_createCameraAvailabilityObserver(
          env, reinterpret_cast<intptr_t>(this)));
  Java_CameraAvailabilityObserver_startObservation(
      env, j_camera_availability_observer_);
}

CameraAvailabilityObserver::~CameraAvailabilityObserver() {
  JNIEnv* env = AttachCurrentThread();
  Java_CameraAvailabilityObserver_stopObservation(
      env, j_camera_availability_observer_);
}

void CameraAvailabilityObserver::OnCameraAvailabilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  notifyVideoCaptureDeviceChanged();
}

}  // namespace media
