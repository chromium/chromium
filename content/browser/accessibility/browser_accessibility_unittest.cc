// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

#define EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants,                 \
                                                expected_descendants)        \
  {                                                                          \
    size_t count = descendants.size();                                       \
    EXPECT_EQ(count, expected_descendants.size());                           \
    for (size_t i = 0; i < count; ++i) {                                     \
      EXPECT_EQ(ui::AXPlatformNode::FromNativeViewAccessible(descendants[i]) \
                    ->GetDelegate()                                          \
                    ->GetData()                                              \
                    .ToString(),                                             \
                ui::AXPlatformNode::FromNativeViewAccessible(                \
                    expected_descendants[i])                                 \
                    ->GetDelegate()                                          \
                    ->GetData()                                              \
                    .ToString());                                            \
    }                                                                        \
  }

class BrowserAccessibilityTest : public testing::Test {
 public:
  BrowserAccessibilityTest();
  ~BrowserAccessibilityTest() override;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  void SetUp() override;

  base::test::TaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityTest);
};

BrowserAccessibilityTest::BrowserAccessibilityTest() {}

BrowserAccessibilityTest::~BrowserAccessibilityTest() {}

void BrowserAccessibilityTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<TestBrowserAccessibilityDelegate>();
}

TEST_F(BrowserAccessibilityTest, TestCanFireEvents) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName("One two three.");

  ui::AXNodeData para1;
  para1.id = 11;
  para1.role = ax::mojom::Role::kParagraph;
  para1.child_ids.push_back(text1.id);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(para1.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, para1, text1),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_obj = manager->GetRoot();
  EXPECT_FALSE(root_obj->PlatformIsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());

  BrowserAccessibility* para_obj = root_obj->PlatformGetChild(0);
  EXPECT_TRUE(para_obj->CanFireEvents());
#if defined(OS_ANDROID)
  EXPECT_TRUE(para_obj->PlatformIsLeaf());
#else
  EXPECT_FALSE(para_obj->PlatformIsLeaf());
#endif

  BrowserAccessibility* text_obj = manager->GetFromID(111);
  EXPECT_TRUE(text_obj->PlatformIsLeaf());
  EXPECT_TRUE(text_obj->CanFireEvents());

  manager.reset();
}

#if defined(OS_WIN) || BUILDFLAG(USE_ATK)
TEST_F(BrowserAccessibilityTest, TestGetDescendants) {
  // Set up ax tree with the following structure:
  //
  // root_____________________
  // |               |       |
  // para1____       text3   para2____ (hidden)
  // |       |               |       |
  // text1   text2           text4   text5 (visible)
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName("One two three.");

  ui::AXNodeData text2;
  text2.id = 112;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName("Two three four.");

  ui::AXNodeData text3;
  text3.id = 113;
  text3.role = ax::mojom::Role::kStaticText;
  text3.SetName("Three four five.");

  ui::AXNodeData text4;
  text4.id = 114;
  text4.role = ax::mojom::Role::kStaticText;
  text4.SetName("four five six.");
  text4.AddState(ax::mojom::State::kIgnored);

  ui::AXNodeData text5;
  text5.id = 115;
  text5.role = ax::mojom::Role::kStaticText;
  text5.SetName("five six seven.");

  ui::AXNodeData para1;
  para1.id = 11;
  para1.role = ax::mojom::Role::kParagraph;
  para1.child_ids.push_back(text1.id);
  para1.child_ids.push_back(text2.id);

  ui::AXNodeData para2;
  para2.id = 12;
  para2.role = ax::mojom::Role::kParagraph;
  para2.child_ids.push_back(text4.id);
  para2.child_ids.push_back(text5.id);
  para2.AddState(ax::mojom::State::kIgnored);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(para1.id);
  root.child_ids.push_back(text3.id);
  root.child_ids.push_back(para2.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, para1, text1, text2, text3, para2, text4,
                           text5),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_obj = manager->GetRoot();
  BrowserAccessibility* para_obj = root_obj->PlatformGetChild(0);
  BrowserAccessibility* text1_obj = manager->GetFromID(111);
  BrowserAccessibility* text2_obj = manager->GetFromID(112);
  BrowserAccessibility* text3_obj = manager->GetFromID(113);
  BrowserAccessibility* para2_obj = manager->GetFromID(12);
  BrowserAccessibility* text4_obj = manager->GetFromID(114);
  BrowserAccessibility* text5_obj = root_obj->PlatformGetChild(2);

  // Leaf nodes should have no children.
  std::vector<gfx::NativeViewAccessible> descendants =
      text1_obj->GetDescendants();
  std::vector<gfx::NativeViewAccessible> expected_descendants = {};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text2_obj->GetDescendants();
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text3_obj->GetDescendants();
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text4_obj->GetDescendants();
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text5_obj->GetDescendants();
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = para2_obj->GetDescendants();
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  // Verify that para1 has two children (text1 and tex2).
  descendants = para_obj->GetDescendants();
  expected_descendants = {text1_obj->GetNativeViewAccessible(),
                          text2_obj->GetNativeViewAccessible()};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  // Calling GetChildNodeIds on the root should encompass the entire
  // right and left subtrees (para1, text1, text2, and text3).
  // para2 and its subtree should be ignored, except for text5
  descendants = root_obj->GetDescendants();
  expected_descendants = {para_obj->GetNativeViewAccessible(),
                          text1_obj->GetNativeViewAccessible(),
                          text2_obj->GetNativeViewAccessible(),
                          text3_obj->GetNativeViewAccessible(),
                          text5_obj->GetNativeViewAccessible()};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  manager.reset();
}

