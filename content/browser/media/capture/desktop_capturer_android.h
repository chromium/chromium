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
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace content {

class CONTENT_EXPORT DesktopCapturerAndroidJniInterface {
 public:
  virtual ~DesktopCapturerAndroidJniInterface() = default;
  virtual base::android::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      jlong native_ptr) = 0;
  virtual jboolean StartCapture(JNIEnv* env,
                                const base::android::JavaRef<jobject>& obj) = 0;
  virtual void Destroy(JNIEnv* env,
                       const base::android::JavaRef<jobject>& obj) = 0;
};

// `DesktopCapturer` implementation for Android. There are a few things
// involved:
// - An instance of `DesktopCapturerAndroid` which creates a Java side object,
//    `ScreenCapture`.
// - `ScreenCapture` Java object, which manages interaction with the OS.
//
// This is complicated by the following factors:
// - Screen capture may be stopped from either the C++ side or the Java side.
// - Buffers must be freed on the Java side, but must be consumed on the desktop
//    capturer thread.
//
// On Android, the desktop capturer thread is created with an Android message
// pump, so we can keep everything on one thread.
class CONTENT_EXPORT DesktopCapturerAndroid final
    : public webrtc::DesktopCapturer {
 public:
  explicit DesktopCapturerAndroid(const webrtc::DesktopCaptureOptions& options);
  DesktopCapturerAndroid(
      const webrtc::DesktopCaptureOptions& options,
      std::unique_ptr<DesktopCapturerAndroidJniInterface> jni_interface);

  DesktopCapturerAndroid(const DesktopCapturerAndroid&) = delete;
  DesktopCapturerAndroid& operator=(const DesktopCapturerAndroid&) = delete;
  ~DesktopCapturerAndroid() override;

  // DesktopCapturer:
  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void CaptureFrame() override;
  bool SelectSource(SourceId id) override;

  // JNI:
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

  void OnI420FrameAvailable(JNIEnv* env,
                            const base::android::JavaRef<jobject>& release_cb,
                            jlong timestamp_ns,
                            const base::android::JavaRef<jobject>& y_buf,
                            jint y_unchecked_pixel_stride,
                            jint y_unchecked_row_stride,
                            const base::android::JavaRef<jobject>& u_buf,
                            jint u_unchecked_pixel_stride,
                            jint u_unchecked_row_stride,
                            const base::android::JavaRef<jobject>& v_buf,
                            jint v_unchecked_pixel_stride,
                            jint v_unchecked_row_stride,
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
  bool finishing_ = false;
  std::unique_ptr<DesktopCapturerAndroidJniInterface> jni_interface_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_ANDROID_H_
