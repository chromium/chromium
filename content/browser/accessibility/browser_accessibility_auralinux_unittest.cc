// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_auralinux.h"

#include <atk/atk.h>
#include <memory>
#include <string>
#include <vector>

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

class BrowserAccessibilityAuraLinuxTest : public testing::Test {
 public:
  BrowserAccessibilityAuraLinuxTest();
  ~BrowserAccessibilityAuraLinuxTest() override;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  void SetUp() override;

  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAuraLinuxTest);
};

BrowserAccessibilityAuraLinuxTest::BrowserAccessibilityAuraLinuxTest() {}

BrowserAccessibilityAuraLinuxTest::~BrowserAccessibilityAuraLinuxTest() {}

void BrowserAccessibilityAuraLinuxTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<TestBrowserAccessibilityDelegate>();
}

TEST_F(BrowserAccessibilityAuraLinuxTest, TestSimpleAtkText) {
  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kStaticText;
  root_data.SetName("\xE2\x98\xBA Multiple Words");

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root_data),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ui::AXPlatformNodeAuraLinux* root_obj =
      ToBrowserAccessibilityAuraLinux(manager->GetRoot())->GetNode();
  AtkObject* root_atk_object(root_obj->GetNativeViewAccessible());
  ASSERT_TRUE(ATK_IS_OBJECT(root_atk_object));
  ASSERT_TRUE(ATK_IS_TEXT(root_atk_object));
  g_object_ref(root_atk_object);

  AtkText* atk_text = ATK_TEXT(root_atk_object);

  auto verify_atk_text_contents = [&](const char* expected_text,
                                      int start_offset, int end_offset) {
    gchar* text = atk_text_get_text(atk_text, start_offset, end_offset);
    EXPECT_STREQ(expected_text, text);
    g_free(text);
  };

  verify_atk_text_contents("\xE2\x98\xBA Multiple Words", 0, -1);
  verify_atk_text_contents("Multiple Words", 2, -1);
  verify_atk_text_contents("\xE2\x98\xBA", 0, 1);

  EXPECT_EQ(16, atk_text_get_character_count(atk_text));

  g_object_unref(root_atk_object);

  manager.reset();
}

TEST_F(BrowserAccessibilityAuraLinuxTest, TestCompositeAtkText) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";
  const int text_name_len = text1_name.length() + text2_name.length();

  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName(text1_name);

  ui::AXNodeData text2;
  text2.id = 12;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName(text2_name);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(text1.id);
  root.child_ids.push_back(text2.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text1, text2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ui::AXPlatformNodeAuraLinux* root_obj =
      ToBrowserAccessibilityAuraLinux(manager->GetRoot())->GetNode();
  AtkObject* root_atk_object(root_obj->GetNativeViewAccessible());

  ASSERT_TRUE(ATK_IS_OBJECT(root_atk_object));
  ASSERT_TRUE(ATK_IS_TEXT(root_atk_object));
  g_object_ref(root_atk_object);
  AtkText* atk_text = ATK_TEXT(root_atk_object);

  EXPECT_EQ(text_name_len, atk_text_get_character_count(atk_text));

  gchar* text = atk_text_get_text(atk_text, 0, -1);
  EXPECT_STREQ((text1_name + text2_name).c_str(), text);
  g_free(text);

  ASSERT_TRUE(ATK_IS_HYPERTEXT(root_atk_object));
  AtkHypertext* atk_hypertext = ATK_HYPERTEXT(root_atk_object);

  // There should be no hyperlinks in the node and trying to get one should
  // always return -1.
  EXPECT_EQ(0, atk_hypertext_get_n_links(atk_hypertext));
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, 0));
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, -1));
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, 1));

  g_object_unref(root_atk_object);

  manager.reset();
}

