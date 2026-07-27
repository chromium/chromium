// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_form_control_element.h"

#include <vector>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

using ::testing::ElementsAre;
using ::testing::Values;

// A fake event listener that logs keys and codes of observed keyboard events.
class FakeEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    event_received_ = true;
    KeyboardEvent* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (!keyboard_event) {
      return;
    }
    codes_.push_back(keyboard_event->code());
    keys_.push_back(keyboard_event->key());
  }

  bool event_received() const { return event_received_; }
  const std::vector<String>& codes() const { return codes_; }
  const std::vector<String>& keys() const { return keys_; }

 private:
  bool event_received_ = false;
  std::vector<String> codes_;
  std::vector<String> keys_;
};

}  // namespace

class WebFormControlElementTest : public PageTestBase {};

// Tests that resetting a form clears the `user_has_edited_the_field_` state.
TEST_F(WebFormControlElementTest, ResetDocumentClearsEditedState) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <body>
      <form id="f">
        <input id="text_id">
        <select id="select_id">
          <option value="Bar">Bar</option>
          <option value="Foo">Foo</option>
        </select>
        <input id="reset" type="reset">
      </form>
    </body>
  )");

  WebFormControlElement text(
      DynamicTo<HTMLFormControlElement>(GetElementById("text_id")));
  WebFormControlElement select(
      DynamicTo<HTMLFormControlElement>(GetElementById("select_id")));

  text.SetUserHasEditedTheField(true);
  select.SetUserHasEditedTheField(true);

  EXPECT_TRUE(text.UserHasEditedTheField());
  EXPECT_TRUE(select.UserHasEditedTheField());

  To<HTMLFormControlElement>(GetElementById("reset"))->click();

  EXPECT_FALSE(text.UserHasEditedTheField());
  EXPECT_FALSE(select.UserHasEditedTheField());
}

