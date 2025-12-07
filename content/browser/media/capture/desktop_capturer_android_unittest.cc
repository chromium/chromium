// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_callback.h"
#include "base/android/jni_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/gfx/geometry/rect.h"

using testing::Return;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 4;
constexpr int kBytesPerPixel = 4;
constexpr int kStride = kWidth * kBytesPerPixel;
constexpr int kFrameSize = kStride * kHeight;
constexpr int kTimestampNs = 1000;

class RunnableFlag {
 public:
  RunnableFlag() = default;

  void Run() { was_run_ = true; }
  bool WasRun() const { return was_run_; }

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() {
    return base::android::ToJniCallback(
        base::android::AttachCurrentThread(),
        base::BindOnce(&RunnableFlag::Run, base::Unretained(this)));
  }

 private:
  bool was_run_ = false;
};

base::android::ScopedJavaLocalRef<jobject> CreateJavaByteBuffer(
    base::span<uint8_t> data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return jni_zero::ScopedJavaLocalRef<>::Adopt(
      env, env->NewDirectByteBuffer(data.data(), data.size()));
}

}  // namespace

namespace content {

class MockDesktopCapturerAndroidJni
    : public DesktopCapturerAndroidJniInterface {
 public:
  ~MockDesktopCapturerAndroidJni() override = default;
  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobject>,
              Create,
              (JNIEnv*, jlong),
              (override));
  MOCK_METHOD(jboolean,
              StartCapture,
              (JNIEnv*, const base::android::JavaRef<jobject>&),
              (override));
  MOCK_METHOD(void,
              Destroy,
              (JNIEnv*, const base::android::JavaRef<jobject>&),
              (override));
};

class DesktopCapturerAndroidTest : public testing::Test,
                                   public webrtc::DesktopCapturer::Callback {
 public:
  void SetUp() override {
    env_ = base::android::AttachCurrentThread();
    auto jni_interface = std::make_unique<MockDesktopCapturerAndroidJni>();
    jni_interface_ = jni_interface.get();
    capturer_ = std::make_unique<DesktopCapturerAndroid>(
        webrtc::DesktopCaptureOptions(), std::move(jni_interface));
  }

  // webrtc::DesktopCapturer::Callback.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override {
    last_result_ = result;
    last_frame_ = std::move(frame);
  }

  void StartCapturer(bool start_success) {
    base::android::ScopedJavaLocalRef<jclass> object_class =
        base::android::GetClass(env_, "java/lang/Object");
    jmethodID constructor =
        env_->GetMethodID(object_class.obj(), "<init>", "()V");
    ON_CALL(*jni_interface_, Create)
        .WillByDefault(Return(jni_zero::ScopedJavaLocalRef<>::Adopt(
            env_, env_->NewObject(object_class.obj(), constructor))));
    ON_CALL(*jni_interface_, StartCapture).WillByDefault(Return(start_success));
    EXPECT_CALL(*jni_interface_, Create).Times(1);
    EXPECT_CALL(*jni_interface_, StartCapture).Times(1);
    EXPECT_CALL(*jni_interface_, Destroy).Times(1);

    capturer_->Start(this);
  }

  std::pair<webrtc::DesktopCapturer::Result,
            std::unique_ptr<webrtc::DesktopFrame>>
  CaptureFrame() {
    CHECK(!last_result_.has_value());
    CHECK_EQ(last_frame_, nullptr);
    capturer_->CaptureFrame();
    CHECK(last_result_.has_value());
    return {std::exchange(last_result_, std::nullopt).value(),
            std::move(last_frame_)};
  }

 protected:
  raw_ptr<JNIEnv> env_ = nullptr;
  std::unique_ptr<DesktopCapturerAndroid> capturer_;

  void PushRgbaFrame(base::span<uint8_t> buffer, int64_t timestamp_ns) {
    auto j_buf = CreateJavaByteBuffer(buffer);
    RunnableFlag release_cb;
    capturer_->OnRgbaFrameAvailable(env_, release_cb.GetJavaObject(),
                                    timestamp_ns, j_buf, kBytesPerPixel,
                                    kStride, 0, 0, kWidth, kHeight);
    EXPECT_TRUE(release_cb.WasRun());
  }

  void PushRgbaFrame(base::span<uint8_t> buffer) {
    PushRgbaFrame(buffer, kTimestampNs);
  }

  void PushRgbaFrame() { PushRgbaFrameWithTimestampNs(kTimestampNs); }

  void PushRgbaFrameWithTimestampNs(int64_t timestamp_ns) {
    std::vector<uint8_t> buffer(kFrameSize, 'A');
    PushRgbaFrame(buffer, timestamp_ns);
  }

  void ExpectFrameWithCaptureTime(base::TimeDelta capture_time) {
    const auto& [result, frame] = CaptureFrame();
    EXPECT_EQ(result, webrtc::DesktopCapturer::Result::SUCCESS);
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->capture_time_ms(), capture_time.InMilliseconds());
  }

 private:
  raw_ptr<MockDesktopCapturerAndroidJni> jni_interface_ = nullptr;
  std::optional<webrtc::DesktopCapturer::Result> last_result_ = std::nullopt;
  std::unique_ptr<webrtc::DesktopFrame> last_frame_;
};