TEST_F(BrowserAccessibilityTest, PlatformChildIterator) {
  // (i) => node is ignored
  // Parent Tree
  // 1
  // |__________
  // |     |   |
  // 2(i)  3   4
  // |__________________________________
  // |              |      |           |
  // 5              6      7(i)        8(i)
  // |              |      |________
  // |              |      |       |
  // Child Tree     9(i)   10(i)   11
  //                |      |____
  //                |      |   |
  //                12(i)  13  14
  // Child Tree
  // 1
  // |_________
  // |    |   |
  // 2    3   4
  //      |
  //      5
  ui::AXTreeID parent_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeID child_tree_id = ui::AXTreeID::CreateNewAXTreeID();

  ui::AXTreeUpdate parent_tree_update;
  parent_tree_update.tree_data.tree_id = parent_tree_id;
  parent_tree_update.has_tree_data = true;
  parent_tree_update.root_id = 1;
  parent_tree_update.nodes.resize(14);
  parent_tree_update.nodes[0].id = 1;
  parent_tree_update.nodes[0].child_ids = {2, 3, 4};

  parent_tree_update.nodes[1].id = 2;
  parent_tree_update.nodes[1].child_ids = {5, 6, 7, 8};
  parent_tree_update.nodes[1].AddState(ax::mojom::State::kIgnored);

  parent_tree_update.nodes[2].id = 3;
  parent_tree_update.nodes[3].id = 4;

  parent_tree_update.nodes[4].id = 5;
  parent_tree_update.nodes[4].AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeId, child_tree_id.ToString());

  parent_tree_update.nodes[5].id = 6;
  parent_tree_update.nodes[5].child_ids = {9};

  parent_tree_update.nodes[6].id = 7;
  parent_tree_update.nodes[6].child_ids = {10, 11};
  parent_tree_update.nodes[6].AddState(ax::mojom::State::kIgnored);

  parent_tree_update.nodes[7].id = 8;
  parent_tree_update.nodes[7].AddState(ax::mojom::State::kIgnored);

  parent_tree_update.nodes[8].id = 9;
  parent_tree_update.nodes[8].child_ids = {12};
  parent_tree_update.nodes[8].AddState(ax::mojom::State::kIgnored);

  parent_tree_update.nodes[9].id = 10;
  parent_tree_update.nodes[9].child_ids = {13, 14};
  parent_tree_update.nodes[9].AddState(ax::mojom::State::kIgnored);

  parent_tree_update.nodes[10].id = 11;

  parent_tree_update.nodes[11].id = 12;
  parent_tree_update.nodes[11].AddState(ax::mojom::State::kIgnored);

  parent_tree_update.nodes[12].id = 13;

  parent_tree_update.nodes[13].id = 14;

  ui::AXTreeUpdate child_tree_update;
  child_tree_update.tree_data.tree_id = child_tree_id;
  child_tree_update.tree_data.parent_tree_id = parent_tree_id;
  child_tree_update.has_tree_data = true;
  child_tree_update.root_id = 1;
  child_tree_update.nodes.resize(5);
  child_tree_update.nodes[0].id = 1;
  child_tree_update.nodes[0].child_ids = {2, 3, 4};

  child_tree_update.nodes[1].id = 2;

  child_tree_update.nodes[2].id = 3;
  child_tree_update.nodes[2].child_ids = {5};

  child_tree_update.nodes[3].id = 4;

  child_tree_update.nodes[4].id = 5;

  std::unique_ptr<BrowserAccessibilityManager> parent_manager(
      BrowserAccessibilityManager::Create(
          parent_tree_update, test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  std::unique_ptr<BrowserAccessibilityManager> child_manager(
      BrowserAccessibilityManager::Create(
          child_tree_update, test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_obj = parent_manager->GetRoot();
  // Test traversal
  // PlatformChildren(root_obj) = {5, 6, 13, 15, 11, 3, 4}
  BrowserAccessibility::PlatformChildIterator platform_iterator =
      root_obj->PlatformChildrenBegin();
  EXPECT_EQ(5, platform_iterator->GetId());
  EXPECT_EQ(nullptr, platform_iterator->PlatformGetPreviousSibling());
  EXPECT_EQ(1u, platform_iterator->PlatformChildCount());

  // Test Child-Tree Traversal
  BrowserAccessibility* child_tree_root =
      platform_iterator->PlatformGetFirstChild();
  EXPECT_EQ(1, child_tree_root->GetId());
  BrowserAccessibility::PlatformChildIterator child_tree_iterator =
      child_tree_root->PlatformChildrenBegin();

  EXPECT_EQ(2, child_tree_iterator->GetId());
  ++child_tree_iterator;
  EXPECT_EQ(3, child_tree_iterator->GetId());
  ++child_tree_iterator;
  EXPECT_EQ(4, child_tree_iterator->GetId());

  ++platform_iterator;
  EXPECT_EQ(6, platform_iterator->GetId());

  ++platform_iterator;
  EXPECT_EQ(13, platform_iterator->GetId());

  ++platform_iterator;
  EXPECT_EQ(14, platform_iterator->GetId());

  --platform_iterator;
  EXPECT_EQ(13, platform_iterator->GetId());

  --platform_iterator;
  EXPECT_EQ(6, platform_iterator->GetId());

  ++platform_iterator;
  EXPECT_EQ(13, platform_iterator->GetId());

  ++platform_iterator;
  EXPECT_EQ(14, platform_iterator->GetId());

  ++platform_iterator;
  EXPECT_EQ(11, platform_iterator->GetId());

  ++platform_iterator;
  EXPECT_EQ(3, platform_iterator->GetId());

  ++platform_iterator;
  EXPECT_EQ(4, platform_iterator->GetId());

  ++platform_iterator;
  EXPECT_EQ(root_obj->PlatformChildrenEnd(), platform_iterator);

  // test empty list
  // PlatformChildren(3) = {}
  BrowserAccessibility* node2 = parent_manager->GetFromID(3);
  platform_iterator = node2->PlatformChildrenBegin();
  EXPECT_EQ(node2->PlatformChildrenEnd(), platform_iterator);

  // empty list from ignored node
  // PlatformChildren(8) = {}
  BrowserAccessibility* node8 = parent_manager->GetFromID(8);
  platform_iterator = node8->PlatformChildrenBegin();
  EXPECT_EQ(node8->PlatformChildrenEnd(), platform_iterator);

  // non-empty list from ignored node
  // PlatformChildren(10) = {13, 15}
  BrowserAccessibility* node10 = parent_manager->GetFromID(10);
  platform_iterator = node10->PlatformChildrenBegin();
  EXPECT_EQ(13, platform_iterator->GetId());

  // Two UnignoredChildIterators from the same parent at the same position
  // should be equivalent, even in end position.
  platform_iterator = root_obj->PlatformChildrenBegin();
  BrowserAccessibility::PlatformChildIterator platform_iterator2 =
      root_obj->PlatformChildrenBegin();
  auto end = root_obj->PlatformChildrenEnd();
  while (platform_iterator != end) {
    ASSERT_EQ(platform_iterator, platform_iterator2);
    ++platform_iterator;
    ++platform_iterator2;
  }
  ASSERT_EQ(platform_iterator, platform_iterator2);
}
#endif  // defined(OS_WIN) || BUILDFLAG(USE_ATK)

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRect) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("Hello, world.");
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 29, 18);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.SetName("Hello, ");
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.relative_bounds.bounds = gfx::RectF(100, 100, 29, 9);
  inline_text1.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(6);
  character_offsets1.push_back(11);
  character_offsets1.push_back(16);
  character_offsets1.push_back(21);
  character_offsets1.push_back(26);
  character_offsets1.push_back(29);
  character_offsets1.push_back(29);
  inline_text1.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets1);
  static_text.child_ids.push_back(3);

  ui::AXNodeData inline_text2;
  inline_text2.id = 4;
  inline_text2.SetName("world.");
  inline_text2.role = ax::mojom::Role::kInlineTextBox;
  inline_text2.relative_bounds.bounds = gfx::RectF(100, 109, 28, 9);
  inline_text2.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets2;
  character_offsets2.push_back(5);
  character_offsets2.push_back(10);
  character_offsets2.push_back(15);
  character_offsets2.push_back(20);
  character_offsets2.push_back(25);
  character_offsets2.push_back(28);
  inline_text2.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets2);
  static_text.child_ids.push_back(4);

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text1, inline_text2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

