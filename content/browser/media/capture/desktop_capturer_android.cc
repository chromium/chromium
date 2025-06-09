// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/media/capture/desktop_capturer_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_bytebuffer.h"
#include "base/numerics/checked_math.h"
#include "base/threading/platform_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/ScreenCapture_jni.h"

namespace content {

DesktopCapturerAndroid::PlaneInfo::PlaneInfo() = default;

DesktopCapturerAndroid::PlaneInfo::~PlaneInfo() = default;

DesktopCapturerAndroid::PlaneInfo::PlaneInfo(PlaneInfo&& other) = default;

DesktopCapturerAndroid::PlaneInfo& DesktopCapturerAndroid::PlaneInfo::operator=(
    PlaneInfo&& other) = default;

DesktopCapturerAndroid::DesktopCapturerAndroid(
    const webrtc::DesktopCaptureOptions& options) {}

DesktopCapturerAndroid::~DesktopCapturerAndroid() {
  CHECK(task_runner_);
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  finishing_ = true;
  JNIEnv* env = base::android::AttachCurrentThread();
  // This will block until all pending Java side calls on the background thread
  // have completed.
  Java_ScreenCapture_destroy(env, screen_capture_);
}

void DesktopCapturerAndroid::Start(Callback* callback) {
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  callback_ = callback;

  JNIEnv* env = base::android::AttachCurrentThread();
  screen_capture_.Reset(
      Java_ScreenCapture_create(env, reinterpret_cast<intptr_t>(this)));

  if (!Java_ScreenCapture_startCapture(env, screen_capture_)) {
    // Error immediately if we can't start capture.
    finishing_ = true;
  }
}

void DesktopCapturerAndroid::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {}

void DesktopCapturerAndroid::CaptureFrame() {
  CHECK(task_runner_);
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(callback_);

  if (finishing_) {
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_PERMANENT,
                               nullptr);
    return;
  }

  if (!next_frame_) {
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY,
                               nullptr);
    return;
  }

  // TODO(crbug.com/352187279): `DesktopCaptureDevice` expects results in ARGB
  // but Android generally produces results in ABGR. We should add
  // `webrtc::FourCC` info to the `DesktopCapturer` interface to handle this.
  callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                             std::move(next_frame_));
  next_frame_.reset();
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
  // Use unsigned checked arithmetic since our operations should never go
  // negative.
  PlaneInfo plane;
  plane.release_cb = base::android::ScopedJavaGlobalRef(release_cb);
  plane.buf = base::android::ScopedJavaGlobalRef(buf);
  plane.pixel_stride = unchecked_pixel_stride;
  plane.row_stride = unchecked_row_stride;
  plane.crop_left = unchecked_crop_left;
  plane.crop_top = unchecked_crop_top;
  plane.crop_right = unchecked_crop_right;
  plane.crop_bottom = unchecked_crop_bottom;

  // It's guaranteed that `this` is valid here because destruction is blocked
  // until all JNI methods are complete.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DesktopCapturerAndroid::ProcessRgbaFrame,
                     weak_ptr_factory_.GetWeakPtr(), std::move(plane)));
}

void DesktopCapturerAndroid::OnStop(JNIEnv* env) {
  // It's guaranteed that `this` is valid here because destruction is blocked
  // until all JNI methods are complete.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DesktopCapturerAndroid::Shutdown,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void DesktopCapturerAndroid::Shutdown() {
  CHECK(task_runner_);
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(!finishing_);
  finishing_ = true;
}

void DesktopCapturerAndroid::ProcessRgbaFrame(PlaneInfo plane) {
  CHECK(task_runner_);
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  // Don't process frames if we are no longer doing anything.
  if (finishing_) {
    return;
  }

  // TODO(crbug.com/352187279): Process the frame.

  base::android::RunRunnableAndroid(plane.release_cb);
}

}  // namespace content
