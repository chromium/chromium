// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_auralinux.h"

#include <atk/atk.h>
#include <memory>
#include <string>
#include <vector>

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

class BrowserAccessibilityAuraLinuxTest : public testing::Test {
 public:
  BrowserAccessibilityAuraLinuxTest();
  ~BrowserAccessibilityAuraLinuxTest() override;

 private:
  void SetUp() override;

  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAuraLinuxTest);
};

BrowserAccessibilityAuraLinuxTest::BrowserAccessibilityAuraLinuxTest() {}

BrowserAccessibilityAuraLinuxTest::~BrowserAccessibilityAuraLinuxTest() {}

void BrowserAccessibilityAuraLinuxTest::SetUp() {}

TEST_F(BrowserAccessibilityAuraLinuxTest, TestSimpleAtkText) {
  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kStaticText;
  root_data.SetName("\xE2\x98\xBA Multiple Words");

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(MakeAXTreeUpdate(root_data), nullptr,
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
      BrowserAccessibilityManager::Create(MakeAXTreeUpdate(root, text1, text2),
                                          nullptr,
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
  const std::string button_text_name = "Red";
  const std::string link_text_name = "Blue";
  // Each control (combo / check box, button and link) will be represented by an
  // embedded object character.
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

  ui::AXNodeData button, button_text;
  button.id = 15;
  button_text.id = 17;
  button_text.SetName(button_text_name);
  button.role = ax::mojom::Role::kButton;
  button_text.role = ax::mojom::Role::kStaticText;
  button.child_ids.push_back(button_text.id);

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
  root.child_ids.push_back(button.id);
  root.child_ids.push_back(link.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text1, combo_box, text2, check_box, button,
                           button_text, link, link_text),
          nullptr, new BrowserAccessibilityFactory()));

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

  // Get the text of the button.
  verify_atk_link_text(button_text_name.c_str(), 2, 31);

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

  manager.reset();
}

}  // namespace content
