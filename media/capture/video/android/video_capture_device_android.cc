// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/video_capture_device_android.h"

#include <stdint.h>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jni/VideoCapture_jni.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/android/photo_capabilities.h"
#include "media/capture/video/android/video_capture_device_factory_android.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/point_f.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::GetClass;
using base::android::JavaParamRef;
using base::android::MethodID;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace media {

namespace {

mojom::MeteringMode ToMojomMeteringMode(
    PhotoCapabilities::AndroidMeteringMode android_mode) {
  switch (android_mode) {
    case PhotoCapabilities::AndroidMeteringMode::FIXED:
      return mojom::MeteringMode::MANUAL;
    case PhotoCapabilities::AndroidMeteringMode::SINGLE_SHOT:
      return mojom::MeteringMode::SINGLE_SHOT;
    case PhotoCapabilities::AndroidMeteringMode::CONTINUOUS:
      return mojom::MeteringMode::CONTINUOUS;
    case PhotoCapabilities::AndroidMeteringMode::NONE:
      return mojom::MeteringMode::NONE;
    case PhotoCapabilities::AndroidMeteringMode::NOT_SET:
      NOTREACHED();
  }
  return mojom::MeteringMode::NONE;
}

PhotoCapabilities::AndroidMeteringMode ToAndroidMeteringMode(
    mojom::MeteringMode mojom_mode) {
  switch (mojom_mode) {
    case mojom::MeteringMode::MANUAL:
      return PhotoCapabilities::AndroidMeteringMode::FIXED;
    case mojom::MeteringMode::SINGLE_SHOT:
      return PhotoCapabilities::AndroidMeteringMode::SINGLE_SHOT;
    case mojom::MeteringMode::CONTINUOUS:
      return PhotoCapabilities::AndroidMeteringMode::CONTINUOUS;
    case mojom::MeteringMode::NONE:
      return PhotoCapabilities::AndroidMeteringMode::NONE;
  }
  NOTREACHED();
  return PhotoCapabilities::AndroidMeteringMode::NOT_SET;
}

mojom::FillLightMode ToMojomFillLightMode(
    PhotoCapabilities::AndroidFillLightMode android_mode) {
  switch (android_mode) {
    case PhotoCapabilities::AndroidFillLightMode::FLASH:
      return mojom::FillLightMode::FLASH;
    case PhotoCapabilities::AndroidFillLightMode::AUTO:
      return mojom::FillLightMode::AUTO;
    case PhotoCapabilities::AndroidFillLightMode::OFF:
      return mojom::FillLightMode::OFF;
    case PhotoCapabilities::AndroidFillLightMode::NOT_SET:
      NOTREACHED();
  }
  NOTREACHED();
  return mojom::FillLightMode::OFF;
}

PhotoCapabilities::AndroidFillLightMode ToAndroidFillLightMode(
    mojom::FillLightMode mojom_mode) {
  switch (mojom_mode) {
    case mojom::FillLightMode::FLASH:
      return PhotoCapabilities::AndroidFillLightMode::FLASH;
    case mojom::FillLightMode::AUTO:
      return PhotoCapabilities::AndroidFillLightMode::AUTO;
    case mojom::FillLightMode::OFF:
      return PhotoCapabilities::AndroidFillLightMode::OFF;
  }
  NOTREACHED();
  return PhotoCapabilities::AndroidFillLightMode::NOT_SET;
}

}  // anonymous namespace

VideoCaptureDeviceAndroid::VideoCaptureDeviceAndroid(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      device_descriptor_(device_descriptor),
      weak_ptr_factory_(this) {}

VideoCaptureDeviceAndroid::~VideoCaptureDeviceAndroid() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  StopAndDeAllocate();
}

bool VideoCaptureDeviceAndroid::Init() {
  int id;
  if (!base::StringToInt(device_descriptor_.device_id, &id))
    return false;

  j_capture_.Reset(VideoCaptureDeviceFactoryAndroid::createVideoCaptureAndroid(
      id, reinterpret_cast<intptr_t>(this)));
  return true;
}

void VideoCaptureDeviceAndroid::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kIdle)
      return;
    client_ = std::move(client);
    got_first_frame_ = false;
  }

  JNIEnv* env = AttachCurrentThread();

  jboolean ret = Java_VideoCapture_allocate(
      env, j_capture_, params.requested_format.frame_size.width(),
      params.requested_format.frame_size.height(),
      params.requested_format.frame_rate);
  if (!ret) {
    SetErrorState(media::VideoCaptureError::kAndroidFailedToAllocate, FROM_HERE,
                  "failed to allocate");
    return;
  }

  capture_format_.frame_size.SetSize(
      Java_VideoCapture_queryWidth(env, j_capture_),
      Java_VideoCapture_queryHeight(env, j_capture_));
  capture_format_.frame_rate =
      Java_VideoCapture_queryFrameRate(env, j_capture_);
  capture_format_.pixel_format = GetColorspace();
  DCHECK_NE(capture_format_.pixel_format, PIXEL_FORMAT_UNKNOWN);
  CHECK(capture_format_.frame_size.GetArea() > 0);
  CHECK(!(capture_format_.frame_size.width() % 2));
  CHECK(!(capture_format_.frame_size.height() % 2));

  if (capture_format_.frame_rate > 0) {
    frame_interval_ = base::TimeDelta::FromMicroseconds(
        (base::Time::kMicrosecondsPerSecond + capture_format_.frame_rate - 1) /
        capture_format_.frame_rate);
  }

  DVLOG(1) << __func__ << " requested ("
           << capture_format_.frame_size.ToString() << ")@ "
           << capture_format_.frame_rate << "fps";

  ret = Java_VideoCapture_startCaptureMaybeAsync(env, j_capture_);
  if (!ret) {
    SetErrorState(media::VideoCaptureError::kAndroidFailedToStartCapture,
                  FROM_HERE, "failed to start capture");
    return;
  }

  {
    base::AutoLock lock(lock_);
    state_ = kConfigured;
  }
}