TEST_F(BrowserAccessibilityAuraLinuxTest, TestComplexHypertext) {
  const std::string text1_name = "One two three.";
  const std::string combo_box_name = "City:";
  const std::string combo_box_value = "Happyland";
  const std::string text2_name = " Four five six.";
  const std::string check_box_name = "I agree";
  const std::string check_box_value = "Checked";
  const std::string radio_button_text_name = "Red";
  const std::string link_text_name = "Blue";
  // Each control (combo / check box, radio button and link) will be represented
  // by an embedded object character.
  const base::string16 string16_embed(
      1, ui::AXPlatformNodeAuraLinux::kEmbeddedCharacter);
  const std::string embed = base::UTF16ToUTF8(string16_embed);
  const std::string root_hypertext =
      text1_name + embed + text2_name + embed + embed + embed;

  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName(text1_name);

  ui::AXNodeData combo_box;
  combo_box.id = 12;
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.SetName(combo_box_name);
  combo_box.SetValue(combo_box_value);

  ui::AXNodeData text2;
  text2.id = 13;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName(text2_name);

  ui::AXNodeData check_box;
  check_box.id = 14;
  check_box.role = ax::mojom::Role::kCheckBox;
  check_box.SetCheckedState(ax::mojom::CheckedState::kTrue);
  check_box.SetName(check_box_name);
  check_box.SetValue(check_box_value);

  ui::AXNodeData radio_button, radio_button_text;
  radio_button.id = 15;
  radio_button_text.id = 17;
  radio_button_text.SetName(radio_button_text_name);
  radio_button.role = ax::mojom::Role::kRadioButton;
  radio_button_text.role = ax::mojom::Role::kStaticText;
  radio_button.child_ids.push_back(radio_button_text.id);

  ui::AXNodeData link, link_text;
  link.id = 16;
  link_text.id = 18;
  link_text.SetName(link_text_name);
  link.role = ax::mojom::Role::kLink;
  link_text.role = ax::mojom::Role::kStaticText;
  link.child_ids.push_back(link_text.id);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(text1.id);
  root.child_ids.push_back(combo_box.id);
  root.child_ids.push_back(text2.id);
  root.child_ids.push_back(check_box.id);
  root.child_ids.push_back(radio_button.id);
  root.child_ids.push_back(link.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text1, combo_box, text2, check_box,
                           radio_button, radio_button_text, link, link_text),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ui::AXPlatformNodeAuraLinux* root_obj =
      ToBrowserAccessibilityAuraLinux(manager->GetRoot())->GetNode();
  AtkObject* root_atk_object(root_obj->GetNativeViewAccessible());

  ASSERT_TRUE(ATK_IS_OBJECT(root_atk_object));
  ASSERT_TRUE(ATK_IS_TEXT(root_atk_object));
  g_object_ref(root_atk_object);
  AtkText* atk_text = ATK_TEXT(root_atk_object);

  EXPECT_EQ(g_utf8_strlen(root_hypertext.c_str(), -1),
            atk_text_get_character_count(atk_text));

  gchar* text = atk_text_get_text(atk_text, 0, -1);
  EXPECT_STREQ(root_hypertext.c_str(), text);
  g_free(text);

  ASSERT_TRUE(ATK_IS_HYPERTEXT(root_atk_object));
  AtkHypertext* atk_hypertext = ATK_HYPERTEXT(root_atk_object);

  EXPECT_EQ(4, atk_hypertext_get_n_links(atk_hypertext));

  auto verify_atk_link_text = [&](const char* expected_text, int link_index,
                                  int expected_start_index) {
    AtkHyperlink* link = atk_hypertext_get_link(atk_hypertext, link_index);
    ASSERT_NE(nullptr, link);
    ASSERT_TRUE(ATK_IS_HYPERLINK(link));

    AtkObject* link_object = atk_hyperlink_get_object(link, 0);
    ASSERT_NE(link_object, nullptr);
    ASSERT_TRUE(ATK_IS_HYPERLINK_IMPL(link_object));

    AtkHyperlink* link_from_object =
        atk_hyperlink_impl_get_hyperlink(ATK_HYPERLINK_IMPL(link_object));
    ASSERT_EQ(link_from_object, link);
    g_object_unref(link_from_object);

    ASSERT_EQ(atk_hyperlink_get_start_index(link), expected_start_index);
    ASSERT_EQ(atk_hyperlink_get_end_index(link), expected_start_index + 1);

    AtkObject* object = atk_hyperlink_get_object(link, 0);
    ASSERT_TRUE(ATK_IS_TEXT(object));

    char* text = atk_text_get_text(ATK_TEXT(object), 0, -1);
    EXPECT_STREQ(expected_text, text);
    g_free(text);
  };

  AtkHyperlink* combo_box_link = atk_hypertext_get_link(atk_hypertext, 0);
  ASSERT_NE(nullptr, combo_box_link);
  ASSERT_TRUE(ATK_IS_HYPERLINK(combo_box_link));

  // Get the text of the combo box. It should be its value.
  verify_atk_link_text(combo_box_value.c_str(), 0, 14);

  // Get the text of the check box. It should be its name.
  verify_atk_link_text(check_box_name.c_str(), 1, 30);

  // Get the text of the radio button.
  verify_atk_link_text(radio_button_text_name.c_str(), 2, 31);

  // Get the text of the link.
  verify_atk_link_text(link_text_name.c_str(), 3, 32);

  // Now test that all the object indices map back to the correct link indices.
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, -1));
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, 0));
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, 1));
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, 5));
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, 13));
  EXPECT_EQ(0, atk_hypertext_get_link_index(atk_hypertext, 14));
  EXPECT_EQ(1, atk_hypertext_get_link_index(atk_hypertext, 30));
  EXPECT_EQ(2, atk_hypertext_get_link_index(atk_hypertext, 31));
  EXPECT_EQ(3, atk_hypertext_get_link_index(atk_hypertext, 32));
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, 33));
  EXPECT_EQ(-1, atk_hypertext_get_link_index(atk_hypertext, 34));

  g_object_unref(root_atk_object);

  text1.SetName(text1_name + text1_name);
  AXEventNotificationDetails event_bundle;
  event_bundle.updates.resize(1);
  event_bundle.updates[0].nodes.push_back(text1);
  event_bundle.updates[0].nodes.push_back(root);
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  // The hypertext offsets should reflect the new length of the static text.
  verify_atk_link_text(combo_box_value.c_str(), 0, 28);
  verify_atk_link_text(check_box_name.c_str(), 1, 44);
  verify_atk_link_text(radio_button_text_name.c_str(), 2, 45);
  verify_atk_link_text(link_text_name.c_str(), 3, 46);

  manager.reset();
}