TEST_F(WebFormControlElementTest, TextControlPreviewDisabledInCanvas) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <form>
      <canvas>
        <input id="input_id">
        <textarea id="textarea_id"></textarea>
      </canvas>
    </form>
  )");

  WebFormControlElement input(
      DynamicTo<HTMLFormControlElement>(GetElementById("input_id")));
  WebFormControlElement textarea(
      DynamicTo<HTMLFormControlElement>(GetElementById("textarea_id")));

  input.SetSuggestedValue("suggestion");
  textarea.SetSuggestedValue("suggestion");

  // Elements inside canvas should not show autofill suggestions, as this can
  // leak the information to javascript.
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
  EXPECT_TRUE(textarea.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledWhenMovingToCanvas) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <form>
      <input id="input_id">
      <textarea id="textarea_id"></textarea>
      <canvas id="canvas"></canvas>
    </form>
  )");

  WebFormControlElement input(
      DynamicTo<HTMLFormControlElement>(GetElementById("input_id")));
  WebFormControlElement textarea(
      DynamicTo<HTMLFormControlElement>(GetElementById("textarea_id")));

  input.SetSuggestedValue("suggestion");
  textarea.SetSuggestedValue("suggestion");

  // Suggestions should work outside canvas.
  EXPECT_EQ(input.SuggestedValue().Ascii(), "suggestion");
  EXPECT_EQ(textarea.SuggestedValue().Ascii(), "suggestion");

  // Moving the element into a canvas subtree should disable autofill
  // suggestions, as these can leak the information to javascript.
  GetElementById("canvas")->appendChild(GetElementById("input_id"));
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
  GetElementById("canvas")->appendChild(GetElementById("textarea_id"));
  EXPECT_TRUE(textarea.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest, SelectPreviewDisabledInCanvas) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <form>
      <canvas>
        <select id="select_id">
          <option value="Bar">Bar</option>
          <option value="Foo">Foo</option>
        </select>
      </canvas>
    </form>
  )");

  WebFormControlElement select(
      DynamicTo<HTMLFormControlElement>(GetElementById("select_id")));

  select.SetSuggestedValue("Foo");

  // Elements inside canvas should not show autofill suggestions, as this can
  // leak the information to javascript.
  EXPECT_TRUE(select.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       SelectPreviewDisabledInCanvasWhenMovingToCanvas) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <form>
        <select id="select_id">
          <option value="Bar">Bar</option>
          <option value="Foo">Foo</option>
        </select>
      <canvas id="canvas"></canvas>
    </form>
  )");

  WebFormControlElement select(
      DynamicTo<HTMLFormControlElement>(GetElementById("select_id")));

  select.SetSuggestedValue("Foo");

  // Suggestions should work outside canvas.
  EXPECT_EQ(select.SuggestedValue().Ascii(), "Foo");

  // Elements inside canvas should not show autofill suggestions, as this can
  // leak the information to javascript.
  GetElementById("canvas")->appendChild(GetElementById("select_id"));
  EXPECT_TRUE(select.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest, TextControlSlottedPreviewDisabledInCanvas) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div>
      <template shadowrootmode="open">
        <canvas layoutsubtree>
          <slot name="slot1"></slot>
        </canvas>
      </template>
      <form id="slotted" slot="slot1">
        <input id="input_id">
        <textarea id="textarea_id"></textarea>
      </form>
    </div>
  )");

  WebFormControlElement input(
      DynamicTo<HTMLFormControlElement>(GetElementById("input_id")));
  WebFormControlElement textarea(
      DynamicTo<HTMLFormControlElement>(GetElementById("textarea_id")));

  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(GetElementById("input_id")->IsInCanvasSubtree());
  EXPECT_TRUE(GetElementById("textarea_id")->IsInCanvasSubtree());

  input.SetSuggestedValue("suggestion");
  textarea.SetSuggestedValue("suggestion");

  // Elements inside canvas should not show autofill suggestions, as this can
  // leak the information to javascript.
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
  EXPECT_TRUE(textarea.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest, TextControlPreviewDisabledWhenMovingToSlot) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id=slotHost>
      <template shadowrootmode="open">
        <canvas layoutsubtree>
          <slot name="slot1"></slot>
        </canvas>
      </template>
    </div>
    <form id="slotted" slot="slot1">
      <input id="input_id">
      <textarea id="textarea_id"></textarea>
    </form>
  )");

  Element* input_elmt = GetElementById("input_id");
  Element* textarea_elmt = GetElementById("textarea_id");

  WebFormControlElement input(DynamicTo<HTMLFormControlElement>(input_elmt));
  WebFormControlElement textarea(
      DynamicTo<HTMLFormControlElement>(textarea_elmt));

  EXPECT_FALSE(input_elmt->IsInCanvasSubtree());
  EXPECT_FALSE(textarea_elmt->IsInCanvasSubtree());

  input.SetSuggestedValue("suggestion");
  textarea.SetSuggestedValue("suggestion");

  // Suggestions should work outside canvas.
  EXPECT_EQ(input.SuggestedValue().Ascii(), "suggestion");
  EXPECT_EQ(textarea.SuggestedValue().Ascii(), "suggestion");

  Element* host = GetElementById("slotHost");
  Element* form = GetElementById("slotted");

  host->moveBefore(form, nullptr, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(input_elmt->IsInCanvasSubtree());
  EXPECT_TRUE(textarea_elmt->IsInCanvasSubtree());

  // Moving the element into a canvas subtree should disable autofill
  // suggestions, as these can leak the information to javascript.
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
  EXPECT_TRUE(textarea.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledInCanvasWhenSlotted) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id="host">
      <template shadowrootmode="open">
        <canvas layoutsubtree>
          <div id="slotwrapper">
            <slot></slot>
          </div>
        </canvas>
      </template>
      <input id="input_id">
      <textarea id="textarea_id"></textarea>
      <select id="select_id">
        <option value="Bar">Bar</option>
        <option value="Foo">Foo</option>
      </select>
    </div>
  )");
  UpdateAllLifecyclePhasesForTest();

  WebFormControlElement input(
      DynamicTo<HTMLFormControlElement>(GetElementById("input_id")));
  WebFormControlElement textarea(
      DynamicTo<HTMLFormControlElement>(GetElementById("textarea_id")));
  WebFormControlElement select(
      DynamicTo<HTMLFormControlElement>(GetElementById("select_id")));

  EXPECT_TRUE(input.Unwrap<HTMLInputElement>()->IsInCanvasSubtree());
  input.SetSuggestedValue("suggestion");
  textarea.SetSuggestedValue("suggestion");
  select.SetSuggestedValue("Foo");

  // Elements slotted inside a canvas should not show autofill suggestions.
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
  EXPECT_TRUE(textarea.SuggestedValue().IsEmpty());
  EXPECT_TRUE(select.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledInCanvasWhenNestedAndSlotted) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id="host">
      <template shadowrootmode="open">
        <div id="normal_div">
          <slot name="s1"></slot>
        </div>
        <canvas id="canvas" layoutsubtree>
          <slot name="s2"></slot>
        </canvas>
      </template>
      <div id="wrapper" slot="s1">
        <input id="input_id">
        <textarea id="textarea_id"></textarea>
      </div>
    </div>
  )");

  WebFormControlElement input(
      DynamicTo<HTMLFormControlElement>(GetElementById("input_id")));
  WebFormControlElement textarea(
      DynamicTo<HTMLFormControlElement>(GetElementById("textarea_id")));

  input.SetSuggestedValue("suggestion");
  textarea.SetSuggestedValue("suggestion");
  EXPECT_EQ(input.SuggestedValue().Ascii(), "suggestion");
  EXPECT_EQ(textarea.SuggestedValue().Ascii(), "suggestion");

  // Now dynamically change the slot to re-slot the wrapper into the canvas.
  GetElementById("wrapper")->setAttribute(html_names::kSlotAttr,
                                          AtomicString("s2"));

  // Force slot assignment recalc and style update.
  GetDocument().UpdateStyleAndLayoutTree();

  // Nested elements slotted inside a canvas should have their suggestions
  // cleared.
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
  EXPECT_TRUE(textarea.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledWhenSlottedInsideIframeUnderCanvas) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  Document* top_doc =
      web_view_helper.LocalMainFrame()->GetFrame()->GetDocument();
  ASSERT_TRUE(top_doc);

  top_doc->body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id="host">
      <template shadowrootmode="open">
        <canvas layoutsubtree>
          <div id="slotwrapper">
            <slot></slot>
          </div>
        </canvas>
      </template>
      <iframe id="iframe_id"></iframe>
    </div>
  )");

  web_view_helper.LocalMainFrame()->FrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  auto* iframe = DynamicTo<HTMLFrameOwnerElement>(
      top_doc->getElementById(AtomicString("iframe_id")));
  ASSERT_TRUE(iframe);
  Document* inner_doc = iframe->contentDocument();
  ASSERT_TRUE(inner_doc);

  inner_doc->body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <input id="inner_input_id">
  )");

  WebFormControlElement inner_input(DynamicTo<HTMLFormControlElement>(
      inner_doc->getElementById(AtomicString("inner_input_id"))));

  inner_input.SetSuggestedValue("suggestion");

  // Elements inside an iframe slotted inside a canvas should have suggestions
  // suppressed.
  EXPECT_TRUE(inner_input.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledWhenIframeDynamicallySlottedIntoCanvas) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  Document* top_doc =
      web_view_helper.LocalMainFrame()->GetFrame()->GetDocument();
  ASSERT_TRUE(top_doc);

  // Initial HTML: The iframe is NOT in a canvas.
  top_doc->body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id="host">
      <template shadowrootmode="open">
        <div id="normal_div">
          <slot name="s1"></slot>
        </div>
        <canvas id="canvas" layoutsubtree>
          <div id="slotwrapper">
            <slot name="s2"></slot>
          </div>
        </canvas>
      </template>
      <iframe id="iframe_id" slot="s1"></iframe>
    </div>
  )");

  web_view_helper.LocalMainFrame()->FrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  auto* iframe = DynamicTo<HTMLFrameOwnerElement>(
      top_doc->getElementById(AtomicString("iframe_id")));
  ASSERT_TRUE(iframe);
  Document* inner_doc = iframe->contentDocument();
  ASSERT_TRUE(inner_doc);

  inner_doc->body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <input id="inner_input_id">
  )");

  WebFormControlElement inner_input(DynamicTo<HTMLFormControlElement>(
      inner_doc->getElementById(AtomicString("inner_input_id"))));

  inner_input.SetSuggestedValue("suggestion");

  // Suggested value should be visible since it's not under a canvas subtree
  // yet.
  EXPECT_EQ(inner_input.SuggestedValue().Ascii(), "suggestion");

  // Dynamically move the iframe into the canvas subtree via the slot attribute.
  iframe->setAttribute(html_names::kSlotAttr, AtomicString("s2"));

  // Force style and layout update.
  web_view_helper.LocalMainFrame()->FrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // The suggested value in the inner document should be cleared.
  EXPECT_TRUE(inner_input.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledWhenFlatTreeTraversalForbiddenInIframe) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  Document* top_doc =
      web_view_helper.LocalMainFrame()->GetFrame()->GetDocument();
  ASSERT_TRUE(top_doc);

  // Initial HTML: The iframe is NOT in a canvas subtree initially.
  top_doc->body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id="host">
      <template shadowrootmode="open">
        <div id="normal_div">
          <slot name="s1"></slot>
        </div>
        <canvas id="canvas" layoutsubtree>
          <div id="slotwrapper">
            <slot name="s2"></slot>
          </div>
        </canvas>
      </template>
      <iframe id="iframe_id" slot="s1"></iframe>
    </div>
  )");

  web_view_helper.LocalMainFrame()->FrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  auto* iframe = DynamicTo<HTMLFrameOwnerElement>(
      top_doc->getElementById(AtomicString("iframe_id")));
  ASSERT_TRUE(iframe);
  Document* inner_doc = iframe->contentDocument();
  ASSERT_TRUE(inner_doc);

  inner_doc->body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <input id="inner_input_id">
  )");

  WebFormControlElement inner_input(DynamicTo<HTMLFormControlElement>(
      inner_doc->getElementById(AtomicString("inner_input_id"))));

  // Move the iframe to the canvas slot dynamically.
  iframe->setAttribute(html_names::kSlotAttr, AtomicString("s2"));

  // Force style and layout update.
  web_view_helper.LocalMainFrame()->FrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Forbid flat tree traversal in the inner document.
  inner_doc->FlatTreeTraversalForbiddenRecursionDepth()++;

  // The dynamic check IsInCanvasSubtree() should correctly return true,
  // crossing the local owner boundary to the iframe even when traversal is
  // forbidden.
  EXPECT_TRUE(inner_input.Unwrap<HTMLInputElement>()->IsInCanvasSubtree());

  // Clean up forbidden scope.
  inner_doc->FlatTreeTraversalForbiddenRecursionDepth()--;
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledWhenAppendedToAlreadySlottedParent) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id="host">
      <template shadowrootmode="open">
        <canvas layoutsubtree>
          <div id="slotwrapper">
            <slot></slot>
          </div>
        </canvas>
      </template>
      <div id="slotted_parent"></div>
    </div>
  )");

  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();

  auto* slotted_parent = GetElementById("slotted_parent");
  ASSERT_TRUE(slotted_parent);

  // Create input element and set suggested value while disconnected.
  auto* input_el = GetDocument().CreateRawElement(html_names::kInputTag);
  input_el->setAttribute(html_names::kIdAttr, AtomicString("new_input_id"));
  WebFormControlElement input(DynamicTo<HTMLFormControlElement>(input_el));
  input.SetSuggestedValue("suggestion");
  EXPECT_EQ(input.SuggestedValue().Ascii(), "suggestion");

  // Now dynamically append the input to the already slotted parent.
  slotted_parent->appendChild(input_el);

  // Suggestions should be immediately cleared.
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledWhenChainedSlotDynamicallySlottedIntoCanvas) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id="host1">
      <template shadowrootmode="open">
        <div id="normal_div">
          <slot name="s1"></slot>
        </div>
        <canvas id="canvas" layoutsubtree>
          <slot name="s2"></slot>
        </canvas>
      </template>
      <div id="host2" slot="s1">
        <template shadowrootmode="open">
          <slot name="s3"></slot>
        </template>
        <input id="input_id" slot="s3">
      </div>
    </div>
  )");

  WebFormControlElement input(
      DynamicTo<HTMLFormControlElement>(GetElementById("input_id")));
  input.SetSuggestedValue("suggestion");

  // The suggested value should be accepted outside canvas.
  EXPECT_EQ(input.SuggestedValue().Ascii(), "suggestion");

  // Now dynamically change the slot to re-slot host2 into the canvas.
  GetElementById("host2")->setAttribute(html_names::kSlotAttr,
                                        AtomicString("s2"));

  // Force slot assignment recalc and style update.
  GetDocument().UpdateStyleAndLayoutTree();

  // The input element's suggested value should be cleared.
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledWhenMovingHostOutOfCanvasSubtree) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id="outer_host">
      <template shadowrootmode="open">
        <div id="outer_normal_div">
          <slot name="s1"></slot>
        </div>
        <canvas id="outer_canvas" layoutsubtree>
          <slot name="s2"></slot>
        </canvas>
      </template>
      <div id="inner_host" slot="s2">
        <template shadowrootmode="open">
          <canvas id="inner_canvas" layoutsubtree>
            <slot name="s3"></slot>
          </canvas>
        </template>
        <div id="slotted_div" slot="s3">
          <input id="leaf_input">
        </div>
      </div>
    </div>
  )");

  WebFormControlElement input(
      DynamicTo<HTMLFormControlElement>(GetElementById("leaf_input")));
  GetDocument().UpdateStyleAndLayoutTree();
  input.SetSuggestedValue("suggestion");

  // The input is inside both an outer and inner canvas, so it should NOT show
  // suggested values.
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());

  // Now dynamically move the inner host out of the outer canvas.
  GetElementById("inner_host")
      ->setAttribute(html_names::kSlotAttr, AtomicString("s1"));

  UpdateAllLifecyclePhasesForTest();

  // Since the inner host is moved out of the outer canvas, but the inner input
  // is still slotted inside the inner canvas, the suggested value on the input
  // element should still be cleared (suppressed).
  input.SetSuggestedValue("suggestion");
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
}

