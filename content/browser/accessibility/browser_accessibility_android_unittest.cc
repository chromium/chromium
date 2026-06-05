// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_android.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/accessibility/ax_style_data.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/accessibility/web_contents_accessibility_android.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"
#include "ui/strings/grit/auto_image_annotation_strings.h"

namespace content {

namespace {
BrowserAccessibilityManagerAndroid* ToBrowserAccessibilityManagerAndroid(
    ui::BrowserAccessibilityManager* manager) {
  return static_cast<BrowserAccessibilityManagerAndroid*>(manager);
}

// A trivial AXNodeIdDelegate that maintains unique AXNodeIDs in a container.
class UniqueAXNodeIdDelegate : public ui::AXNodeIdDelegate {
 public:
  ui::AXPlatformNodeId GetOrCreateAXNodeUniqueId(
      ui::AXNodeID ax_node_id) override {
    auto [iter, inserted] =
        unique_ids_.try_emplace(ax_node_id, ui::AXUniqueId::CreateInvalid());
    if (inserted) {
      iter->second = ui::AXUniqueId::Create();
    }
    return iter->second;
  }
  void OnAXNodeDeleted(ui::AXNodeID ax_node_id) override {
    unique_ids_.erase(ax_node_id);
  }

 private:
  absl::flat_hash_map<ui::AXNodeID, ui::AXUniqueId> unique_ids_;
};
}  // namespace

using RetargetEventType = ui::AXTreeManager::RetargetEventType;
using RangePairs = AXStyleData::RangePairs;

using ::testing::Eq;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class MockContentClient : public TestContentClient {
 public:
  std::u16string GetLocalizedString(int message_id) override {
    switch (message_id) {
      case IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION:
        return u"Unlabeled image";
      case IDS_AX_IMAGE_ELIGIBLE_FOR_ANNOTATION_ANDROID:
        return u"This image isn't labeled. Double tap on the more options "
               u"button at the top of the browser to get image descriptions.";
      case IDS_AX_IMAGE_ANNOTATION_PENDING:
        return u"Getting description...";
      case IDS_AX_IMAGE_ANNOTATION_ADULT:
        return u"Appears to contain adult content. No description available.";
      case IDS_AX_IMAGE_ANNOTATION_NO_DESCRIPTION:
        return u"No description available.";
      default:
        return std::u16string();
    }
  }
};

class MockWebContentsAccessibilityAndroid
    : public WebContentsAccessibilityAndroid {
 public:
  MockWebContentsAccessibilityAndroid() {}
  explicit MockWebContentsAccessibilityAndroid(int64_t ax_tree_update_ptr)
      : WebContentsAccessibilityAndroid(ax_tree_update_ptr) {}

  BrowserAccessibilityAndroid* GetAXFromUniqueIDForTesting(
      int32_t unique_id) const {
    return GetAXFromUniqueID(unique_id);
  }
};

class BrowserAccessibilityAndroidTest : public ::testing::Test {
 public:
  BrowserAccessibilityAndroidTest();

  BrowserAccessibilityAndroidTest(const BrowserAccessibilityAndroidTest&) =
      delete;
  BrowserAccessibilityAndroidTest& operator=(
      const BrowserAccessibilityAndroidTest&) = delete;

  ~BrowserAccessibilityAndroidTest() override;

  std::unique_ptr<ui::BrowserAccessibilityManager> CreateManager(
      const ui::AXTreeUpdate& tree_update) {
    return std::unique_ptr<ui::BrowserAccessibilityManager>(
        BrowserAccessibilityManagerAndroid::Create(
            tree_update, node_id_delegate_,
            test_browser_accessibility_delegate_.get()));
  }

 protected:
  static const ui::AXNodeID ROOT_ID = 100;

  std::unique_ptr<ui::TestAXPlatformTreeManagerDelegate>
      test_browser_accessibility_delegate_;
  ui::TestAXNodeIdDelegate node_id_delegate_;
  MockWebContentsAccessibilityAndroid mock_web_contents_accessibility_android_;

 private:
  void SetUp() override;
  MockContentClient client_;

  // This is needed to prevent a DCHECK failure when OnAccessibilityApiUsage
  // is called in BrowserAccessibility::GetRole.
  content::BrowserTaskEnvironment task_environment_;
};

BrowserAccessibilityAndroidTest::BrowserAccessibilityAndroidTest() = default;

BrowserAccessibilityAndroidTest::~BrowserAccessibilityAndroidTest() = default;

void BrowserAccessibilityAndroidTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<ui::TestAXPlatformTreeManagerDelegate>();
  test_browser_accessibility_delegate_->SetWebContentsAccessibility(
      &mock_web_contents_accessibility_android_);
  SetContentClient(&client_);
}

TEST_F(BrowserAccessibilityAndroidTest, TestRetargetTextOnly) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName("Hello, world");

  ui::AXNodeData para1;
  para1.id = 11;
  para1.role = ax::mojom::Role::kParagraph;
  para1.child_ids = {text1.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {para1.id};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, para1, text1), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  ui::BrowserAccessibility* root_obj = manager->GetBrowserAccessibilityRoot();
  EXPECT_FALSE(root_obj->IsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());
  ui::BrowserAccessibility* para_obj = root_obj->PlatformGetChild(0);
  EXPECT_TRUE(para_obj->IsLeaf());
  EXPECT_TRUE(para_obj->CanFireEvents());
  ui::BrowserAccessibility* text_obj = manager->GetFromID(111);
  EXPECT_TRUE(text_obj->IsLeaf());
  EXPECT_FALSE(text_obj->CanFireEvents());
  ui::BrowserAccessibility* updated =
      manager->RetargetBrowserAccessibilityForEvents(
          text_obj, RetargetEventType::RetargetEventTypeBlinkHover);
  // |updated| should be the paragraph.
  EXPECT_EQ(11, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  manager.reset();
}

