// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_ANDROID_VIDEO_CAPTURE_DEVICE_ANDROID_H_
#define MEDIA_CAPTURE_VIDEO_ANDROID_VIDEO_CAPTURE_DEVICE_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device.h"

namespace base {
class Location;
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// VideoCaptureDevice on Android. The VideoCaptureDevice API's are called
// by VideoCaptureManager on its own thread, while OnFrameAvailable is called
// on JAVA thread (i.e., UI thread). Both will access |state_| and |client_|,
// but only VideoCaptureManager would change their value.
class CAPTURE_EXPORT VideoCaptureDeviceAndroid : public VideoCaptureDevice {
 public:
  // Automatically generated enum to interface with Java world.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum AndroidImageFormat {
    // Android graphics ImageFormat mapping, see reference in:
    // http://developer.android.com/reference/android/graphics/ImageFormat.html
    ANDROID_IMAGE_FORMAT_NV21 = 17,
    ANDROID_IMAGE_FORMAT_YUV_420_888 = 35,
    ANDROID_IMAGE_FORMAT_YV12 = 842094169,
    ANDROID_IMAGE_FORMAT_UNKNOWN = 0,
  };

  // A Java counterpart will be generated for this enum.
  // The values of these are matched with the ones in media::VideoCaptureError
  // to allow direct static_casting.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum class AndroidVideoCaptureError {
    ANDROID_API_1_CAMERA_ERROR_CALLBACK_RECEIVED = 68,
    ANDROID_API_2_CAMERA_DEVICE_ERROR_RECEIVED = 69,
    ANDROID_API_2_CAPTURE_SESSION_CONFIGURE_FAILED = 70,
    ANDROID_API_2_IMAGE_READER_UNEXPECTED_IMAGE_FORMAT = 71,
    ANDROID_API_2_IMAGE_READER_SIZE_DID_NOT_MATCH_IMAGE_SIZE = 72,
    ANDROID_API_2_ERROR_RESTARTING_PREVIEW = 73,
    ANDROID_API_2_ERROR_CONFIGURING_CAMERA = 114,
  };

  // A Java counterpart will be generated for this enum.
  // The values of these are matched with the ones in
  // media::VideoCaptureFrameDropReason to allow direct static_casting.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum class AndroidVideoCaptureFrameDropReason {
    ANDROID_API_1_UNEXPECTED_DATA_LENGTH = 8,
    ANDROID_API_2_ACQUIRED_IMAGE_IS_NULL = 9,
  };

  VideoCaptureDeviceAndroid() = delete;

  explicit VideoCaptureDeviceAndroid(
      const VideoCaptureDeviceDescriptor& device_descriptor);

  VideoCaptureDeviceAndroid(const VideoCaptureDeviceAndroid&) = delete;
  VideoCaptureDeviceAndroid& operator=(const VideoCaptureDeviceAndroid&) =
      delete;

  ~VideoCaptureDeviceAndroid() override;

  static VideoCaptureDevice* Create(
      const VideoCaptureDeviceDescriptor& device_descriptor);

  // Registers the Java VideoCaptureDevice pointer, used by the rest of the
  // methods of the class to operate the Java capture code. This method must be
  // called after the class constructor and before AllocateAndStart().
  bool Init();

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;

  // Implement org.chromium.media.VideoCapture.nativeOnFrameAvailable.
  void OnFrameAvailable(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jbyteArray>& data,
                        jint length,
                        jint rotation);

  // Implement org.chromium.media.VideoCapture.nativeOnI420FrameAvailable.
  void OnI420FrameAvailable(JNIEnv* env,
                            jobject obj,
                            jobject y_buffer,
                            jint y_stride,
                            jobject u_buffer,
                            jobject v_buffer,
                            jint uv_row_stride,
                            jint uv_pixel_stride,
                            jint width,
                            jint height,
                            jint rotation,
                            jlong timestamp);

  // Implement org.chromium.media.VideoCapture.nativeOnError.
  void OnError(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               int android_video_capture_error,
               const base::android::JavaParamRef<jstring>& message);

  // Implement org.chromium.media.VideoCapture.nativeOnFrameDropped.
  void OnFrameDropped(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      int android_video_capture_frame_drop_reason);

  void OnGetPhotoCapabilitiesReply(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong callback_id,
      jobject photo_capabilities);

  // Implement org.chromium.media.VideoCapture.nativeOnPhotoTaken.
  void OnPhotoTaken(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jlong callback_id,
                    const base::android::JavaParamRef<jbyteArray>& data);

  // Implement org.chromium.media.VideoCapture.nativeOnStarted.
  void OnStarted(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Implement
  // org.chromium.media.VideoCapture.nativeDCheckCurrentlyOnIncomingTaskRunner.
  void DCheckCurrentlyOnIncomingTaskRunner(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void ConfigureForTesting();

 protected:
  // Helper code executed when the frame is available; if it is the first frame,
  // setup time fluctuation control and process any pending photo requests.
  void ProcessFirstFrameAvailable(base::TimeTicks current_time);

  // Checks if there is a client and if the |state_| is kConfigured.
  bool IsClientConfigured();

  // Checks if the incoming frame arrived too early so that is needs to be
  // dropped. If not, advance the next frame expectation time and return false;
  bool ThrottleFrame(base::TimeTicks current_time);

  void SendIncomingDataToClient(const uint8_t* data,
                                int length,
                                int rotation,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp);

 private:
  enum InternalState {
    kIdle,        // The device is opened but not in use.
    kConfigured,  // The device has been AllocateAndStart()ed.
    kError        // Hit error. User needs to recover by destroying the object.
  };

  VideoPixelFormat GetColorspace();
  void SetErrorState(media::VideoCaptureError error,
                     const base::Location& from_here,
                     const std::string& reason);

  void DoGetPhotoState(GetPhotoStateCallback callback);
  void DoSetPhotoOptions(mojom::PhotoSettingsPtr settings,
                         SetPhotoOptionsCallback callback);
  void DoTakePhoto(TakePhotoCallback callback);

  // Thread on which we are created.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // |lock_| protects |state_|, |client_|, |got_first_frame_| and
  // |photo_requests_queue_| from concurrent access.
  base::Lock lock_;
  InternalState state_ = kIdle;
  std::unique_ptr<VideoCaptureDevice::Client> client_;
  bool got_first_frame_ = false;
  // Photo-related requests waiting for |got_first_frame_| to be served. Android
  // APIs need the device capturing or nearly-capturing to be fully operational.
  std::list<base::OnceClosure> photo_requests_queue_;

  base::TimeTicks expected_next_frame_time_;
  base::TimeDelta frame_interval_;

  // List of callbacks for photo API in flight, being served in Java side.
  base::Lock photo_callbacks_lock_;
  std::list<std::unique_ptr<GetPhotoStateCallback>> get_photo_state_callbacks_;
  std::list<std::unique_ptr<TakePhotoCallback>> take_photo_callbacks_;

  const VideoCaptureDeviceDescriptor device_descriptor_;
  VideoCaptureFormat capture_format_;
  gfx::ColorSpace capture_color_space_;

  // Java VideoCaptureAndroid instance.
  base::android::ScopedJavaLocalRef<jobject> j_capture_;

  base::WeakPtrFactory<VideoCaptureDeviceAndroid> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_ANDROID_VIDEO_CAPTURE_DEVICE_ANDROID_H_
