// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility.h"

#include "build/build_config.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#endif

namespace content {

namespace {

ui::BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
    const ui::AXTreeUpdate& initial_tree,
    ui::AXNodeIdDelegate& node_id_delegate,
    ui::AXPlatformTreeManagerDelegate* delegate) {
#if BUILDFLAG(IS_ANDROID)
  return content::BrowserAccessibilityManagerAndroid::Create(
      initial_tree, node_id_delegate, delegate);
#else
  return ui::BrowserAccessibilityManager::Create(initial_tree, node_id_delegate,
                                                 delegate);
#endif
}
}  // namespace

using RetargetEventType = ui::AXTreeManager::RetargetEventType;

class BrowserAccessibilityTest : public ::testing::Test {
 public:
  BrowserAccessibilityTest();

  BrowserAccessibilityTest(const BrowserAccessibilityTest&) = delete;
  BrowserAccessibilityTest& operator=(const BrowserAccessibilityTest&) = delete;

  ~BrowserAccessibilityTest() override;

 protected:
  std::unique_ptr<ui::TestAXPlatformTreeManagerDelegate>
      test_browser_accessibility_delegate_;
  ui::TestAXNodeIdDelegate node_id_delegate_;

 private:
  void SetUp() override;

  BrowserTaskEnvironment task_environment_;
};

BrowserAccessibilityTest::BrowserAccessibilityTest() {}

BrowserAccessibilityTest::~BrowserAccessibilityTest() = default;

void BrowserAccessibilityTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<ui::TestAXPlatformTreeManagerDelegate>();
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

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root, para1, text1), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  ui::BrowserAccessibility* root_obj = manager->GetBrowserAccessibilityRoot();
  EXPECT_FALSE(root_obj->IsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());

  ui::BrowserAccessibility* para_obj = root_obj->PlatformGetChild(0);
  EXPECT_TRUE(para_obj->CanFireEvents());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(para_obj->IsLeaf());
#else
  EXPECT_FALSE(para_obj->IsLeaf());
#endif

  ui::BrowserAccessibility* text_obj = manager->GetFromID(111);
  EXPECT_TRUE(text_obj->IsLeaf());
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(text_obj->CanFireEvents());
#endif
  ui::BrowserAccessibility* retarget =
      manager->RetargetBrowserAccessibilityForEvents(
          text_obj, RetargetEventType::RetargetEventTypeBlinkHover);
  EXPECT_TRUE(retarget->CanFireEvents());

  manager.reset();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(USE_ATK)
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
  parent_tree_update.nodes[4].AddChildTreeId(child_tree_id);

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

  std::unique_ptr<ui::BrowserAccessibilityManager> parent_manager(
      CreateBrowserAccessibilityManager(parent_tree_update, node_id_delegate_,
                                        nullptr));

  std::unique_ptr<ui::BrowserAccessibilityManager> child_manager(
      CreateBrowserAccessibilityManager(child_tree_update, node_id_delegate_,
                                        nullptr));

  ui::BrowserAccessibility* root_obj =
      parent_manager->GetBrowserAccessibilityRoot();
  // Test traversal
  // PlatformChildren(root_obj) = {5, 6, 13, 15, 11, 3, 4}
  ui::BrowserAccessibility::PlatformChildIterator platform_iterator =
      root_obj->PlatformChildrenBegin();
  EXPECT_EQ(5, platform_iterator->GetId());
  EXPECT_EQ(nullptr, platform_iterator->PlatformGetPreviousSibling());
  EXPECT_EQ(1u, platform_iterator->PlatformChildCount());

  // Test Child-Tree Traversal
  ui::BrowserAccessibility* child_tree_root =
      platform_iterator->PlatformGetFirstChild();
  EXPECT_EQ(1, child_tree_root->GetId());
  ui::BrowserAccessibility::PlatformChildIterator child_tree_iterator =
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
  ui::BrowserAccessibility* node2 = parent_manager->GetFromID(3);
  platform_iterator = node2->PlatformChildrenBegin();
  EXPECT_EQ(node2->PlatformChildrenEnd(), platform_iterator);

  // empty list from ignored node
  // PlatformChildren(8) = {}
  ui::BrowserAccessibility* node8 = parent_manager->GetFromID(8);
  platform_iterator = node8->PlatformChildrenBegin();
  EXPECT_EQ(node8->PlatformChildrenEnd(), platform_iterator);

  // non-empty list from ignored node
  // PlatformChildren(10) = {13, 15}
  ui::BrowserAccessibility* node10 = parent_manager->GetFromID(10);
  platform_iterator = node10->PlatformChildrenBegin();
  EXPECT_EQ(13, platform_iterator->GetId());

  // Two UnignoredChildIterators from the same parent at the same position
  // should be equivalent, even in end position.
  platform_iterator = root_obj->PlatformChildrenBegin();
  ui::BrowserAccessibility::PlatformChildIterator platform_iterator2 =
      root_obj->PlatformChildrenBegin();
  auto end = root_obj->PlatformChildrenEnd();
  while (platform_iterator != end) {
    ASSERT_EQ(platform_iterator, platform_iterator2);
    ++platform_iterator;
    ++platform_iterator2;
  }
  ASSERT_EQ(platform_iterator, platform_iterator2);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(USE_ATK)

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRect) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetName("Hello, world.");
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 29, 18);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.SetName("Hello, ");
  inline_text1.relative_bounds.bounds = gfx::RectF(100, 100, 29, 9);
  inline_text1.SetTextDirection(ax::mojom::WritingDirection::kLtr);
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
  inline_text2.role = ax::mojom::Role::kInlineTextBox;
  inline_text2.SetName("world.");
  inline_text2.relative_bounds.bounds = gfx::RectF(100, 109, 28, 9);
  inline_text2.SetTextDirection(ax::mojom::WritingDirection::kLtr);
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

  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root, static_text, inline_text1,
                                     inline_text2),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ui::BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);
  ui::BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