TEST_F(BrowserAccessibilityAndroidTest, TestRetargetHeading) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData heading1;
  heading1.id = 11;
  heading1.role = ax::mojom::Role::kHeading;
  heading1.SetName("heading");
  heading1.child_ids = {text1.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {heading1.id};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, heading1, text1), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  ui::BrowserAccessibility* root_obj = manager->GetBrowserAccessibilityRoot();
  EXPECT_FALSE(root_obj->IsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());
  ui::BrowserAccessibility* heading_obj = root_obj->PlatformGetChild(0);
  EXPECT_TRUE(heading_obj->IsLeaf());
  EXPECT_TRUE(heading_obj->CanFireEvents());
  ui::BrowserAccessibility* text_obj = manager->GetFromID(111);
  EXPECT_TRUE(text_obj->IsLeaf());
  EXPECT_FALSE(text_obj->CanFireEvents());
  ui::BrowserAccessibility* updated =
      manager->RetargetBrowserAccessibilityForEvents(
          text_obj, RetargetEventType::RetargetEventTypeBlinkHover);
  // |updated| should be the heading.
  EXPECT_EQ(11, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  manager.reset();
}

TEST_F(BrowserAccessibilityAndroidTest, TestRetargetFocusable) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData para1;
  para1.id = 11;
  para1.role = ax::mojom::Role::kParagraph;
  para1.AddState(ax::mojom::State::kFocusable);
  para1.SetName("focusable");
  para1.child_ids = {text1.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {para1.id};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, para1, text1), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  ui::BrowserAccessibility* root_obj = manager->GetBrowserAccessibilityRoot();
  EXPECT_FALSE(root_obj->IsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());
  ui::BrowserAccessibility* para_obj = root_obj->PlatformGetChild(0);
  EXPECT_FALSE(para_obj->IsLeaf());
  EXPECT_TRUE(para_obj->CanFireEvents());
  ui::BrowserAccessibility* text_obj = manager->GetFromID(111);
  EXPECT_TRUE(text_obj->IsLeaf());
  EXPECT_TRUE(text_obj->CanFireEvents());
  ui::BrowserAccessibility* updated =
      manager->RetargetBrowserAccessibilityForEvents(
          text_obj, RetargetEventType::RetargetEventTypeBlinkHover);
  // |updated| should be the paragraph.
  EXPECT_EQ(11, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  manager.reset();
}

TEST_F(BrowserAccessibilityAndroidTest, TestRetargetInputControl) {
  // Build the tree that has a form with input time.
  // +rootWebArea
  // ++genericContainer
  // +++form
  // ++++labelText
  // +++++staticText
  // ++++inputTime
  // +++++genericContainer
  // ++++++staticText
  // ++++button
  // +++++staticText
  ui::AXNodeData label_text;
  label_text.id = 11111;
  label_text.role = ax::mojom::Role::kStaticText;
  label_text.SetName("label");

  ui::AXNodeData label;
  label.id = 1111;
  label.role = ax::mojom::Role::kLabelText;
  label.child_ids = {label_text.id};

  ui::AXNodeData input_text;
  input_text.id = 111211;
  input_text.role = ax::mojom::Role::kStaticText;
  input_text.SetName("input_text");

  ui::AXNodeData input_container;
  input_container.id = 11121;
  input_container.role = ax::mojom::Role::kGenericContainer;
  input_container.child_ids = {input_text.id};

  ui::AXNodeData input_time;
  input_time.id = 1112;
  input_time.role = ax::mojom::Role::kInputTime;
  input_time.AddState(ax::mojom::State::kFocusable);
  input_time.child_ids = {input_container.id};

  ui::AXNodeData button_text;
  button_text.id = 11131;
  button_text.role = ax::mojom::Role::kStaticText;
  button_text.AddState(ax::mojom::State::kFocusable);
  button_text.SetName("button");

  ui::AXNodeData button;
  button.id = 1113;
  button.role = ax::mojom::Role::kButton;
  button.child_ids = {button_text.id};

  ui::AXNodeData form;
  form.id = 111;
  form.role = ax::mojom::Role::kForm;
  form.child_ids = {label.id, input_time.id, button.id};

  ui::AXNodeData container;
  container.id = 11;
  container.role = ax::mojom::Role::kGenericContainer;
  container.child_ids = {form.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {container.id};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, container, form, label, label_text,
                                     input_time, input_container, input_text,
                                     button, button_text),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  ui::BrowserAccessibility* root_obj = manager->GetBrowserAccessibilityRoot();
  EXPECT_FALSE(root_obj->IsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());
  ui::BrowserAccessibility* label_obj = manager->GetFromID(label.id);
  EXPECT_TRUE(label_obj->IsLeaf());
  EXPECT_TRUE(label_obj->CanFireEvents());
  ui::BrowserAccessibility* label_text_obj = manager->GetFromID(label_text.id);
  EXPECT_TRUE(label_text_obj->IsLeaf());
  EXPECT_FALSE(label_text_obj->CanFireEvents());
  ui::BrowserAccessibility* updated =
      manager->RetargetBrowserAccessibilityForEvents(
          label_text_obj, RetargetEventType::RetargetEventTypeBlinkHover);
  EXPECT_EQ(label.id, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());

  ui::BrowserAccessibility* input_time_obj = manager->GetFromID(input_time.id);
  EXPECT_TRUE(input_time_obj->IsLeaf());
  EXPECT_TRUE(input_time_obj->CanFireEvents());
  ui::BrowserAccessibility* input_time_container_obj =
      manager->GetFromID(input_container.id);
  EXPECT_TRUE(input_time_container_obj->IsLeaf());
  EXPECT_FALSE(input_time_container_obj->CanFireEvents());
  updated = manager->RetargetBrowserAccessibilityForEvents(
      input_time_container_obj, RetargetEventType::RetargetEventTypeBlinkHover);
  EXPECT_EQ(input_time.id, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  ui::BrowserAccessibility* input_text_obj = manager->GetFromID(input_text.id);
  EXPECT_TRUE(input_text_obj->IsLeaf());
  EXPECT_FALSE(input_text_obj->CanFireEvents());
  updated = manager->RetargetBrowserAccessibilityForEvents(
      input_text_obj, RetargetEventType::RetargetEventTypeBlinkHover);
  EXPECT_EQ(input_time.id, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());

  ui::BrowserAccessibility* button_obj = manager->GetFromID(button.id);
  EXPECT_TRUE(button_obj->IsLeaf());
  EXPECT_TRUE(button_obj->CanFireEvents());
  ui::BrowserAccessibility* button_text_obj =
      manager->GetFromID(button_text.id);
  EXPECT_TRUE(button_text_obj->IsLeaf());
  EXPECT_FALSE(button_text_obj->CanFireEvents());
  updated = manager->RetargetBrowserAccessibilityForEvents(
      button_text_obj, RetargetEventType::RetargetEventTypeBlinkHover);
  EXPECT_EQ(button.id, updated->GetId());
  EXPECT_TRUE(updated->CanFireEvents());
  manager.reset();
}

TEST_F(BrowserAccessibilityAndroidTest, TestGetTextContent) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName("1Foo");

  ui::AXNodeData text2;
  text2.id = 112;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName("2Bar");

  ui::AXNodeData text3;
  text3.id = 113;
  text3.role = ax::mojom::Role::kStaticText;
  text3.SetName("3Baz");

  ui::AXNodeData container_para;
  container_para.id = 11;
  container_para.role = ax::mojom::Role::kGenericContainer;
  container_para.child_ids = {text1.id, text2.id, text3.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {container_para.id};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, container_para, text1, text2, text3),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));
  ui::BrowserAccessibility* container_obj = manager->GetFromID(11);
  // Default caller gets full text.
  EXPECT_EQ(u"1Foo2Bar3Baz", container_obj->GetTextContentUTF16());

  BrowserAccessibilityAndroid* node =
      static_cast<BrowserAccessibilityAndroid*>(container_obj);
  // No predicate returns all text.
  EXPECT_EQ(u"1Foo2Bar3Baz", node->GetSubstringTextContentUTF16(std::nullopt));
  // Non-empty predicate terminates after one text node.
  EXPECT_EQ(u"1Foo", node->GetSubstringTextContentUTF16(1));
  // Length of 5 not satisfied by one node.
  EXPECT_EQ(u"1Foo2Bar", node->GetSubstringTextContentUTF16(5));
  // Length of 10 not satisfied by two nodes.
  EXPECT_EQ(u"1Foo2Bar3Baz", node->GetSubstringTextContentUTF16(10));
  manager.reset();
}

