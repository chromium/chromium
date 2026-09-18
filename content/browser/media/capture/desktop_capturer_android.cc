// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_bytebuffer.h"
#include "base/feature_list.h"
#include "base/numerics/checked_math.h"
#include "third_party/webrtc/api/video/video_common.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/ScreenCapture_jni.h"

namespace content {

BASE_FEATURE(kDesktopCaptureAndroidFrameBufferReuse,
             "DesktopCaptureAndroidFrameBufferReuse",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

class DesktopCapturerAndroidJni : public DesktopCapturerAndroidJniInterface {
 public:
  ~DesktopCapturerAndroidJni() override = default;
  base::android::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      int64_t native_ptr) override {
    return Java_ScreenCapture_create(env, native_ptr);
  }
  bool StartCapture(JNIEnv* env,
                    const base::android::JavaRef<jobject>& obj) override {
    return Java_ScreenCapture_startCapture(env, obj);
  }
  void Destroy(JNIEnv* env,
               const base::android::JavaRef<jobject>& obj) override {
    Java_ScreenCapture_destroy(env, obj);
  }
};

}  // namespace

DesktopCapturerAndroid::PlaneInfo::PlaneInfo() = default;

DesktopCapturerAndroid::PlaneInfo::~PlaneInfo() = default;

DesktopCapturerAndroid::PlaneInfo::PlaneInfo(PlaneInfo&& other) = default;

DesktopCapturerAndroid::PlaneInfo& DesktopCapturerAndroid::PlaneInfo::operator=(
    PlaneInfo&& other) = default;

DesktopCapturerAndroid::DesktopCapturerAndroid(
    const webrtc::DesktopCaptureOptions& options)
    : DesktopCapturerAndroid(options,
                             std::make_unique<DesktopCapturerAndroidJni>()) {}

DesktopCapturerAndroid::DesktopCapturerAndroid(
    const webrtc::DesktopCaptureOptions& options,
    std::unique_ptr<DesktopCapturerAndroidJniInterface> jni_interface)
    : jni_interface_(std::move(jni_interface)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DesktopCapturerAndroid::~DesktopCapturerAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = base::android::AttachCurrentThread();
  jni_interface_->Destroy(env, screen_capture_);
}

void DesktopCapturerAndroid::Start(Callback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;
  frame_is_dirty_ = false;
  current_frame_.reset();

  JNIEnv* env = base::android::AttachCurrentThread();
  screen_capture_.Reset(
      jni_interface_->Create(env, reinterpret_cast<intptr_t>(this)));

  if (!jni_interface_->StartCapture(env, screen_capture_)) {
    // Error immediately if we can't start capture.
    finishing_ = true;
  }
}

void DesktopCapturerAndroid::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {}

void DesktopCapturerAndroid::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback_);

  if (finishing_) {
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_PERMANENT,
                               nullptr);
    return;
  }

  if (!current_frame_) {
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY,
                               nullptr);
    return;
  }

  std::unique_ptr<webrtc::DesktopFrame> frame = current_frame_->Share();
  if (frame_is_dirty_) {
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeSize(frame->size()));
    frame_is_dirty_ = false;
  } else {
    frame->mutable_updated_region()->Clear();
    // Serving a cached static frame requires no new capture work; reset
    // `capture_time_ms` to avoid reporting stale capture durations to metrics.
    frame->set_capture_time_ms(0);
  }

  callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
}

bool DesktopCapturerAndroid::SelectSource(SourceId id) {
  return true;
}

void DesktopCapturerAndroid::OnRgbaFrameAvailable(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& release_cb,
    int64_t timestamp_ns,
    const base::android::JavaRef<jobject>& buf,
    int32_t unchecked_pixel_stride,
    int32_t unchecked_row_stride,
    int32_t unchecked_crop_left,
    int32_t unchecked_crop_top,
    int32_t unchecked_crop_right,
    int32_t unchecked_crop_bottom) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  ProcessRgbaFrame(timestamp_ns, std::move(plane));
}

void DesktopCapturerAndroid::OnI420FrameAvailable(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& release_cb,
    int64_t timestamp_ns,
    const base::android::JavaRef<jobject>& y_buf,
    int32_t y_unchecked_pixel_stride,
    int32_t y_unchecked_row_stride,
    const base::android::JavaRef<jobject>& u_buf,
    int32_t u_unchecked_pixel_stride,
    int32_t u_unchecked_row_stride,
    const base::android::JavaRef<jobject>& v_buf,
    int32_t v_unchecked_pixel_stride,
    int32_t v_unchecked_row_stride,
    int32_t unchecked_crop_left,
    int32_t unchecked_crop_top,
    int32_t unchecked_crop_right,
    int32_t unchecked_crop_bottom) {
  // TODO(crbug.com/352187279): Implement processing of I420 frames.
  NOTREACHED();
}

void DesktopCapturerAndroid::OnStop(JNIEnv* env) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Shutdown();
}

void DesktopCapturerAndroid::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!finishing_);
  finishing_ = true;
}

