// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/slot_scoped_traversal.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class SlotScopedTraversalTest : public testing::Test {
 protected:
  Document& GetDocument() const;

  void SetupSampleHTML(const char* main_html,
                       const char* shadow_html,
                       unsigned);
  void AttachOpenShadowRoot(Element& shadow_host,
                            const char* shadow_inner_html);
  void TestExpectedSequence(const Vector<Element*>& list);

 private:
  void SetUp() override;

  Persistent<Document> document_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void SlotScopedTraversalTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  document_ = &dummy_page_holder_->GetDocument();
  DCHECK(document_);
}

Document& SlotScopedTraversalTest::GetDocument() const {
  return *document_;
}

void SlotScopedTraversalTest::SetupSampleHTML(const char* main_html,
                                              const char* shadow_html,
                                              unsigned index) {
  Element* body = GetDocument().body();
  body->setInnerHTML(String::FromUTF8(main_html));
  if (shadow_html) {
    auto* shadow_host = To<Element>(NodeTraversal::ChildAt(*body, index));
    AttachOpenShadowRoot(*shadow_host, shadow_html);
  }
}

void SlotScopedTraversalTest::AttachOpenShadowRoot(
    Element& shadow_host,
    const char* shadow_inner_html) {
  ShadowRoot& shadow_root =
      shadow_host.AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(String::FromUTF8(shadow_inner_html));
}

TEST_F(SlotScopedTraversalTest, simpleSlot) {
  const char* main_html =
      "<div id='host'>"
      "<div id='inner1'></div>"
      "<div id='inner2'></div>"
      "</div>";

  const char* shadow_html = "<slot></slot>";

  SetupSampleHTML(main_html, shadow_html, 0);

  Element* host = GetDocument().QuerySelector("#host");
  Element* inner1 = GetDocument().QuerySelector("#inner1");
  Element* inner2 = GetDocument().QuerySelector("#inner2");
  ShadowRoot* shadow_root = host->OpenShadowRoot();
  auto* slot = To<HTMLSlotElement>(shadow_root->QuerySelector("slot"));

  EXPECT_EQ(nullptr,
            SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(*host));
  EXPECT_EQ(nullptr,
            SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(*slot));
  EXPECT_EQ(inner1, SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(
                        *inner1));
  EXPECT_EQ(inner2, SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(
                        *inner2));
  EXPECT_EQ(slot, SlotScopedTraversal::FindScopeOwnerSlot(*inner1));
  EXPECT_EQ(slot, SlotScopedTraversal::FindScopeOwnerSlot(*inner2));
}

}  // namespace blink