TEST_F(
    WebFormControlElementTest,
    TextControlPreviewDisabledWhenSlottedIntoCanvasAfterStyleEnsuredOutsideFlatTree) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  ScopedGetComputedStyleOutsideFlatTreeForTest scoped_feature(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div>
      <template shadowrootmode="open">
        <canvas layoutsubtree>
          <div id="slotHost">
            <slot name="slot1"></slot>
          </div>
        </canvas>
      </template>
      <input id="input" slot="unslotted">
    </div>
  )");

  HTMLInputElement* input =
      DynamicTo<HTMLInputElement>(GetElementById("input"));
  ASSERT_TRUE(input);

  GetDocument().UpdateStyleAndLayoutTree();

  // Ensure computed style outside the flat tree before assigning to a slot.
  input->EnsureComputedStyle();

  WebFormControlElement form_control(input);
  form_control.SetSuggestedValue("suggestion");
  EXPECT_EQ(input->SuggestedValue().Ascii(), "suggestion");

  // Now slot 'input' into the canvas slot inside the shadow root.
  input->setAttribute(html_names::kSlotAttr, AtomicString("slot1"));
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_TRUE(input->IsInCanvasSubtree());
  EXPECT_TRUE(input->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(form_control.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlCanvasSubtreeStaleWhenUnslottedFromCanvas) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div>
      <template shadowrootmode="open">
        <canvas layoutsubtree>
          <div id="slotHost">
            <slot name="slot1"></slot>
          </div>
        </canvas>
      </template>
      <input id="input" slot="slot1">
    </div>
  )");

  HTMLInputElement* input =
      DynamicTo<HTMLInputElement>(GetElementById("input"));
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_TRUE(input->IsInCanvasSubtree());
  EXPECT_TRUE(input->IsCanvasOrInCanvasSubtree());

  // Unslot the input element so it is removed from the flat tree.
  input->setAttribute(html_names::kSlotAttr, AtomicString("nonexistent"));
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_FALSE(input->IsInCanvasSubtree());
  // When removed from the flat tree (RemovedFromFlatTree), its canvas subtree
  // state should be updated so IsCanvasOrInCanvasSubtree() is false.
  EXPECT_FALSE(input->IsCanvasOrInCanvasSubtree());
}