void VideoCaptureDeviceAndroid::StopAndDeAllocate() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured && state_ != kError)
      return;
  }

  JNIEnv* env = AttachCurrentThread();

  const jboolean ret =
      Java_VideoCapture_stopCaptureAndBlockUntilStopped(env, j_capture_);
  if (!ret) {
    SetErrorState(media::VideoCaptureError::kAndroidFailedToStopCapture,
                  FROM_HERE, "failed to stop capture");
    return;
  }

  {
    base::AutoLock lock(lock_);
    state_ = kIdle;
    client_.reset();
  }

  Java_VideoCapture_deallocate(env, j_capture_);
}

void VideoCaptureDeviceAndroid::TakePhoto(TakePhotoCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured)
      return;
    if (!got_first_frame_) {  // We have to wait until we get the first frame.
      photo_requests_queue_.push_back(
          base::Bind(&VideoCaptureDeviceAndroid::DoTakePhoto,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
      return;
    }
  }
  DoTakePhoto(std::move(callback));
}

void VideoCaptureDeviceAndroid::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured)
      return;
    if (!got_first_frame_) {  // We have to wait until we get the first frame.
      photo_requests_queue_.push_back(
          base::Bind(&VideoCaptureDeviceAndroid::DoGetPhotoState,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
      return;
    }
  }
  DoGetPhotoState(std::move(callback));
}

void VideoCaptureDeviceAndroid::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured)
      return;
    if (!got_first_frame_) {  // We have to wait until we get the first frame.
      photo_requests_queue_.push_back(
          base::Bind(&VideoCaptureDeviceAndroid::DoSetPhotoOptions,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&settings),
                     base::Passed(&callback)));
      return;
    }
  }
  DoSetPhotoOptions(std::move(settings), std::move(callback));
}

void VideoCaptureDeviceAndroid::OnFrameAvailable(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jbyteArray>& data,
    jint length,
    jint rotation) {
  if (!IsClientConfigured())
    return;

  const base::TimeTicks current_time = base::TimeTicks::Now();
  ProcessFirstFrameAvailable(current_time);
  // Using |expected_next_frame_time_| to estimate a proper capture timestamp
  // since android.hardware.Camera API doesn't expose a better timestamp.
  const base::TimeDelta capture_time =
      expected_next_frame_time_ - base::TimeTicks();

  // Deliver the frame when it doesn't arrive too early.
  if (ThrottleFrame(current_time)) {
    client_->OnFrameDropped(VideoCaptureFrameDropReason::kAndroidThrottling);
    return;
  }

  jbyte* buffer = env->GetByteArrayElements(data, NULL);
  if (!buffer) {
    LOG(ERROR) << "VideoCaptureDeviceAndroid::OnFrameAvailable: "
                  "failed to GetByteArrayElements";
    // In case of error, restore back the throttle control value.
    expected_next_frame_time_ -= frame_interval_;
    client_->OnFrameDropped(
        VideoCaptureFrameDropReason::kAndroidGetByteArrayElementsFailed);
    return;
  }

  // TODO(qiangchen): Investigate how to get raw timestamp for Android,
  // rather than using reference time to calculate timestamp.
  SendIncomingDataToClient(reinterpret_cast<uint8_t*>(buffer), length, rotation,
                           current_time, capture_time);

  env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
}