TEST_F(BrowserAccessibilityAuraLinuxTest,
       TestTextAttributesInContentEditables) {
  auto has_attribute = [](AtkAttributeSet* attributes,
                          AtkTextAttribute text_attribute,
                          base::Optional<const char*> expected_value) {
    const char* name = atk_text_attribute_get_name(text_attribute);
    while (attributes) {
      const AtkAttribute* attribute =
          static_cast<AtkAttribute*>(attributes->data);
      if (!g_strcmp0(attribute->name, name)) {
        if (!expected_value.has_value())
          return true;
        if (!g_strcmp0(attribute->value, *expected_value))
          return true;
      }
      attributes = g_slist_next(attributes);
    }
    return false;
  };

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData div_editable;
  div_editable.id = 2;
  div_editable.role = ax::mojom::Role::kGenericContainer;
  div_editable.AddState(ax::mojom::State::kEditable);
  div_editable.AddState(ax::mojom::State::kFocusable);
  div_editable.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                                  "Helvetica");

  ui::AXNodeData text_before;
  text_before.id = 3;
  text_before.role = ax::mojom::Role::kStaticText;
  text_before.AddState(ax::mojom::State::kEditable);
  text_before.SetName("Before ");
  text_before.AddTextStyle(ax::mojom::TextStyle::kBold);
  text_before.AddTextStyle(ax::mojom::TextStyle::kItalic);

  ui::AXNodeData link;
  link.id = 4;
  link.role = ax::mojom::Role::kLink;
  link.AddState(ax::mojom::State::kEditable);
  link.AddState(ax::mojom::State::kFocusable);
  link.AddState(ax::mojom::State::kLinked);
  link.SetName("lnk");
  link.AddTextStyle(ax::mojom::TextStyle::kUnderline);

  ui::AXNodeData link_text;
  link_text.id = 5;
  link_text.role = ax::mojom::Role::kStaticText;
  link_text.AddState(ax::mojom::State::kEditable);
  link_text.AddState(ax::mojom::State::kFocusable);
  link_text.AddState(ax::mojom::State::kLinked);
  link_text.SetName("lnk");
  link_text.AddTextStyle(ax::mojom::TextStyle::kUnderline);
  link_text.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr");

  // The name "lnk" is misspelled.
  std::vector<int32_t> marker_types{
      static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)};
  std::vector<int32_t> marker_starts{0};
  std::vector<int32_t> marker_ends{3};
  link_text.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes,
                                marker_types);
  link_text.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                marker_starts);
  link_text.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                marker_ends);

  ui::AXNodeData text_after;
  text_after.id = 6;
  text_after.role = ax::mojom::Role::kStaticText;
  text_after.AddState(ax::mojom::State::kEditable);
  text_after.SetName(" after.");
  // Leave text style as normal.

  root.child_ids.push_back(div_editable.id);
  div_editable.child_ids = {text_before.id, link.id, text_after.id};
  link.child_ids.push_back(link_text.id);

  ui::AXTreeUpdate update = MakeAXTreeUpdate(root, div_editable, text_before,
                                             link, link_text, text_after);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update, test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityAuraLinux* ax_root =
      ToBrowserAccessibilityAuraLinux(manager->GetRoot());
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibilityAuraLinux* ax_div =
      ToBrowserAccessibilityAuraLinux(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(3U, ax_div->PlatformChildCount());

  BrowserAccessibilityAuraLinux* ax_before =
      ToBrowserAccessibilityAuraLinux(ax_div->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_before);
  BrowserAccessibilityAuraLinux* ax_link =
      ToBrowserAccessibilityAuraLinux(ax_div->PlatformGetChild(1));
  ASSERT_NE(nullptr, ax_link);
  ASSERT_EQ(1U, ax_link->PlatformChildCount());
  BrowserAccessibilityAuraLinux* ax_after =
      ToBrowserAccessibilityAuraLinux(ax_div->PlatformGetChild(2));
  ASSERT_NE(nullptr, ax_after);

  BrowserAccessibilityAuraLinux* ax_link_text =
      ToBrowserAccessibilityAuraLinux(ax_link->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_link_text);

  AtkObject* root_atk_object = ax_root->GetNode()->GetNativeViewAccessible();
  AtkObject* ax_div_atk_object = ax_div->GetNode()->GetNativeViewAccessible();

  ASSERT_EQ(1, atk_text_get_character_count(ATK_TEXT(root_atk_object)));
  ASSERT_EQ(15, atk_text_get_character_count(ATK_TEXT(ax_div_atk_object)));

  // Test the style of the root.
  int start_offset, end_offset;
  AtkAttributeSet* attributes = atk_text_get_run_attributes(
      ATK_TEXT(root_atk_object), 0, &start_offset, &end_offset);

  ASSERT_EQ(0, start_offset);
  ASSERT_EQ(1, end_offset);

  ASSERT_TRUE(
      has_attribute(attributes, ATK_TEXT_ATTR_FAMILY_NAME, "Helvetica"));
  atk_attribute_set_free(attributes);

  // Test the style of text_before.
  for (int offset = 0; offset < 7; ++offset) {
    start_offset = end_offset = -1;
    attributes = atk_text_get_run_attributes(
        ATK_TEXT(ax_div_atk_object), offset, &start_offset, &end_offset);

    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(7, end_offset);

    ASSERT_TRUE(
        has_attribute(attributes, ATK_TEXT_ATTR_FAMILY_NAME, "Helvetica"));
    ASSERT_TRUE(has_attribute(attributes, ATK_TEXT_ATTR_WEIGHT, "700"));
    ASSERT_TRUE(has_attribute(attributes, ATK_TEXT_ATTR_STYLE, "italic"));

    atk_attribute_set_free(attributes);
  }

  // Test the style of the link.
  AtkObject* ax_link_text_atk_object =
      ax_link_text->GetNode()->GetNativeViewAccessible();
  for (int offset = 0; offset < 3; offset++) {
    start_offset = end_offset = -1;
    attributes = atk_text_get_run_attributes(
        ATK_TEXT(ax_link_text_atk_object), offset, &start_offset, &end_offset);

    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(3, end_offset);

    ASSERT_TRUE(
        has_attribute(attributes, ATK_TEXT_ATTR_FAMILY_NAME, "Helvetica"));
    ASSERT_FALSE(
        has_attribute(attributes, ATK_TEXT_ATTR_WEIGHT, base::nullopt));
    ASSERT_FALSE(has_attribute(attributes, ATK_TEXT_ATTR_STYLE, base::nullopt));
    ASSERT_TRUE(has_attribute(attributes, ATK_TEXT_ATTR_UNDERLINE, "single"));
    ASSERT_TRUE(has_attribute(attributes, ATK_TEXT_ATTR_LANGUAGE, "fr"));

    // For compatibility with Firefox, spelling attributes should also be
    // propagated to the parent of static text leaves.
    ASSERT_TRUE(has_attribute(attributes, ATK_TEXT_ATTR_UNDERLINE, "error"));
    ASSERT_TRUE(has_attribute(attributes, ATK_TEXT_ATTR_INVALID, "spelling"));

    atk_attribute_set_free(attributes);
  }

  // Test the style of text_after.
  for (int offset = 8; offset < 15; ++offset) {
    start_offset = end_offset = -1;
    attributes = atk_text_get_run_attributes(
        ATK_TEXT(ax_div_atk_object), offset, &start_offset, &end_offset);
    EXPECT_EQ(8, start_offset);
    EXPECT_EQ(15, end_offset);

    ASSERT_TRUE(
        has_attribute(attributes, ATK_TEXT_ATTR_FAMILY_NAME, "Helvetica"));
    ASSERT_FALSE(
        has_attribute(attributes, ATK_TEXT_ATTR_WEIGHT, base::nullopt));
    ASSERT_FALSE(has_attribute(attributes, ATK_TEXT_ATTR_STYLE, base::nullopt));
    ASSERT_FALSE(
        has_attribute(attributes, ATK_TEXT_ATTR_UNDERLINE, base::nullopt));
    ASSERT_FALSE(
        has_attribute(attributes, ATK_TEXT_ATTR_INVALID, base::nullopt));

    atk_attribute_set_free(attributes);
  }

  // Test the style of the static text nodes.
  AtkObject* ax_before_atk_object =
      ax_before->GetNode()->GetNativeViewAccessible();
  start_offset = end_offset = -1;
  attributes = atk_text_get_run_attributes(ATK_TEXT(ax_before_atk_object), 6,
                                           &start_offset, &end_offset);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(7, end_offset);
  ASSERT_TRUE(
      has_attribute(attributes, ATK_TEXT_ATTR_FAMILY_NAME, "Helvetica"));
  ASSERT_TRUE(has_attribute(attributes, ATK_TEXT_ATTR_WEIGHT, "700"));
  ASSERT_TRUE(has_attribute(attributes, ATK_TEXT_ATTR_STYLE, "italic"));
  ASSERT_FALSE(
      has_attribute(attributes, ATK_TEXT_ATTR_UNDERLINE, base::nullopt));
  ASSERT_FALSE(has_attribute(attributes, ATK_TEXT_ATTR_INVALID, base::nullopt));
  atk_attribute_set_free(attributes);

  AtkObject* ax_after_atk_object =
      ax_after->GetNode()->GetNativeViewAccessible();
  attributes = atk_text_get_run_attributes(ATK_TEXT(ax_after_atk_object), 6,
                                           &start_offset, &end_offset);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(7, end_offset);
  ASSERT_TRUE(
      has_attribute(attributes, ATK_TEXT_ATTR_FAMILY_NAME, "Helvetica"));
  ASSERT_FALSE(has_attribute(attributes, ATK_TEXT_ATTR_WEIGHT, base::nullopt));
  ASSERT_FALSE(has_attribute(attributes, ATK_TEXT_ATTR_STYLE, "italic"));
  ASSERT_FALSE(
      has_attribute(attributes, ATK_TEXT_ATTR_UNDERLINE, base::nullopt));
  atk_attribute_set_free(attributes);

  manager.reset();
}

