// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/media/capture/desktop_capturer_android.h"

#include "base/android/jni_android.h"
#include "base/notimplemented.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/ScreenCapture_jni.h"

namespace content {

DesktopCapturerAndroid::DesktopCapturerAndroid(
    const webrtc::DesktopCaptureOptions& options) {}

DesktopCapturerAndroid::~DesktopCapturerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ScreenCapture_destroy(env, screen_capture_);
}

void DesktopCapturerAndroid::Start(Callback* callback) {
  callback_ = callback;

  JNIEnv* env = base::android::AttachCurrentThread();
  screen_capture_.Reset(
      Java_ScreenCapture_create(env, reinterpret_cast<intptr_t>(this)));

  Java_ScreenCapture_startCapture(env, screen_capture_);
}

void DesktopCapturerAndroid::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {}

void DesktopCapturerAndroid::CaptureFrame() {
  // TODO(crbug.com/352187279): Implement this.
  callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_PERMANENT,
                             nullptr);
  NOTIMPLEMENTED();
}

bool DesktopCapturerAndroid::SelectSource(SourceId id) {
  return true;
}

void DesktopCapturerAndroid::OnRgbaFrameAvailable(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& release_cb,
    const base::android::JavaRef<jobject>& buf,
    jint unchecked_pixel_stride,
    jint unchecked_row_stride,
    jint unchecked_crop_left,
    jint unchecked_crop_top,
    jint unchecked_crop_right,
    jint unchecked_crop_bottom) {
  // TODO(crbug.com/352187279): Implement this.
  NOTIMPLEMENTED();
}

}  // namespace content