TEST_F(BrowserAccessibilityAndroidTest,
       TestImageRoleDescription_UnlabeledImage) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(6);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3, 4, 5, 6};

  // Images with these annotation statuses should report "Unlabeled image"
  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationPending);

  tree.nodes[3].id = 4;
  tree.nodes[3].role = ax::mojom::Role::kImage;
  tree.nodes[3].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationEmpty);

  tree.nodes[4].id = 5;
  tree.nodes[4].role = ax::mojom::Role::kImage;
  tree.nodes[4].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationAdult);

  tree.nodes[5].id = 6;
  tree.nodes[5].role = ax::mojom::Role::kImage;
  tree.nodes[5].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed);

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityManagerAndroid* android_manager =
      ToBrowserAccessibilityManagerAndroid(manager.get());
  android_manager->set_allow_image_descriptions_for_testing(true);

  for (int child_index = 0;
       child_index < static_cast<int>(tree.nodes[0].child_ids.size());
       ++child_index) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(
            manager->GetBrowserAccessibilityRoot()->PlatformGetChild(
                child_index));

    EXPECT_EQ(u"Unlabeled image", child->GetAndroidRoleDescription());
  }
}

TEST_F(BrowserAccessibilityAndroidTest, TestImageRoleDescription_Empty) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(6);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3, 4, 5, 6};

  // Images with these annotation statuses should report nothing.
  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kNone);

  tree.nodes[3].id = 4;
  tree.nodes[3].role = ax::mojom::Role::kImage;
  tree.nodes[3].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme);

  tree.nodes[4].id = 5;
  tree.nodes[4].role = ax::mojom::Role::kImage;
  tree.nodes[4].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);

  tree.nodes[5].id = 6;
  tree.nodes[5].role = ax::mojom::Role::kImage;
  tree.nodes[5].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation);

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityManagerAndroid* android_manager =
      ToBrowserAccessibilityManagerAndroid(manager.get());
  android_manager->set_allow_image_descriptions_for_testing(true);

  for (int child_index = 0;
       child_index < static_cast<int>(tree.nodes[0].child_ids.size());
       ++child_index) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(
            manager->GetBrowserAccessibilityRoot()->PlatformGetChild(
                child_index));

    EXPECT_EQ(std::u16string(), child->GetAndroidRoleDescription());
  }
}

TEST_F(BrowserAccessibilityAndroidTest, TestImageInnerText_Eligible) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(3);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  tree.nodes[1].AddIntAttribute(
      ax::mojom::IntAttribute::kTextDirection,
      static_cast<int32_t>(ax::mojom::WritingDirection::kLtr));

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].SetName("image_name");
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  tree.nodes[2].AddIntAttribute(
      ax::mojom::IntAttribute::kTextDirection,
      static_cast<int32_t>(ax::mojom::WritingDirection::kRtl));

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityManagerAndroid* android_manager =
      ToBrowserAccessibilityManagerAndroid(manager.get());
  android_manager->set_allow_image_descriptions_for_testing(true);

  BrowserAccessibilityAndroid* image_ltr =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(0));

  EXPECT_EQ(
      u"This image isn't labeled. Double tap on the more options "
      u"button at the top of the browser to get image descriptions.",
      image_ltr->GetAndroidContentDescription());
  EXPECT_EQ(std::u16string(), image_ltr->GetAndroidSupplementalDescription());
  EXPECT_EQ(std::u16string(), image_ltr->GetTextContentUTF16());

  BrowserAccessibilityAndroid* image_rtl =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(1));

  EXPECT_EQ(u"image_name", image_rtl->GetAndroidContentDescription());
  EXPECT_EQ(
      u"This image isn't labeled. Double tap on the more options "
      u"button at the top of the browser to get image descriptions.",
      image_rtl->GetAndroidSupplementalDescription());
  EXPECT_EQ(std::u16string(), image_rtl->GetTextContentUTF16());
}

TEST_F(BrowserAccessibilityAndroidTest,
       TestImageInnerText_PendingAdultOrEmpty) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(5);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3, 4, 5};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationPending);

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationEmpty);

  tree.nodes[3].id = 4;
  tree.nodes[3].role = ax::mojom::Role::kImage;
  tree.nodes[3].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationAdult);

  tree.nodes[4].id = 5;
  tree.nodes[4].role = ax::mojom::Role::kImage;
  tree.nodes[4].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed);

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityManagerAndroid* android_manager =
      ToBrowserAccessibilityManagerAndroid(manager.get());
  android_manager->set_allow_image_descriptions_for_testing(true);

  BrowserAccessibilityAndroid* image_pending =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(0));

  BrowserAccessibilityAndroid* image_empty =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(1));

  BrowserAccessibilityAndroid* image_adult =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(2));

  BrowserAccessibilityAndroid* image_failed =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(3));

  EXPECT_EQ(u"Getting description...",
            image_pending->GetAndroidContentDescription());
  EXPECT_EQ(u"No description available.",
            image_empty->GetAndroidContentDescription());
  EXPECT_EQ(u"Appears to contain adult content. No description available.",
            image_adult->GetAndroidContentDescription());
  EXPECT_EQ(u"No description available.",
            image_failed->GetAndroidContentDescription());
}