TEST_F(BrowserAccessibilityAuraLinuxTest,
       TestAtkObjectsNotDeletedDuringTreeUpdate) {
  ui::AXNodeData container;
  container.id = 4;
  container.role = ax::mojom::Role::kGenericContainer;

  ui::AXNodeData combo_box;
  combo_box.id = 6;
  combo_box.role = ax::mojom::Role::kPopUpButton;
  combo_box.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "select");
  combo_box.AddState(ax::mojom::State::kCollapsed);
  combo_box.SetValue("1");
  container.child_ids.push_back(combo_box.id);

  ui::AXNodeData menu_list;
  menu_list.id = 8;
  menu_list.role = ax::mojom::Role::kMenuListPopup;
  menu_list.AddState(ax::mojom::State::kInvisible);
  combo_box.child_ids.push_back(menu_list.id);

  ui::AXNodeData menu_option_1;
  menu_option_1.id = 9;
  menu_option_1.role = ax::mojom::Role::kMenuListOption;
  menu_option_1.SetName("1");
  menu_list.child_ids.push_back(menu_option_1.id);

  ui::AXNodeData menu_option_2;
  menu_option_2.id = 10;
  menu_option_2.role = ax::mojom::Role::kMenuListOption;
  menu_option_2.SetName("2");
  menu_option_2.AddState(ax::mojom::State::kInvisible);
  menu_list.child_ids.push_back(menu_option_2.id);

  ui::AXNodeData root;
  root.id = 2;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(container.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, container, combo_box, menu_list, menu_option_1,
                           menu_option_2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ui::AXPlatformNodeAuraLinux* combo_box_node =
      ToBrowserAccessibilityAuraLinux(manager->GetFromID(combo_box.id))
          ->GetNode();
  ASSERT_FALSE(combo_box_node->IsChildOfLeaf());
  AtkObject* original_atk_object = combo_box_node->GetNativeViewAccessible();
  g_object_ref(original_atk_object);

  // The interface mask is only dependent on IsChildOfLeaf. Create an update
  // which won't modify this.
  container.SetName("container");
  ui::AXTree* tree = const_cast<ui::AXTree*>(manager->ax_tree());
  ASSERT_TRUE(tree->Unserialize(MakeAXTreeUpdate(container)));
  ASSERT_FALSE(combo_box_node->IsChildOfLeaf());
  ASSERT_EQ(original_atk_object, combo_box_node->GetNativeViewAccessible());

  g_object_unref(original_atk_object);
  manager.reset();
}

