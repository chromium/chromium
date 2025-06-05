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
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace content {

// `DesktopCapturer` implementation for Android.
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

  void OnRgbaFrameAvailable(JNIEnv* env,
                            const base::android::JavaRef<jobject>& release_cb,
                            const base::android::JavaRef<jobject>& buf,
                            jint unchecked_pixel_stride,
                            jint unchecked_row_stride,
                            jint unchecked_crop_left,
                            jint unchecked_crop_top,
                            jint unchecked_crop_right,
                            jint unchecked_crop_bottom);

 private:
  raw_ptr<Callback> callback_ = nullptr;
  base::android::ScopedJavaLocalRef<jobject> screen_capture_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_ANDROID_H_
