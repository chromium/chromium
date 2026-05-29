// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/wheel_event.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class WheelEventTest : public RenderingTest {};

TEST_F(WheelEventTest, MomentumExposed) {
  ScopedWheelEventMomentumForTest scoped_feature(true);

  WebMouseWheelEvent web_event(WebInputEvent::Type::kMouseWheel,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::GetStaticTimeStampForTests());

  web_event.momentum_phase = WebMouseWheelEvent::kPhaseBegan;
  WheelEvent* event = WheelEvent::Create(web_event, *GetDocument().domWindow());
  EXPECT_TRUE(event->momentum());

  web_event.momentum_phase = WebMouseWheelEvent::kPhaseChanged;
  event = WheelEvent::Create(web_event, *GetDocument().domWindow());
  EXPECT_TRUE(event->momentum());

  web_event.momentum_phase = WebMouseWheelEvent::kPhaseNone;
  event = WheelEvent::Create(web_event, *GetDocument().domWindow());
  EXPECT_FALSE(event->momentum());
}

}  // namespace blink