TEST_F(DesktopCapturerAndroidTest, StartSuccessInitialState) {
  StartCapturer(/*start_success=*/true);
  // If Start succeeds, CaptureFrame should initially report TEMPORARY error
  // until the first frame arrives from Java.
  const auto& [result, frame] = CaptureFrame();
  EXPECT_EQ(result, webrtc::DesktopCapturer::Result::ERROR_TEMPORARY);
  EXPECT_EQ(frame, nullptr);
}

TEST_F(DesktopCapturerAndroidTest, StartFailurePermanentError) {
  StartCapturer(/*start_success=*/false);
  const auto& [result, frame] = CaptureFrame();
  EXPECT_EQ(result, webrtc::DesktopCapturer::Result::ERROR_PERMANENT);
  EXPECT_EQ(frame, nullptr);
}

TEST_F(DesktopCapturerAndroidTest, OnRgbaFrameAvailable) {
  StartCapturer(/*start_success=*/true);

  PushRgbaFrame();
  const auto& [result, frame] = CaptureFrame();
  EXPECT_EQ(result, webrtc::DesktopCapturer::Result::SUCCESS);
  EXPECT_NE(frame, nullptr);
}

TEST_F(DesktopCapturerAndroidTest, OnStopBehavior) {
  StartCapturer(/*start_success=*/true);
  capturer_->OnStop(env_);

  // Should return PERMANENT error because we are stopping.
  const auto& [result0, frame0] = CaptureFrame();
  EXPECT_EQ(result0, webrtc::DesktopCapturer::Result::ERROR_PERMANENT);
  EXPECT_EQ(frame0, nullptr);

  // Send a frame after OnStop.
  PushRgbaFrame();

  const auto& [result1, frame1] = CaptureFrame();
  EXPECT_EQ(result1, webrtc::DesktopCapturer::Result::ERROR_PERMANENT);
  EXPECT_EQ(frame1, nullptr);
}

TEST_F(DesktopCapturerAndroidTest, FrameArrivesThenOnStopReleasesFrame) {
  StartCapturer(/*start_success=*/true);

  // The release callback should be run even though the frame is never captured.
  PushRgbaFrame();

  // Stop the capturer before the frame is consumed.
  capturer_->OnStop(env_);

  // Subsequent captures should fail permanently.
  const auto& [result, frame] = CaptureFrame();
  EXPECT_EQ(result, webrtc::DesktopCapturer::Result::ERROR_PERMANENT);
  EXPECT_EQ(frame, nullptr);
}

TEST_F(DesktopCapturerAndroidTest, OnStopCalledTwiceDeathTest) {
  StartCapturer(/*start_success=*/true);
  capturer_->OnStop(env_);
  EXPECT_DEATH(capturer_->OnStop(env_), "");
}

