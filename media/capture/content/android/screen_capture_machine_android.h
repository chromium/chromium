// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CONTENT_ANDROID_SCREEN_CAPTURE_MACHINE_ANDROID_H_
#define MEDIA_CAPTURE_CONTENT_ANDROID_SCREEN_CAPTURE_MACHINE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "media/capture/capture_export.h"

namespace media {

class ThreadSafeCaptureOracle;
struct VideoCaptureParams;
class VideoFrame;

// ScreenCaptureMachineAndroid captures 32bit RGB or YUV420 triplanar.
class CAPTURE_EXPORT ScreenCaptureMachineAndroid {
 public:
  ScreenCaptureMachineAndroid();

  ScreenCaptureMachineAndroid(const ScreenCaptureMachineAndroid&) = delete;
  ScreenCaptureMachineAndroid& operator=(const ScreenCaptureMachineAndroid&) =
      delete;

  virtual ~ScreenCaptureMachineAndroid();

  static base::android::ScopedJavaLocalRef<jobject>
  createScreenCaptureMachineAndroid(jlong nativeScreenCaptureMachineAndroid);

  // Implement org.chromium.media.ScreenCapture.nativeOnRGBAFrameAvailable.
  void OnRGBAFrameAvailable(JNIEnv* env,
                            const base::android::JavaRef<jobject>& obj,
                            const base::android::JavaRef<jobject>& buf,
                            jint row_stride,
                            jint left,
                            jint top,
                            jint width,
                            jint height,
                            jlong timestamp);
  // Implement org.chromium.media.ScreenCapture.nativeOnI420FrameAvailable.
  void OnI420FrameAvailable(JNIEnv* env,
                            const base::android::JavaRef<jobject>& obj,
                            const base::android::JavaRef<jobject>& y_buffer,
                            jint y_stride,
                            const base::android::JavaRef<jobject>& u_buffer,
                            const base::android::JavaRef<jobject>& v_buffer,
                            jint uv_row_stride,
                            jint uv_pixel_stride,
                            jint left,
                            jint top,
                            jint width,
                            jint height,
                            jlong timestamp);

  // Implement org.chromium.media.ScreenCapture.nativeOnActivityResult.
  void OnActivityResult(JNIEnv* env,
                        const base::android::JavaRef<jobject>& obj,
                        jboolean result);

  // Implement org.chromium.media.ScreenCaptuer.nativeOnOrientationChange.
  void OnOrientationChange(JNIEnv* env,
                           const base::android::JavaRef<jobject>& obj,
                           jint rotation);

  // Starts/Stops capturing.
  bool Start(scoped_refptr<ThreadSafeCaptureOracle> oracle_proxy,
             const VideoCaptureParams& params);
  void Stop();

  // If there is a cached frame, and the oracle allows sending another frame
  // right now, the cached captured frame is redelivered.
  void MaybeCaptureForRefresh();

 private:
  // Indicates the orientation of the device.
  enum DeviceOrientation { kLandscape, kPortrait, kDefault };

  // Makes all the decisions about which frames to copy, and how.
  scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy_;

  // Cache the last frame for possible refreshing.
  scoped_refptr<VideoFrame> lastFrame_;

  // Java VideoCaptureAndroid instance.
  base::android::ScopedJavaLocalRef<jobject> j_capture_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_CONTENT_ANDROID_SCREEN_CAPTURE_MACHINE_ANDROID_H_