void VideoCaptureDeviceAndroid::OnI420FrameAvailable(JNIEnv* env,
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
                                                     jlong timestamp) {
  if (!IsClientConfigured())
    return;
  const int64_t absolute_micro =
      timestamp / base::Time::kNanosecondsPerMicrosecond;
  const base::TimeDelta capture_time =
      base::TimeDelta::FromMicroseconds(absolute_micro);

  const base::TimeTicks current_time = base::TimeTicks::Now();
  ProcessFirstFrameAvailable(current_time);

  // Deliver the frame when it doesn't arrive too early.
  if (ThrottleFrame(current_time)) {
    client_->OnFrameDropped(VideoCaptureFrameDropReason::kAndroidThrottling);
    return;
  }

  uint8_t* const y_src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(y_buffer));
  CHECK(y_src);
  uint8_t* const u_src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(u_buffer));
  CHECK(u_src);
  uint8_t* const v_src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(v_buffer));
  CHECK(v_src);

  const int y_plane_length = width * height;
  const int uv_plane_length = y_plane_length / 4;
  const int buffer_length = y_plane_length + uv_plane_length * 2;
  std::unique_ptr<uint8_t> buffer(new uint8_t[buffer_length]);

  libyuv::Android420ToI420(y_src, y_stride, u_src, uv_row_stride, v_src,
                           uv_row_stride, uv_pixel_stride, buffer.get(), width,
                           buffer.get() + y_plane_length, width / 2,
                           buffer.get() + y_plane_length + uv_plane_length,
                           width / 2, width, height);

  SendIncomingDataToClient(buffer.get(), buffer_length, rotation, current_time,
                           capture_time);
}

void VideoCaptureDeviceAndroid::OnError(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        int android_video_capture_error,
                                        const JavaParamRef<jstring>& message) {
  SetErrorState(
      static_cast<media::VideoCaptureError>(android_video_capture_error),
      FROM_HERE, base::android::ConvertJavaStringToUTF8(env, message));
}

void VideoCaptureDeviceAndroid::OnFrameDropped(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    int android_video_capture_frame_drop_reason) {
  base::AutoLock lock(lock_);
  if (!client_)
    return;
  client_->OnFrameDropped(static_cast<media::VideoCaptureFrameDropReason>(
      android_video_capture_frame_drop_reason));
}