TEST_F(BrowserAccessibilityAndroidTest, TestImageInnerText_Ineligible) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(5);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3, 4, 5};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kNone);

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].SetName("image_name");
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme);

  tree.nodes[3].id = 4;
  tree.nodes[3].role = ax::mojom::Role::kImage;
  tree.nodes[3].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);

  tree.nodes[4].id = 5;
  tree.nodes[4].role = ax::mojom::Role::kImage;
  tree.nodes[4].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation);

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityManagerAndroid* android_manager =
      ToBrowserAccessibilityManagerAndroid(manager.get());
  android_manager->set_allow_image_descriptions_for_testing(true);

  BrowserAccessibilityAndroid* image_none =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(0));

  BrowserAccessibilityAndroid* image_scheme =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(1));

  BrowserAccessibilityAndroid* image_ineligible =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(2));

  BrowserAccessibilityAndroid* image_silent =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(3));

  EXPECT_EQ(std::u16string(), image_none->GetTextContentUTF16());

  EXPECT_EQ(u"image_name", image_scheme->GetAndroidContentDescription());
  EXPECT_EQ(std::u16string(), image_scheme->GetTextContentUTF16());
  EXPECT_EQ(std::u16string(),
            image_scheme->GetAndroidSupplementalDescription());

  EXPECT_EQ(std::u16string(), image_ineligible->GetTextContentUTF16());
  EXPECT_EQ(std::u16string(), image_silent->GetTextContentUTF16());
}

TEST_F(BrowserAccessibilityAndroidTest,
       TestImageInnerText_AnnotationSucceeded) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(3);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "test_annotation");
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].SetName("image_name");
  tree.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "test_annotation");
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityManagerAndroid* android_manager =
      ToBrowserAccessibilityManagerAndroid(manager.get());
  android_manager->set_allow_image_descriptions_for_testing(true);

  BrowserAccessibilityAndroid* image_succeeded =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(0));

  BrowserAccessibilityAndroid* image_succeeded_with_name =
      static_cast<BrowserAccessibilityAndroid*>(
          manager->GetBrowserAccessibilityRoot()->PlatformGetChild(1));

  // contentDescription holds author-provided non-visible alt text, textContent
  // holds visible text, and supplementalDescription holds secondary
  // information. When there is no alt text, the annotation should be promoted
  // to contentDescription as it is the primary label for the node.
  EXPECT_EQ(u"test_annotation",
            image_succeeded->GetAndroidContentDescription());
  EXPECT_EQ(std::u16string(), image_succeeded->GetTextContentUTF16());
  EXPECT_EQ(std::u16string(),
            image_succeeded->GetAndroidSupplementalDescription());

  // When alt text is present, it should be mapped to contentDescription, and
  // the annotation should be mapped to supplementalDescription. Mapping to two
  // separate fields (instead of concatenating the two into one field) allows
  // accessibility services to distinguish between the author intent and
  // generated description.
  EXPECT_EQ(u"image_name",
            image_succeeded_with_name->GetAndroidContentDescription());
  EXPECT_EQ(u"test_annotation",
            image_succeeded_with_name->GetAndroidSupplementalDescription());
  EXPECT_EQ(std::u16string(), image_succeeded_with_name->GetTextContentUTF16());
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_Suggestions) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101};
  last_node->role = ax::mojom::Role::kTextField;
  last_node->SetValue(u"Some very wrrrongly spelled words");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->child_ids = {201, 202, 203};
  last_node->role = ax::mojom::Role::kGenericContainer;

  last_node = &tree.nodes.emplace_back();
  last_node->id = 201;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 202;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("very wrrrongly spelled");
  last_node->AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int>(ax::mojom::MarkerType::kSuggestion)});
  last_node->AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                 {5});
  last_node->AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                 {14});

  last_node = &tree.nodes.emplace_back();
  last_node->id = 203;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" words");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some very wrrrongly spelled words"));
  ASSERT_TRUE(style_data.suggestions);
  EXPECT_THAT(*style_data.suggestions,
              UnorderedElementsAre(Pair(u"", RangePairs{{10, 19}})));
}