void DesktopCapturerAndroid::ProcessRgbaFrame(int64_t timestamp_ns,
                                              PlaneInfo plane) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't process frames if we are no longer doing anything.
  if (finishing_) {
    jni_zero::RunRunnable(plane.release_cb);
    return;
  }

  const auto width = plane.crop_right - plane.crop_left;
  const auto height = plane.crop_bottom - plane.crop_top;
  const webrtc::DesktopSize size(width.ValueOrDie<int32_t>(),
                                 height.ValueOrDie<int32_t>());
  // Reuse `current_frame_` in-place if the buffer is not currently shared and
  // its dimensions match. `DesktopCaptureDevice` synchronously processes and
  // releases the `DesktopFrame` inside `OnCaptureResult()`, so
  // `!current_frame_->IsShared()` is normally true on the next frame.
  // Note: `DesktopCapturerAndroid` does not implement consumer-side frame
  // capping or flow control; the consumer (`Callback`) is expected to discard
  // shared frames ASAP to bound memory usage. If the callback retains the
  // frame beyond `OnCaptureResult()` or the screen size changes, allocate a
  // new buffer to avoid overwriting shared memory.
  if (!current_frame_ || !current_frame_->size().equals(size) ||
      current_frame_->IsShared() ||
      !base::FeatureList::IsEnabled(kDesktopCaptureAndroidFrameBufferReuse)) {
    current_frame_ = webrtc::SharedDesktopFrame::Wrap(
        std::make_unique<webrtc::BasicDesktopFrame>(size, webrtc::FOURCC_ABGR));
  }

  webrtc::DesktopFrame* current_frame = current_frame_.get();

  // We don't have access to this information to Android, but this is only
  // used for mouse cursor stuff, which we don't support currently.
  current_frame->set_top_left(webrtc::DesktopVector());

  // TODO(crbug.com/352187279): Set DPI based on display.
  current_frame->set_dpi(webrtc::DesktopVector());

  // TODO(crbug.com/352187279): The cursor is captured for screen capture but
  // not for window capture. Currently there is no way to determine if we are
  // doing screen or window capture on Android. If we can determine this and set
  // it conditionally here we also need a way to get the cursor position by
  // implementing `MouseCursorMonitor`.
  current_frame->set_may_contain_cursor(true);

  // Calculate the time delta from the previous frame's timestamp. It does not
  // seem guaranteed that the timestamp we get from Android is always monotonic,
  // and there's no guarantee about how it is not monotonic (e.g. unsigned
  // wrapping), so don't provide a timestamp in this case.
  if (last_frame_time_ns_ == 0 || timestamp_ns <= last_frame_time_ns_) {
    current_frame->set_capture_time_ms(0);
  } else {
    current_frame->set_capture_time_ms((timestamp_ns - last_frame_time_ns_) /
                                       base::Time::kNanosecondsPerMillisecond);
  }
  last_frame_time_ns_ = timestamp_ns;

  // TODO(crbug.com/352187279): Create `DesktopCapturerId` for Android.
  current_frame->set_capturer_id(webrtc::DesktopCapturerId::kUnknown);

  // There is no way to get an ICC profile on Android.
  current_frame->set_icc_profile({});

  JNIEnv* env = base::android::AttachCurrentThread();
  const auto span = base::android::JavaByteBufferToSpan(env, plane.buf);
  const auto offset =
      plane.crop_top * plane.row_stride + plane.crop_left * plane.pixel_stride;

  CHECK_EQ(
      static_cast<int>(static_cast<uint32_t>(plane.pixel_stride.ValueOrDie())),
      webrtc::DesktopFrame::kBytesPerPixel);
  CHECK_LE(static_cast<uint32_t>((width * plane.pixel_stride).ValueOrDie()),
           static_cast<uint32_t>(plane.row_stride.ValueOrDie()));
  CHECK_LE(static_cast<uint32_t>(offset.ValueOrDie()), span.size_bytes());
  // In the case that we have a crop rectangle, width will definitely be less
  // than row_stride, so only look at the number of actual pixels for the last
  // row. Also, even if there is no crop, it's conceptually possible for
  // row_stride to be larger than width*pixel_stride but for the buffer not to
  // be large enough to include the difference between row_stride and
  // width*pixel_stride at the end.
  CHECK_LE(static_cast<uint32_t>((offset + (height - 1) * plane.row_stride +
                                  width * plane.pixel_stride)
                                     .ValueOrDie()),
           span.size_bytes());

  // TODO(crbug.com/352187279): Extract to `SharedMemory` instead of copying if
  // possible.
  current_frame->CopyPixelsFrom(
      span.get_at(offset.ValueOrDie()),
      static_cast<uint32_t>(plane.row_stride.ValueOrDie()),
      webrtc::DesktopRect::MakeSize(size));

  frame_is_dirty_ = true;

  jni_zero::RunRunnable(plane.release_cb);
}

}  // namespace content

DEFINE_JNI(ScreenCapture)