void VideoCaptureDeviceAndroid::OnGetPhotoCapabilitiesReply(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong callback_id,
    jobject result) {
  base::AutoLock lock(photo_callbacks_lock_);
  GetPhotoStateCallback* const cb =
      reinterpret_cast<GetPhotoStateCallback*>(callback_id);
  // Search for the pointer |cb| in the list of |take_photo_callbacks_|.
  const auto reference_it = std::find_if(
      get_photo_state_callbacks_.begin(), get_photo_state_callbacks_.end(),
      [cb](const std::unique_ptr<GetPhotoStateCallback>& callback) {
        return callback.get() == cb;
      });
  if (reference_it == get_photo_state_callbacks_.end()) {
    NOTREACHED() << "|callback_id| not found.";
    return;
  }
  if (result == nullptr) {
    get_photo_state_callbacks_.erase(reference_it);
    return;
  }

  base::android::ScopedJavaLocalRef<jobject> scoped_photo_capabilities(env,
                                                                       result);
  PhotoCapabilities caps(scoped_photo_capabilities);

  // TODO(mcasas): Manual member copying sucks, consider adding typemapping from
  // PhotoCapabilities to mojom::PhotoStatePtr, https://crbug.com/622002.
  mojom::PhotoStatePtr photo_capabilities = mojo::CreateEmptyPhotoState();

  const auto jni_white_balance_modes = caps.getWhiteBalanceModes();
  std::vector<mojom::MeteringMode> white_balance_modes;
  for (const auto& white_balance_mode : jni_white_balance_modes)
    white_balance_modes.push_back(ToMojomMeteringMode(white_balance_mode));
  photo_capabilities->supported_white_balance_modes = white_balance_modes;
  photo_capabilities->current_white_balance_mode =
      ToMojomMeteringMode(caps.getWhiteBalanceMode());

  const auto jni_exposure_modes = caps.getExposureModes();
  std::vector<mojom::MeteringMode> exposure_modes;
  for (const auto& exposure_mode : jni_exposure_modes)
    exposure_modes.push_back(ToMojomMeteringMode(exposure_mode));
  photo_capabilities->supported_exposure_modes = exposure_modes;
  photo_capabilities->current_exposure_mode =
      ToMojomMeteringMode(caps.getExposureMode());

  const auto jni_focus_modes = caps.getFocusModes();
  std::vector<mojom::MeteringMode> focus_modes;
  for (const auto& focus_mode : jni_focus_modes)
    focus_modes.push_back(ToMojomMeteringMode(focus_mode));
  photo_capabilities->supported_focus_modes = focus_modes;
  photo_capabilities->current_focus_mode =
      ToMojomMeteringMode(caps.getFocusMode());

  photo_capabilities->focus_distance = mojom::Range::New();
  photo_capabilities->focus_distance->current = caps.getCurrentFocusDistance();
  photo_capabilities->focus_distance->max = caps.getMaxFocusDistance();
  photo_capabilities->focus_distance->min = caps.getMinFocusDistance();
  photo_capabilities->focus_distance->step = caps.getStepFocusDistance();

  photo_capabilities->exposure_compensation = mojom::Range::New();
  photo_capabilities->exposure_compensation->current =
      caps.getCurrentExposureCompensation();
  photo_capabilities->exposure_compensation->max =
      caps.getMaxExposureCompensation();
  photo_capabilities->exposure_compensation->min =
      caps.getMinExposureCompensation();
  photo_capabilities->exposure_compensation->step =
      caps.getStepExposureCompensation();

  photo_capabilities->exposure_time = mojom::Range::New();
  photo_capabilities->exposure_time->current = caps.getCurrentExposureTime();
  photo_capabilities->exposure_time->max = caps.getMaxExposureTime();
  photo_capabilities->exposure_time->min = caps.getMinExposureTime();
  photo_capabilities->exposure_time->step = caps.getStepExposureTime();

  photo_capabilities->color_temperature = mojom::Range::New();
  photo_capabilities->color_temperature->current =
      caps.getCurrentColorTemperature();
  photo_capabilities->color_temperature->max = caps.getMaxColorTemperature();
  photo_capabilities->color_temperature->min = caps.getMinColorTemperature();
  photo_capabilities->color_temperature->step = caps.getStepColorTemperature();
  photo_capabilities->iso = mojom::Range::New();
  photo_capabilities->iso->current = caps.getCurrentIso();
  photo_capabilities->iso->max = caps.getMaxIso();
  photo_capabilities->iso->min = caps.getMinIso();
  photo_capabilities->iso->step = caps.getStepIso();

  photo_capabilities->brightness = mojom::Range::New();
  photo_capabilities->contrast = mojom::Range::New();
  photo_capabilities->saturation = mojom::Range::New();
  photo_capabilities->sharpness = mojom::Range::New();

  photo_capabilities->zoom = mojom::Range::New();
  photo_capabilities->zoom->current = caps.getCurrentZoom();
  photo_capabilities->zoom->max = caps.getMaxZoom();
  photo_capabilities->zoom->min = caps.getMinZoom();
  photo_capabilities->zoom->step = caps.getStepZoom();

  photo_capabilities->supports_torch = caps.getSupportsTorch();
  photo_capabilities->torch = caps.getTorch();

  photo_capabilities->red_eye_reduction =
      caps.getRedEyeReduction() ? mojom::RedEyeReduction::CONTROLLABLE
                                : mojom::RedEyeReduction::NEVER;
  photo_capabilities->height = mojom::Range::New();
  photo_capabilities->height->current = caps.getCurrentHeight();
  photo_capabilities->height->max = caps.getMaxHeight();
  photo_capabilities->height->min = caps.getMinHeight();
  photo_capabilities->height->step = caps.getStepHeight();
  photo_capabilities->width = mojom::Range::New();
  photo_capabilities->width->current = caps.getCurrentWidth();
  photo_capabilities->width->max = caps.getMaxWidth();
  photo_capabilities->width->min = caps.getMinWidth();
  photo_capabilities->width->step = caps.getStepWidth();
  const auto fill_light_modes = caps.getFillLightModes();
  std::vector<mojom::FillLightMode> modes;
  for (const auto& fill_light_mode : fill_light_modes)
    modes.push_back(ToMojomFillLightMode(fill_light_mode));
  photo_capabilities->fill_light_mode = modes;

  std::move(*cb).Run(std::move(photo_capabilities));
  get_photo_state_callbacks_.erase(reference_it);
}