// TODO: aluh - Enable once link nodes are merged into text content.
TEST_F(BrowserAccessibilityAndroidTest, DISABLED_TextStyling_Links) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("A ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->child_ids = {201};
  last_node->role = ax::mojom::Role::kLink;
  last_node->AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                                "https://www.example.com/");
  last_node->SetName("simple");
  last_node->SetNameFrom(ax::mojom::NameFrom::kContents);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 201;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("simple");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" link");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"A simple link"));
  ASSERT_TRUE(style_data.links);
  EXPECT_THAT(*style_data.links,
              UnorderedElementsAre(
                  Pair(u"https://www.example.com/", RangePairs{{2, 8}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_NestedStyle) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("bold");
  last_node->AddTextStyle(ax::mojom::TextStyle::kBold);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some bold text"));
  ASSERT_TRUE(style_data.text_styles);
  EXPECT_THAT(*style_data.text_styles,
              UnorderedElementsAre(
                  Pair(ax::mojom::TextStyle::kBold, RangePairs{{5, 9}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_MixedStyles) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103, 104, 105};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("bold ");
  last_node->AddTextStyle(ax::mojom::TextStyle::kBold);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("and");
  last_node->AddTextStyle(ax::mojom::TextStyle::kBold);
  last_node->AddTextStyle(ax::mojom::TextStyle::kItalic);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 104;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" italic");
  last_node->AddTextStyle(ax::mojom::TextStyle::kItalic);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 105;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some bold and italic text"));
  ASSERT_TRUE(style_data.text_styles);
  EXPECT_THAT(
      *style_data.text_styles,
      UnorderedElementsAre(
          Pair(ax::mojom::TextStyle::kBold, RangePairs{{5, 10}, {10, 13}}),
          Pair(ax::mojom::TextStyle::kItalic, RangePairs{{10, 13}, {13, 20}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_TextSizes) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103, 104, 105};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("big");
  last_node->AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 24.0f);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" and");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 104;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" invisible");
  last_node->AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 0.0f);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 105;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some big and invisible text"));
  ASSERT_TRUE(style_data.text_sizes);
  EXPECT_THAT(*style_data.text_sizes,
              UnorderedElementsAre(Pair(24.0f, RangePairs{{5, 8}}),
                                   Pair(0.0f, RangePairs{{12, 22}})));
}

// TODO: aluh - Enable once super/subscript nodes are merged into text content.
TEST_F(BrowserAccessibilityAndroidTest, DISABLED_TextStyling_TextPositions) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 104};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kSuperscript;
  last_node->SetTextPosition(ax::mojom::TextPosition::kSuperscript);
  last_node->child_ids = {103};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("superscript");
  last_node->SetTextPosition(ax::mojom::TextPosition::kSuperscript);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 104;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some superscript text"));
  ASSERT_TRUE(style_data.text_positions);
  EXPECT_THAT(*style_data.text_positions,
              UnorderedElementsAre(Pair(ax::mojom::TextPosition::kSuperscript,
                                        RangePairs{{5, 16}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_ForegroundColors) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("red");
  last_node->AddIntAttribute(ax::mojom::IntAttribute::kColor, 0xFFFF0000);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some red text"));
  ASSERT_TRUE(style_data.foreground_colors);
  EXPECT_THAT(
      *style_data.foreground_colors,
      UnorderedElementsAre(Pair(0x00000000, RangePairs{{0, 5}, {8, 13}}),
                           Pair(0xFFFF0000, RangePairs{{5, 8}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_BackgroundColors) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("highlighted");
  last_node->AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                             0xFF00FF00);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some highlighted text"));
  ASSERT_TRUE(style_data.background_colors);
  EXPECT_THAT(
      *style_data.background_colors,
      UnorderedElementsAre(Pair(0x00000000, RangePairs{{0, 5}, {16, 21}}),
                           Pair(0xFF00FF00, RangePairs{{5, 16}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_BlendedColors) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};
  last_node->role = ax::mojom::Role::kGenericContainer;
  last_node->AddIntAttribute(ax::mojom::IntAttribute::kColor, 0xFFFF0000);
  last_node->AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                             0xFFFFFF00);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("blended color");
  last_node->AddIntAttribute(ax::mojom::IntAttribute::kColor, 0x55007788);
  last_node->AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                             0x8800FFFF);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some blended color text"));
  ASSERT_TRUE(style_data.foreground_colors);
  EXPECT_THAT(
      *style_data.foreground_colors,
      UnorderedElementsAre(Pair(0xFFFF0000, RangePairs{{0, 5}, {18, 23}}),
                           Pair(0xFFAA282D, RangePairs{{5, 18}})));
  ASSERT_TRUE(style_data.background_colors);
  EXPECT_THAT(
      *style_data.background_colors,
      UnorderedElementsAre(Pair(0xFFFFFF00, RangePairs{{0, 5}, {18, 23}}),
                           Pair(0xFF77FF88, RangePairs{{5, 18}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_FontFamilies) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};
  last_node->AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                                "serif");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("sans serif");
  last_node->AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                                "sans-serif");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some sans serif text"));
  ASSERT_TRUE(style_data.font_families);
  EXPECT_THAT(*style_data.font_families,
              UnorderedElementsAre(Pair("serif", RangePairs{{0, 5}, {15, 20}}),
                                   Pair("sans-serif", RangePairs{{5, 15}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_Locales) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};
  last_node->AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "en-US");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("繁體中文");
  last_node->AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "zh-TW");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some 繁體中文 text"));
  ASSERT_TRUE(style_data.locales);
  EXPECT_THAT(*style_data.locales,
              UnorderedElementsAre(Pair("en-US", RangePairs{{0, 5}, {9, 14}}),
                                   Pair("zh-TW", RangePairs{{5, 9}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_ManyAttributes) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("fancy");
  last_node->AddTextStyle(ax::mojom::TextStyle::kBold);
  last_node->AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 32.0f);
  last_node->AddIntAttribute(ax::mojom::IntAttribute::kColor, 0xFFFF0000);
  last_node->AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                             0xFF0000FF);
  last_node->AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                                "serif");
  last_node->AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "ja-JP");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some fancy text"));
  ASSERT_TRUE(style_data.text_styles);
  EXPECT_THAT(*style_data.text_styles,
              UnorderedElementsAre(
                  Pair(ax::mojom::TextStyle::kBold, RangePairs{{5, 10}})));
  ASSERT_TRUE(style_data.text_sizes);
  EXPECT_THAT(*style_data.text_sizes,
              UnorderedElementsAre(Pair(32.0f, RangePairs{{5, 10}})));
  ASSERT_TRUE(style_data.foreground_colors);
  EXPECT_THAT(
      *style_data.foreground_colors,
      UnorderedElementsAre(Pair(0x00000000, RangePairs{{0, 5}, {10, 15}}),
                           Pair(0xFFFF0000, RangePairs{{5, 10}})));
  ASSERT_TRUE(style_data.background_colors);
  EXPECT_THAT(
      *style_data.background_colors,
      UnorderedElementsAre(Pair(0x00000000, RangePairs{{0, 5}, {10, 15}}),
                           Pair(0xFF0000FF, RangePairs{{5, 10}})));
  ASSERT_TRUE(style_data.font_families);
  EXPECT_THAT(*style_data.font_families,
              UnorderedElementsAre(Pair("serif", RangePairs{{5, 10}})));
  ASSERT_TRUE(style_data.locales);
  EXPECT_THAT(*style_data.locales,
              UnorderedElementsAre(Pair("ja-JP", RangePairs{{5, 10}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_IgnoreInvalidValues) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("normal");
  last_node->AddIntAttribute(ax::mojom::IntAttribute::kTextStyle, 0);
  last_node->AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, -1.0);
  last_node->AddStringAttribute(ax::mojom::StringAttribute::kFontFamily, "");
  last_node->AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some normal text"));
  EXPECT_FALSE(style_data.text_styles);
  EXPECT_FALSE(style_data.text_sizes);
  EXPECT_FALSE(style_data.font_families);
  EXPECT_FALSE(style_data.locales);
}

TEST_F(BrowserAccessibilityAndroidTest, TextStyling_EmptyStyledText) {
  ui::AXTreeUpdate tree;
  tree.root_id = ROOT_ID;

  ui::AXNodeData* last_node = &tree.nodes.emplace_back();
  last_node->id = ROOT_ID;
  last_node->child_ids = {101, 102, 103};

  last_node = &tree.nodes.emplace_back();
  last_node->id = 101;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("Some ");

  last_node = &tree.nodes.emplace_back();
  last_node->id = 102;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName("");
  last_node->AddTextStyle(ax::mojom::TextStyle::kBold);

  last_node = &tree.nodes.emplace_back();
  last_node->id = 103;
  last_node->role = ax::mojom::Role::kStaticText;
  last_node->SetName(" text");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  AXStyleData style_data;
  BrowserAccessibilityAndroid* container =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(ROOT_ID));
  EXPECT_THAT(
      container->GetSubstringTextContentUTF16(std::nullopt, &style_data),
      Eq(u"Some  text"));
  ASSERT_TRUE(style_data.text_styles);
  EXPECT_THAT(*style_data.text_styles,
              UnorderedElementsAre(
                  Pair(ax::mojom::TextStyle::kBold, RangePairs{{5, 5}})));
}

TEST_F(BrowserAccessibilityAndroidTest, TestJavaNodeCache_AttributeChange) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(2);
  tree.nodes[0].id = 1;
  tree.nodes[0].role = ax::mojom::Role::kRootWebArea;
  tree.nodes[0].child_ids = {2};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kButton;

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityManagerAndroid* android_manager =
      ToBrowserAccessibilityManagerAndroid(manager.get());
  const auto& actual = android_manager->nodes_already_cleared_for_test();
  EXPECT_EQ(2, actual.size());
  EXPECT_TRUE(actual.contains(1));
  EXPECT_TRUE(actual.contains(2));

  ui::AXUpdatesAndEvents updates_and_events;
  updates_and_events.updates.resize(1);
  updates_and_events.updates[0].nodes.resize(1);
  updates_and_events.updates[0].nodes[0].id = 2;
  updates_and_events.updates[0].nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kName, "hello");

  manager->OnAccessibilityEvents(updates_and_events);

  EXPECT_EQ(2, actual.size());
  EXPECT_TRUE(actual.contains(1));
  EXPECT_TRUE(actual.contains(2));
}

TEST_F(BrowserAccessibilityAndroidTest, TestJavaNodeCache_NodeDeleted) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(2);
  tree.nodes[0].id = 1;
  tree.nodes[0].role = ax::mojom::Role::kRootWebArea;
  tree.nodes[0].child_ids = {2};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kButton;

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityManagerAndroid* android_manager =
      ToBrowserAccessibilityManagerAndroid(manager.get());
  const auto& actual = android_manager->nodes_already_cleared_for_test();
  EXPECT_EQ(2, actual.size());
  EXPECT_TRUE(actual.contains(1));
  EXPECT_TRUE(actual.contains(2));

  ui::AXUpdatesAndEvents updates_and_events;
  updates_and_events.updates.resize(1);
  updates_and_events.updates[0].nodes.resize(1);
  updates_and_events.updates[0].nodes[0].id = 1;
  updates_and_events.updates[0].nodes[0].role = ax::mojom::Role::kRootWebArea;

  manager->OnAccessibilityEvents(updates_and_events);

  EXPECT_EQ(2, actual.size());
  EXPECT_TRUE(actual.contains(1));
  EXPECT_TRUE(actual.contains(2));
}

TEST_F(BrowserAccessibilityAndroidTest, TestJavaNodeCache_NodeUnignored) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(3);
  tree.nodes[0].id = 1;
  tree.nodes[0].role = ax::mojom::Role::kRootWebArea;
  tree.nodes[0].child_ids = {2};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kButton;
  tree.nodes[1].AddState(ax::mojom::State::kIgnored);
  tree.nodes[1].child_ids = {3};

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kStaticText;

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityManagerAndroid* android_manager =
      ToBrowserAccessibilityManagerAndroid(manager.get());
  const auto& actual = android_manager->nodes_already_cleared_for_test();
  EXPECT_EQ(3, actual.size());
  EXPECT_TRUE(actual.contains(1));
  EXPECT_TRUE(actual.contains(2));
  EXPECT_TRUE(actual.contains(3));

  ui::AXUpdatesAndEvents updates_and_events;
  updates_and_events.updates.resize(1);
  updates_and_events.updates[0].nodes.resize(1);
  updates_and_events.updates[0].nodes[0].id = 2;
  updates_and_events.updates[0].nodes[0].role = ax::mojom::Role::kButton;

  manager->OnAccessibilityEvents(updates_and_events);

  EXPECT_EQ(3, actual.size());
  // From an AXEventGenerator::Event::CHILDREN_CHANGED.
  EXPECT_TRUE(actual.contains(1));
  // From an AXTreeObserver::Change; the only actual tree update.
  EXPECT_TRUE(actual.contains(2));
  // From an AXEventGenerator::Event::PARENT_CHANGED.
  EXPECT_TRUE(actual.contains(3));
}