TEST_F(DesktopCapturerAndroidTest, CaptureAndTemporaryError) {
  StartCapturer(/*start_success=*/true);

  const auto& [result0, frame0] = CaptureFrame();
  EXPECT_EQ(result0, webrtc::DesktopCapturer::Result::ERROR_TEMPORARY);

  // Simulate a frame from the OS.
  std::vector<uint8_t> buffer(kFrameSize, 'A');
  PushRgbaFrame(buffer);

  // We should get the frame back.
  const auto& [result1, frame1] = CaptureFrame();
  EXPECT_EQ(result1, webrtc::DesktopCapturer::Result::SUCCESS);
  ASSERT_TRUE(frame1);
  EXPECT_EQ(frame1->size().width(), kWidth);
  ASSERT_EQ(frame1->size().height(), kHeight);
  ASSERT_EQ(frame1->stride(), kStride);
  EXPECT_EQ(frame1->pixel_format(), webrtc::FOURCC_ABGR);
  webrtc::DesktopRegion full_region(
      webrtc::DesktopRect::MakeSize(frame1->size()));
  EXPECT_TRUE(frame1->updated_region().Equals(full_region));

  // SAFETY: No safe interface to DesktopFrame. Size must be equal to `buffer`
  // if the stride and height are the same.
  EXPECT_EQ(UNSAFE_BUFFERS(base::span(frame1->data(), buffer.size())),
            base::span(buffer));

  const auto& [result2, frame2] = CaptureFrame();
  EXPECT_EQ(result2, webrtc::DesktopCapturer::Result::ERROR_TEMPORARY);
}

TEST_F(DesktopCapturerAndroidTest, MultipleFramesArriveBeforeCapture) {
  StartCapturer(/*start_success=*/true);

  std::vector<uint8_t> buffer_a(kFrameSize, 'A');
  std::vector<uint8_t> buffer_b(kFrameSize, 'B');

  PushRgbaFrame(buffer_a, kTimestampNs);
  PushRgbaFrame(buffer_b, kTimestampNs + 1000);

  // We should get the more recent frame back.
  const auto& [result, frame] = CaptureFrame();
  EXPECT_EQ(result, webrtc::DesktopCapturer::Result::SUCCESS);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->size().width(), kWidth);
  ASSERT_EQ(frame->size().height(), kHeight);
  ASSERT_EQ(frame->stride(), kStride);

  // Verify the data is from the second frame.
  // SAFETY: No safe interface to DesktopFrame. Size must be equal to
  // `buffer_b` if the stride and height are the same.
  EXPECT_EQ(UNSAFE_BUFFERS(base::span(frame->data(), buffer_b.size())),
            base::span(buffer_b));
}

TEST_F(DesktopCapturerAndroidTest, TimestampCalculation) {
  StartCapturer(/*start_success=*/true);

  // The capture time should be how long it took to capture the frame. Android
  // does not provide this value, so we estimate it as the time since the last
  // frame was produced. Initially, we have it be zero to represent an unknown
  // amount.
  PushRgbaFrameWithTimestampNs(100 * base::Time::kNanosecondsPerMillisecond);
  ExpectFrameWithCaptureTime(base::Milliseconds(0));

  // Frame 2: 33ms later.
  PushRgbaFrameWithTimestampNs(133 * base::Time::kNanosecondsPerMillisecond);
  ExpectFrameWithCaptureTime(base::Milliseconds(33));

  // Frame 3: Timestamp goes backwards (non-monotonic). We have it be zero in
  // this case, since it's unknown.
  PushRgbaFrameWithTimestampNs(130 * base::Time::kNanosecondsPerMillisecond);
  ExpectFrameWithCaptureTime(base::Milliseconds(0));

  // Frame 4: Timestamp is the same. Time delta should be 0.
  PushRgbaFrameWithTimestampNs(130 * base::Time::kNanosecondsPerMillisecond);
  ExpectFrameWithCaptureTime(base::Milliseconds(0));

  // Frame 5: Timestamp goes forward again. Difference should be based on the
  // last timestamp, even if it was a non-monotonic update.
  PushRgbaFrameWithTimestampNs(160 * base::Time::kNanosecondsPerMillisecond);
  ExpectFrameWithCaptureTime(base::Milliseconds(30));
}

