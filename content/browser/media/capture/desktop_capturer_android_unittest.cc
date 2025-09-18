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

using testing::Return;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 4;
constexpr int kBytesPerPixel = 4;
constexpr int kStride = kWidth * kBytesPerPixel;
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
  return base::android::ScopedJavaLocalRef<jobject>(
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
        .WillByDefault(Return(base::android::ScopedJavaLocalRef<jobject>(
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

  void PushRgbaFrame() {
    std::vector<uint8_t> buffer(kStride * kHeight, 'A');
    auto j_buf = CreateJavaByteBuffer(buffer);
    RunnableFlag release_cb;
    capturer_->OnRgbaFrameAvailable(env_, release_cb.GetJavaObject(),
                                    kTimestampNs, j_buf, kBytesPerPixel,
                                    kStride, 0, 0, kWidth, kHeight);
    EXPECT_TRUE(release_cb.WasRun());
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

}  // namespace content
