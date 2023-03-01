// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_input_event_tracker_android.h"

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/android/motion_event_android.h"

namespace content {
namespace {

ui::MotionEventAndroid CreateTouchEventAt(
    float x,
    float y,
    jobject event,
    base::TimeTicks event_time = base::TimeTicks()) {
  ui::MotionEventAndroid::Pointer pointer0(0, x, y, 0, 0, 0, 0, 0);
  ui::MotionEventAndroid::Pointer pointer1(0, 0, 0, 0, 0, 0, 0, 0);
  return ui::MotionEventAndroid(nullptr, event, 1.f, 0, 0, 0, event_time, 0, 1,
                                0, 0, 0, 0, 0, 0, 0, 0, false, &pointer0,
                                &pointer1);
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
      base::StringPiece str) {
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
  EXPECT_TRUE(input_event_tracker_->GetMostRecentEvent().is_null());

  base::android::ScopedJavaLocalRef<jstring> str = GetJavaString("str");
  OnTouchEvent(CreateTouchEventAt(100.f, 100.f, str.obj()));
  EXPECT_TRUE(IsSameObject(input_event_tracker_->GetMostRecentEvent(), str));

  task_environment()->FastForwardBy(
      AttributionInputEventTrackerAndroid::kEventExpiry);
  EXPECT_TRUE(IsSameObject(input_event_tracker_->GetMostRecentEvent(), str));

  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(input_event_tracker_->GetMostRecentEvent().is_null());
}

}  // namespace content