TEST_F(DesktopCapturerAndroidTest, RgbaFrameWithStrideAndCrop) {
  StartCapturer(/*start_success=*/true);

  // Setup a buffer larger than the actual image and crop a region.
  constexpr int kBufferWidth = 32;
  constexpr int kBufferHeight = 32;
  // Add extra padding to the stride.
  constexpr int kStride = kBufferWidth * kBytesPerPixel + 16;

  constexpr gfx::Rect kCropRect(4, 8, 16, 12);

  std::vector<uint8_t> buffer(kStride * kBufferHeight, 'A');
  auto buffer_span = base::span(buffer);
  for (int y = 0; y < kBufferHeight; ++y) {
    for (int x = 0; x < kBufferWidth; ++x) {
      auto pixel_span = buffer_span.subspan(
          base::checked_cast<size_t>(y * kStride + x * kBytesPerPixel), 4u);
      // Store coordinates in the first two bytes to verify correct copying.
      pixel_span[0] = static_cast<uint8_t>(x);
      pixel_span[1] = static_cast<uint8_t>(y);
    }
  }

  auto j_buf = CreateJavaByteBuffer(buffer);
  RunnableFlag release_cb;
  capturer_->OnRgbaFrameAvailable(env_, release_cb.GetJavaObject(),
                                  kTimestampNs, j_buf, kBytesPerPixel, kStride,
                                  kCropRect.x(), kCropRect.y(),
                                  kCropRect.right(), kCropRect.bottom());
  EXPECT_TRUE(release_cb.WasRun());

  const auto& [result, frame] = CaptureFrame();
  EXPECT_EQ(result, webrtc::DesktopCapturer::Result::SUCCESS);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->size().width(), kCropRect.width());
  ASSERT_EQ(frame->size().height(), kCropRect.height());

  // The frame should be tightly packed.
  ASSERT_EQ(frame->stride(), kCropRect.width() * kBytesPerPixel);

  // SAFETY: No safe interface to DesktopFrame.
  auto frame_span = UNSAFE_BUFFERS(base::span(
      frame->data(),
      base::checked_cast<size_t>(frame->stride() * frame->size().height())));
  for (int y = 0; y < kCropRect.height(); ++y) {
    for (int x = 0; x < kCropRect.width(); ++x) {
      const auto pixel_span = frame_span.subspan(
          base::checked_cast<size_t>(y * frame->stride() + x * kBytesPerPixel),
          2u);
      const int src_x = x + kCropRect.x();
      const int src_y = y + kCropRect.y();
      EXPECT_EQ(pixel_span[0], static_cast<uint8_t>(src_x));
      EXPECT_EQ(pixel_span[1], static_cast<uint8_t>(src_y));
    }
  }
}

TEST_F(DesktopCapturerAndroidTest, RgbaFrameInvalidBufferSizeDeathTest) {
  StartCapturer(/*start_success=*/true);

  // Buffer is one byte too small.
  std::vector<uint8_t> buffer(kFrameSize - 1);
  auto j_buf = CreateJavaByteBuffer(buffer);
  RunnableFlag release_cb;

  // Should CHECK.
  EXPECT_DEATH(capturer_->OnRgbaFrameAvailable(
                   env_, release_cb.GetJavaObject(), kTimestampNs, j_buf,
                   kBytesPerPixel, kStride, 0, 0, kWidth, kHeight),
               "");
  EXPECT_FALSE(release_cb.WasRun());
}

TEST_F(DesktopCapturerAndroidTest, RgbaFrameInvalidStrideDeathTest) {
  StartCapturer(/*start_success=*/true);

  std::vector<uint8_t> buffer(kFrameSize);
  auto j_buf = CreateJavaByteBuffer(buffer);
  RunnableFlag release_cb;

  // Stride is one byte too small.
  const int kStride = kWidth * kBytesPerPixel - 1;
  EXPECT_DEATH(capturer_->OnRgbaFrameAvailable(
                   env_, release_cb.GetJavaObject(), kTimestampNs, j_buf,
                   kBytesPerPixel, kStride, 0, 0, kWidth, kHeight),
               "");
  EXPECT_FALSE(release_cb.WasRun());
}

TEST_F(DesktopCapturerAndroidTest, RgbaFrameInvalidCropRectDeathTest) {
  StartCapturer(/*start_success=*/true);

  std::vector<uint8_t> buffer(kFrameSize);
  auto j_buf = CreateJavaByteBuffer(buffer);
  RunnableFlag release_cb;

  EXPECT_DEATH(capturer_->OnRgbaFrameAvailable(
                   env_, release_cb.GetJavaObject(), kTimestampNs, j_buf,
                   kBytesPerPixel, kStride, 1, 0, 0, kHeight),
               "");
  EXPECT_FALSE(release_cb.WasRun());
}

