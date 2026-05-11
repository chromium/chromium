// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/on_demand_spell_check_controller.h"

#include <gtest/gtest.h>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_test_base.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

// This is a paragraph of 50 sentences, each sentence has 50 characters,
// including the space after each full stop.
const char kLongText[] =
    "This is an amazing awesome beautiful sentence 01. This is an amazing "
    "awesome beautiful sentence 02. This is an amazing awesome beautiful "
    "sentence 03. This is an amazing awesome beautiful sentence 04. This is an "
    "amazing awesome beautiful sentence 05. This is an amazing awesome "
    "beautiful sentence 06. This is an amazing awesome beautiful sentence 07. "
    "This is an amazing awesome beautiful sentence 08. This is an amazing "
    "awesome beautiful sentence 09. This is an amazing awesome beautiful "
    "sentence 10. This is an amazing awesome beautiful sentence 11. This is an "
    "amazing awesome beautiful sentence 12. This is an amazing awesome "
    "beautiful sentence 13. This is an amazing awesome beautiful sentence 14. "
    "This is an amazing awesome beautiful sentence 15. This is an amazing "
    "awesome beautiful sentence 16. This is an amazing awesome beautiful "
    "sentence 17. This is an amazing awesome beautiful sentence 18. This is an "
    "amazing awesome beautiful sentence 19. This is an amazing awesome "
    "beautiful sentence 20. This is an amazing awesome beautiful sentence 21. "
    "This is an amazing awesome beautiful sentence 22. This is an amazing "
    "awesome beautiful sentence 23. This is an amazing awesome beautiful "
    "sentence 24. This is an amazing awesome beautiful sentence 25. This is an "
    "amazing awesome beautiful sentence 26. This is an amazing awesome "
    "beautiful sentence 27. This is an amazing awesome beautiful sentence 28. "
    "This is an amazing awesome beautiful sentence 29. This is an amazing "
    "awesome beautiful sentence 30. This is an amazing awesome beautiful "
    "sentence 31. This is an amazing awesome beautiful sentence 32. This is an "
    "amazing awesome beautiful sentence 33. This is an amazing awesome "
    "beautiful sentence 34. This is an amazing awesome beautiful sentence 35. "
    "This is an amazing awesome beautiful sentence 36. This is an amazing "
    "awesome beautiful sentence 37. This is an amazing awesome beautiful "
    "sentence 38. This is an amazing awesome beautiful sentence 39. This is an "
    "amazing awesome beautiful sentence 40. This is an amazing awesome "
    "beautiful sentence 41. This is an amazing awesome beautiful sentence 42. "
    "This is an amazing awesome beautiful sentence 43. This is an amazing "
    "awesome beautiful sentence 44. This is an amazing awesome beautiful "
    "sentence 45. This is an amazing awesome beautiful sentence 46. This is an "
    "amazing awesome beautiful sentence 47. This is an amazing awesome "
    "beautiful sentence 48. This is an amazing awesome beautiful sentence 49. "
    "This is an amazing awesome beautiful sentence 50.";

}  // namespace

class OnDemandSpellCheckControllerTestPeer {
 public:
  static void ClearProgress(OnDemandSpellCheckController& controller) {
    controller.ClearProgress();
  }
};

class OnDemandSpellCheckControllerTest : public SpellCheckTestBase {
 protected:
  OnDemandSpellCheckController& OnDemandController() {
    return GetSpellChecker().GetOnDemandSpellCheckController();
  }

  SpellCheckRequester& Requester() {
    return GetSpellChecker().GetSpellCheckRequester();
  }

  void TearDown() override {
    OnDemandSpellCheckControllerTestPeer::ClearProgress(OnDemandController());
    Requester().Deactivate();
    SpellCheckTestBase::TearDown();
  }

 protected:
  void SetSpellCheckChunkingEnabled(bool enabled) {
    spell_check_chunking_.emplace(enabled);
  }

 private:
  std::optional<ScopedSpellCheckChunkingForTest> spell_check_chunking_;
};

TEST_F(OnDemandSpellCheckControllerTest,
       RequestFullCheckingWhenSpellcheckFalse) {
  SetBodyContent("<div contenteditable='true' spellcheck='false'>foo</div>");
  Element* div = QuerySelector("div");

  OnDemandController().RequestFullChecking(div);
  test::RunPendingTasks();

  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  EXPECT_EQ(0, Requester().LastRequestSequence());
}

