// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"

namespace blink {

// TODO(nektar): Break test up into multiple tests.
TEST_F(AccessibilityTest, IsARIAWidget) {
  String test_content =
      "<body>"
      "<span id=\"plain\">plain</span><br>"
      "<span id=\"button\" role=\"button\">button</span><br>"
      "<span id=\"button-parent\" "
      "role=\"button\"><span>button-parent</span></span><br>"
      "<span id=\"button-caps\" role=\"BUTTON\">button-caps</span><br>"
      "<span id=\"button-second\" role=\"another-role "
      "button\">button-second</span><br>"
      "<span id=\"aria-bogus\" aria-bogus=\"bogus\">aria-bogus</span><br>"
      "<span id=\"aria-selected\" aria-selected>aria-selected</span><br>"
      "<span id=\"haspopup\" "
      "aria-haspopup=\"true\">aria-haspopup-true</span><br>"
      "<div id=\"focusable\" tabindex=\"1\">focusable</div><br>"
      "<div tabindex=\"2\"><div "
      "id=\"focusable-parent\">focusable-parent</div></div><br>"
      "</body>";

  SetBodyInnerHTML(test_content);
  Element* root(GetDocument().documentElement());
  EXPECT_FALSE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("plain")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button-parent")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button-caps")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button-second")));
  EXPECT_FALSE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("aria-bogus")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("aria-selected")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("haspopup")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("focusable")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("focusable-parent")));
}

class MockAXObject : public AXObject {
 public:
  explicit MockAXObject(AXObjectCacheImpl& ax_object_cache)
      : AXObject(ax_object_cache) {}
  static unsigned num_children_changed_calls_;

  void ChildrenChanged() final { num_children_changed_calls_++; }
  AXObject* ComputeParent() const final { return nullptr; }
  Document* GetDocument() const final { return &AXObjectCache().GetDocument(); }
};

unsigned MockAXObject::num_children_changed_calls_ = 0;

TEST_F(AccessibilityTest, PauseUpdatesAfterMaxNumberQueued) {
  auto& document = GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  DCHECK(ax_object_cache);

  wtf_size_t max_updates = 10;
  ax_object_cache->SetMaxPendingUpdatesForTesting(max_updates);

  MockAXObject* ax_obj = MakeGarbageCollected<MockAXObject>(*ax_object_cache);
  ax_object_cache->AssociateAXID(ax_obj);
  for (unsigned i = 0; i < max_updates + 1; i++) {
    ax_object_cache->DeferTreeUpdate(
        &AXObjectCacheImpl::ChildrenChangedWithCleanLayout, nullptr, ax_obj);
  }
  document.Lifecycle().AdvanceTo(DocumentLifecycle::kInAccessibility);
  ax_object_cache->ProcessCleanLayoutCallbacks(document);

  ASSERT_EQ(0u, MockAXObject::num_children_changed_calls_);
}

}  // namespace blink
