// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_dialog_element.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HTMLDialogElementTest : public PageTestBase {};

namespace {

void EnterFullscreen(Document& document, Element& element) {
  LocalFrame::NotifyUserActivation(
      document.GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(element);
  Fullscreen::DidResolveEnterFullscreenRequest(document, /*granted=*/true);
}

}  // namespace

// The dialog event should not be closed in response to cancel events.
TEST_F(HTMLDialogElementTest, CancelEventDontClose) {
  auto* dialog = MakeGarbageCollected<HTMLDialogElement>(GetDocument());
  GetDocument().FirstBodyElement()->AppendChild(dialog);
  dialog->showModal(ASSERT_NO_EXCEPTION);
  dialog->DispatchScopedEvent(*Event::CreateBubble(event_type_names::kCancel));
  EXPECT_TRUE(dialog->FastHasAttribute(html_names::kOpenAttr));
}

TEST_F(HTMLDialogElementTest, ShowModalWithContentVisibilityAuto) {
  SetBodyInnerHTML(R"HTML(
    <dialog id="d" style="content-visibility:auto">
      <rp></rp>
    </dialog>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* dialog = To<HTMLDialogElement>(GetElementById("d"));
  dialog->showModal(ASSERT_NO_EXCEPTION);
}

TEST_F(HTMLDialogElementTest, ShowModalWithContentVisibilityAutoOnDescendant) {
  SetBodyInnerHTML(R"HTML(
    <dialog id="d">
      <label style="content-visibility:auto">
        <rp></rp>
      </label>
    </dialog>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* dialog = To<HTMLDialogElement>(GetElementById("d"));
  dialog->showModal(ASSERT_NO_EXCEPTION);
}

TEST_F(HTMLDialogElementTest,
       ShowModalWithNestedContentVisibilityAutoSiblingBranches) {
  SetBodyInnerHTML(R"HTML(
    <dialog id="d" style="content-visibility:auto">
      <input id="i" style="content-visibility:auto" value="">
      <label>text</label>
    </dialog>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* dialog = To<HTMLDialogElement>(GetElementById("d"));
  dialog->showModal(ASSERT_NO_EXCEPTION);
}

TEST_F(HTMLDialogElementTest,
       ShowModalAfterFullscreenAndContentVisibilityAuto) {
  ScopedDialogNewFocusBehaviorForTest scoped_dialog_new_focus_behavior(true);
  SetBodyInnerHTML(R"HTML(
    <dialog id="d" style="visibility:hidden">
      ~
      <iframe id="id_7"></iframe>
    </dialog>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* dialog = To<HTMLDialogElement>(GetElementById("d"));
  EnterFullscreen(GetDocument(), *dialog);
  UpdateAllLifecyclePhasesForTest();
  dialog->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                 CSSValueID::kAuto);

  dialog->showModal(ASSERT_NO_EXCEPTION);
}

}  // namespace blink