TEST_F(OnDemandSpellCheckControllerTest,
       RequestFullCheckingWithoutUserActivation) {
  SetSpellCheckChunkingEnabled(true);
  SetBodyContent("<div contenteditable='true'>foo</div>");
  Element* div = QuerySelector("div");

  // Focus the element programmatically (not from user gesture).
  div->Focus();
  ASSERT_FALSE(div->WasLastFocusFromUserGesture());

  OnDemandController().RequestFullChecking(div);
  test::RunPendingTasks();

  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  EXPECT_EQ(0, Requester().LastRequestSequence());
}

TEST_F(OnDemandSpellCheckControllerTest,
       RequestFullCheckingWithTransientUserActivation) {
  SetSpellCheckChunkingEnabled(true);
  SetBodyContent("<div contenteditable='true'>foo</div>");
  Element* div = QuerySelector("div");

  // Focus the element programmatically (not from user gesture).
  div->Focus();
  ASSERT_FALSE(div->WasLastFocusFromUserGesture());

  // Simulate transient user activation.
  GetFrame().NotifyUserActivation(
      mojom::blink::UserActivationNotificationType::kInteraction);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  OnDemandController().RequestFullChecking(div);
  test::RunPendingTasks();

  // It should now be active because of the transient activation.
  EXPECT_EQ(1, Requester().LastRequestSequence());
}

TEST_F(OnDemandSpellCheckControllerTest,
       RequestFullCheckingWithFocusedElementOutsideContainer) {
  SetSpellCheckChunkingEnabled(true);
  SetBodyContent(
      "<div id='container' contenteditable='true'>foo</div>"
      "<div id='other' contenteditable='true'>bar</div>");
  Element* container = QuerySelector("#container");
  Element* other = QuerySelector("#other");

  // Focus the 'other' element with a simulated user gesture.
  other->Focus(FocusParams(SelectionBehaviorOnFocus::kRestore,
                           mojom::blink::FocusType::kMouse, nullptr));
  ASSERT_TRUE(other->WasLastFocusFromUserGesture());
  ASSERT_FALSE(container->contains(other));

  // Request full checking on 'container'.
  OnDemandController().RequestFullChecking(container);
  test::RunPendingTasks();

  // It should NOT be active because the focused element is outside the
  // container.
  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  EXPECT_EQ(0, Requester().LastRequestSequence());
}

TEST_F(OnDemandSpellCheckControllerTest,
       RequestFullCheckingWhenGlobalSpellcheckDisabled) {
  class DisabledTextCheckerClient : public WebTextCheckClient {
   public:
    bool IsSpellCheckingEnabled() const override { return false; }
  };
  DisabledTextCheckerClient disabled_client;
  EmptyLocalFrameClient* frame_client =
      static_cast<EmptyLocalFrameClient*>(GetFrame().Client());
  frame_client->SetTextCheckerClientForTesting(&disabled_client);

  SetSpellCheckChunkingEnabled(true);
  SetBodyContent("<div contenteditable='true'>foo</div>");
  Element* div = QuerySelector("div");

  OnDemandController().RequestFullChecking(div);
  test::RunPendingTasks();

  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  EXPECT_EQ(0, Requester().LastRequestSequence());

  // Restore the original client
  frame_client->SetTextCheckerClientForTesting(nullptr);
}

TEST_F(OnDemandSpellCheckControllerTest, RequestFullCheckingWithoutChunking) {
  SetSpellCheckChunkingEnabled(false);
  SetBodyContent(
      base::StrCat({"<div contenteditable='true'>", kLongText, "</div>"}));
  Element* div = QuerySelector("div");

  ASSERT_EQ(0, Requester().LastRequestSequence());

  // Simulate transient user activation.
  GetFrame().NotifyUserActivation(
      mojom::blink::UserActivationNotificationType::kInteraction);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  OnDemandController().RequestFullChecking(div);
  test::RunPendingTasks();

  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  // When flag is off, it should not be chunked, and should send only one
  // request.
  EXPECT_EQ(1, Requester().LastRequestSequence());
  // The total checked length should match the length of kLongText.
  EXPECT_EQ(std::string(kLongText).length(),
            static_cast<size_t>(Requester().SpellCheckedTextLength()));
}

