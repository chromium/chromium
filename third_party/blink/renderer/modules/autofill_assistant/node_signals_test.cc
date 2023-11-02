// Copyright 2021 The Chromium Authors
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
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());

  SetBodyContent(R"(<input style="visibility: hidden;">)");
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());

  SetBodyContent(R"(
    <input style="height: 0; line-height: 0; padding: 0; border: none;">)");
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());

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

  EXPECT_TRUE(results[2].node_features.text.empty());

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

  EXPECT_TRUE(results[1].label_features.text.empty());
}

TEST_F(NodeSignalsTest, GetLabelByForAttribute) {
  SetBodyContent(R"(
    <label for="id">Name</label><input id="id"><input id="other">)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 2u);

  ASSERT_EQ(results[0].label_features.text.size(), 1u);
  EXPECT_EQ(results[0].label_features.text[0], "Name");

  EXPECT_TRUE(results[1].label_features.text.empty());
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
  EXPECT_EQ(results[0].label_features.text[1], "Name");  // From Aria
}

TEST_F(NodeSignalsTest, GetLabelFromGeometryWithNestedElements) {
  // Make sure nested elements don't get added multiple times.
  // Make sure elements that are not "pure text" get added.
  SetBodyContent(R"(
    <div><div>Label <i class="icon"></i></div></div>
    <div><input></div>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].label_features.text.size(), 1u);
  EXPECT_EQ(results[0].label_features.text[0], "Label");
}

TEST_F(NodeSignalsTest,
       GetLabelFromGeometryWithNestedElementsClosestLabelOnly) {
  // Make sure for nested elements only the "closest" gets added.
  SetBodyContent(R"(
    <div>Label <div>Sublabel</div></div>
    <div><input></div>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].label_features.text.size(), 1u);
  EXPECT_EQ(results[0].label_features.text[0], "Sublabel");
}

TEST_F(NodeSignalsTest, GetLabelFromGeometryAddInlineElements) {
  SetBodyContent(R"(
    <div>Name <i>First</i></div>
    <div><input></div>)");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].label_features.text.size(), 1u);
  EXPECT_EQ(results[0].label_features.text[0], "Name  First");
}

TEST_F(NodeSignalsTest, AddFieldsetLegendInsideForm) {
  SetBodyContent(R"(
    <fieldset>
      <legend>Outside</legend>
      <form>
        <fieldset>
          <legend>Inside</legend>
          <input>
        </fieldset>
      </form>
    </fieldset>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].context_features.header_text.size(), 1u);
  EXPECT_EQ(results[0].context_features.header_text[0], "Inside");
}

TEST_F(NodeSignalsTest, AddFieldsetLegendFromParentNodesOnly) {
  SetBodyContent(R"(
    <fieldset>
      <legend>Unrelated</legend>
    </fieldset>
    <fieldset>
      <legend>Related</legend>
      <input>
    </fieldset>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].context_features.header_text.size(), 1u);
  EXPECT_EQ(results[0].context_features.header_text[0], "Related");
}

TEST_F(NodeSignalsTest, AddFieldsetLegendFromAllParentalFieldsets) {
  SetBodyContent(R"(
    <fieldset>
      <legend>Outer</legend>
      <fieldset>
        <legend>Inner</legend>
        <input>
      </fieldset>
    </fieldset>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].context_features.header_text.size(), 2u);
  EXPECT_EQ(results[0].context_features.header_text[0], "Inner");
  EXPECT_EQ(results[0].context_features.header_text[1], "Outer");
}

TEST_F(NodeSignalsTest, AddFieldsetLegendFirstLabelOnly) {
  SetBodyContent(R"(
    <fieldset>
      <legend>A</legend>
      <legend>B</legend>
      <input>
    </fieldset>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].context_features.header_text.size(), 1u);
  EXPECT_EQ(results[0].context_features.header_text[0], "A");
}

TEST_F(NodeSignalsTest, AddHeadersDeepestOnly) {
  SetBodyContent(R"(
    <div>
      <h1>A</h1>
      <div>
        <h2>B</h2>
        <div>
          <input>
        </div>
      </div>
    </div>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].context_features.header_text.size(), 1u);
  EXPECT_EQ(results[0].context_features.header_text[0], "B");
}

