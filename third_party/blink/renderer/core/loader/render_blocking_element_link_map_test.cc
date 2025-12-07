// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "third_party/blink/renderer/core/loader/render_blocking_element_link_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class RenderBlockingElementLinkMapTest : public PageTestBase {
 public:
  RenderBlockingElementLinkMapTest()
      : element_link_map_(MakeGarbageCollected<
                          RenderBlockingElementLinkMap>(BindRepeating(
            &RenderBlockingElementLinkMapTest::OnRenderBlockingElementSetEmpty,
            base::Unretained(this)))) {}

 protected:
  using AllLinksLevelMap =
      GCedHeapHashMap<Member<const HTMLLinkElement>, RenderBlockingLevel>;

  void OnRenderBlockingElementSetEmpty(RenderBlockingLevel level) {
    empty_handler_call_count_++;
    last_empty_handler_level_ = level;
  }

  AllLinksLevelMap& GetAllLinksInMap() {
    AllLinksLevelMap* elements = MakeGarbageCollected<AllLinksLevelMap>();
    element_link_map_->ForEach(BindRepeating(
        [](AllLinksLevelMap* elements, RenderBlockingLevel level,
           const HTMLLinkElement& element) {
          elements->insert(&element, level);
        },
        WrapPersistent(elements)));
    return *elements;
  }

  Persistent<RenderBlockingElementLinkMap> element_link_map_;
  int empty_handler_call_count_ = 0;
  RenderBlockingLevel last_empty_handler_level_;
};

TEST_F(RenderBlockingElementLinkMapTest, EmptyOnConstruction) {
  EXPECT_FALSE(element_link_map_->HasElement(RenderBlockingLevel::kBlock));
  EXPECT_FALSE(
      element_link_map_->HasElement(RenderBlockingLevel::kLimitFrameRate));
}

TEST_F(RenderBlockingElementLinkMapTest, AddLinkWithTargetElement) {
  auto* link_element1 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element2 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());

  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element1, RenderBlockingLevel::kBlock);
  EXPECT_TRUE(element_link_map_->HasElement(RenderBlockingLevel::kBlock));
  EXPECT_FALSE(
      element_link_map_->HasElement(RenderBlockingLevel::kLimitFrameRate));

  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id2"), link_element2, RenderBlockingLevel::kLimitFrameRate);
  EXPECT_TRUE(element_link_map_->HasElement(RenderBlockingLevel::kBlock));
  EXPECT_TRUE(
      element_link_map_->HasElement(RenderBlockingLevel::kLimitFrameRate));
}

TEST_F(RenderBlockingElementLinkMapTest, AddLinkWithEmptyTargetElement) {
  auto* link_element = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  element_link_map_->AddLinkWithTargetElement(AtomicString(""), link_element,
                                              RenderBlockingLevel::kBlock);
  EXPECT_FALSE(element_link_map_->HasElement(RenderBlockingLevel::kBlock));
}

TEST_F(RenderBlockingElementLinkMapTest, RemoveTargetElement) {
  auto* link_element1 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element2 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element3 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());

  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element1, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element2, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id2"), link_element3, RenderBlockingLevel::kLimitFrameRate);

  element_link_map_->RemoveTargetElement(AtomicString("id1"));

  EXPECT_FALSE(element_link_map_->HasElement(RenderBlockingLevel::kBlock));
  EXPECT_TRUE(
      element_link_map_->HasElement(RenderBlockingLevel::kLimitFrameRate));

  auto& links = GetAllLinksInMap();
  EXPECT_EQ(links.size(), 1u);
  EXPECT_TRUE(links.Contains(link_element3));
}

