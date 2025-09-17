// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using testing::Return;

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

  void StartCapturer() {
    base::android::ScopedJavaLocalRef<jclass> object_class =
        base::android::GetClass(env_, "java/lang/Object");
    jmethodID constructor =
        env_->GetMethodID(object_class.obj(), "<init>", "()V");
    ON_CALL(*jni_interface_, Create)
        .WillByDefault(Return(base::android::ScopedJavaLocalRef<jobject>(
            env_, env_->NewObject(object_class.obj(), constructor))));
    ON_CALL(*jni_interface_, StartCapture).WillByDefault(Return(true));
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

 private:
  std::unique_ptr<DesktopCapturerAndroid> capturer_;
  raw_ptr<MockDesktopCapturerAndroidJni> jni_interface_ = nullptr;
  raw_ptr<JNIEnv> env_ = nullptr;
  std::optional<webrtc::DesktopCapturer::Result> last_result_ = std::nullopt;
  std::unique_ptr<webrtc::DesktopFrame> last_frame_;
};

TEST_F(DesktopCapturerAndroidTest, StartSuccessInitialState) {
  StartCapturer();
  // If Start succeeds, CaptureFrame should initially report TEMPORARY error
  // until the first frame arrives from Java.
  auto [result, frame] = CaptureFrame();
  EXPECT_EQ(result, webrtc::DesktopCapturer::Result::ERROR_TEMPORARY);
  EXPECT_EQ(frame, nullptr);
}

}  // namespace content