TEST_F(NodeSignalsTest, AddHeadersOnlyIfAboveInHierarchy) {
  SetBodyContent(R"(
    <h1>A</h1>
    <input style="position: absolute; top: 200px;">
    <h1>B</h1>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  ASSERT_EQ(results[0].context_features.header_text.size(), 1u);
  EXPECT_EQ(results[0].context_features.header_text[0], "A");
}

TEST_F(NodeSignalsTest, DoNotAddEmptyHeaders) {
  SetBodyContent(R"(
    <h1></h1>
    <input>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_TRUE(results[0].context_features.header_text.empty());
}

TEST_F(NodeSignalsTest, AddHeadersOnlyIfAboveVisually) {
  SetBodyContent(R"(
    <h1 style="position: absolute; top: 100px;">Header</h1>
    <input>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_TRUE(results[0].context_features.header_text.empty());
}

TEST_F(NodeSignalsTest, GetShippingFormTypeFromText) {
  SetBodyContent(R"(
    <div>Shipping</div>
    <input>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_EQ(results[0].context_features.form_type, "SHIPPING");
}

TEST_F(NodeSignalsTest, GetBillingFormTypeFromText) {
  SetBodyContent(R"(
    <div>Billing</div>
    <input>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_EQ(results[0].context_features.form_type, "BILLING");
}

TEST_F(NodeSignalsTest, DoNotGetFormTypeFromUnrelatedText) {
  SetBodyContent(R"(
    <div>Enter Address</div>
    <input>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_TRUE(results[0].context_features.form_type.IsEmpty());
}

TEST_F(NodeSignalsTest, DoNotGetFormTypeFromTextBelow) {
  SetBodyContent(R"(
    <input>
    <div>Shipping</div>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_TRUE(results[0].context_features.form_type.IsEmpty());
}

TEST_F(NodeSignalsTest, GetFormTypeFromAncestorId) {
  SetBodyContent(R"(
    <div id="shipping_address">
      <input>
    </div>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_EQ(results[0].context_features.form_type, "SHIPPING");
}

TEST_F(NodeSignalsTest, GetFormTypeFromAncestorName) {
  SetBodyContent(R"(
    <div name="shipping_address">
      <input>
    </div>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_EQ(results[0].context_features.form_type, "SHIPPING");
}

TEST_F(NodeSignalsTest, GetFormTypeFromTextOverAncestor) {
  SetBodyContent(R"(
    <div name="shipping_address">
      <div>Billing</div>
      <input>
    </div>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_EQ(results[0].context_features.form_type, "BILLING");
}

TEST_F(NodeSignalsTest, DoNotGetFormTypeFromLabel) {
  SetBodyContent(R"(
    <label>Shipping</label>
    <input>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_TRUE(results[0].context_features.form_type.IsEmpty());
}

TEST_F(NodeSignalsTest, GetFormTypeAfterLabel) {
  SetBodyContent(R"(
    <div>Billing</div>
    <label>Shipping address is the same</label>
    <input>
  )");

  WebVector<AutofillAssistantNodeSignals> results =
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument()));
  ASSERT_EQ(results.size(), 1u);

  EXPECT_EQ(results[0].context_features.form_type, "AFTRLBL BILLING");
}

TEST_F(NodeSignalsTest, IsSupportedByClient) {
  SetBodyContent(R"(<input type="text">)");
  EXPECT_EQ(GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).size(),
            1u);

  SetBodyContent(R"(<input>)");
  EXPECT_EQ(GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).size(),
            1u);

  SetBodyContent(R"(<input type="checkbox">)");
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());

  SetBodyContent(R"(<input type="Radio">)");
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());

  SetBodyContent(R"(<input type="radio">)");
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());

  SetBodyContent(R"(<input type="submit">)");
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());

  SetBodyContent(R"(<input type="button">)");
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());

  SetBodyContent(R"(<input type="hidden">)");
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());

  SetBodyContent(R"(<select>)");
  EXPECT_EQ(GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).size(),
            1u);

  SetBodyContent(R"(<textarea>)");
  EXPECT_EQ(GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).size(),
            1u);

  SetBodyContent(R"(<div>)");
  EXPECT_TRUE(
      GetAutofillAssistantNodeSignals(WebDocument(&GetDocument())).empty());
}

}  // namespace blink