TEST_F(BrowserAccessibilityAuraLinuxTest,
       TestExistingMisspellingsInSimpleTextFields) {
  std::string value1("Testing .");
  // The word "helo" is misspelled.
  std::string value2("Helo there.");

  int value1_length = value1.length();
  int value2_length = value2.length();
  int combo_box_value_length = value1_length + value2_length;

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData combo_box;
  combo_box.id = 2;
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.AddState(ax::mojom::State::kFocusable);
  combo_box.SetValue(value1 + value2);

  ui::AXNodeData combo_box_div;
  combo_box_div.id = 3;
  combo_box_div.role = ax::mojom::Role::kGenericContainer;
  combo_box_div.AddState(ax::mojom::State::kEditable);

  ui::AXNodeData static_text1;
  static_text1.id = 4;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.AddState(ax::mojom::State::kEditable);
  static_text1.SetName(value1);

  ui::AXNodeData static_text2;
  static_text2.id = 5;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.AddState(ax::mojom::State::kEditable);
  static_text2.SetName(value2);

  std::vector<int32_t> marker_types;
  marker_types.push_back(
      static_cast<int32_t>(ax::mojom::MarkerType::kSpelling));
  std::vector<int32_t> marker_starts;
  marker_starts.push_back(0);
  std::vector<int32_t> marker_ends;
  marker_ends.push_back(4);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes,
                                   marker_types);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                   marker_starts);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                   marker_ends);

  root.child_ids.push_back(combo_box.id);
  combo_box.child_ids.push_back(combo_box_div.id);
  combo_box_div.child_ids.push_back(static_text1.id);
  combo_box_div.child_ids.push_back(static_text2.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, combo_box, combo_box_div, static_text1,
                           static_text2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityAuraLinux* ax_root =
      ToBrowserAccessibilityAuraLinux(manager->GetRoot());
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibilityAuraLinux* ax_combo_box =
      ToBrowserAccessibilityAuraLinux(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_combo_box);

  AtkText* combo_box_text =
      ATK_TEXT(ax_combo_box->GetNode()->GetNativeViewAccessible());
  int start_offset, end_offset;

  auto contains_spelling_attribute = [](AtkAttributeSet* attributes) {
    const char* invalid_str =
        atk_text_attribute_get_name(ATK_TEXT_ATTR_INVALID);
    while (attributes) {
      AtkAttribute* attribute = static_cast<AtkAttribute*>(attributes->data);
      if (!g_strcmp0(attribute->name, invalid_str) &&
          !g_strcmp0(attribute->value, "spelling"))
        return true;
      attributes = g_slist_next(attributes);
    }

    return false;
  };

  // Ensure that the first part of the value is not marked misspelled.
  for (int offset = 0; offset < value1_length; ++offset) {
    AtkAttributeSet* attributes = atk_text_get_run_attributes(
        combo_box_text, offset, &start_offset, &end_offset);
    EXPECT_FALSE(contains_spelling_attribute(attributes));
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(value1_length, end_offset);
    atk_attribute_set_free(attributes);
  }

  // Ensure that "helo" is marked misspelled.
  for (int offset = value1_length; offset < value1_length + 4; ++offset) {
    AtkAttributeSet* attributes = atk_text_get_run_attributes(
        combo_box_text, offset, &start_offset, &end_offset);
    EXPECT_EQ(value1_length, start_offset);
    EXPECT_EQ(value1_length + 4, end_offset);
    EXPECT_TRUE(contains_spelling_attribute(attributes));
    atk_attribute_set_free(attributes);
  }

  // Ensure that the last part of the value is not marked misspelled.
  for (int offset = value1_length + 4; offset < combo_box_value_length;
       ++offset) {
    AtkAttributeSet* attributes = atk_text_get_run_attributes(
        combo_box_text, offset, &start_offset, &end_offset);
    EXPECT_FALSE(contains_spelling_attribute(attributes));
    EXPECT_EQ(value1_length + 4, start_offset);
    EXPECT_EQ(combo_box_value_length, end_offset);
    atk_attribute_set_free(attributes);
  }

  manager.reset();
}

TEST_F(BrowserAccessibilityAuraLinuxTest, TextAtkStaticTextChange) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData div_editable;
  div_editable.id = 2;
  div_editable.role = ax::mojom::Role::kGenericContainer;

  ui::AXNodeData text;
  text.id = 3;
  text.role = ax::mojom::Role::kStaticText;
  text.SetName("Text1 ");

  root.child_ids.push_back(div_editable.id);
  div_editable.child_ids.push_back(text.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, div_editable, text),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  text.SetName("Text2");
  ui::AXTree* tree = const_cast<ui::AXTree*>(manager->ax_tree());
  ASSERT_TRUE(tree->Unserialize(MakeAXTreeUpdate(text)));

  // The change to the static text node should have triggered an update of the
  // containing div's hypertext.
  ui::AXPlatformNodeAuraLinux* div_node =
      ToBrowserAccessibilityAuraLinux(manager->GetFromID(div_editable.id))
          ->GetNode();
  EXPECT_STREQ(base::UTF16ToUTF8(div_node->GetHypertext()).c_str(), "Text2");
}

}  // namespace content