class WebFormControlElementSetAutofillValueTest
    : public WebFormControlElementTest,
      public testing::WithParamInterface<const char*> {
 protected:
  void InsertHTML() {
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        GetParam());
  }

  WebFormControlElement TestElement() {
    HTMLFormControlElement* control_element = DynamicTo<HTMLFormControlElement>(
        GetDocument().getElementById(AtomicString("testElement")));
    DCHECK(control_element);
    return WebFormControlElement(control_element);
  }
};

TEST_P(WebFormControlElementSetAutofillValueTest, SetAutofillValue) {
  InsertHTML();
  WebFormControlElement element = TestElement();
  auto* keypress_handler = MakeGarbageCollected<FakeEventListener>();
  element.Unwrap<HTMLFormControlElement>()->addEventListener(
      event_type_names::kKeydown, keypress_handler);

  EXPECT_EQ(TestElement().Value(), "test value");
  EXPECT_EQ(element.GetAutofillState(), WebAutofillState::kNotFilled);

  // We expect to see one generic keydown event (not a KeyboardEvent).
  element.SetAutofillValue("new value", WebAutofillState::kAutofilled);
  EXPECT_EQ(element.Value(), "new value");
  EXPECT_EQ(element.GetAutofillState(), WebAutofillState::kAutofilled);
  EXPECT_TRUE(keypress_handler->event_received());
  EXPECT_TRUE(keypress_handler->codes().empty());
  EXPECT_TRUE(keypress_handler->keys().empty());
}