TEST_F(OnDemandSpellCheckControllerTest, RequestFullCheckingWithChunking) {
  SetSpellCheckChunkingEnabled(true);
  SetBodyContent(
      base::StrCat({"<div contenteditable='true'>", kLongText, "</div>"}));
  Element* div = QuerySelector("div");

  ASSERT_EQ(0, Requester().LastRequestSequence());

  // Simulate transient user activation.
  GetFrame().NotifyUserActivation(
      mojom::blink::UserActivationNotificationType::kInteraction);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  OnDemandController().RequestFullChecking(div);
  test::RunPendingTasks();

  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  // The long text is approximately 2499 characters long, and should be divided
  // into 3 chunks (1050, 1050, 399).
  EXPECT_EQ(3, Requester().LastRequestSequence());
  // The total checked length should match the length of kLongText.
  EXPECT_EQ(std::string(kLongText).length(),
            static_cast<size_t>(Requester().SpellCheckedTextLength()));
}

TEST_F(OnDemandSpellCheckControllerTest, SetSpellCheckingDisabled) {
  SetSpellCheckChunkingEnabled(true);
  SetBodyContent("<div contenteditable='true'>foo</div>");
  Element* div = QuerySelector("div");

  // Simulate transient user activation.
  GetFrame().NotifyUserActivation(
      mojom::blink::UserActivationNotificationType::kInteraction);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  OnDemandController().RequestFullChecking(div);
  OnDemandController().SetSpellCheckingDisabled(*div);

  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  EXPECT_EQ(0, Requester().LastRequestSequence());
}

TEST_F(OnDemandSpellCheckControllerTest, SetElementRemoved) {
  SetSpellCheckChunkingEnabled(true);
  SetBodyContent("<div contenteditable='true'>foo</div>");
  Element* div = QuerySelector("div");

  // Simulate transient user activation.
  GetFrame().NotifyUserActivation(
      mojom::blink::UserActivationNotificationType::kInteraction);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  OnDemandController().RequestFullChecking(div);
  OnDemandController().ElementRemoved(*div);

  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  EXPECT_EQ(0, Requester().LastRequestSequence());
}

TEST_F(OnDemandSpellCheckControllerTest, RequestFullCheckingWithChunkingShort) {
  SetSpellCheckChunkingEnabled(true);
  SetBodyContent("<div contenteditable='true'>short text</div>");
  Element* div = QuerySelector("div");

  ASSERT_EQ(0, Requester().LastRequestSequence());

  // Simulate transient user activation.
  GetFrame().NotifyUserActivation(
      mojom::blink::UserActivationNotificationType::kInteraction);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  OnDemandController().RequestFullChecking(div);
  test::RunPendingTasks();

  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  EXPECT_EQ(1, Requester().LastRequestSequence());
  EXPECT_EQ(10, static_cast<size_t>(Requester().SpellCheckedTextLength()));
}

TEST_F(OnDemandSpellCheckControllerTest,
       RequestFullCheckingWithChunkingMultipleChunks) {
  SetSpellCheckChunkingEnabled(true);
  SetBodyContent(
      base::StrCat({"<div contenteditable='true'>", kLongText, "</div>"}));
  Element* div = QuerySelector("div");

  ASSERT_EQ(0, Requester().LastRequestSequence());

  // Simulate transient user activation.
  GetFrame().NotifyUserActivation(
      mojom::blink::UserActivationNotificationType::kInteraction);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  OnDemandController().RequestFullChecking(div);
  test::RunPendingTasks();

  EXPECT_EQ(OnDemandSpellCheckController::State::kInactive,
            OnDemandController().GetState());
  // The long text is approximately 2499 characters long, and should be divided
  // into 3 chunks (1050, 1050, 399).
  EXPECT_EQ(3, Requester().LastRequestSequence());
  // The total checked length should match the length of kLongText.
  EXPECT_EQ(std::string(kLongText).length(),
            static_cast<size_t>(Requester().SpellCheckedTextLength()));
}

}  // namespace blink