void VideoCaptureDeviceAndroid::OnPhotoTaken(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong callback_id,
    const base::android::JavaParamRef<jbyteArray>& data) {
  DCHECK(callback_id);

  base::AutoLock lock(photo_callbacks_lock_);

  TakePhotoCallback* const cb =
      reinterpret_cast<TakePhotoCallback*>(callback_id);
  // Search for the pointer |cb| in the list of |take_photo_callbacks_|.
  const auto reference_it =
      std::find_if(take_photo_callbacks_.begin(), take_photo_callbacks_.end(),
                   [cb](const std::unique_ptr<TakePhotoCallback>& callback) {
                     return callback.get() == cb;
                   });
  if (reference_it == take_photo_callbacks_.end()) {
    NOTREACHED() << "|callback_id| not found.";
    return;
  }

  if (data != nullptr) {
    mojom::BlobPtr blob = mojom::Blob::New();
    base::android::JavaByteArrayToByteVector(env, data, &blob->data);
    blob->mime_type = blob->data.empty() ? "" : "image/jpeg";
    std::move(*cb).Run(std::move(blob));
  }

  take_photo_callbacks_.erase(reference_it);
}

void VideoCaptureDeviceAndroid::OnStarted(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  if (client_)
    client_->OnStarted();
}

void VideoCaptureDeviceAndroid::DCheckCurrentlyOnIncomingTaskRunner(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void VideoCaptureDeviceAndroid::ConfigureForTesting() {
  Java_VideoCapture_setTestMode(AttachCurrentThread(), j_capture_);
}

void VideoCaptureDeviceAndroid::ProcessFirstFrameAvailable(
    base::TimeTicks current_time) {
  base::AutoLock lock(lock_);
  if (got_first_frame_)
    return;
  got_first_frame_ = true;

  // Set aside one frame allowance for fluctuation.
  expected_next_frame_time_ = current_time - frame_interval_;
  for (const auto& request : photo_requests_queue_)
    main_task_runner_->PostTask(FROM_HERE, request);
  photo_requests_queue_.clear();
}

bool VideoCaptureDeviceAndroid::IsClientConfigured() {
  base::AutoLock lock(lock_);
  return (state_ == kConfigured && client_);
}

bool VideoCaptureDeviceAndroid::ThrottleFrame(base::TimeTicks current_time) {
  if (expected_next_frame_time_ > current_time)
    return true;
  expected_next_frame_time_ += frame_interval_;
  return false;
}

void VideoCaptureDeviceAndroid::SendIncomingDataToClient(
    const uint8_t* data,
    int length,
    int rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  base::AutoLock lock(lock_);
  if (!client_)
    return;
  client_->OnIncomingCapturedData(data, length, capture_format_, rotation,
                                  reference_time, timestamp);
}

VideoPixelFormat VideoCaptureDeviceAndroid::GetColorspace() {
  JNIEnv* env = AttachCurrentThread();
  const int current_capture_colorspace =
      Java_VideoCapture_getColorspace(env, j_capture_);
  switch (current_capture_colorspace) {
    case ANDROID_IMAGE_FORMAT_YV12:
      return PIXEL_FORMAT_YV12;
    case ANDROID_IMAGE_FORMAT_YUV_420_888:
      return PIXEL_FORMAT_I420;
    case ANDROID_IMAGE_FORMAT_NV21:
      return PIXEL_FORMAT_NV21;
    case ANDROID_IMAGE_FORMAT_UNKNOWN:
    default:
      return PIXEL_FORMAT_UNKNOWN;
  }
}

void VideoCaptureDeviceAndroid::SetErrorState(media::VideoCaptureError error,
                                              const base::Location& from_here,
                                              const std::string& reason) {
  {
    base::AutoLock lock(lock_);
    state_ = kError;
    if (!client_)
      return;
    client_->OnError(error, from_here, reason);
  }
}

void VideoCaptureDeviceAndroid::DoTakePhoto(TakePhotoCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(lock_);
    DCHECK_EQ(kConfigured, state_);
    DCHECK(got_first_frame_);
  }
#endif
  JNIEnv* env = AttachCurrentThread();

  // Make copy on the heap so we can pass the pointer through JNI.
  std::unique_ptr<TakePhotoCallback> heap_callback(
      new TakePhotoCallback(std::move(callback)));
  const intptr_t callback_id = reinterpret_cast<intptr_t>(heap_callback.get());
  {
    base::AutoLock lock(photo_callbacks_lock_);
    take_photo_callbacks_.push_back(std::move(heap_callback));
  }
  Java_VideoCapture_takePhotoAsync(env, j_capture_, callback_id);
}