TEST_F(BrowserAccessibilityAndroidTest, ExplicitlyEmptyName) {
  // Create parent node with the empty name.
  ui::AXNodeData parent_data;
  parent_data.id = 1;
  parent_data.role = ax::mojom::Role::kGenericContainer;
  parent_data.SetNameExplicitlyEmpty();
  parent_data.child_ids = {2};

  // Create a child node with text that should be ignored.
  ui::AXNodeData child_data;
  child_data.id = 2;
  child_data.role = ax::mojom::Role::kStaticText;
  child_data.SetName("This text should be hidden");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(parent_data, child_data),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityAndroid* parent_node =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(1));
  ASSERT_NE(nullptr, parent_node);

  EXPECT_EQ(u"", parent_node->GetAndroidContentDescription());
}

TEST_F(BrowserAccessibilityAndroidTest,
       RelatedElementMapsToSupplementalWhenLabeledByDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {features::kAccessibilityPopulateSupplementalDescriptionApi},
      /*disabled_features=*/{features::kAccessibilityLabeledBy});

  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(2);

  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kButton;
  tree.nodes[1].SetName("Label Text");
  tree.nodes[1].SetNameFrom(ax::mojom::NameFrom::kRelatedElement);
  tree.nodes[1].AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                                    {99});

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityAndroid* node = static_cast<BrowserAccessibilityAndroid*>(
      manager->GetBrowserAccessibilityRoot()->PlatformGetChild(0));

  EXPECT_EQ(u"Label Text", node->GetAndroidSupplementalDescription());
  EXPECT_TRUE(node->GetTextContentUTF16().empty());
}

