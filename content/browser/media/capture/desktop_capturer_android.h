// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_ANDROID_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_ANDROID_H_

#include <jni.h>

#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace content {

// `DesktopCapturer` implementation for Android. The lifetime model is somewhat
// complex because there are a few things involved:
// - An instance of `DesktopCapturerAndroid` which creates a Java side object,
//    `ScreenCapture`.
// - `ScreenCapture` Java object, which manages interaction with the OS.
// -  A background thread created by `ScreenCapture` which calls back into
//     `DesktopCapturerAndroid` with the actual buffers from the OS.
//
// This is additionally complicated by the following factors:
// - Screen capture may be stopped from either the C++ side or the Java side.
// - Buffers must be freed on the Java side, but must be consumed on the desktop
//    capturer thread.
//
// We make the following observations:
// - We don't want to try to send frames (i.e. call any methods of `Callback`)
//    while `DesktopCapturerAndroid` is being destructed, since that happens
//    during destruction of the owning objects too.
// - C++ side JNI methods must have a valid `this` pointer at least some of the
//    time for them to know anything about what's safe to do, so necessarily we
//    may sometimes have to block in the destructor. This class is marked final
//    just in case to avoid future usages that access derived members that are
//    already destructed in this case. Blocking is accomplished using locking on
//    the Java side to wait for methods calling C++ side JNI methods.
// - In-flight but not yet executed tasks (e.g. processing a frame that came
//    from the background thread) need to be cancellable in some way to avoid
//    calling `Callback` methods during destruction.
// - Since destruction waits on C++ side JNI methods to complete from the
//    desktop capturer thread, C++ side JNI methods must not wait on progress on
//    the desktop capturer thread during destruction (i.e. while waiting on C++
//    side JNI methods) or there will be deadlocks.
//
// To handle these, we adopt the following rules:
// - C++ side JNI methods must not directly touch member variables that are
//   modified on the desktop capturer thread, since they are called from the
//   background thread.
// - C++ side JNI methods must not block on the desktop capturer thread, or
//   there could be a deadlock with destruction.
// - Java side code calling C++ side JNI methods must participate in locking to
//   prevent destruction of `DesktopCapturerAndroid` (to ensure C++ side JNI
//   methods have a valid `this` pointer).
class DesktopCapturerAndroid final : public webrtc::DesktopCapturer {
 public:
  DesktopCapturerAndroid(const webrtc::DesktopCaptureOptions& options);
  DesktopCapturerAndroid(const DesktopCapturerAndroid&) = delete;
  DesktopCapturerAndroid& operator=(const DesktopCapturerAndroid&) = delete;
  ~DesktopCapturerAndroid() override;

  // DesktopCapturer:
  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void CaptureFrame() override;
  bool SelectSource(SourceId id) override;

  // JNI - these methods are called from Java and may be invoked on a different
  // thread. They should not perform any work on member variables that are
  // accessed from the main thread, and should instead post a task to the main
  // thread to do so.
  void OnRgbaFrameAvailable(JNIEnv* env,
                            const base::android::JavaRef<jobject>& release_cb,
                            jlong timestamp_ns,
                            const base::android::JavaRef<jobject>& buf,
                            jint unchecked_pixel_stride,
                            jint unchecked_row_stride,
                            jint unchecked_crop_left,
                            jint unchecked_crop_top,
                            jint unchecked_crop_right,
                            jint unchecked_crop_bottom);

  void OnStop(JNIEnv* env);

 private:
  // `PlaneInfo` stores all info needed to process buffers received from
  // Android.
  struct PlaneInfo {
    PlaneInfo();
    ~PlaneInfo();
    PlaneInfo(PlaneInfo&& other);
    PlaneInfo& operator=(PlaneInfo&& other);

    PlaneInfo(const PlaneInfo&) = delete;
    PlaneInfo& operator=(const PlaneInfo&) = delete;

    // Java callback to run when this plane's buffer is no longer in use.
    base::android::ScopedJavaGlobalRef<jobject> release_cb;
    // Timestamp of the frame in nanoseconds.
    int64_t timestamp_ns;
    // Java ByteBuffer containing the plane data.
    base::android::ScopedJavaGlobalRef<jobject> buf;
    // The number of bytes between the start of adjacent pixels in a row.
    base::CheckedNumeric<uint32_t> pixel_stride;
    // The number of bytes between the start of adjacent rows of pixels.
    base::CheckedNumeric<uint32_t> row_stride;
    // The x-coordinate of the top-left corner of the crop rectangle.
    base::CheckedNumeric<uint32_t> crop_left;
    // The y-coordinate of the top-left corner of the crop rectangle.
    base::CheckedNumeric<uint32_t> crop_top;
    // The x-coordinate of the bottom-right corner of the crop rectangle.
    base::CheckedNumeric<uint32_t> crop_right;
    // The y-coordinate of the bottom-right corner of the crop rectangle.
    base::CheckedNumeric<uint32_t> crop_bottom;
  };

  void Shutdown();

  void ProcessRgbaFrame(int64_t timestamp_ns, PlaneInfo plane);

  raw_ptr<Callback> callback_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> screen_capture_;

  std::unique_ptr<webrtc::DesktopFrame> next_frame_;
  int64_t last_frame_time_ns_ = 0;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool finishing_ = false;

  base::WeakPtrFactory<DesktopCapturerAndroid> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_ANDROID_H_