INSTANTIATE_TEST_SUITE_P(
    WebFormControlElementTest,
    WebFormControlElementSetAutofillValueTest,
    Values("<input type='text' id=testElement value='test value'>",
           "<textarea id=testElement>test value</textarea>"));

TEST_F(WebFormControlElementTest,
       SetAutofillAndSuggestedValueMaxLengthForInput) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<input type='text' id=testElement maxlength='5'>");

  auto element = WebFormControlElement(To<HTMLFormControlElement>(
      GetDocument().getElementById(AtomicString("testElement"))));

  element.SetSuggestedValue("valueTooLong");
  EXPECT_EQ(element.SuggestedValue().Ascii(), "value");

  element.SetAutofillValue("valueTooLong");
  EXPECT_EQ(element.Value().Ascii(), "value");
}

TEST_F(WebFormControlElementTest,
       SetAutofillAndSuggestedValueMaxLengthForTextarea) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<textarea id=testElement maxlength='5'></textarea>");

  auto element = WebFormControlElement(To<HTMLFormControlElement>(
      GetDocument().getElementById(AtomicString("testElement"))));

  element.SetSuggestedValue("valueTooLong");
  EXPECT_EQ(element.SuggestedValue().Ascii(), "value");

  element.SetAutofillValue("valueTooLong");
  EXPECT_EQ(element.Value().Ascii(), "value");
}

