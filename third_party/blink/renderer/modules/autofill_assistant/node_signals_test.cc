// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/autofill_assistant/node_signals.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

void SetShadowContent(const Document& document,
                      const char* host,
                      const char* shadow_content,
                      ShadowRootType type) {
  ShadowRoot& shadow_root =
      document.getElementById(AtomicString::FromUTF8(host))
          ->AttachShadowRootInternal(type);
  shadow_root.setInnerHTML(String::FromUTF8(shadow_content),
                           ASSERT_NO_EXCEPTION);
  document.View()->UpdateAllLifecyclePhasesForTest();
}

}  // namespace

using NodeSignalsTest = PageTestBase;

TEST_F(NodeSignalsTest, GetNodeIdForInput) {
  SetBodyContent(R"(<input id="a">)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  Node* a = GetDocument().getElementById("a");
  DOMNodeId id_a = DOMNodeIds::IdForNode(a);
  EXPECT_EQ(results[0].backend_node_id, static_cast<int32_t>(id_a));
}

TEST_F(NodeSignalsTest, GetNodeIdsForAllInputs) {
  SetBodyContent(
      R"(<input id="a"><select id="b"></select><textarea id="c"></textarea>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  EXPECT_EQ(results.size(), 3u);
}

TEST_F(NodeSignalsTest, PiercesOpenShadowDom) {
  SetBodyContent(R"(<div id="host"></div>)");
  SetShadowContent(GetDocument(), "host", R"(<input id="a"/>)",
                   ShadowRootType::kOpen);

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  EXPECT_EQ(results.size(), 1u);
}

TEST_F(NodeSignalsTest, PiercesClosedShadowDom) {
  SetBodyContent(R"(<div id="host"></div>)");
  SetShadowContent(GetDocument(), "host", R"(<input id="a"/>)",
                   ShadowRootType::kClosed);

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  EXPECT_EQ(results.size(), 1u);
}

TEST_F(NodeSignalsTest, AssignsTagNames) {
  SetBodyContent(R"(<input><select></select><textarea></textarea>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 3u);

  EXPECT_EQ(results[0].node_features.html_tag, "INPUT");
  EXPECT_EQ(results[1].node_features.html_tag, "SELECT");
  EXPECT_EQ(results[2].node_features.html_tag, "TEXTAREA");
}

TEST_F(NodeSignalsTest, IgnoresNonVisibleElements) {
  SetBodyContent(R"(<input style="display: none;">)");
  EXPECT_EQ(GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).size(),
            0u);

  SetBodyContent(R"(<input style="visibility: hidden;">)");
  EXPECT_EQ(GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).size(),
            0u);

  SetBodyContent(R"(
    <input style="height: 0; line-height: 0; padding: 0; border: none;">)");
  EXPECT_EQ(GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).size(),
            0u);

  // An element outside of the visible viewport should still be returned.
  SetBodyContent(
      R"(<input style="position: absolute; left: -1000px; top: -1000px;">)");
  EXPECT_EQ(GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).size(),
            1u);
}

TEST_F(NodeSignalsTest, CollectsAriaAttributes) {
  SetBodyContent(R"(
    <input aria-label="label" aria-description="description"
      aria-placeholder="placeholder">
    <input>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 2u);

  EXPECT_EQ(results[0].node_features.aria, "label description placeholder");
  EXPECT_EQ(results[1].node_features.aria, "");
}

TEST_F(NodeSignalsTest, CollectsInvisibleAttributes) {
  SetBodyContent(R"(
    <input name="name" title="title" label="label" pattern="pattern">
    <input>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 2u);

  EXPECT_EQ(results[0].node_features.invisible_attributes,
            "name title label pattern");
  EXPECT_EQ(results[1].node_features.invisible_attributes, "");
}

TEST_F(NodeSignalsTest, AssignsType) {
  SetBodyContent(R"(<input type="text">)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_EQ(results[0].node_features.type, "text");
}

TEST_F(NodeSignalsTest, CollectInnerText) {
  SetBodyContent(R"(
    <input placeholder="placeholder">
    <input readonly value="value">
    <input value="value">
    <select><option>A</option><option>B</option></select>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 4u);

  ASSERT_EQ(results[0].node_features.text.size(), 1u);
  EXPECT_EQ(results[0].node_features.text[0], "placeholder");

  ASSERT_EQ(results[1].node_features.text.size(), 1u);
  EXPECT_EQ(results[1].node_features.text[0], "value");

  ASSERT_EQ(results[2].node_features.text.size(), 0u);

  ASSERT_EQ(results[3].node_features.text.size(), 2u);
  EXPECT_EQ(results[3].node_features.text[0], "A");
  EXPECT_EQ(results[3].node_features.text[1], "B");
}

TEST_F(NodeSignalsTest, GetLabelFromParentElement) {
  SetBodyContent(R"(<label>Name<input></label><input>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 2u);

  ASSERT_EQ(results[0].label_features.text.size(), 1u);
  EXPECT_EQ(results[0].label_features.text[0], "Name");

  ASSERT_EQ(results[1].label_features.text.size(), 0u);
}

TEST_F(NodeSignalsTest, GetLabelByForAttribute) {
  SetBodyContent(R"(
    <label for="id">Name</label><input id="id"><input id="other">)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 2u);

  ASSERT_EQ(results[0].label_features.text.size(), 1u);
  EXPECT_EQ(results[0].label_features.text[0], "Name");

  ASSERT_EQ(results[1].label_features.text.size(), 0u);
}

TEST_F(NodeSignalsTest, GetLabelByForAttributeWithCollision) {
  SetBodyContent(R"(
    <label for="id">Name</label><input id="id">
    <label for="id">Other</label><input id="id">)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 2u);

  ASSERT_EQ(results[0].label_features.text.size(), 1u);
  EXPECT_EQ(results[0].label_features.text[0], "Name");

  ASSERT_EQ(results[1].label_features.text.size(), 1u);
  ASSERT_EQ(results[1].label_features.text[0], "Name");
}

TEST_F(NodeSignalsTest, GetLabelByAriaElements) {
  SetBodyContent(R"(
    <div id="form">Billing</div>
    <div>
      <div id="input">Name</div>
      <input aria-labelledby="form input">
    </div>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].label_features.text.size(), 2u);
  EXPECT_EQ(results[0].label_features.text[0], "Billing");
  EXPECT_EQ(results[0].label_features.text[1], "Name");
}

}  // namespace blink