#if BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_ANDROID)
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

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRectPlainTextField) {
  // Text area with 'Hello' text
  // rootWebArea
  // ++textField
  // ++++genericContainer
  // ++++++staticText
  // ++++++++inlineTextBox
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData textarea;
  textarea.id = 2;
  textarea.role = ax::mojom::Role::kTextField;
  textarea.SetValue("Hello");
  textarea.relative_bounds.bounds = gfx::RectF(100, 100, 150, 20);
  root.child_ids.push_back(2);

  ui::AXNodeData container;
  container.id = 3;
  container.role = ax::mojom::Role::kGenericContainer;
  container.relative_bounds.bounds = textarea.relative_bounds.bounds;
  textarea.child_ids.push_back(3);

  ui::AXNodeData static_text;
  static_text.id = 4;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetName("Hello");
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 50, 10);
  container.child_ids.push_back(4);

  ui::AXNodeData inline_text1;
  inline_text1.id = 5;
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.SetName("Hello");
  inline_text1.relative_bounds.bounds = gfx::RectF(100, 100, 50, 10);
  inline_text1.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, {10, 20, 30, 40, 50});

  inline_text1.SetTextDirection(ax::mojom::WritingDirection::kLtr);
  static_text.child_ids.push_back(5);

  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root, textarea, container, static_text,
                                     inline_text1),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ui::BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);
  ui::BrowserAccessibility* textarea_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, textarea_accessible);

  // Validate the bounds of 'ell'.
  EXPECT_EQ(gfx::Rect(110, 100, 30, 10),
            textarea_accessible->GetInnerTextRangeBoundsRect(
                1, 4, ui::AXCoordinateSystem::kRootFrame,
                ui::AXClippingBehavior::kUnclipped));
}

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRectMultiElement) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetName("ABC");
  static_text.relative_bounds.bounds = gfx::RectF(0, 20, 33, 9);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.SetName("ABC");
  inline_text1.relative_bounds.bounds = gfx::RectF(0, 20, 33, 9);
  inline_text1.SetTextDirection(ax::mojom::WritingDirection::kLtr);
  std::vector<int32_t> character_offsets{10, 21, 33};
  inline_text1.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets);
  static_text.child_ids.push_back(3);

  ui::AXNodeData static_text2;
  static_text2.id = 4;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetName("ABC");
  static_text2.relative_bounds.bounds = gfx::RectF(10, 40, 33, 9);
  root.child_ids.push_back(4);

  ui::AXNodeData inline_text2;
  inline_text2.id = 5;
  inline_text2.role = ax::mojom::Role::kInlineTextBox;
  inline_text2.SetName("ABC");
  inline_text2.relative_bounds.bounds = gfx::RectF(10, 40, 33, 9);
  inline_text2.SetTextDirection(ax::mojom::WritingDirection::kLtr);
  inline_text2.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets);
  static_text2.child_ids.push_back(5);

  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root, static_text, inline_text1,
                                     static_text2, inline_text2),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ui::BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);
  ui::BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);
  ui::BrowserAccessibility* static_text_accessible2 =
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

