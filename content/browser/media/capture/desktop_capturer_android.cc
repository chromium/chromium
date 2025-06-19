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
    jlong timestamp_ns,
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
      FROM_HERE, base::BindOnce(&DesktopCapturerAndroid::ProcessRgbaFrame,
                                weak_ptr_factory_.GetWeakPtr(), timestamp_ns,
                                std::move(plane)));
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

namespace {

// TODO(crbug.com/352187279): `DesktopCaptureDevice` expects results in ARGB
// but Android generally produces results in ABGR. We should add
// `webrtc::FourCC` info to the `DesktopCapturer` interface to handle this.
void RgbaToBgra(webrtc::DesktopFrame& frame) {
  static_assert(webrtc::DesktopFrame::kBytesPerPixel == 4,
                "kBytesPerPixel must be 4");
  uint8_t* data = frame.data();
  for (int r = 0; r < frame.size().height(); ++r) {
    for (int c = 2; c < frame.stride();
         c += webrtc::DesktopFrame::kBytesPerPixel) {
      // SAFETY: `c - 2` is non-negative and c is less than the stride.
      UNSAFE_BUFFERS(std::swap(data[c - 2], data[c]));
    }
    // SAFETY: It's guaranteed that the size of the memory pointed to by
    // `frame.data()` is at least height * stride.
    UNSAFE_BUFFERS(data += frame.stride());
  }
}

}  // namespace

void DesktopCapturerAndroid::ProcessRgbaFrame(int64_t timestamp_ns,
                                              PlaneInfo plane) {
  CHECK(task_runner_);
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  // Don't process frames if we are no longer doing anything.
  if (finishing_) {
    return;
  }

  const auto width = plane.crop_right - plane.crop_left;
  const auto height = plane.crop_bottom - plane.crop_top;
  const webrtc::DesktopSize size(width.ValueOrDie<int32_t>(),
                                 height.ValueOrDie<int32_t>());
  next_frame_.reset(new webrtc::BasicDesktopFrame(size));

  // We don't have access to this information to Android, but this is only
  // used for mouse cursor stuff, which we don't support currently.
  next_frame_->set_top_left(webrtc::DesktopVector());

  // We don't have damage information on Android, so damage the whole frame.
  next_frame_->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(next_frame_->size()));

  // TODO(crbug.com/352187279): Set DPI based on display.
  next_frame_->set_dpi(webrtc::DesktopVector());

  // TODO(crbug.com/352187279): The cursor is captured for screen capture but
  // not for window capture. Currently there is no way to determine if we are
  // doing screen or window capture on Android. If we can determine this and set
  // it conditionally here we also need a way to get the cursor position by
  // implementing `MouseCursorMonitor`.
  next_frame_->set_may_contain_cursor(true);

  // Calculate the time delta from the previous frame's timestamp. It does not
  // seem guaranteed that the timestamp we get from Android is always monotonic,
  // and there's no guarantee about how it is not monotonic (e.g. unsigned
  // wrapping), so don't provide a timestamp in this case.
  if (last_frame_time_ns_ == 0 || timestamp_ns <= last_frame_time_ns_) {
    next_frame_->set_capture_time_ms(0);
  } else {
    next_frame_->set_capture_time_ms((timestamp_ns - last_frame_time_ns_) /
                                     base::Time::kNanosecondsPerMillisecond);
  }
  last_frame_time_ns_ = timestamp_ns;

  // TODO(crbug.com/352187279): Create `DesktopCapturerId` for Android.
  next_frame_->set_capturer_id(webrtc::DesktopCapturerId::kUnknown);

  // There is no way to get an ICC profile on Android.
  next_frame_->set_icc_profile({});

  JNIEnv* env = base::android::AttachCurrentThread();
  const auto span = base::android::JavaByteBufferToSpan(env, plane.buf.obj());
  const auto offset =
      plane.crop_top * plane.row_stride + plane.crop_left * plane.pixel_stride;

  CHECK_EQ(
      static_cast<int>(static_cast<uint32_t>(plane.pixel_stride.ValueOrDie())),
      webrtc::DesktopFrame::kBytesPerPixel);
  CHECK_LE(static_cast<uint32_t>((width * plane.pixel_stride).ValueOrDie()),
           static_cast<uint32_t>(plane.row_stride.ValueOrDie()));
  CHECK_LE(static_cast<uint32_t>(offset.ValueOrDie()), span.size_bytes());
  CHECK_LE(
      static_cast<uint32_t>((offset + height * plane.row_stride).ValueOrDie()),
      span.size());

  // TODO(crbug.com/352187279): Extract to `SharedMemory` instead of copying if
  // possible, or, use `ScreenCaptureFrameQueue` and `ResolutionTracker` to
  // reuse frames.
  next_frame_->CopyPixelsFrom(
      span.get_at(offset.ValueOrDie()),
      static_cast<uint32_t>(plane.row_stride.ValueOrDie()),
      webrtc::DesktopRect::MakeSize(size));

  RgbaToBgra(*next_frame_);

  base::android::RunRunnableAndroid(plane.release_cb);
}

}  // namespace content
