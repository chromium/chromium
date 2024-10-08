// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_input_event_tracker_android.h"

#include <jni.h>

#include <memory>
#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/android/motion_event_android_java.h"

namespace content {
namespace {

std::unique_ptr<ui::MotionEventAndroid> CreateTouchEventAt(
    float x,
    float y,
    jobject event,
    base::TimeTicks event_time = base::TimeTicks()) {
  ui::MotionEventAndroid::Pointer pointer0(0, x, y, 0, 0, 0, 0, 0);
  ui::MotionEventAndroid::Pointer pointer1(0, 0, 0, 0, 0, 0, 0, 0);
  return std::unique_ptr<ui::MotionEventAndroid>(new ui::MotionEventAndroidJava(
      nullptr, event, 1.f, 0, 0, 0, event_time, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      false, &pointer0, &pointer1));
}

}  // namespace

class AttributionInputEventTrackerAndroidTest
    : public RenderViewHostTestHarness {
 public:
  AttributionInputEventTrackerAndroidTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    env_ = base::android::AttachCurrentThread();

    input_event_tracker_ =
        std::make_unique<AttributionInputEventTrackerAndroid>(web_contents());
  }

  void OnTouchEvent(const ui::MotionEventAndroid& event) {
    input_event_tracker_->OnTouchEvent(event);
  }

 protected:
  // The Java strings are used as standins for the input events.
  base::android::ScopedJavaLocalRef<jstring> GetJavaString(
      std::string_view str) {
    return base::android::ConvertUTF8ToJavaString(env_, str);
  }

  template <typename U, typename V>
  bool IsSameObject(const base::android::JavaRef<U>& a,
                    const base::android::JavaRef<V>& b) {
    return static_cast<bool>(env_->IsSameObject(a.obj(), b.obj()));
  }

  raw_ptr<JNIEnv> env_;
  std::unique_ptr<AttributionInputEventTrackerAndroid> input_event_tracker_;
};

TEST_F(AttributionInputEventTrackerAndroidTest, EventExpiryApplied) {
  AttributionInputEventTrackerAndroid::InputEvent input1 =
      input_event_tracker_->GetMostRecentEvent();
  EXPECT_TRUE(input1.event.is_null());
  EXPECT_FALSE(input1.id.has_value());

  base::android::ScopedJavaLocalRef<jstring> str = GetJavaString("str");
  std::unique_ptr<ui::MotionEventAndroid> event =
      CreateTouchEventAt(100.f, 100.f, str.obj());
  OnTouchEvent(*event);
  AttributionInputEventTrackerAndroid::InputEvent input2 =
      input_event_tracker_->GetMostRecentEvent();
  EXPECT_TRUE(IsSameObject(input2.event, str));
  EXPECT_TRUE(input2.id.has_value());
  EXPECT_GE(input2.id, 1u);

  task_environment()->FastForwardBy(
      AttributionInputEventTrackerAndroid::kEventExpiry);

  AttributionInputEventTrackerAndroid::InputEvent input3 =
      input_event_tracker_->GetMostRecentEvent();
  EXPECT_TRUE(IsSameObject(input3.event, str));
  EXPECT_EQ(input2.id, input3.id);

  task_environment()->FastForwardBy(base::Milliseconds(1));
  AttributionInputEventTrackerAndroid::InputEvent input4 =
      input_event_tracker_->GetMostRecentEvent();
  EXPECT_TRUE(input4.event.is_null());
}

}  // namespace content