TEST_F(DesktopCapturerAndroidTest, RgbaFramePreciseBufferBoundaryWithPadding) {
  StartCapturer(/*start_success=*/true);

  constexpr int kBufferWidth = 32;
  constexpr gfx::Rect kCropRect(2, 3, 16, 12);
  constexpr int kStride = kBufferWidth + 100;
  constexpr int kOffset =
      kCropRect.y() * kStride + kCropRect.x() * kBytesPerPixel;

  constexpr size_t kBufferSize =
      kStride * (kCropRect.bottom() - 1) + kCropRect.right() * kBytesPerPixel;

  std::vector<uint8_t> buffer(kBufferSize, 'A');
  auto buffer_span = base::span(buffer);
  for (int y = 0; y < kCropRect.height(); ++y) {
    for (int x = 0; x < kCropRect.width(); ++x) {
      auto pixel_span =
          buffer_span.subspan(base::checked_cast<size_t>(kOffset + y * kStride +
                                                         x * kBytesPerPixel),
                              2u);
      pixel_span[0] = static_cast<uint8_t>(x + kCropRect.x());
      pixel_span[1] = static_cast<uint8_t>(y + kCropRect.y());
    }
  }

  auto j_buf = CreateJavaByteBuffer(buffer);
  RunnableFlag release_cb;
  capturer_->OnRgbaFrameAvailable(env_, release_cb.GetJavaObject(),
                                  kTimestampNs, j_buf, kBytesPerPixel, kStride,
                                  kCropRect.x(), kCropRect.y(),
                                  kCropRect.right(), kCropRect.bottom());
  EXPECT_TRUE(release_cb.WasRun());

  auto [result, frame] = CaptureFrame();
  EXPECT_EQ(result, webrtc::DesktopCapturer::Result::SUCCESS);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->size().width(), kCropRect.width());
  ASSERT_EQ(frame->size().height(), kCropRect.height());
  ASSERT_EQ(frame->stride(), kCropRect.width() * kBytesPerPixel);

  // SAFETY: No safe interface to DesktopFrame.
  auto frame_span = UNSAFE_BUFFERS(base::span(
      frame->data(),
      base::checked_cast<size_t>(frame->stride() * frame->size().height())));
  for (int y = 0; y < kCropRect.height(); ++y) {
    for (int x = 0; x < kCropRect.width(); ++x) {
      const auto pixel_span = frame_span.subspan(
          base::checked_cast<size_t>(y * frame->stride() + x * kBytesPerPixel),
          2u);
      const int src_x = x + kCropRect.x();
      const int src_y = y + kCropRect.y();
      EXPECT_EQ(pixel_span[0], static_cast<uint8_t>(src_x));
      EXPECT_EQ(pixel_span[1], static_cast<uint8_t>(src_y));
    }
  }
}

TEST_F(DesktopCapturerAndroidTest,
       RgbaFramePreciseBufferBoundaryTooSmallDeathTest) {
  StartCapturer(/*start_success=*/true);

  constexpr int kBufferWidth = 32;
  constexpr gfx::Rect kCropRect(2, 3, 16, 12);
  constexpr int kStride = kBufferWidth + 100;

  constexpr size_t kBufferSize =
      kStride * (kCropRect.bottom() - 1) + kCropRect.right() * kBytesPerPixel;

  std::vector<uint8_t> buffer(kBufferSize - 1, 'A');
  auto j_buf = CreateJavaByteBuffer(buffer);
  RunnableFlag release_cb;

  // This should CHECK because the buffer was not large enough.
  EXPECT_DEATH(capturer_->OnRgbaFrameAvailable(
                   env_, release_cb.GetJavaObject(), kTimestampNs, j_buf,
                   kBytesPerPixel, kStride, kCropRect.x(), kCropRect.y(),
                   kCropRect.right(), kCropRect.bottom()),
               "");
  EXPECT_FALSE(release_cb.WasRun());
}

}  // namespace content
