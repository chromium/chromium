// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/menu_mutation_observer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/protocol/audits.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

// Tests that MenuMutationObserver reports a DevTools issue and UseCounter for
// each kind of menu content model violation.
class MenuMutationObserverTest : public PageTestBase {
 protected:
  void PerformMicrotaskCheckpoint() {
    GetDocument().GetAgent().event_loop()->PerformMicrotaskCheckpoint();
  }

  InspectorIssueStorage& IssueStorage() {
    return GetPage().GetInspectorIssueStorage();
  }

  // Returns the ElementAccessibilityIssue details for `node` that were added
  // since `start_index`, or nullptr if there are none.
  protocol::Audits::ElementAccessibilityIssueDetails* IssueForNode(
      wtf_size_t start_index,
      Node& node) {
    for (wtf_size_t i = start_index; i < IssueStorage().size(); ++i) {
      protocol::Audits::InspectorIssue* issue = IssueStorage().at(i);
      if (issue->getCode() !=
          protocol::Audits::InspectorIssueCodeEnum::ElementAccessibilityIssue) {
        continue;
      }
      auto* details =
          issue->getDetails()->getElementAccessibilityIssueDetails(nullptr);
      if (details && details->getNodeId() == node.GetDomNodeId()) {
        return details;
      }
    }
    return nullptr;
  }

  bool HasAnyElementAccessibilityIssueSince(wtf_size_t start_index) {
    for (wtf_size_t i = start_index; i < IssueStorage().size(); ++i) {
      if (IssueStorage().at(i)->getCode() ==
          protocol::Audits::InspectorIssueCodeEnum::ElementAccessibilityIssue) {
        return true;
      }
    }
    return false;
  }

  ScopedMenuElementsForTest menu_elements_{true};
};

TEST_F(MenuMutationObserverTest, NoIssueForValidMenu) {
  wtf_size_t start = IssueStorage().size();
  SetBodyInnerHTML(R"HTML(
    <menubar>
      <menuitem>File</menuitem>
      <menuitem>Edit</menuitem>
    </menubar>
  )HTML");
  PerformMicrotaskCheckpoint();
  EXPECT_FALSE(HasAnyElementAccessibilityIssueSince(start));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kMenuBadContentModel));
}

TEST_F(MenuMutationObserverTest, InteractiveContentPresentOnInsertion) {
  wtf_size_t start = IssueStorage().size();
  SetBodyInnerHTML(R"HTML(
    <menubar>
      <menuitem>File</menuitem>
      <button id="violation">Invalid</button>
    </menubar>
  )HTML");
  PerformMicrotaskCheckpoint();
  auto* details = IssueForNode(start, *GetElementById("violation"));
  ASSERT_TRUE(details);
  EXPECT_EQ(protocol::Audits::ElementAccessibilityIssueReasonEnum::
                InteractiveContentMenuDescendant,
            details->getElementAccessibilityIssueReason());
  EXPECT_FALSE(details->getHasDisallowedAttributes());
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kInteractiveContentMenuDescendant));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kMenuBadContentModel));
}

TEST_F(MenuMutationObserverTest, InteractiveContentAddedLater) {
  SetBodyInnerHTML(R"HTML(
    <menulist id="menu">
      <menuitem>Copy</menuitem>
    </menulist>
  )HTML");
  PerformMicrotaskCheckpoint();

  wtf_size_t start = IssueStorage().size();
  auto* button = GetDocument().CreateRawElement(html_names::kButtonTag);
  GetElementById("menu")->AppendChild(button);
  // MenuMutationObserver gets mutation records in a microtask.
  EXPECT_FALSE(IssueForNode(start, *button));
  PerformMicrotaskCheckpoint();
  auto* details = IssueForNode(start, *button);
  ASSERT_TRUE(details);
  EXPECT_EQ(protocol::Audits::ElementAccessibilityIssueReasonEnum::
                InteractiveContentMenuDescendant,
            details->getElementAccessibilityIssueReason());
}

TEST_F(MenuMutationObserverTest, Anchor) {
  wtf_size_t start = IssueStorage().size();
  SetBodyInnerHTML(R"HTML(
    <menubar>
      <menuitem>File</menuitem>
      <a id="violation">Link</a>
    </menubar>
  )HTML");
  PerformMicrotaskCheckpoint();
  auto* details = IssueForNode(start, *GetElementById("violation"));
  ASSERT_TRUE(details);
  EXPECT_EQ(protocol::Audits::ElementAccessibilityIssueReasonEnum::
                AnchorMenuDescendant,
            details->getElementAccessibilityIssueReason());
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kAnchorMenuDescendant));
}

TEST_F(MenuMutationObserverTest, TextOutsideMenuItem) {
  SetBodyInnerHTML(R"HTML(
    <menubar id="menu">
      <menuitem>File</menuitem>
    </menubar>
  )HTML");
  PerformMicrotaskCheckpoint();

  wtf_size_t start = IssueStorage().size();
  Text* text = GetDocument().createTextNode("Stray");
  GetElementById("menu")->AppendChild(text);
  PerformMicrotaskCheckpoint();
  auto* details = IssueForNode(start, *text);
  ASSERT_TRUE(details);
  EXPECT_EQ(
      protocol::Audits::ElementAccessibilityIssueReasonEnum::TextMenuDescendant,
      details->getElementAccessibilityIssueReason());
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kTextMenuDescendant));
}

TEST_F(MenuMutationObserverTest, TabindexAdded) {
  SetBodyInnerHTML(R"HTML(
    <menubar>
      <menuitem>File</menuitem>
      <div id="target"></div>
    </menubar>
  )HTML");
  PerformMicrotaskCheckpoint();

  wtf_size_t start = IssueStorage().size();
  Element* target = GetElementById("target");
  target->setAttribute(html_names::kTabindexAttr, AtomicString("0"));
  PerformMicrotaskCheckpoint();
  auto* details = IssueForNode(start, *target);
  ASSERT_TRUE(details);
  EXPECT_EQ(protocol::Audits::ElementAccessibilityIssueReasonEnum::
                InteractiveContentMenuDescendant,
            details->getElementAccessibilityIssueReason());
  EXPECT_TRUE(details->getHasDisallowedAttributes());
}

}  // namespace blink