class WebFormControlElementGetOwningFormForAutofillTest
    : public WebFormControlElementTest {
 protected:
  template <typename DocumentOrShadowRoot>
  WebFormElement GetFormElementById(
      const DocumentOrShadowRoot& document_or_shadow_root,
      std::string_view id) {
    return WebFormElement(DynamicTo<HTMLFormElement>(
        document_or_shadow_root.getElementById(WebString::FromAscii(id))));
  }  // namespace blink

  template <typename DocumentOrShadowRoot>
  WebFormControlElement GetFormControlElementById(
      const DocumentOrShadowRoot& document_or_shadow_root,
      std::string_view id) {
    return WebFormControlElement(DynamicTo<HTMLFormControlElement>(
        document_or_shadow_root.getElementById(WebString::FromAscii(id))));
  }
};

// Tests that the owning form of a form control element in light DOM is its
// associated form (i.e. the form explicitly set via form attribute or its
// closest ancestor).
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInLightDom) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f>
      <input id=t1>
      <input id=t2>
    </form>
    <input id=t3>)");
  WebFormElement f = GetFormElementById(document, "f");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(document, "t1");
  WebFormControlElement t2 = GetFormControlElementById(document, "t2");
  WebFormControlElement t3 = GetFormControlElementById(document, "t3");
  EXPECT_EQ(t1.GetOwningFormForAutofill(), f);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f_unowned);
}

// Tests that explicit association overrules DOM ancestry when determining the
// owning form.
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInLightDomWithExplicitAssociation) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div>
      <form id=f1>
        <input id=t1>
        <input id=t2 form=f2>
      </form>
    </div>
    <form id=f2>
      <input id=t3>
      <input id=t4 form=f1>
      <input id=t5 form=f_unowned>
    </form>
    <input id=t6 form=f1>
    <input id=t7 form=f2>
    <input id=t8>)");
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f2 = GetFormElementById(document, "f2");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(document, "t1");
  WebFormControlElement t2 = GetFormControlElementById(document, "t2");
  WebFormControlElement t3 = GetFormControlElementById(document, "t3");
  WebFormControlElement t4 = GetFormControlElementById(document, "t4");
  WebFormControlElement t5 = GetFormControlElementById(document, "t5");
  WebFormControlElement t6 = GetFormControlElementById(document, "t6");
  WebFormControlElement t7 = GetFormControlElementById(document, "t7");
  WebFormControlElement t8 = GetFormControlElementById(document, "t8");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f2);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f2);
  EXPECT_EQ(t4.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t5.GetOwningFormForAutofill(), f_unowned);
  EXPECT_EQ(t6.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t7.GetOwningFormForAutofill(), f2);
  EXPECT_EQ(t8.GetOwningFormForAutofill(), f_unowned);
}

// Tests that input elements in shadow DOM whose closest ancestor is in the
// light DOM are extracted correctly.
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInShadowDomWithoutFormInShadowDom) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f1>
      <div id=host1>
        <template shadowrootmode="open">
          <div>
            <input id=t1>
          </div>
        </template>
        <input id=t2>
      </div>
    </form>
    <div id=host2>
      <template shadowrootmode="open">
        <input id=t3>
      </template>
    </div>)");
  const ShadowRoot& shadow_root1 = *GetElementById("host1")->GetShadowRoot();
  const ShadowRoot& shadow_root2 = *GetElementById("host2")->GetShadowRoot();
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(document, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root2, "t3");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f_unowned);
}

