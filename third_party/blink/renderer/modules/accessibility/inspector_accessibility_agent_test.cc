// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
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

}  // namespace blink