TEST_F(RenderBlockingElementLinkMapTest, RemoveLinkWithTargetElement) {
  auto* link_element1 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element2 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element3 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());

  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element1, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element2, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id2"), link_element3, RenderBlockingLevel::kLimitFrameRate);

  element_link_map_->RemoveLinkWithTargetElement(AtomicString("id1"),
                                                 link_element1);

  EXPECT_TRUE(element_link_map_->HasElement(RenderBlockingLevel::kBlock));
  EXPECT_TRUE(
      element_link_map_->HasElement(RenderBlockingLevel::kLimitFrameRate));

  auto& links_after_remove1 = GetAllLinksInMap();
  EXPECT_EQ(links_after_remove1.size(), 2u);
  EXPECT_TRUE(links_after_remove1.Contains(link_element2));
  EXPECT_TRUE(links_after_remove1.Contains(link_element3));

  element_link_map_->RemoveLinkWithTargetElement(AtomicString("id1"),
                                                 link_element2);

  EXPECT_FALSE(element_link_map_->HasElement(RenderBlockingLevel::kBlock));
  EXPECT_TRUE(
      element_link_map_->HasElement(RenderBlockingLevel::kLimitFrameRate));
  auto& links_after_remove2 = GetAllLinksInMap();
  EXPECT_EQ(links_after_remove2.size(), 1u);
  EXPECT_TRUE(links_after_remove2.Contains(link_element3));
}

TEST_F(RenderBlockingElementLinkMapTest, ForEachLink) {
  auto* link_element1 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element2 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element3 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());

  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element1, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id2"), link_element2, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id3"), link_element3, RenderBlockingLevel::kLimitFrameRate);

  auto& links = GetAllLinksInMap();

  EXPECT_EQ(links.size(), 3u);
  EXPECT_TRUE(links.Contains(link_element1));
  EXPECT_EQ(links.at(link_element1), RenderBlockingLevel::kBlock);
  EXPECT_TRUE(links.Contains(link_element2));
  EXPECT_EQ(links.at(link_element2), RenderBlockingLevel::kBlock);
  EXPECT_TRUE(links.Contains(link_element3));
  EXPECT_EQ(links.at(link_element3), RenderBlockingLevel::kLimitFrameRate);
}

TEST_F(RenderBlockingElementLinkMapTest, Clear) {
  auto* link_element1 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element2 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());

  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element1, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id2"), link_element2, RenderBlockingLevel::kLimitFrameRate);

  EXPECT_EQ(empty_handler_call_count_, 0);
  element_link_map_->Clear();

  EXPECT_FALSE(element_link_map_->HasElement(RenderBlockingLevel::kBlock));
  EXPECT_FALSE(
      element_link_map_->HasElement(RenderBlockingLevel::kLimitFrameRate));
  EXPECT_EQ(empty_handler_call_count_, 2);
}

TEST_F(RenderBlockingElementLinkMapTest,
       EmptyHandlerCalledOnceForDifferentLevel) {
  empty_handler_call_count_ = 0;
  auto* link_element1 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element2 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());

  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element1, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id2"), link_element2, RenderBlockingLevel::kLimitFrameRate);
  EXPECT_EQ(empty_handler_call_count_, 0);

  element_link_map_->RemoveTargetElement(AtomicString("id1"));
  EXPECT_EQ(empty_handler_call_count_, 1);
  EXPECT_EQ(last_empty_handler_level_, RenderBlockingLevel::kBlock);

  element_link_map_->RemoveTargetElement(AtomicString("id2"));
  EXPECT_EQ(empty_handler_call_count_, 2);
  EXPECT_EQ(last_empty_handler_level_, RenderBlockingLevel::kLimitFrameRate);
}

TEST_F(RenderBlockingElementLinkMapTest,
       EmptyHandlerCalledCorrectlyForRemoveLink) {
  empty_handler_call_count_ = 0;
  auto* link_element1 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element2 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  auto* link_element3 = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());

  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element1, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id1"), link_element2, RenderBlockingLevel::kBlock);
  element_link_map_->AddLinkWithTargetElement(
      AtomicString("id2"), link_element3, RenderBlockingLevel::kLimitFrameRate);
  EXPECT_EQ(empty_handler_call_count_, 0);

  element_link_map_->RemoveLinkWithTargetElement(AtomicString("id1"),
                                                 link_element1);
  EXPECT_EQ(empty_handler_call_count_, 0);

  element_link_map_->RemoveLinkWithTargetElement(AtomicString("id1"),
                                                 link_element2);
  EXPECT_EQ(empty_handler_call_count_, 1);
  EXPECT_EQ(last_empty_handler_level_, RenderBlockingLevel::kBlock);

  element_link_map_->RemoveTargetElement(AtomicString("id2"));
  EXPECT_EQ(empty_handler_call_count_, 2);
  EXPECT_EQ(last_empty_handler_level_, RenderBlockingLevel::kLimitFrameRate);
}

}  // namespace blink