TEST_F(BrowserAccessibilityAndroidTest, CaptionMapsToLabeledBy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessibilityLabeledBy);

  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(3);

  tree.nodes[0].id = 1;
  tree.nodes[0].role = ax::mojom::Role::kRootWebArea;
  tree.nodes[0].child_ids = {2};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kTable;
  tree.nodes[1].child_ids = {3};
  tree.nodes[1].SetName("My Caption");
  tree.nodes[1].SetNameFrom(ax::mojom::NameFrom::kCaption);

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kCaption;
  tree.nodes[2].SetName("My Caption");

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          tree, node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityAndroid* table_node =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(2));
  BrowserAccessibilityAndroid* caption_node =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(3));
  ASSERT_NE(nullptr, table_node);
  ASSERT_NE(nullptr, caption_node);

  std::vector<int> labeled_by_ids = table_node->GetLabelledByAndroidIds();
  ASSERT_EQ(1u, labeled_by_ids.size());
  EXPECT_EQ(caption_node->GetUniqueId(), labeled_by_ids[0]);
}

TEST_F(BrowserAccessibilityAndroidTest, TestGetLineBoundariesWithLineIds) {
  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ax::mojom::Role::kStaticText;
  text1.child_ids = {111, 112, 113};

  ui::AXNodeData box1;
  box1.id = 111;
  box1.role = ax::mojom::Role::kInlineTextBox;
  box1.SetName("One ");
  box1.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId, 112);

  ui::AXNodeData box2;
  box2.id = 112;
  box2.role = ax::mojom::Role::kInlineTextBox;
  box2.SetName("Two");
  box2.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId, 111);
  // box2 has NO kNextOnLineId, so end of line

  ui::AXNodeData box3;
  box3.id = 113;
  box3.role = ax::mojom::Role::kInlineTextBox;
  box3.SetName("Three");
  // box3 has NO kPreviousOnLineId, so start of new line

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {text1.id};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, text1, box1, box2, box3),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityAndroid* text_node =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(11));

  std::vector<int32_t> starts;
  std::vector<int32_t> ends;
  text_node->GetLineBoundaries(&starts, &ends, 0);

  EXPECT_THAT(starts, ::testing::ElementsAre(0, 7));
  EXPECT_THAT(ends, ::testing::ElementsAre(7, 12));
}

TEST_F(BrowserAccessibilityAndroidTest, TestGetLineBoundariesMissingLineIds) {
  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ax::mojom::Role::kStaticText;
  text1.child_ids = {111, 112, 113};

  ui::AXNodeData box1;
  box1.id = 111;
  box1.role = ax::mojom::Role::kInlineTextBox;
  box1.SetName("One ");

  ui::AXNodeData box2;
  box2.id = 112;
  box2.role = ax::mojom::Role::kInlineTextBox;
  box2.SetName("Two");

  ui::AXNodeData box3;
  box3.id = 113;
  box3.role = ax::mojom::Role::kInlineTextBox;
  box3.SetName("Three");

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {text1.id};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, text1, box1, box2, box3),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityAndroid* text_node =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(11));

  std::vector<int32_t> starts;
  std::vector<int32_t> ends;
  text_node->GetLineBoundaries(&starts, &ends, 0);

  // When attributes are missing, it will treat each inline text box as on a
  // separate line.
  EXPECT_THAT(starts, ::testing::ElementsAre(0, 4, 7));
  EXPECT_THAT(ends, ::testing::ElementsAre(4, 7, 12));
}

TEST_F(BrowserAccessibilityAndroidTest,
       IsFocusableContainerWithCollectionChildren) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName("Item");

  ui::AXNodeData list_item;
  list_item.id = 11;
  list_item.role = ax::mojom::Role::kListItem;
  list_item.child_ids = {text1.id};

  ui::AXNodeData list;
  list.id = 2;
  list.role = ax::mojom::Role::kList;
  list.child_ids = {list_item.id};

  ui::AXNodeData container;
  container.id = 1;
  container.role = ax::mojom::Role::kGenericContainer;
  container.AddState(ax::mojom::State::kFocusable);
  container.child_ids = {list.id};

  ui::AXNodeData root;
  root.id = 100;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {container.id};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, container, list, list_item, text1),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityAndroid* container_node =
      static_cast<BrowserAccessibilityAndroid*>(manager->GetFromID(1));

  EXPECT_FALSE(container_node->IsFocusable());
}

TEST_F(BrowserAccessibilityAndroidTest, SnapshotIdsDoNotCollideWithLiveTree) {
  UniqueAXNodeIdDelegate live_node_id_delegate;
  ui::AXNodeData live_root;
  live_root.id = 1;
  live_root.role = ax::mojom::Role::kRootWebArea;

  std::unique_ptr<ui::BrowserAccessibilityManager> live_manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(live_root), live_node_id_delegate,
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityAndroid* live_node =
      static_cast<BrowserAccessibilityAndroid*>(
          live_manager->GetBrowserAccessibilityRoot());
  int32_t live_unique_id = live_node->GetUniqueId();

  // Now create snapshot tree using MockWebContentsAccessibilityAndroid as
  // delegate.
  ui::AXNodeData snapshot_root;
  snapshot_root.id = 1;  // Same Blink ID
  snapshot_root.role = ax::mojom::Role::kRootWebArea;

  auto* update =
      new ui::AXTreeUpdate(MakeAXTreeUpdateForTesting(snapshot_root));
  MockWebContentsAccessibilityAndroid snapshot_wcaa(
      reinterpret_cast<intptr_t>(update));

  int32_t snapshot_unique_id = snapshot_wcaa.GetRootId(nullptr);

  // If the fix is active, snapshot_unique_id should be different from
  // live_unique_id.
  EXPECT_NE(live_unique_id, snapshot_unique_id);

  // Also test that lookup works for both, and they don't interfere.
  EXPECT_EQ(live_node,
            BrowserAccessibilityAndroid::GetFromUniqueId(live_unique_id));

  BrowserAccessibilityAndroid* snapshot_node =
      BrowserAccessibilityAndroid::GetFromUniqueId(snapshot_unique_id);
  EXPECT_NE(nullptr, snapshot_node);

  EXPECT_EQ(snapshot_node,
            snapshot_wcaa.GetAXFromUniqueIDForTesting(snapshot_unique_id));
  EXPECT_EQ(nullptr, snapshot_wcaa.GetAXFromUniqueIDForTesting(live_unique_id));
}

