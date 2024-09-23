// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/validation_message_overlay_delegate.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/page/validation_message_client_impl.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/web/win/web_font_rendering.h"
#endif

namespace blink {

class ValidationMessageOverlayDelegateTest : public PaintTestConfigurations,
                                             public RenderingTest {
#if BUILDFLAG(IS_WIN)
 public:
  void SetUp() override {
    RenderingTest::SetUp();

    // These tests appear to trigger a requirement for system fonts. On windows,
    // an extra step is required to ensure that the system font is configured.
    // See https://crbug.com/969622
    blink::WebFontRendering::SetMenuFontMetrics(
        blink::WebString::FromASCII("Arial"), 12);
  }
#endif

 private:
  // When WebTestSupport::IsRunningWebTest is set, the animations in
  // ValidationMessageOverlayDelegate are disabled. We are specifically testing
  // animations, so make sure that doesn't happen.
  ScopedWebTestMode web_test_mode{false};
};

INSTANTIATE_PAINT_TEST_SUITE_P(ValidationMessageOverlayDelegateTest);

// Regression test for https://crbug.com/990680, where we accidentally
// composited the animations created by ValidationMessageOverlayDelegate. Since
// overlays operate in a Page that has no compositor, the animations broke.
TEST_P(ValidationMessageOverlayDelegateTest,
       OverlayAnimationsShouldNotBeComposited) {
  SetBodyInnerHTML("<div id='anchor'></div>");
  Element* anchor = GetElementById("anchor");
  ASSERT_TRUE(anchor);

  auto delegate = std::make_unique<ValidationMessageOverlayDelegate>(
      GetPage(), *anchor, "Test message", TextDirection::kLtr, "Sub-message",
      TextDirection::kLtr);
  ValidationMessageOverlayDelegate* delegate_ptr = delegate.get();

  auto* overlay =
      MakeGarbageCollected<FrameOverlay>(&GetFrame(), std::move(delegate));
  delegate_ptr->CreatePage(*overlay);
  ASSERT_TRUE(GetFrame().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest));

  // Trigger the overlay animations.
  delegate_ptr->UpdateFrameViewState(*overlay);

  // Now find the related animations, and make sure they weren't composited.
  Document* internal_document =
      To<LocalFrame>(delegate_ptr->GetPageForTesting()->MainFrame())
          ->GetDocument();
  HeapVector<Member<Animation>> animations =
      internal_document->GetDocumentAnimations().getAnimations(
          *internal_document);
  ASSERT_FALSE(animations.empty());

  for (const auto& animation : animations) {
    EXPECT_FALSE(animation->HasActiveAnimationsOnCompositor());
  }

  overlay->Destroy();
}

// Regression test for https://crbug.com/990680, where we found we were not
// properly advancing the AnimationClock in the internal Page created by
// ValidationMessageOverlayDelegate. When combined with the fix for
// https://crbug.com/785940, this caused Animations to never be updated.
TEST_P(ValidationMessageOverlayDelegateTest,
       DelegatesInternalPageShouldHaveAnimationTimesUpdated) {
  // We use a ValidationMessageClientImpl here to create our delegate since we
  // need the official path from Page::Animate to work.
  auto* client = MakeGarbageCollected<ValidationMessageClientImpl>(GetPage());
  ValidationMessageClient* original_client =
      &GetPage().GetValidationMessageClient();
  GetPage().SetValidationMessageClientForTesting(client);

  SetBodyInnerHTML(R"HTML(
    <style>#anchor { width: 100px; height: 100px; }</style>
    <div id='anchor'></div>
  )HTML");
  Element* anchor = GetElementById("anchor");
  ASSERT_TRUE(anchor);

  client->ShowValidationMessage(*anchor, "Test message", TextDirection::kLtr,
                                "Sub-message", TextDirection::kLtr);
  ValidationMessageOverlayDelegate* delegate = client->GetDelegateForTesting();
  ASSERT_TRUE(delegate);

  // Initially the AnimationClock will be at 0.
  // TODO(crbug.com/785940): Re-enable this EXPECT_EQ once the AnimationClock no
  // longer jumps ahead on its own accord.
  AnimationClock& internal_clock =
      delegate->GetPageForTesting()->Animator().Clock();
  // EXPECT_EQ(internal_clock.CurrentTime(), 0);

  // Now update the main Page's clock. This should trickle down and update the
  // inner Page's clock too.
  AnimationClock& external_clock = GetPage().Animator().Clock();
  base::TimeTicks current_time = external_clock.CurrentTime();

  base::TimeTicks new_time = current_time + base::Seconds(1);
  GetPage().Animate(new_time);

  // TODO(crbug.com/785940): Until this bug is fixed, this comparison could pass
  // even if the underlying behavior regresses (because calling CurrentTime
  // could advance the clocks anyway).
  EXPECT_EQ(external_clock.CurrentTime(), internal_clock.CurrentTime());

  GetPage().SetValidationMessageClientForTesting(original_client);

  static_cast<ValidationMessageClient*>(client)->WillBeDestroyed();
}

TEST_P(ValidationMessageOverlayDelegateTest, Repaint) {
  auto* client = MakeGarbageCollected<ValidationMessageClientImpl>(GetPage());
  ValidationMessageClient* original_client =
      &GetPage().GetValidationMessageClient();
  GetPage().SetValidationMessageClientForTesting(client);

  SetBodyInnerHTML(R"HTML(
    <style>#anchor { width: 100px; height: 100px; }</style>
    <div id='anchor'></div>
  )HTML");

  EXPECT_FALSE(
      GetDocument().View()->VisualViewportOrOverlayNeedsRepaintForTesting());

  Element* anchor = GetElementById("anchor");
  ASSERT_TRUE(anchor);

  client->ShowValidationMessage(*anchor, "Test message", TextDirection::kLtr,
                                "Sub-message", TextDirection::kLtr);
  ValidationMessageOverlayDelegate* delegate = client->GetDelegateForTesting();
  ASSERT_TRUE(delegate);

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(
      GetDocument().View()->VisualViewportOrOverlayNeedsRepaintForTesting());
  UpdateAllLifecyclePhasesForTest();

  // The flag should be set again for animation update.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(
      GetDocument().View()->VisualViewportOrOverlayNeedsRepaintForTesting());
  UpdateAllLifecyclePhasesForTest();

  GetPage().SetValidationMessageClientForTesting(original_client);
  static_cast<ValidationMessageClient*>(client)->WillBeDestroyed();
}

}  // namespace blink
