// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"

#include <memory>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class InspectorAccessibilityAgentTest : public PageTestBase {};

// Without cleanup for a disabled agent, Dispose() leaves the AXContext
// created by a one-shot request alive until GC finalizes the agent.
TEST_F(InspectorAccessibilityAgentTest,
       DisableClearsContextCreatedWithoutEnable) {
  auto* inspected_frames =
      MakeGarbageCollected<InspectedFrames>(GetDocument().GetFrame());
  auto* agent = MakeGarbageCollected<InspectorAccessibilityAgent>(
      inspected_frames,
      MakeGarbageCollected<InspectorDOMAgent>(
          GetDocument().GetExecutionContext()->GetIsolate(), inspected_frames));

  std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>> nodes;
  ASSERT_TRUE(
      agent->getFullAXTree(std::nullopt, std::nullopt, &nodes).IsSuccess());
  ASSERT_TRUE(GetDocument().ExistingAXObjectCache());

  agent->Dispose();

  EXPECT_FALSE(GetDocument().ExistingAXObjectCache());
}

// A node's description depends on where its name came from: text that names
// the node is not repeated as its description.
TEST_F(InspectorAccessibilityAgentTest, DescriptionDoesNotRepeatName) {
  SetBodyInnerHTML(R"HTML(
    <button id="title" title="Save file"></button>
    <button id="label" aria-label="Save" title="Save file"></button>
    <details><summary id="summary">More info</summary>Hidden</details>
  )HTML");

  auto* inspected_frames =
      MakeGarbageCollected<InspectedFrames>(GetDocument().GetFrame());
  auto* agent = MakeGarbageCollected<InspectorAccessibilityAgent>(
      inspected_frames,
      MakeGarbageCollected<InspectorDOMAgent>(
          GetDocument().GetExecutionContext()->GetIsolate(), inspected_frames));
  std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>> nodes;
  ASSERT_TRUE(
      agent->getFullAXTree(std::nullopt, std::nullopt, &nodes).IsSuccess());

  // Returns the description of the AXNode for the element with `id`.
  auto description_of = [&](const char* id) -> std::string {
    const int backend_node_id =
        IdentifiersFactory::IntIdForNode(GetElementById(id));
    for (const auto& node : *nodes) {
      if (node->getBackendDOMNodeId(0) != backend_node_id) {
        continue;
      }
      String description;
      if (protocol::Accessibility::AXValue* value =
              node->getDescription(nullptr)) {
        value->getValue()->asString(&description);
      }
      return description.Utf8();
    }
    ADD_FAILURE() << "No AXNode for #" << id;
    return std::string();
  };

  // Named from its title, so the title is not also its description.
  EXPECT_EQ("", description_of("title"));
  // Named from aria-label, so the title is its description.
  EXPECT_EQ("Save file", description_of("label"));
  // Named from its contents, so a summary does not repeat them.
  EXPECT_EQ("", description_of("summary"));

  agent->Dispose();
}

}  // namespace blink