// Tests that the owning form of a form control element is the furthest
// shadow-including ancestor form element (in absence of explicit associations).
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInShadowDomWithFormInShadowDom) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f1>
      <div id=host1>
        <template shadowrootmode=open>
          <div>
            <form id=f2>
              <input id=t1>
            </form>
          </div>
          <input id=t2>
        </template>
      </div>
    </form>
    <div id=host2>
      <template shadowrootmode=open>
        <form id=f3>
          <input id=t3>
        </form>
      </template>
    </div>)");
  const ShadowRoot& shadow_root1 = *GetElementById("host1")->GetShadowRoot();
  const ShadowRoot& shadow_root2 = *GetElementById("host2")->GetShadowRoot();
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f3 = GetFormElementById(shadow_root2, "f3");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(shadow_root1, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root2, "t3");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f3);
}

// Tests that the owning form is returned correctly even if there are
// multiple levels of Shadow DOM.
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInShadowDomWithFormInShadowDomWithMultipleLevels) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f1>
      <div id=host1>
        <template shadowrootmode=open>
          <form id=f2>
            <input id=t1>
          </form>
          <div id=host2>
            <template shadowrootmode=open>
              <form id=f3>
                <input id=t2>
              </form>
              <input id=t3>
            </template>
            <input id=t4>
          </div>
          <input id=t5>
        </template>
      </div>
    </form>)");

  const ShadowRoot& shadow_root1 = *GetElementById("host1")->GetShadowRoot();
  const ShadowRoot& shadow_root2 =
      *shadow_root1.getElementById(AtomicString("host2"))->GetShadowRoot();
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(shadow_root2, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root2, "t3");
  WebFormControlElement t4 = GetFormControlElementById(shadow_root1, "t4");
  WebFormControlElement t5 = GetFormControlElementById(shadow_root1, "t5");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t4.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t5.GetOwningFormForAutofill(), f1);
}

// Tests that the owning form is computed correctly for form control elements
// inside the shadow DOM that have explicit form attributes.
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInShadowDomWithFormInShadowDomAndExplicitAssociation) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f1>
      <div id=host1>
        <template shadowrootmode=open>
          <form id=f2>
            <input id=t1>
          </form>
          <input id=t2>
          <form id=f3>
            <input id=t3 form=f2>
          </form>
          <input id=t4 form=f2>
          <input id=t5 form=f3>
          <input id=t6 form=f1>
        </template>
      </div>
    </form>
    <div id=host2>
      <template shadowrootmode=open>
        <form id=f4>
          <input id=t7>
        </form>
      </template>
    </div>)");
  const ShadowRoot& shadow_root1 = *GetElementById("host1")->GetShadowRoot();
  const ShadowRoot& shadow_root2 = *GetElementById("host2")->GetShadowRoot();
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f4 = GetFormElementById(shadow_root2, "f4");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(shadow_root1, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root1, "t3");
  WebFormControlElement t4 = GetFormControlElementById(shadow_root1, "t4");
  WebFormControlElement t5 = GetFormControlElementById(shadow_root1, "t5");
  WebFormControlElement t6 = GetFormControlElementById(shadow_root1, "t6");
  WebFormControlElement t7 = GetFormControlElementById(shadow_root2, "t7");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t4.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t5.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t6.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t7.GetOwningFormForAutofill(), f4);
}

TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInLightDomWithSlots) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f>
      <div>
        <template shadowrootmode=open>
          <form id=f_unowned>
            <slot></slot>
          </form>
        </template>
        <input id=t1>
      </div>
    </form>)");
  WebFormElement f = GetFormElementById(document, "f");
  WebFormControlElement t1 = GetFormControlElementById(document, "t1");
  EXPECT_EQ(t1.GetOwningFormForAutofill(), f);
}

// Tests that FormControlTypeForAutofill() == kInputPassword is sticky unless
// the type changes to a non-text type.
//
// That is, once an <input> has become an <input type=password>,
// FormControlTypeForAutofill() keeps returning kInputPassword, even if it
// the FormControlType() changes, provided it's a text-type.
TEST_F(WebFormControlElementTest, FormControlTypeForAutofill) {
  using enum FormControlType;
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes("<input id=t>");
  HTMLInputElement* input = To<HTMLInputElement>(GetElementById("t"));
  WebFormControlElement control = input;
  ASSERT_TRUE(input);
  ASSERT_TRUE(control);

  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputText);
  input->setType(input_type_names::kPassword);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputPassword);
  input->setType(input_type_names::kText);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputPassword);
  input->setType(input_type_names::kNumber);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputPassword);
  input->setType(input_type_names::kRadio);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputRadio);

  // MaybeSetHasBeenPasswordField() only has an effect on IsTextType() elements.
  input->setType(input_type_names::kUrl);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputUrl);
  input->MaybeSetHasBeenPasswordField();
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputPassword);
  input->setType(input_type_names::kRadio);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputRadio);
  input->MaybeSetHasBeenPasswordField();
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputRadio);
}