TEST_F(BrowserAccessibilityAndroidTest, TwoSnapshotsDoNotCollide) {
  ui::AXNodeData root1;
  root1.id = 1;
  root1.role = ax::mojom::Role::kRootWebArea;

  auto* update1 = new ui::AXTreeUpdate(MakeAXTreeUpdateForTesting(root1));
  MockWebContentsAccessibilityAndroid wcaa1(
      reinterpret_cast<intptr_t>(update1));
  int32_t unique_id1 = wcaa1.GetRootId(nullptr);

  ui::AXNodeData root2;
  root2.id = 1;  // Same Blink ID
  root2.role = ax::mojom::Role::kRootWebArea;

  auto* update2 = new ui::AXTreeUpdate(MakeAXTreeUpdateForTesting(root2));
  MockWebContentsAccessibilityAndroid wcaa2(
      reinterpret_cast<intptr_t>(update2));
  int32_t unique_id2 = wcaa2.GetRootId(nullptr);

  // They should have different unique IDs.
  EXPECT_NE(unique_id1, unique_id2);

  // Each should only find its own node.
  BrowserAccessibilityAndroid* node1 =
      BrowserAccessibilityAndroid::GetFromUniqueId(unique_id1);
  BrowserAccessibilityAndroid* node2 =
      BrowserAccessibilityAndroid::GetFromUniqueId(unique_id2);
  ASSERT_NE(nullptr, node1);
  ASSERT_NE(nullptr, node2);

  EXPECT_EQ(node1, wcaa1.GetAXFromUniqueIDForTesting(unique_id1));
  EXPECT_EQ(nullptr, wcaa1.GetAXFromUniqueIDForTesting(unique_id2));

  EXPECT_EQ(node2, wcaa2.GetAXFromUniqueIDForTesting(unique_id2));
  EXPECT_EQ(nullptr, wcaa2.GetAXFromUniqueIDForTesting(unique_id1));
}

// Test that BrowserAccessibilityAndroid::IsLeaf() returns true for a
// non-atomic-text-field without children.
TEST_F(BrowserAccessibilityAndroidTest,
       TestIsLeafContentEditableWithoutChildren) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAccessibilityExposeNonAtomicTextFieldChildren);

  int kNonAtomicTextFieldId = 11;

  ui::AXNodeData non_atomic_text_field;
  non_atomic_text_field.id = kNonAtomicTextFieldId;
  non_atomic_text_field.role = ax::mojom::Role::kGenericContainer;
  non_atomic_text_field.AddBoolAttribute(
      ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot, true);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {kNonAtomicTextFieldId};

  // With 0 children, IsLeaf() must return true.
  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, non_atomic_text_field),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));
  ui::BrowserAccessibility* node = manager->GetFromID(kNonAtomicTextFieldId);
  EXPECT_TRUE(node->GetData().IsNonAtomicTextField());
  EXPECT_TRUE(node->IsLeaf());
}

void RunIsLeafContentEditableWithChildrenTest(
    BrowserAccessibilityAndroidTest* test,
    bool should_enable_expose_non_atomic_children_feature) {
  base::test::ScopedFeatureList scoped_feature_list;
  if (should_enable_expose_non_atomic_children_feature) {
    scoped_feature_list.InitAndEnableFeature(
        features::kAccessibilityExposeNonAtomicTextFieldChildren);
  } else {
    scoped_feature_list.InitAndDisableFeature(
        features::kAccessibilityExposeNonAtomicTextFieldChildren);
  }

  int kNonAtomicTextFieldId = 11;

  ui::AXNodeData text;
  text.id = 111;
  text.role = ax::mojom::Role::kStaticText;
  text.SetName("Hello");

  ui::AXNodeData non_atomic_text_field;
  non_atomic_text_field.id = kNonAtomicTextFieldId;
  non_atomic_text_field.role = ax::mojom::Role::kGenericContainer;
  non_atomic_text_field.AddBoolAttribute(
      ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot, true);
  non_atomic_text_field.child_ids = {text.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {kNonAtomicTextFieldId};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager =
      test->CreateManager(
          ui::MakeAXTreeUpdateForTesting(root, non_atomic_text_field, text));

  ui::BrowserAccessibility* node = manager->GetFromID(kNonAtomicTextFieldId);
  EXPECT_TRUE(node->GetData().IsNonAtomicTextField());
  EXPECT_EQ(!should_enable_expose_non_atomic_children_feature, node->IsLeaf());
}

TEST_F(
    BrowserAccessibilityAndroidTest,
    TestIsLeafContentEditableWithChildren_ExposeNonAtomicChildrenFeatureDisabled) {
  RunIsLeafContentEditableWithChildrenTest(
      this, /*should_enable_expose_non_atomic_children_feature=*/false);
}

TEST_F(
    BrowserAccessibilityAndroidTest,
    TestIsLeafContentEditableWithChildren_ExposeNonAtomicChildrenFeatureEnabled) {
  RunIsLeafContentEditableWithChildrenTest(
      this, /*should_enable_expose_non_atomic_children_feature=*/true);
}

// Check that BrowserAccessibilityAndroid::IsLeaf() can be called on an ignored
// node with ignored children.
TEST_F(BrowserAccessibilityAndroidTest, TestIsLeafIgnoredWithChildren) {
  int kParentId = 11;

  ui::AXNodeData child1;
  child1.id = 12;
  child1.role = ax::mojom::Role::kRowGroup;

  ui::AXNodeData child2;
  child2.id = 13;
  child2.role = ax::mojom::Role::kRowGroup;

  ui::AXNodeData parent;
  parent.id = kParentId;
  parent.role = ax::mojom::Role::kRowGroup;
  parent.child_ids = {12, 13};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {kParentId};

  std::unique_ptr<ui::BrowserAccessibilityManager> manager(
      BrowserAccessibilityManagerAndroid::Create(
          MakeAXTreeUpdateForTesting(root, parent, child1, child2),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));
  ui::BrowserAccessibility* node = manager->GetFromID(kParentId);
  EXPECT_TRUE(node->IsLeaf());
}

}  // namespace content
