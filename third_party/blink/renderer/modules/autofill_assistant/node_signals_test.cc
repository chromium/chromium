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

}  // namespace blink