#if BUILDFLAG(IS_ANDROID)
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
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetName("123abc");
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 60, 20);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.SetName("123");
  inline_text1.relative_bounds.bounds = gfx::RectF(100, 100, 30, 20);
  inline_text1.SetTextDirection(ax::mojom::WritingDirection::kLtr);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(10);  // 0
  character_offsets1.push_back(20);  // 1
  character_offsets1.push_back(30);  // 2
  inline_text1.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets1);
  static_text.child_ids.push_back(3);

  ui::AXNodeData inline_text2;
  inline_text2.id = 4;
  inline_text2.role = ax::mojom::Role::kInlineTextBox;
  inline_text2.SetName("abc");
  inline_text2.relative_bounds.bounds = gfx::RectF(130, 100, 30, 20);
  inline_text2.SetTextDirection(ax::mojom::WritingDirection::kRtl);
  std::vector<int32_t> character_offsets2;
  character_offsets2.push_back(10);
  character_offsets2.push_back(20);
  character_offsets2.push_back(30);
  inline_text2.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets2);
  static_text.child_ids.push_back(4);

  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root, static_text, inline_text1,
                                     inline_text2),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ui::BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);
  ui::BrowserAccessibility* static_text_accessible =
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
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetName("ABC");
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 16, 9);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text;
  inline_text.id = 3;
  inline_text.role = ax::mojom::Role::kInlineTextBox;
  inline_text.SetName("ABC");
  inline_text.relative_bounds.bounds = gfx::RectF(100, 100, 16, 9);
  inline_text.SetTextDirection(ax::mojom::WritingDirection::kLtr);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(6);   // 0
  character_offsets1.push_back(11);  // 1
  character_offsets1.push_back(16);  // 2
  inline_text.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets1);
  static_text.child_ids.push_back(3);

  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root, static_text, inline_text),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  browser_accessibility_manager
      ->SetUseRootScrollOffsetsWhenComputingBoundsForTesting(true);

  ui::BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);
  ui::BrowserAccessibility* static_text_accessible =
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
  root.AddStringAttribute(ax::mojom::StringAttribute::kHtmlId, "my_html_id");

  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));
  ASSERT_NE(nullptr, browser_accessibility_manager.get());

  ui::BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);

  ASSERT_EQ(u"my_html_id", root_accessible->GetAuthorUniqueId());
}

