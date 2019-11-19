// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/plugin_instance_throttler_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/canvas.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::Return;

namespace content {

class PluginInstanceThrottlerImplTest
    : public testing::Test,
      public PluginInstanceThrottler::Observer {
 protected:
  const int kMaximumFramesToExamine =
      PluginInstanceThrottlerImpl::kMaximumFramesToExamine;

  PluginInstanceThrottlerImplTest() : change_callback_calls_(0) {}
  ~PluginInstanceThrottlerImplTest() override {
    throttler_->RemoveObserver(this);
  }

  void SetUp() override {
    throttler_.reset(
        new PluginInstanceThrottlerImpl(RenderFrame::DONT_RECORD_DECISION));
    throttler_->Initialize(nullptr,
                           url::Origin::Create(GURL("http://example.com")),
                           "Shockwave Flash", gfx::Size(100, 100));
    throttler_->AddObserver(this);
  }

  PluginInstanceThrottlerImpl* throttler() {
    DCHECK(throttler_.get());
    return throttler_.get();
  }

  void DisablePowerSaverByRetroactiveWhitelist() {
    throttler()->MarkPluginEssential(
        PluginInstanceThrottlerImpl::UNTHROTTLE_METHOD_BY_WHITELIST);
  }

  int change_callback_calls() { return change_callback_calls_; }

  void EngageThrottle() { throttler_->EngageThrottle(); }

  void SendEventAndTest(blink::WebInputEvent::Type event_type,
                        bool expect_consumed,
                        bool expect_throttled,
                        int expect_change_callback_count) {
    blink::WebMouseEvent event(event_type,
                               blink::WebInputEvent::Modifiers::kLeftButtonDown,
                               ui::EventTimeForNow());
    EXPECT_EQ(expect_consumed, throttler()->ConsumeInputEvent(event));
    EXPECT_EQ(expect_throttled, throttler()->IsThrottled());
    EXPECT_EQ(expect_change_callback_count, change_callback_calls());
  }

 private:
  // PluginInstanceThrottlerImpl::Observer
  void OnThrottleStateChange() override { ++change_callback_calls_; }

  std::unique_ptr<PluginInstanceThrottlerImpl> throttler_;

  int change_callback_calls_;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(PluginInstanceThrottlerImplTest, ThrottleAndUnthrottleByClick) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());

  // MouseUp while throttled should be consumed and disengage throttling.
  SendEventAndTest(blink::WebInputEvent::Type::kMouseUp, true, false, 2);
}

TEST_F(PluginInstanceThrottlerImplTest, ThrottleByKeyframe) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  SkBitmap boring_bitmap;
  gfx::Canvas canvas(gfx::Size(20, 10), 1.0f, true);
  canvas.FillRect(gfx::Rect(20, 10), SK_ColorBLACK);
  canvas.FillRect(gfx::Rect(10, 10), SK_ColorWHITE);
  SkBitmap interesting_bitmap = canvas.GetBitmap();

  // Don't throttle for a boring frame.
  throttler()->OnImageFlush(boring_bitmap);
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  // Throttle after an interesting frame.
  throttler()->OnImageFlush(interesting_bitmap);
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());
}

TEST_F(PluginInstanceThrottlerImplTest, MaximumKeyframesAnalyzed) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  SkBitmap boring_bitmap;

  // Throttle after tons of boring bitmaps.
  for (int i = 0; i < kMaximumFramesToExamine; ++i) {
    throttler()->OnImageFlush(boring_bitmap);
  }
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());
}
TEST_F(PluginInstanceThrottlerImplTest, IgnoreThrottlingAfterMouseUp) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  // MouseUp before throttling engaged should not be consumed, but should
  // prevent subsequent throttling from engaging.
  SendEventAndTest(blink::WebInputEvent::Type::kMouseUp, false, false, 0);

  EngageThrottle();
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());
}

TEST_F(PluginInstanceThrottlerImplTest, FastWhitelisting) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  DisablePowerSaverByRetroactiveWhitelist();

  EngageThrottle();
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());
}

TEST_F(PluginInstanceThrottlerImplTest, SlowWhitelisting) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());

  DisablePowerSaverByRetroactiveWhitelist();
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(2, change_callback_calls());
}

TEST_F(PluginInstanceThrottlerImplTest, EventConsumption) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());

  // Consume but don't unthrottle on a variety of other events.
  SendEventAndTest(blink::WebInputEvent::Type::kMouseDown, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::kMouseWheel, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::kMouseMove, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::kKeyDown, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::kKeyUp, true, true, 1);

  // Consume and unthrottle on MouseUp
  SendEventAndTest(blink::WebInputEvent::Type::kMouseUp, true, false, 2);

  // Don't consume events after unthrottle.
  SendEventAndTest(blink::WebInputEvent::Type::kMouseDown, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::kMouseWheel, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::kMouseMove, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::kKeyDown, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::kKeyUp, false, false, 2);

  // Subsequent MouseUps should also not be consumed.
  SendEventAndTest(blink::WebInputEvent::Type::kMouseUp, false, false, 2);
}

TEST_F(PluginInstanceThrottlerImplTest, ThrottleOnLeftClickOnly) {
  EXPECT_FALSE(throttler()->IsThrottled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->IsThrottled());
  EXPECT_EQ(1, change_callback_calls());

  blink::WebMouseEvent event(blink::WebInputEvent::Type::kMouseUp,
                             blink::WebInputEvent::Modifiers::kRightButtonDown,
                             ui::EventTimeForNow());
  EXPECT_FALSE(throttler()->ConsumeInputEvent(event));
  EXPECT_TRUE(throttler()->IsThrottled());

  event.SetModifiers(blink::WebInputEvent::Modifiers::kMiddleButtonDown);
  EXPECT_TRUE(throttler()->ConsumeInputEvent(event));
  EXPECT_TRUE(throttler()->IsThrottled());

  event.SetModifiers(blink::WebInputEvent::Modifiers::kLeftButtonDown);
  EXPECT_TRUE(throttler()->ConsumeInputEvent(event));
  EXPECT_FALSE(throttler()->IsThrottled());
}

}  // namespace content