class WebFormElementIntersectionObserverTest : public PageTestBase {
 public:
  WebFormElementIntersectionObserverTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override { PageTestBase::SetUp(gfx::Size(400, 300)); }

  static constexpr base::TimeDelta kMinimumVisibleDuration =
      base::Milliseconds(800);
};

// Tests that WebFormElementIntersectionObserver returns true for a visible,
// intersecting element.
TEST_F(WebFormElementIntersectionObserverTest, VisibleElement) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <input id="t1" style="width: 10px; height: 10px;">
  )");
  UpdateAllLifecyclePhasesForTest();

  WebFormControlElement element(
      DynamicTo<HTMLFormControlElement>(GetElementById("t1")));
  ASSERT_TRUE(element);

  base::MockOnceClosure mock_callback;
  EXPECT_CALL(mock_callback, Run());

  base::ScopedClosureRunner runner =
      element.MonitorVisibility(kMinimumVisibleDuration, mock_callback.Get());

  UpdateAllLifecyclePhasesForTest();
  FastForwardBy(kMinimumVisibleDuration);
  UpdateAllLifecyclePhasesForTest();
  FastForwardUntilNoTasksRemain();
}

// Tests that WebFormElementIntersectionObserver does not trigger the callback
// if intersecting element is invisible (e.g., due to opacity).
TEST_F(WebFormElementIntersectionObserverTest, InvisibleElement) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <input id="t1" style="width: 10px; height: 10px; opacity: 0;">
  )");
  UpdateAllLifecyclePhasesForTest();

  WebFormControlElement element(
      DynamicTo<HTMLFormControlElement>(GetElementById("t1")));
  ASSERT_TRUE(element);

  bool callback_called = false;
  base::ScopedClosureRunner runner = element.MonitorVisibility(
      kMinimumVisibleDuration,
      base::BindOnce([](bool* called) { *called = true; }, &callback_called));

  UpdateAllLifecyclePhasesForTest();
  FastForwardBy(kMinimumVisibleDuration);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(callback_called);
  FastForwardUntilNoTasksRemain();
}

// Tests that WebFormElementIntersectionObserver does not trigger the callback
// if the element has not intersected the viewport.
TEST_F(WebFormElementIntersectionObserverTest, OffscreenElement) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div style="height: 2000px;"></div>
    <input id="t1" style="width: 10px; height: 10px;">
  )");
  UpdateAllLifecyclePhasesForTest();

  WebFormControlElement element(
      DynamicTo<HTMLFormControlElement>(GetElementById("t1")));
  ASSERT_TRUE(element);

  bool callback_called = false;
  base::ScopedClosureRunner runner = element.MonitorVisibility(
      kMinimumVisibleDuration,
      base::BindOnce([](bool* called) { *called = true; }, &callback_called));

  UpdateAllLifecyclePhasesForTest();
  FastForwardBy(kMinimumVisibleDuration);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(callback_called);
  FastForwardUntilNoTasksRemain();
}

// Tests that an offscreen element does not trigger the callback initially,
// but does trigger it with true once it is scrolled into view.
TEST_F(WebFormElementIntersectionObserverTest, ScrollIntoView) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div style="height: 2000px;"></div>
    <input id="t1" style="width: 10px; height: 10px;">
  )");
  UpdateAllLifecyclePhasesForTest();

  WebFormControlElement element(
      DynamicTo<HTMLFormControlElement>(GetElementById("t1")));
  ASSERT_TRUE(element);

  bool callback_called = false;
  base::ScopedClosureRunner runner = element.MonitorVisibility(
      kMinimumVisibleDuration,
      base::BindOnce([](bool* called) { *called = true; }, &callback_called));

  UpdateAllLifecyclePhasesForTest();
  FastForwardBy(base::Milliseconds(100));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(callback_called);

  // Scroll into view.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 2000), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  UpdateAllLifecyclePhasesForTest();
  FastForwardBy(kMinimumVisibleDuration);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(callback_called);
  FastForwardUntilNoTasksRemain();
}

}  // namespace blink