TEST_F(BrowserAccessibilityTest, NextWordPositionWithHypertext) {
  // Build a tree simulating an INPUT control with placeholder text.
  ui::AXNodeData root;
  root.id = 1;
  ui::AXNodeData input;
  input.id = 2;
  ui::AXNodeData text_container;
  text_container.id = 3;
  ui::AXNodeData static_text;
  static_text.id = 4;
  ui::AXNodeData inline_text;
  inline_text.id = 5;

  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {input.id};

  input.role = ax::mojom::Role::kTextField;
  input.AddState(ax::mojom::State::kEditable);
  input.SetName("Search the web");
  input.child_ids = {text_container.id};

  text_container.role = ax::mojom::Role::kGenericContainer;
  text_container.child_ids = {static_text.id};

  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetName("Search the web");
  static_text.child_ids = {inline_text.id};

  inline_text.role = ax::mojom::Role::kInlineTextBox;
  inline_text.SetName("Search the web");
  inline_text.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                  {0, 7, 11});
  inline_text.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                  {6, 10, 14});

  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root, input, text_container, static_text,
                                     inline_text),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));
  ASSERT_NE(nullptr, browser_accessibility_manager.get());

  ui::BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_NE(0u, root_accessible->InternalChildCount());
  ui::BrowserAccessibility* input_accessible =
      root_accessible->InternalGetChild(0);
  ASSERT_NE(nullptr, input_accessible);

  // Create a text position at offset 0 in the input control
  ui::BrowserAccessibility::AXPosition position =
      input_accessible->CreateTextPositionAt(0);

  // On platforms that expose IA2 or ATK hypertext, moving by word should work
  // the same as if the value of the text field is equal to the placeholder
  // text.
  //
  // This is because visually the placeholder text appears in the text field in
  // the same location as its value, and the user should be able to read it
  // using standard screen reader commands, such as "read current word" and
  // "read current line". Only once the user starts typing should the
  // placeholder disappear.

  ui::BrowserAccessibility::AXPosition next_word_start =
      position->CreateNextWordStartPosition(
          {ui::AXBoundaryBehavior::kCrossBoundary,
           ui::AXBoundaryDetection::kDontCheckInitialPosition});
  if (position->MaxTextOffset() == 0) {
    EXPECT_TRUE(next_word_start->IsNullPosition());
  } else {
    EXPECT_EQ(
        "TextPosition anchor_id=2 text_offset=7 affinity=downstream "
        "annotated_text=Search <t>he web",
        next_word_start->ToString());
  }

  ui::BrowserAccessibility::AXPosition next_word_end =
      position->CreateNextWordEndPosition(
          {ui::AXBoundaryBehavior::kCrossBoundary,
           ui::AXBoundaryDetection::kDontCheckInitialPosition});
  if (position->MaxTextOffset() == 0) {
    EXPECT_TRUE(next_word_end->IsNullPosition());
  } else {
    EXPECT_EQ(
        "TextPosition anchor_id=2 text_offset=6 affinity=downstream "
        "annotated_text=Search< >the web",
        next_word_end->ToString());
  }
}

TEST_F(BrowserAccessibilityTest, GetIndexInParent) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {2};

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetName("ABC");

  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root, static_text), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));
  ASSERT_NE(nullptr, browser_accessibility_manager.get());

  ui::BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, root_accessible);
  // Should be nullopt for kRootWebArea since it doesn't have a calculated
  // index.
  EXPECT_FALSE(root_accessible->GetIndexInParent().has_value());
  ui::BrowserAccessibility* child_accessible =
      root_accessible->InternalGetChild(0);
  ASSERT_NE(nullptr, child_accessible);
  // Returns the index calculated in AXNode.
  EXPECT_EQ(0u, child_accessible->GetIndexInParent());
}

TEST_F(BrowserAccessibilityTest, CreatePositionAt) {
  ui::AXNodeData root_1;
  root_1.id = 1;
  root_1.role = ax::mojom::Role::kRootWebArea;
  root_1.child_ids = {2};

  ui::AXNodeData gc_2;
  gc_2.id = 2;
  gc_2.role = ax::mojom::Role::kGenericContainer;
  gc_2.child_ids = {3};

  ui::AXNodeData text_3;
  text_3.id = 3;
  text_3.role = ax::mojom::Role::kStaticText;
  text_3.SetName("text");

  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(CreateBrowserAccessibilityManager(
          MakeAXTreeUpdateForTesting(root_1, gc_2, text_3), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));
  ASSERT_NE(nullptr, browser_accessibility_manager.get());

  ui::BrowserAccessibility* gc_accessible =
      browser_accessibility_manager->GetBrowserAccessibilityRoot()
          ->PlatformGetChild(0);
  ASSERT_NE(nullptr, gc_accessible);

  ui::BrowserAccessibility::AXPosition pos = gc_accessible->CreatePositionAt(0);
  EXPECT_TRUE(pos->IsTreePosition());

  ASSERT_EQ(1U, gc_accessible->InternalChildCount());
#if BUILDFLAG(IS_ANDROID)
  // On Android, nodes with only static text can drop their children.
  ASSERT_EQ(0U, gc_accessible->PlatformChildCount());
#else
  ASSERT_EQ(1U, gc_accessible->PlatformChildCount());
  ui::BrowserAccessibility* text_accessible =
      gc_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, text_accessible);

  pos = text_accessible->CreatePositionAt(0);
  EXPECT_TRUE(pos->IsTextPosition());
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace content