#ifdef OS_ANDROID
  // Android disallows getting inner text from root accessibility nodes.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 1, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#else
  // Validate the bounding box of 'H' from root.
  EXPECT_EQ(gfx::Rect(100, 100, 6, 9).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 1, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#endif

  // Validate the bounding box of 'H' from static text.
  EXPECT_EQ(gfx::Rect(100, 100, 6, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 1, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounding box of 'Hello' from static text.
  EXPECT_EQ(gfx::Rect(100, 100, 26, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 5, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounding box of 'Hello, world.' from static text.
  EXPECT_EQ(gfx::Rect(100, 100, 29, 18).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 13, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

#ifdef OS_ANDROID
  // Android disallows getting inner text from root accessibility nodes.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 13, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#else
  // Validate the bounding box of 'Hello, world.' from root.
  EXPECT_EQ(gfx::Rect(100, 100, 29, 18).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 13, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#endif
}

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRectMultiElement) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("ABC");
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.relative_bounds.bounds = gfx::RectF(0, 20, 33, 9);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.SetName("ABC");
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.relative_bounds.bounds = gfx::RectF(0, 20, 33, 9);
  inline_text1.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets{10, 21, 33};
  inline_text1.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets);
  static_text.child_ids.push_back(3);

  ui::AXNodeData static_text2;
  static_text2.id = 4;
  static_text2.SetName("ABC");
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.relative_bounds.bounds = gfx::RectF(10, 40, 33, 9);
  root.child_ids.push_back(4);

  ui::AXNodeData inline_text2;
  inline_text2.id = 5;
  inline_text2.SetName("ABC");
  inline_text2.role = ax::mojom::Role::kInlineTextBox;
  inline_text2.relative_bounds.bounds = gfx::RectF(10, 40, 33, 9);
  inline_text2.SetTextDirection(ax::mojom::TextDirection::kLtr);
  inline_text2.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets);
  static_text2.child_ids.push_back(5);

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text1, static_text2,
                           inline_text2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);
  BrowserAccessibility* static_text_accessible2 =
      root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, static_text_accessible);

  // Validate the bounds of 'ABC' on the first line.
  EXPECT_EQ(gfx::Rect(0, 20, 33, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 3, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounds of only 'AB' on the first line.
  EXPECT_EQ(gfx::Rect(0, 20, 21, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 2, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounds of only 'BC' on the first line.
  EXPECT_EQ(gfx::Rect(10, 20, 23, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    1, 3, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounds of 'ABC' on the second line.
  EXPECT_EQ(gfx::Rect(10, 40, 33, 9).ToString(),
            static_text_accessible2
                ->GetInnerTextRangeBoundsRect(
                    0, 3, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

#ifdef OS_ANDROID
  // Android disallows getting inner text from accessibility root nodes.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 6, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Android disallows getting inner text from accessibility root nodes.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    2, 4, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#else
  // Validate the bounds of 'ABCABC' from both lines.
  EXPECT_EQ(gfx::Rect(0, 20, 43, 29).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 6, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounds of 'CA' from both lines.
  EXPECT_EQ(gfx::Rect(10, 20, 23, 29).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    2, 4, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#endif
}

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRectBiDi) {
  // In this example, we assume that the string "123abc" is rendered with "123"
  // going left-to-right and "abc" going right-to-left. In other words,
  // on-screen it would look like "123cba".
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("123abc");
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 60, 20);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.SetName("123");
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.relative_bounds.bounds = gfx::RectF(100, 100, 30, 20);
  inline_text1.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(10);  // 0
  character_offsets1.push_back(20);  // 1
  character_offsets1.push_back(30);  // 2
  inline_text1.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets1);
  static_text.child_ids.push_back(3);

  ui::AXNodeData inline_text2;
  inline_text2.id = 4;
  inline_text2.SetName("abc");
  inline_text2.role = ax::mojom::Role::kInlineTextBox;
  inline_text2.relative_bounds.bounds = gfx::RectF(130, 100, 30, 20);
  inline_text2.SetTextDirection(ax::mojom::TextDirection::kRtl);
  std::vector<int32_t> character_offsets2;
  character_offsets2.push_back(10);
  character_offsets2.push_back(20);
  character_offsets2.push_back(30);
  inline_text2.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets2);
  static_text.child_ids.push_back(4);

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text1, inline_text2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

  EXPECT_EQ(gfx::Rect(100, 100, 60, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 6, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 10, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 1, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 30, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 3, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  EXPECT_EQ(gfx::Rect(150, 100, 10, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    3, 4, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  EXPECT_EQ(gfx::Rect(130, 100, 30, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    3, 6, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // This range is only two characters, but because of the direction switch
  // the bounds are as wide as four characters.
  EXPECT_EQ(gfx::Rect(120, 100, 40, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    2, 4, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
}

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRectScrolledWindow) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 25);
  root.AddIntAttribute(ax::mojom::IntAttribute::kScrollY, 50);
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("ABC");
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 16, 9);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text;
  inline_text.id = 3;
  inline_text.SetName("ABC");
  inline_text.role = ax::mojom::Role::kInlineTextBox;
  inline_text.relative_bounds.bounds = gfx::RectF(100, 100, 16, 9);
  inline_text.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(6);   // 0
  character_offsets1.push_back(11);  // 1
  character_offsets1.push_back(16);  // 2
  inline_text.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets1);
  static_text.child_ids.push_back(3);

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

  if (browser_accessibility_manager
          ->UseRootScrollOffsetsWhenComputingBounds()) {
    EXPECT_EQ(gfx::Rect(75, 50, 16, 9).ToString(),
              static_text_accessible
                  ->GetInnerTextRangeBoundsRect(
                      0, 3, ui::AXCoordinateSystem::kRootFrame,
                      ui::AXClippingBehavior::kUnclipped)
                  .ToString());
  } else {
    EXPECT_EQ(gfx::Rect(100, 100, 16, 9).ToString(),
              static_text_accessible
                  ->GetInnerTextRangeBoundsRect(
                      0, 3, ui::AXCoordinateSystem::kRootFrame,
                      ui::AXClippingBehavior::kUnclipped)
                  .ToString());
  }
}

TEST_F(BrowserAccessibilityTest, GetAuthorUniqueId) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.html_attributes.push_back(std::make_pair("id", "my_html_id"));

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root), test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));
  ASSERT_NE(nullptr, browser_accessibility_manager.get());

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);

  ASSERT_EQ(base::WideToUTF16(L"my_html_id"),
            root_accessible->GetAuthorUniqueId());
}

TEST_F(BrowserAccessibilityTest, NextWordPositionWithHypertext) {
  // Build a tree simulating an INPUT control with placeholder text.
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {2};

  ui::AXNodeData input;
  input.id = 2;
  input.role = ax::mojom::Role::kTextField;
  input.child_ids = {3};
  input.SetName("Search the web");

  ui::AXNodeData static_text;
  static_text.id = 3;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.child_ids = {4};
  static_text.SetName("Search the web");

  ui::AXNodeData inline_text;
  inline_text.id = 4;
  inline_text.role = ax::mojom::Role::kInlineTextBox;
  inline_text.SetName("Search the web");
  inline_text.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                  {0, 7, 11});
  inline_text.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                  {6, 10, 14});

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, input, static_text, inline_text),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));
  ASSERT_NE(nullptr, browser_accessibility_manager.get());

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_NE(0u, root_accessible->InternalChildCount());
  BrowserAccessibility* input_accessible = root_accessible->InternalGetChild(0);
  ASSERT_NE(nullptr, input_accessible);

  // Create a text position at offset 0 in the input control
  auto position = input_accessible->CreatePositionAt(
      0, ax::mojom::TextAffinity::kDownstream);

  // On platforms that expose IA2 or ATK hypertext, moving by word should have
  // no effect, i.e. return the same position, since the visible text is just a
  // placeholder and should not appear in the input field's hypertext.
  auto next_word_start = position->CreateNextWordStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
  ASSERT_TRUE(next_word_start->IsTextPosition());
  if (position->MaxTextOffset() == 0) {
    EXPECT_EQ(*position, *next_word_start);
  } else {
    EXPECT_EQ(
        "TextPosition anchor_id=2 text_offset=7 affinity=downstream "
        "annotated_text=Search <t>he web",
        next_word_start->ToString());
  }

  auto next_word_end = position->CreateNextWordEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
  ASSERT_TRUE(next_word_end->IsTextPosition());
  if (position->MaxTextOffset() == 0) {
    EXPECT_EQ(*position, *next_word_end);
  } else {
    EXPECT_EQ(
        "TextPosition anchor_id=2 text_offset=6 affinity=downstream "
        "annotated_text=Search< >the web",
        next_word_end->ToString());
  }
}

}  // namespace content
