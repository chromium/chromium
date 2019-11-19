// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"

namespace blink {

class EventTargetTest : public RenderingTest {
 public:
  EventTargetTest() = default;
  ~EventTargetTest() override = default;
};

TEST_F(EventTargetTest, UseCountPassiveTouchEventListener) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetDocument().GetFrame()->GetScriptController().ExecuteScriptInMainWorld(
      "window.addEventListener('touchstart', function() {}, {passive: true});");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
}

TEST_F(EventTargetTest, UseCountNonPassiveTouchEventListener) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetDocument().GetFrame()->GetScriptController().ExecuteScriptInMainWorld(
      "window.addEventListener('touchstart', function() {}, {passive: "
      "false});");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
}

TEST_F(EventTargetTest, UseCountPassiveTouchEventListenerPassiveNotSpecified) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetDocument().GetFrame()->GetScriptController().ExecuteScriptInMainWorld(
      "window.addEventListener('touchstart', function() {});");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kPassiveTouchEventListener));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kNonPassiveTouchEventListener));
}

}  // namespace blink