void VideoCaptureDeviceAndroid::DoGetPhotoState(
    GetPhotoStateCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(lock_);
    DCHECK_EQ(kConfigured, state_);
    DCHECK(got_first_frame_);
  }
#endif
  JNIEnv* env = AttachCurrentThread();

  // Make copy on the heap so we can pass the pointer through JNI.
  std::unique_ptr<GetPhotoStateCallback> heap_callback(
      new GetPhotoStateCallback(std::move(callback)));
  const intptr_t callback_id = reinterpret_cast<intptr_t>(heap_callback.get());
  {
    base::AutoLock lock(photo_callbacks_lock_);
    get_photo_state_callbacks_.push_back(std::move(heap_callback));
  }
  Java_VideoCapture_getPhotoCapabilitiesAsync(env, j_capture_, callback_id);
}

void VideoCaptureDeviceAndroid::DoSetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(lock_);
    DCHECK_EQ(kConfigured, state_);
    DCHECK(got_first_frame_);
  }
#endif
  JNIEnv* env = AttachCurrentThread();

  const double zoom = settings->has_zoom ? settings->zoom : 0.0;

  const double focusDistance =
      settings->has_focus_distance ? settings->focus_distance : 0.0;

  const PhotoCapabilities::AndroidMeteringMode focus_mode =
      settings->has_focus_mode
          ? ToAndroidMeteringMode(settings->focus_mode)
          : PhotoCapabilities::AndroidMeteringMode::NOT_SET;

  const PhotoCapabilities::AndroidMeteringMode exposure_mode =
      settings->has_exposure_mode
          ? ToAndroidMeteringMode(settings->exposure_mode)
          : PhotoCapabilities::AndroidMeteringMode::NOT_SET;

  const double width = settings->has_width ? settings->width : 0.0;
  const double height = settings->has_height ? settings->height : 0.0;

  std::vector<float> points_of_interest_marshalled;
  for (const auto& point : settings->points_of_interest) {
    points_of_interest_marshalled.push_back(point->x);
    points_of_interest_marshalled.push_back(point->y);
  }
  ScopedJavaLocalRef<jfloatArray> points_of_interest =
      base::android::ToJavaFloatArray(env, points_of_interest_marshalled);

  const double exposure_compensation = settings->has_exposure_compensation
                                           ? settings->exposure_compensation
                                           : 0.0;
  const double exposure_time =
      settings->has_exposure_time ? settings->exposure_time : 0.0;

  const PhotoCapabilities::AndroidMeteringMode white_balance_mode =
      settings->has_white_balance_mode
          ? ToAndroidMeteringMode(settings->white_balance_mode)
          : PhotoCapabilities::AndroidMeteringMode::NOT_SET;

  const double iso = settings->has_iso ? settings->iso : 0.0;

  const PhotoCapabilities::AndroidFillLightMode fill_light_mode =
      settings->has_fill_light_mode
          ? ToAndroidFillLightMode(settings->fill_light_mode)
          : PhotoCapabilities::AndroidFillLightMode::NOT_SET;

  const double color_temperature =
      settings->has_color_temperature ? settings->color_temperature : 0.0;

  Java_VideoCapture_setPhotoOptions(
      env, j_capture_, zoom, static_cast<int>(focus_mode), focusDistance,
      static_cast<int>(exposure_mode), width, height, points_of_interest,
      settings->has_exposure_compensation, exposure_compensation, exposure_time,
      static_cast<int>(white_balance_mode), iso,
      settings->has_red_eye_reduction, settings->red_eye_reduction,
      static_cast<int>(fill_light_mode), settings->has_torch, settings->torch,
      color_temperature);

  std::move(callback).Run(true);
}

}  // namespace media
