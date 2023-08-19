// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/tree_ordered_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class TreeOrderedMapTest : public EditingTestBase {
 protected:
  void SetUp() override {
    EditingTestBase::SetUp();
    root_ = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    root_->setAttribute(html_names::kIdAttr, AtomicString("ROOT"));
    GetDocument().body()->appendChild(root_);
  }

  Element* AddElement(AtomicString slot_name) {
    auto* slot = MakeGarbageCollected<HTMLSlotElement>(GetDocument());
    slot->setAttribute(html_names::kNameAttr, slot_name);
    std::string id = "SLOT_" + base::NumberToString(++element_num);
    slot->setAttribute(html_names::kIdAttr, AtomicString(id.c_str()));
    root_->appendChild(slot);
    return static_cast<Element*>(slot);
  }
  TreeScope& GetTreeScope() { return root_->GetTreeScope(); }

 private:
  int element_num{0};
  Persistent<HTMLDivElement> root_;
};

TEST_F(TreeOrderedMapTest, Basic) {
  auto* map = MakeGarbageCollected<TreeOrderedMap>();
  AtomicString key("test");
  auto& element = *AddElement(key);
  map->Add(key, element);
  EXPECT_TRUE(map->Contains(key));
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key), element);
  map->Remove(key, element);
  EXPECT_FALSE(map->Contains(key));
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key), nullptr);
}

TEST_F(TreeOrderedMapTest, DuplicateKeys) {
  auto* map = MakeGarbageCollected<TreeOrderedMap>();
  AtomicString key("test");
  auto& element1 = *AddElement(key);
  auto& element2 = *AddElement(key);
  map->Add(key, element1);
  EXPECT_TRUE(map->Contains(key));
  EXPECT_FALSE(map->ContainsMultiple(key));
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key), element1);
  map->Add(key, element2);
  EXPECT_TRUE(map->Contains(key));
  EXPECT_TRUE(map->ContainsMultiple(key));
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key), nullptr)
      << "No tree walk yet";
  EXPECT_EQ(map->GetSlotByName(key, GetTreeScope()), element1);
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key), element1)
      << "Tree walk forced by GetSlotByName";
  element1.remove();  // Remove it from the tree also.
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key), element1)
      << "Make sure we don't touch the tree";
  map->Remove(key, element1);
  EXPECT_TRUE(map->Contains(key));
  EXPECT_FALSE(map->ContainsMultiple(key));
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key), nullptr);
  EXPECT_EQ(map->GetSlotByName(key, GetTreeScope()), element2);
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key), element2);
  map->Remove(key, element2);
  EXPECT_FALSE(map->Contains(key));
  EXPECT_FALSE(map->ContainsMultiple(key));
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key), nullptr);
  EXPECT_EQ(map->GetSlotByName(key, GetTreeScope()), nullptr)
      << "nullptr even though we never removed element2 from the tree";
}

TEST_F(TreeOrderedMapTest, ManyKeys) {
  auto* map = MakeGarbageCollected<TreeOrderedMap>();
  AtomicString key1("test1");
  AtomicString key2 = g_empty_atom;  // Empty should be handled as a unique key
  auto& element1 = *AddElement(key1);
  auto& element2 = *AddElement(key1);
  auto& element3 = *AddElement(key2);
  auto& element4 = *AddElement(key2);
  map->Add(key1, element1);
  map->Add(key1, element2);
  map->Add(key2, element3);
  map->Add(key2, element4);
  EXPECT_TRUE(map->Contains(key1));
  EXPECT_TRUE(map->Contains(key2));
  EXPECT_TRUE(map->ContainsMultiple(key1));
  EXPECT_TRUE(map->ContainsMultiple(key2));
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key1), nullptr);
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key2), nullptr);
  EXPECT_EQ(map->GetSlotByName(key1, GetTreeScope()), element1);
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key1), element1);
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key2), nullptr);
  EXPECT_EQ(map->GetSlotByName(key2, GetTreeScope()), element3);
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key2), element3);
  map->Remove(key1, element2);
  map->Remove(key1, element1);
  map->Remove(key2, element3);
  element3.remove();
  EXPECT_FALSE(map->Contains(key1));
  EXPECT_TRUE(map->Contains(key2));
  EXPECT_FALSE(map->ContainsMultiple(key2));
  EXPECT_EQ(map->GetCachedFirstElementWithoutAccessingNodeTree(key2), nullptr);
  EXPECT_EQ(map->GetSlotByName(key2, GetTreeScope()), element4);
}

TEST_F(TreeOrderedMapTest, RemovedDuplicateKeys) {
  auto* map = MakeGarbageCollected<TreeOrderedMap>();
  AtomicString key("test");
  auto& outer = *AddElement(key);
  auto& inner = *AddElement(key);
  outer.appendChild(&inner);
  map->Add(key, outer);
  map->Add(key, inner);
  EXPECT_EQ(map->GetSlotByName(key, GetTreeScope()), outer);
  EXPECT_TRUE(map->ContainsMultiple(key));
  outer.remove();  // This removes both elements from the tree
  EXPECT_TRUE(map->ContainsMultiple(key)) << "We haven't touched the map yet";
  TreeOrderedMap::RemoveScope tree_remove_scope;
  map->Remove(key, outer);
  EXPECT_TRUE(map->Contains(key))
      << "The map will still contain the entry for inner at this point";
  EXPECT_FALSE(map->ContainsMultiple(key));
  EXPECT_EQ(map->GetSlotByName(key, GetTreeScope()), nullptr);
  EXPECT_FALSE(map->Contains(key))
      << "The call to GetSlotByName should have cleared the key entirely";
}

}  // namespace blink
