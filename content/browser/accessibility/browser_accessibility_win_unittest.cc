// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_win.h"

#include <objbase.h>
#include <stdint.h>
#include <wrl/client.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/common/accessibility_messages.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/base/win/atl_module.h"

namespace content {

// BrowserAccessibilityWinTest ------------------------------------------------

class BrowserAccessibilityWinTest : public testing::Test {
 public:
  BrowserAccessibilityWinTest();
  ~BrowserAccessibilityWinTest() override;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  void SetUp() override;

  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityWinTest);
};

BrowserAccessibilityWinTest::BrowserAccessibilityWinTest() {}

BrowserAccessibilityWinTest::~BrowserAccessibilityWinTest() {}

void BrowserAccessibilityWinTest::SetUp() {
  ui::win::CreateATLModuleIfNeeded();
  test_browser_accessibility_delegate_ =
      std::make_unique<TestBrowserAccessibilityDelegate>();
}

// Actual tests ---------------------------------------------------------------

// Test that BrowserAccessibilityManager correctly releases the tree of
// BrowserAccessibility instances upon delete.
TEST_F(BrowserAccessibilityWinTest, TestNoLeaks) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData button;
  button.id = 2;
  button.SetName("Button");
  button.role = ax::mojom::Role::kButton;

  ui::AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.SetName("Checkbox");
  checkbox.role = ax::mojom::Role::kCheckBox;

  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility, and ensure that exactly 3 instances were
  // created. Note that the manager takes ownership of the factory.
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, button, checkbox),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  // Delete the manager and test that all 3 instances are deleted.
  manager.reset();

  // Construct a manager again, and this time use the IAccessible interface
  // to get new references to two of the three nodes in the tree.
  manager.reset(BrowserAccessibilityManager::Create(
      MakeAXTreeUpdate(root, button, checkbox),
      test_browser_accessibility_delegate_.get(),
      new BrowserAccessibilityFactory()));
  IAccessible* root_accessible =
      ToBrowserAccessibilityWin(manager->GetRoot())->GetCOM();
  IDispatch* root_iaccessible = NULL;
  IDispatch* child1_iaccessible = NULL;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = root_accessible->get_accChild(childid_self, &root_iaccessible);
  ASSERT_EQ(S_OK, hr);
  base::win::ScopedVariant one(1);
  hr = root_accessible->get_accChild(one, &child1_iaccessible);
  ASSERT_EQ(S_OK, hr);

  // Now delete the manager, and only one of the three nodes in the tree
  // should be released.
  manager.reset();

  // Release each of our references and make sure that each one results in
  // the instance being deleted as its reference count hits zero.
  root_iaccessible->Release();
  child1_iaccessible->Release();
}

TEST_F(BrowserAccessibilityWinTest, TestChildrenChange) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData text;
  text.id = 2;
  text.role = ax::mojom::Role::kStaticText;
  text.SetName("old text");

  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility.
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  // Query for the text IAccessible and verify that it returns "old text" as its
  // value.
  base::win::ScopedVariant one(1);
  Microsoft::WRL::ComPtr<IDispatch> text_dispatch;
  HRESULT hr = ToBrowserAccessibilityWin(manager->GetRoot())
                   ->GetCOM()
                   ->get_accChild(one, text_dispatch.GetAddressOf());
  ASSERT_EQ(S_OK, hr);

  Microsoft::WRL::ComPtr<IAccessible> text_accessible;
  hr = text_dispatch.CopyTo(text_accessible.GetAddressOf());
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr name;
  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"old text", base::string16(name));
  name.Reset();

  text_dispatch.Reset();
  text_accessible.Reset();

  // Notify the BrowserAccessibilityManager that the text child has changed.
  AXContentNodeData text2;
  text2.id = 2;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName("new text");
  AXEventNotificationDetails event_bundle;
  event_bundle.updates.resize(1);
  event_bundle.updates[0].nodes.push_back(text2);
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  // Query for the text IAccessible and verify that it now returns "new text"
  // as its value.
  hr = ToBrowserAccessibilityWin(manager->GetRoot())
           ->GetCOM()
           ->get_accChild(one, text_dispatch.GetAddressOf());
  ASSERT_EQ(S_OK, hr);

  hr = text_dispatch.CopyTo(text_accessible.GetAddressOf());
  ASSERT_EQ(S_OK, hr);

  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"new text", base::string16(name));

  text_dispatch.Reset();
  text_accessible.Reset();

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestChildrenChangeNoLeaks) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData div;
  div.id = 2;
  div.role = ax::mojom::Role::kGroup;

  ui::AXNodeData text3;
  text3.id = 3;
  text3.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData text4;
  text4.id = 4;
  text4.role = ax::mojom::Role::kStaticText;

  div.child_ids.push_back(3);
  div.child_ids.push_back(4);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility and ensure that exactly 4 instances were
  // created. Note that the manager takes ownership of the factory.
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, div, text3, text4),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  // Notify the BrowserAccessibilityManager that the div node and its children
  // were removed and ensure that only one BrowserAccessibility instance exists.
  root.child_ids.clear();
  AXEventNotificationDetails event_bundle;
  event_bundle.updates.resize(1);
  event_bundle.updates[0].nodes.push_back(root);
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestTextBoundaries) {
  std::string line1 = "One two three.";
  std::string line2 = "Four five six.";
  std::string text_value = line1 + '\n' + line2;

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(2);

  ui::AXNodeData text_field;
  text_field.id = 2;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.SetValue(text_value);
  std::vector<int32_t> line_start_offsets;
  line_start_offsets.push_back(15);
  text_field.AddIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                                 line_start_offsets);
  text_field.child_ids.push_back(3);
  text_field.child_ids.push_back(5);
  text_field.child_ids.push_back(6);

  ui::AXNodeData static_text1;
  static_text1.id = 3;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.AddState(ax::mojom::State::kEditable);
  static_text1.SetName(line1);
  static_text1.child_ids.push_back(4);

  ui::AXNodeData inline_box1;
  inline_box1.id = 4;
  inline_box1.role = ax::mojom::Role::kInlineTextBox;
  inline_box1.AddState(ax::mojom::State::kEditable);
  inline_box1.SetName(line1);
  std::vector<int32_t> word_start_offsets1;
  word_start_offsets1.push_back(0);
  word_start_offsets1.push_back(4);
  word_start_offsets1.push_back(8);
  inline_box1.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                  word_start_offsets1);

  ui::AXNodeData line_break;
  line_break.id = 5;
  line_break.role = ax::mojom::Role::kLineBreak;
  line_break.AddState(ax::mojom::State::kEditable);
  line_break.SetName("\n");

  inline_box1.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                              line_break.id);
  line_break.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                             inline_box1.id);

  ui::AXNodeData static_text2;
  static_text2.id = 6;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.AddState(ax::mojom::State::kEditable);
  static_text2.SetName(line2);
  static_text2.child_ids.push_back(7);

  ui::AXNodeData inline_box2;
  inline_box2.id = 7;
  inline_box2.role = ax::mojom::Role::kInlineTextBox;
  inline_box2.AddState(ax::mojom::State::kEditable);
  inline_box2.SetName(line2);
  std::vector<int32_t> word_start_offsets2;
  word_start_offsets2.push_back(0);
  word_start_offsets2.push_back(5);
  word_start_offsets2.push_back(10);
  inline_box2.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                  word_start_offsets2);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text_field, static_text1, inline_box1,
                           line_break, static_text2, inline_box2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibilityWin* root_obj =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, root_obj);
  ASSERT_EQ(1U, root_obj->PlatformChildCount());

  BrowserAccessibilityComWin* text_field_obj =
      ToBrowserAccessibilityWin(root_obj->PlatformGetChild(0))->GetCOM();
  ASSERT_NE(nullptr, text_field_obj);

  LONG text_len;
  EXPECT_EQ(S_OK, text_field_obj->get_nCharacters(&text_len));

  base::win::ScopedBstr text;
  EXPECT_EQ(S_OK, text_field_obj->get_text(0, text_len, text.Receive()));
  EXPECT_EQ(text_value, base::UTF16ToUTF8(base::string16(text)));
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_text(0, 4, text.Receive()));
  EXPECT_STREQ(L"One ", text);
  text.Reset();

  LONG start;
  LONG end;
  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
                      1, IA2_TEXT_BOUNDARY_CHAR, &start, &end, text.Receive()));
  EXPECT_EQ(1, start);
  EXPECT_EQ(2, end);
  EXPECT_STREQ(L"n", text);
  text.Reset();

  EXPECT_EQ(S_FALSE,
            text_field_obj->get_textAtOffset(text_len, IA2_TEXT_BOUNDARY_CHAR,
                                             &start, &end, text.Receive()));
  EXPECT_EQ(0, start);
  EXPECT_EQ(0, end);
  EXPECT_EQ(nullptr, text);
  text.Reset();

  EXPECT_EQ(S_FALSE,
            text_field_obj->get_textAtOffset(text_len, IA2_TEXT_BOUNDARY_WORD,
                                             &start, &end, text.Receive()));
  EXPECT_EQ(0, start);
  EXPECT_EQ(0, end);
  EXPECT_EQ(nullptr, text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
                      1, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  EXPECT_EQ(0, start);
  EXPECT_EQ(4, end);
  EXPECT_STREQ(L"One ", text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
                      6, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  EXPECT_EQ(4, start);
  EXPECT_EQ(8, end);
  EXPECT_STREQ(L"two ", text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
                      text_len - 1, IA2_TEXT_BOUNDARY_WORD, &start, &end,
                      text.Receive()));
  EXPECT_EQ(25, start);
  EXPECT_EQ(29, end);
  EXPECT_STREQ(L"six.", text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
                      1, IA2_TEXT_BOUNDARY_LINE, &start, &end, text.Receive()));
  EXPECT_EQ(0, start);
  EXPECT_EQ(15, end);
  EXPECT_STREQ(L"One two three.\n", text);
  text.Reset();

  EXPECT_EQ(S_OK,
            text_field_obj->get_textAtOffset(text_len, IA2_TEXT_BOUNDARY_LINE,
                                             &start, &end, text.Receive()));
  EXPECT_EQ(15, start);
  EXPECT_EQ(text_len, end);
  EXPECT_STREQ(L"Four five six.", text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_text(0, IA2_TEXT_OFFSET_LENGTH,
                                           text.Receive()));
  EXPECT_EQ(text_value, base::UTF16ToUTF8(base::string16(text)));

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestSimpleHypertext) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";
  const LONG text_name_len = text1_name.length() + text2_name.length();

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

  BrowserAccessibilityComWin* root_obj =
      ToBrowserAccessibilityWin(manager->GetRoot())->GetCOM();

  LONG text_len;
  EXPECT_EQ(S_OK, root_obj->get_nCharacters(&text_len));
  EXPECT_EQ(text_name_len, text_len);

  base::win::ScopedBstr text;
  EXPECT_EQ(S_OK, root_obj->get_text(0, text_name_len, text.Receive()));
  EXPECT_EQ(text1_name + text2_name, base::UTF16ToUTF8(base::string16(text)));

  LONG hyperlink_count;
  EXPECT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(0, hyperlink_count);

  Microsoft::WRL::ComPtr<IAccessibleHyperlink> hyperlink;
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlink(-1, hyperlink.GetAddressOf()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(0, hyperlink.GetAddressOf()));
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlink(text_name_len, hyperlink.GetAddressOf()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(text_name_len + 1,
                                                  hyperlink.GetAddressOf()));

  LONG hyperlink_index;
  EXPECT_EQ(S_FALSE, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  // Invalid arguments should not be modified.
  hyperlink_index = -2;
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlinkIndex(text_name_len, &hyperlink_index));
  EXPECT_EQ(-2, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlinkIndex(-1, &hyperlink_index));
  EXPECT_EQ(-2, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlinkIndex(text_name_len + 1, &hyperlink_index));
  EXPECT_EQ(-2, hyperlink_index);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestComplexHypertext) {
  const base::string16 text1_name = L"One two three.";
  const base::string16 combo_box_name = L"City:";
  const base::string16 combo_box_value = L"Happyland";
  const base::string16 text2_name = L" Four five six.";
  const base::string16 check_box_name = L"I agree";
  const base::string16 check_box_value = L"Checked";
  const base::string16 button_text_name = L"Red";
  const base::string16 link_text_name = L"Blue";
  // Each control (combo / check box, button and link) will be represented by an
  // embedded object character.
  const base::string16 embed(1, BrowserAccessibilityComWin::kEmbeddedCharacter);
  const base::string16 root_hypertext =
      text1_name + embed + text2_name + embed + embed + embed;
  const LONG root_hypertext_len = root_hypertext.length();

  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName(base::UTF16ToUTF8(text1_name));

  ui::AXNodeData combo_box;
  combo_box.id = 12;
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.SetName(base::UTF16ToUTF8(combo_box_name));
  combo_box.SetValue(base::UTF16ToUTF8(combo_box_value));

  ui::AXNodeData text2;
  text2.id = 13;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName(base::UTF16ToUTF8(text2_name));

  ui::AXNodeData check_box;
  check_box.id = 14;
  check_box.role = ax::mojom::Role::kCheckBox;
  check_box.SetCheckedState(ax::mojom::CheckedState::kTrue);
  check_box.SetName(base::UTF16ToUTF8(check_box_name));
  check_box.SetValue(base::UTF16ToUTF8(check_box_value));

  ui::AXNodeData button, button_text;
  button.id = 15;
  button_text.id = 17;
  button_text.SetName(base::UTF16ToUTF8(button_text_name));
  button.role = ax::mojom::Role::kButton;
  button_text.role = ax::mojom::Role::kStaticText;
  button.child_ids.push_back(button_text.id);

  ui::AXNodeData link, link_text;
  link.id = 16;
  link_text.id = 18;
  link_text.SetName(base::UTF16ToUTF8(link_text_name));
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
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibilityComWin* root_obj =
      ToBrowserAccessibilityWin(manager->GetRoot())->GetCOM();

  LONG text_len;
  EXPECT_EQ(S_OK, root_obj->get_nCharacters(&text_len));
  EXPECT_EQ(root_hypertext_len, text_len);

  base::win::ScopedBstr text;
  EXPECT_EQ(S_OK, root_obj->get_text(0, root_hypertext_len, text.Receive()));
  EXPECT_STREQ(root_hypertext.c_str(), text);
  text.Reset();

  LONG hyperlink_count;
  EXPECT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(4, hyperlink_count);

  Microsoft::WRL::ComPtr<IAccessibleHyperlink> hyperlink;
  Microsoft::WRL::ComPtr<IAccessibleText> hypertext;
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlink(-1, hyperlink.GetAddressOf()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(4, hyperlink.GetAddressOf()));

  // Get the text of the combo box.
  // It should be its value.
  EXPECT_EQ(S_OK, root_obj->get_hyperlink(0, hyperlink.GetAddressOf()));
  EXPECT_EQ(S_OK, hyperlink.CopyTo(hypertext.GetAddressOf()));
  EXPECT_EQ(S_OK,
            hypertext->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
  EXPECT_STREQ(combo_box_value.c_str(), text);
  text.Reset();
  hyperlink.Reset();
  hypertext.Reset();

  // Get the text of the check box.
  // It should be its name.
  EXPECT_EQ(S_OK, root_obj->get_hyperlink(1, hyperlink.GetAddressOf()));
  EXPECT_EQ(S_OK, hyperlink.CopyTo(hypertext.GetAddressOf()));
  EXPECT_EQ(S_OK,
            hypertext->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
  EXPECT_STREQ(check_box_name.c_str(), text);
  text.Reset();
  hyperlink.Reset();
  hypertext.Reset();

  // Get the text of the button.
  EXPECT_EQ(S_OK, root_obj->get_hyperlink(2, hyperlink.GetAddressOf()));
  EXPECT_EQ(S_OK, hyperlink.CopyTo(hypertext.GetAddressOf()));
  EXPECT_EQ(S_FALSE,
            hypertext->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
  EXPECT_EQ(nullptr, text);
  text.Reset();
  hyperlink.Reset();
  hypertext.Reset();

  // Get the text of the link.
  EXPECT_EQ(S_OK, root_obj->get_hyperlink(3, hyperlink.GetAddressOf()));
  EXPECT_EQ(S_OK, hyperlink.CopyTo(hypertext.GetAddressOf()));
  EXPECT_EQ(S_OK, hypertext->get_text(0, 4, text.Receive()));
  EXPECT_STREQ(link_text_name.c_str(), text);
  text.Reset();
  hyperlink.Reset();
  hypertext.Reset();

  LONG hyperlink_index;
  EXPECT_EQ(S_FALSE, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  // Invalid arguments should not be modified.
  hyperlink_index = -2;
  EXPECT_EQ(E_INVALIDARG,
            root_obj->get_hyperlinkIndex(root_hypertext_len, &hyperlink_index));
  EXPECT_EQ(-2, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(14, &hyperlink_index));
  EXPECT_EQ(0, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(30, &hyperlink_index));
  EXPECT_EQ(1, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(31, &hyperlink_index));
  EXPECT_EQ(2, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(32, &hyperlink_index));
  EXPECT_EQ(3, hyperlink_index);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestCreateEmptyDocument) {
  // Try creating an empty document with busy state. Readonly is
  // set automatically.
  std::unique_ptr<BrowserAccessibilityManager> manager(
      new BrowserAccessibilityManagerWin(
          BrowserAccessibilityManagerWin::GetEmptyDocument(),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  // Verify the root is as we expect by default.
  BrowserAccessibility* root = manager->GetRoot();
  EXPECT_EQ(1, root->GetId());
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, root->GetRole());
  EXPECT_EQ(0, root->GetState());
  EXPECT_EQ(true, root->GetBoolAttribute(ax::mojom::BoolAttribute::kBusy));

  // Tree with a child textfield.
  ui::AXNodeData tree1_1;
  tree1_1.id = 1;
  tree1_1.role = ax::mojom::Role::kRootWebArea;
  tree1_1.child_ids.push_back(2);

  ui::AXNodeData tree1_2;
  tree1_2.id = 2;
  tree1_2.role = ax::mojom::Role::kTextField;

  // Process a load complete.
  AXEventNotificationDetails event_bundle;
  event_bundle.updates.resize(1);
  event_bundle.updates[0].node_id_to_clear = root->GetId();
  event_bundle.updates[0].root_id = tree1_1.id;
  event_bundle.updates[0].nodes.push_back(tree1_1);
  event_bundle.updates[0].nodes.push_back(tree1_2);
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  // The root should have been cleared,not replaced, because in the former case
  // this could cause multiple focus and load complete events.
  EXPECT_EQ(root, manager->GetRoot());

  BrowserAccessibility* acc1_2 = manager->GetFromID(2);
  EXPECT_EQ(ax::mojom::Role::kTextField, acc1_2->GetRole());
  EXPECT_EQ(2, acc1_2->GetId());

  // Tree with a child button.
  ui::AXNodeData tree2_1;
  tree2_1.id = 1;
  tree2_1.role = ax::mojom::Role::kRootWebArea;
  tree2_1.child_ids.push_back(3);

  ui::AXNodeData tree2_2;
  tree2_2.id = 3;
  tree2_2.role = ax::mojom::Role::kButton;

  event_bundle.updates[0].nodes.clear();
  event_bundle.updates[0].node_id_to_clear = tree1_1.id;
  event_bundle.updates[0].root_id = tree2_1.id;
  event_bundle.updates[0].nodes.push_back(tree2_1);
  event_bundle.updates[0].nodes.push_back(tree2_2);

  // Unserialize the new tree update.
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  // Verify that the root has been cleared, not replaced.
  EXPECT_EQ(root, manager->GetRoot());

  BrowserAccessibility* acc2_2 = manager->GetFromID(3);
  EXPECT_EQ(ax::mojom::Role::kButton, acc2_2->GetRole());
  EXPECT_EQ(3, acc2_2->GetId());

  // Ensure we properly cleaned up.
  manager.reset();
}

int32_t GetUniqueId(BrowserAccessibility* accessibility) {
  BrowserAccessibilityWin* win_root = ToBrowserAccessibilityWin(accessibility);
  return win_root->GetCOM()->GetUniqueId();
}

// This is a regression test for a bug where the initial empty document
// loaded by a BrowserAccessibilityManagerWin couldn't be looked up by
// its UniqueIDWin, because the AX Tree was loaded in
// BrowserAccessibilityManager code before BrowserAccessibilityManagerWin
// was initialized.
TEST_F(BrowserAccessibilityWinTest, EmptyDocHasUniqueIdWin) {
  std::unique_ptr<BrowserAccessibilityManagerWin> manager(
      new BrowserAccessibilityManagerWin(
          BrowserAccessibilityManagerWin::GetEmptyDocument(),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  // Verify the root is as we expect by default.
  BrowserAccessibility* root = manager->GetRoot();
  EXPECT_EQ(1, root->GetId());
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, root->GetRole());
  EXPECT_EQ(0, root->GetState());
  EXPECT_EQ(true, root->GetBoolAttribute(ax::mojom::BoolAttribute::kBusy));

  BrowserAccessibilityWin* win_root = ToBrowserAccessibilityWin(root);

  ui::AXPlatformNode* node = static_cast<ui::AXPlatformNode*>(
      ui::AXPlatformNodeWin::GetFromUniqueId(GetUniqueId(win_root)));

  ui::AXPlatformNode* other_node =
      static_cast<ui::AXPlatformNode*>(win_root->GetCOM());
  ASSERT_EQ(node, other_node);
}

TEST_F(BrowserAccessibilityWinTest, TestIA2Attributes) {
  ui::AXNodeData pseudo_before;
  pseudo_before.id = 2;
  pseudo_before.role = ax::mojom::Role::kGenericContainer;
  pseudo_before.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                   "<pseudo:before>");
  pseudo_before.AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                   "none");

  ui::AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.SetName("Checkbox");
  checkbox.role = ax::mojom::Role::kCheckBox;
  checkbox.SetCheckedState(ax::mojom::CheckedState::kTrue);

  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, pseudo_before, checkbox),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* pseudo_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, pseudo_accessible);

  base::win::ScopedBstr attributes;
  HRESULT hr =
      pseudo_accessible->GetCOM()->get_attributes(attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_NE(nullptr, static_cast<BSTR>(attributes));
  std::wstring attributes_str(attributes, attributes.Length());
  EXPECT_EQ(L"display:none;tag:<pseudo\\:before>;", attributes_str);

  BrowserAccessibilityWin* checkbox_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, checkbox_accessible);

  attributes.Reset();
  hr = checkbox_accessible->GetCOM()->get_attributes(attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_NE(nullptr, static_cast<BSTR>(attributes));
  attributes_str = std::wstring(attributes, attributes.Length());
  EXPECT_EQ(L"checkable:true;", attributes_str);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestValueAttributeInTextControls) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData combo_box, combo_box_text;
  combo_box.id = 2;
  combo_box_text.id = 3;
  combo_box.SetName("Combo box:");
  combo_box_text.SetName("Combo box text");
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box_text.role = ax::mojom::Role::kStaticText;
  combo_box.AddBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot, true);
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.AddState(ax::mojom::State::kRichlyEditable);
  combo_box.AddState(ax::mojom::State::kFocusable);
  combo_box_text.AddState(ax::mojom::State::kEditable);
  combo_box_text.AddState(ax::mojom::State::kRichlyEditable);
  combo_box.child_ids.push_back(combo_box_text.id);

  ui::AXNodeData search_box, search_box_text, new_line;
  search_box.id = 4;
  search_box_text.id = 5;
  new_line.id = 6;
  search_box.SetName("Search for:");
  search_box_text.SetName("Search box text");
  new_line.SetName("\n");
  search_box.role = ax::mojom::Role::kSearchBox;
  search_box_text.role = ax::mojom::Role::kStaticText;
  new_line.role = ax::mojom::Role::kLineBreak;
  search_box.AddBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot, true);
  search_box.AddState(ax::mojom::State::kEditable);
  search_box.AddState(ax::mojom::State::kRichlyEditable);
  search_box.AddState(ax::mojom::State::kFocusable);
  search_box_text.AddState(ax::mojom::State::kEditable);
  search_box_text.AddState(ax::mojom::State::kRichlyEditable);
  new_line.AddState(ax::mojom::State::kEditable);
  new_line.AddState(ax::mojom::State::kRichlyEditable);
  search_box.child_ids.push_back(search_box_text.id);
  search_box.child_ids.push_back(new_line.id);

  ui::AXNodeData text_field;
  text_field.id = 7;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot, true);
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddState(ax::mojom::State::kFocusable);
  text_field.SetValue("Text field text");

  ui::AXNodeData link, link_text;
  link.id = 8;
  link_text.id = 9;
  link_text.SetName("Link text");
  link.role = ax::mojom::Role::kLink;
  link_text.role = ax::mojom::Role::kStaticText;
  link.child_ids.push_back(link_text.id);

  ui::AXNodeData slider, slider_text;
  slider.id = 10;
  slider_text.id = 11;
  slider.AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, 5.0F);
  slider_text.SetName("Slider text");
  slider.role = ax::mojom::Role::kSlider;
  slider_text.role = ax::mojom::Role::kStaticText;
  slider.child_ids.push_back(slider_text.id);

  root.child_ids.push_back(2);   // Combo box.
  root.child_ids.push_back(4);   // Search box.
  root.child_ids.push_back(7);   // Text field.
  root.child_ids.push_back(8);   // Link.
  root.child_ids.push_back(10);  // Slider.

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, combo_box, combo_box_text, search_box,
                           search_box_text, new_line, text_field, link,
                           link_text, slider, slider_text),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(5U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* combo_box_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, combo_box_accessible);
  manager->SetFocusLocallyForTesting(combo_box_accessible);
  ASSERT_EQ(combo_box_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));
  BrowserAccessibilityWin* search_box_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, search_box_accessible);
  BrowserAccessibilityWin* text_field_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(2));
  ASSERT_NE(nullptr, text_field_accessible);
  BrowserAccessibilityWin* link_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(3));
  ASSERT_NE(nullptr, link_accessible);
  BrowserAccessibilityWin* slider_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(4));
  ASSERT_NE(nullptr, slider_accessible);

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedVariant childid_slider(5);
  base::win::ScopedBstr value;

  HRESULT hr = combo_box_accessible->GetCOM()->get_accValue(childid_self,
                                                            value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_STREQ(L"Combo box text", value);
  value.Reset();
  hr = search_box_accessible->GetCOM()->get_accValue(childid_self,
                                                     value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_STREQ(L"Search box text\n", value);
  value.Reset();
  hr = text_field_accessible->GetCOM()->get_accValue(childid_self,
                                                     value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_STREQ(L"Text field text", value);
  value.Reset();

  // Other controls, such as links, should not use their inner text as their
  // value. Only text entry controls.
  hr = link_accessible->GetCOM()->get_accValue(childid_self, value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0u, value.Length());
  value.Reset();

  // Sliders and other range controls should expose their current value and not
  // their inner text.
  // Also, try accessing the slider via its child number instead of directly.
  hr = root_accessible->GetCOM()->get_accValue(childid_slider, value.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_STREQ(L"5", value);
  value.Reset();

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestWordBoundariesInTextControls) {
  const base::string16 line1(L"This is a very LONG line of text that ");
  const base::string16 line2(L"should wrap on more than one lines ");
  const base::string16 text(line1 + line2);

  std::vector<int32_t> line1_word_starts;
  line1_word_starts.push_back(0);
  line1_word_starts.push_back(5);
  line1_word_starts.push_back(8);
  line1_word_starts.push_back(10);
  line1_word_starts.push_back(15);
  line1_word_starts.push_back(20);
  line1_word_starts.push_back(25);
  line1_word_starts.push_back(28);
  line1_word_starts.push_back(33);

  std::vector<int32_t> line2_word_starts;
  line2_word_starts.push_back(0);
  line2_word_starts.push_back(7);
  line2_word_starts.push_back(12);
  line2_word_starts.push_back(15);
  line2_word_starts.push_back(20);
  line2_word_starts.push_back(25);
  line2_word_starts.push_back(29);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData textarea, textarea_div, textarea_text;
  textarea.id = 2;
  textarea_div.id = 3;
  textarea_text.id = 4;
  textarea.role = ax::mojom::Role::kTextField;
  textarea_div.role = ax::mojom::Role::kGenericContainer;
  textarea_text.role = ax::mojom::Role::kStaticText;
  textarea.AddState(ax::mojom::State::kEditable);
  textarea.AddState(ax::mojom::State::kFocusable);
  textarea.AddState(ax::mojom::State::kMultiline);
  textarea_div.AddState(ax::mojom::State::kEditable);
  textarea_text.AddState(ax::mojom::State::kEditable);
  textarea.SetValue(base::UTF16ToUTF8(text));
  textarea_text.SetName(base::UTF16ToUTF8(text));
  textarea.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "textarea");
  textarea.child_ids.push_back(textarea_div.id);
  textarea_div.child_ids.push_back(textarea_text.id);

  ui::AXNodeData textarea_line1, textarea_line2;
  textarea_line1.id = 5;
  textarea_line2.id = 6;
  textarea_line1.role = ax::mojom::Role::kInlineTextBox;
  textarea_line2.role = ax::mojom::Role::kInlineTextBox;
  textarea_line1.AddState(ax::mojom::State::kEditable);
  textarea_line2.AddState(ax::mojom::State::kEditable);
  textarea_line1.SetName(base::UTF16ToUTF8(line1));
  textarea_line2.SetName(base::UTF16ToUTF8(line2));
  textarea_line1.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                     line1_word_starts);
  textarea_line2.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                     line2_word_starts);
  textarea_text.child_ids.push_back(textarea_line1.id);
  textarea_text.child_ids.push_back(textarea_line2.id);

  ui::AXNodeData text_field, text_field_div, text_field_text;
  text_field.id = 7;
  text_field_div.id = 8;
  text_field_text.id = 9;
  text_field.role = ax::mojom::Role::kTextField;
  text_field_div.role = ax::mojom::Role::kGenericContainer;
  text_field_text.role = ax::mojom::Role::kStaticText;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddState(ax::mojom::State::kFocusable);
  text_field_div.AddState(ax::mojom::State::kEditable);
  text_field_text.AddState(ax::mojom::State::kEditable);
  text_field.SetValue(base::UTF16ToUTF8(line1));
  text_field_text.SetName(base::UTF16ToUTF8(line1));
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  text_field.html_attributes.push_back(std::make_pair("type", "text"));
  text_field.child_ids.push_back(text_field_div.id);
  text_field_div.child_ids.push_back(text_field_text.id);

  ui::AXNodeData text_field_line;
  text_field_line.id = 10;
  text_field_line.role = ax::mojom::Role::kInlineTextBox;
  text_field_line.AddState(ax::mojom::State::kEditable);
  text_field_line.SetName(base::UTF16ToUTF8(line1));
  text_field_line.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                      line1_word_starts);
  text_field_text.child_ids.push_back(text_field_line.id);

  root.child_ids.push_back(2);  // Textarea.
  root.child_ids.push_back(7);  // Text field.

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, textarea, textarea_div, textarea_text,
                           textarea_line1, textarea_line2, text_field,
                           text_field_div, text_field_text, text_field_line),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* textarea_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, textarea_accessible);
  BrowserAccessibilityWin* text_field_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, text_field_accessible);

  Microsoft::WRL::ComPtr<IAccessibleText> textarea_object;
  EXPECT_HRESULT_SUCCEEDED(textarea_accessible->GetCOM()->QueryInterface(
      IID_IAccessibleText,
      reinterpret_cast<void**>(textarea_object.GetAddressOf())));
  Microsoft::WRL::ComPtr<IAccessibleText> text_field_object;
  EXPECT_HRESULT_SUCCEEDED(text_field_accessible->GetCOM()->QueryInterface(
      IID_IAccessibleText,
      reinterpret_cast<void**>(text_field_object.GetAddressOf())));

  LONG offset = 0;
  while (offset < static_cast<LONG>(text.length())) {
    LONG start, end;
    base::win::ScopedBstr word;
    EXPECT_EQ(S_OK,
              textarea_object->get_textAtOffset(offset, IA2_TEXT_BOUNDARY_WORD,
                                                &start, &end, word.Receive()));
    EXPECT_EQ(offset, start);
    EXPECT_LT(offset, end);
    LONG space_offset = static_cast<LONG>(text.find(' ', offset));
    EXPECT_EQ(space_offset + 1, end);
    LONG length = end - start;
    EXPECT_STREQ(text.substr(start, length).c_str(), word);
    word.Reset();
    offset = end;
  }

  offset = 0;
  while (offset < static_cast<LONG>(line1.length())) {
    LONG start, end;
    base::win::ScopedBstr word;
    EXPECT_EQ(S_OK, text_field_object->get_textAtOffset(
                        offset, IA2_TEXT_BOUNDARY_WORD, &start, &end,
                        word.Receive()));
    EXPECT_EQ(offset, start);
    EXPECT_LT(offset, end);
    LONG space_offset = static_cast<LONG>(line1.find(' ', offset));
    EXPECT_EQ(space_offset + 1, end);
    LONG length = end - start;
    EXPECT_STREQ(text.substr(start, length).c_str(), word);
    word.Reset();
    offset = end;
  }

  textarea_object.Reset();
  text_field_object.Reset();

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestCaretAndSelectionInSimpleFields) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData combo_box;
  combo_box.id = 2;
  combo_box.role = ax::mojom::Role::kTextFieldWithComboBox;
  combo_box.AddState(ax::mojom::State::kEditable);
  combo_box.AddState(ax::mojom::State::kFocusable);
  combo_box.SetValue("Test1");
  // Place the caret between 't' and 'e'.
  combo_box.AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart, 1);
  combo_box.AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, 1);

  ui::AXNodeData text_field;
  text_field.id = 3;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddState(ax::mojom::State::kFocusable);
  text_field.SetValue("Test2");
  // Select the letter 'e'.
  text_field.AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart, 1);
  text_field.AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, 2);

  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, combo_box, text_field),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* combo_box_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, combo_box_accessible);
  manager->SetFocusLocallyForTesting(combo_box_accessible);
  ASSERT_EQ(combo_box_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));
  BrowserAccessibilityWin* text_field_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, text_field_accessible);

  // -2 is never a valid offset.
  LONG caret_offset = -2;
  LONG n_selections = -2;
  LONG selection_start = -2;
  LONG selection_end = -2;

  // Test get_caretOffset.
  HRESULT hr = combo_box_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, caret_offset);
  // The caret should be at the end of the selection.
  hr = text_field_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2, caret_offset);

  // Move the focus to the text field.
  manager->SetFocusLocallyForTesting(text_field_accessible);
  ASSERT_EQ(text_field_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));

  // The caret should not have moved.
  hr = text_field_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2, caret_offset);

  // Test get_nSelections.
  hr = combo_box_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);
  hr = text_field_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);

  // Test get_selection.
  hr = combo_box_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(E_INVALIDARG, hr);  // No selections available.
  hr = text_field_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, selection_start);
  EXPECT_EQ(2, selection_end);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestCaretInContentEditables) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData div_editable;
  div_editable.id = 2;
  div_editable.role = ax::mojom::Role::kGenericContainer;
  div_editable.AddState(ax::mojom::State::kEditable);
  div_editable.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData text;
  text.id = 3;
  text.role = ax::mojom::Role::kStaticText;
  text.AddState(ax::mojom::State::kEditable);
  text.SetName("Click ");

  ui::AXNodeData link;
  link.id = 4;
  link.role = ax::mojom::Role::kLink;
  link.AddState(ax::mojom::State::kEditable);
  link.AddState(ax::mojom::State::kFocusable);
  link.AddState(ax::mojom::State::kLinked);
  link.SetName("here");

  ui::AXNodeData link_text;
  link_text.id = 5;
  link_text.role = ax::mojom::Role::kStaticText;
  link_text.AddState(ax::mojom::State::kEditable);
  link_text.AddState(ax::mojom::State::kFocusable);
  link_text.AddState(ax::mojom::State::kLinked);
  link_text.SetName("here");

  root.child_ids.push_back(2);
  div_editable.child_ids.push_back(3);
  div_editable.child_ids.push_back(4);
  link.child_ids.push_back(5);

  ui::AXTreeUpdate update =
      MakeAXTreeUpdate(root, div_editable, link, link_text, text);

  // Place the caret between 'h' and 'e'.
  update.has_tree_data = true;
  update.tree_data.sel_anchor_object_id = 5;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_focus_offset = 1;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update, test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* div_editable_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, div_editable_accessible);
  ASSERT_EQ(2U, div_editable_accessible->PlatformChildCount());

  // -2 is never a valid offset.
  LONG caret_offset = -2;
  LONG n_selections = -2;

  // No selection should be present.
  HRESULT hr =
      div_editable_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);

  // The caret should be on the embedded object character.
  hr = div_editable_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(6, caret_offset);

  // Move the focus to the content editable.
  manager->SetFocusLocallyForTesting(div_editable_accessible);
  ASSERT_EQ(div_editable_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));

  BrowserAccessibilityWin* text_accessible =
      ToBrowserAccessibilityWin(div_editable_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, text_accessible);
  BrowserAccessibilityWin* link_accessible =
      ToBrowserAccessibilityWin(div_editable_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, link_accessible);
  ASSERT_EQ(1U, link_accessible->PlatformChildCount());

  BrowserAccessibilityWin* link_text_accessible =
      ToBrowserAccessibilityWin(link_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, link_text_accessible);

  // The caret should not have moved.
  hr = div_editable_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);
  hr = div_editable_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(6, caret_offset);

  hr = link_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);
  hr = link_text_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, n_selections);

  hr = link_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, caret_offset);
  hr = link_text_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, caret_offset);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestSelectionInContentEditables) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData div_editable;
  div_editable.id = 2;
  div_editable.role = ax::mojom::Role::kGenericContainer;
  div_editable.AddState(ax::mojom::State::kFocusable);
  div_editable.AddState(ax::mojom::State::kEditable);

  ui::AXNodeData text;
  text.id = 3;
  text.role = ax::mojom::Role::kStaticText;
  text.AddState(ax::mojom::State::kFocusable);
  text.AddState(ax::mojom::State::kEditable);
  text.SetName("Click ");

  ui::AXNodeData link;
  link.id = 4;
  link.role = ax::mojom::Role::kLink;
  link.AddState(ax::mojom::State::kFocusable);
  link.AddState(ax::mojom::State::kEditable);
  link.AddState(ax::mojom::State::kLinked);
  link.SetName("here");

  ui::AXNodeData link_text;
  link_text.id = 5;
  link_text.role = ax::mojom::Role::kStaticText;
  link_text.AddState(ax::mojom::State::kFocusable);
  link_text.AddState(ax::mojom::State::kEditable);
  link_text.AddState(ax::mojom::State::kLinked);
  link_text.SetName("here");

  root.child_ids.push_back(2);
  div_editable.child_ids.push_back(3);
  div_editable.child_ids.push_back(4);
  link.child_ids.push_back(5);

  ui::AXTreeUpdate update =
      MakeAXTreeUpdate(root, div_editable, link, link_text, text);

  // Select the following part of the text: "lick here".
  update.has_tree_data = true;
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_focus_offset = 4;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update, test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* div_editable_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, div_editable_accessible);
  ASSERT_EQ(2U, div_editable_accessible->PlatformChildCount());

  // -2 is never a valid offset.
  LONG caret_offset = -2;
  LONG n_selections = -2;
  LONG selection_start = -2;
  LONG selection_end = -2;

  BrowserAccessibilityWin* text_accessible =
      ToBrowserAccessibilityWin(div_editable_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, text_accessible);
  BrowserAccessibilityWin* link_accessible =
      ToBrowserAccessibilityWin(div_editable_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, link_accessible);
  ASSERT_EQ(1U, link_accessible->PlatformChildCount());

  BrowserAccessibilityWin* link_text_accessible =
      ToBrowserAccessibilityWin(link_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, link_text_accessible);

  // get_nSelections should work on all objects.
  HRESULT hr =
      div_editable_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);
  hr = text_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);
  hr = link_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);
  hr = link_text_accessible->GetCOM()->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_selections);

  // get_selection should be unaffected by focus placement.
  hr = div_editable_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, selection_start);
  // selection_end should be after embedded object character.
  EXPECT_EQ(7, selection_end);

  hr = text_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, selection_start);
  // No embedded character on this object, only the first part of the text.
  EXPECT_EQ(6, selection_end);
  hr = link_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, selection_start);
  EXPECT_EQ(4, selection_end);
  hr = link_text_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, selection_start);
  EXPECT_EQ(4, selection_end);

  // The caret should be at the focus (the end) of the selection.
  hr = div_editable_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(7, caret_offset);

  // Move the focus to the content editable.
  manager->SetFocusLocallyForTesting(div_editable_accessible);
  ASSERT_EQ(div_editable_accessible,
            ToBrowserAccessibilityWin(manager->GetFocus()));

  // The caret should not have moved.
  hr = div_editable_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(7, caret_offset);

  // The caret offset should reflect the position of the selection's focus in
  // any given object.
  hr = link_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(4, caret_offset);
  hr = link_text_accessible->GetCOM()->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(4, caret_offset);

  hr = div_editable_accessible->GetCOM()->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, selection_start);
  EXPECT_EQ(7, selection_end);

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestIAccessibleHyperlink) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData div;
  div.id = 2;
  div.role = ax::mojom::Role::kGenericContainer;
  div.AddState(ax::mojom::State::kFocusable);

  ui::AXNodeData text;
  text.id = 3;
  text.role = ax::mojom::Role::kStaticText;
  text.SetName("Click ");

  ui::AXNodeData link;
  link.id = 4;
  link.role = ax::mojom::Role::kLink;
  link.AddState(ax::mojom::State::kFocusable);
  link.AddState(ax::mojom::State::kLinked);
  link.SetName("here");
  link.AddStringAttribute(ax::mojom::StringAttribute::kUrl, "example.com");

  root.child_ids.push_back(2);
  div.child_ids.push_back(3);
  div.child_ids.push_back(4);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, div, link, text),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* div_accessible =
      ToBrowserAccessibilityWin(root_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, div_accessible);
  ASSERT_EQ(2U, div_accessible->PlatformChildCount());

  BrowserAccessibilityWin* text_accessible =
      ToBrowserAccessibilityWin(div_accessible->PlatformGetChild(0));
  ASSERT_NE(nullptr, text_accessible);
  BrowserAccessibilityWin* link_accessible =
      ToBrowserAccessibilityWin(div_accessible->PlatformGetChild(1));
  ASSERT_NE(nullptr, link_accessible);

  // -1 is never a valid value.
  LONG n_actions = -1;
  LONG start_index = -1;
  LONG end_index = -1;

  Microsoft::WRL::ComPtr<IAccessibleHyperlink> hyperlink;
  base::win::ScopedVariant anchor;
  base::win::ScopedVariant anchor_target;
  base::win::ScopedBstr bstr;

  base::string16 div_hypertext(L"Click ");
  div_hypertext.push_back(BrowserAccessibilityComWin::kEmbeddedCharacter);

  // div_accessible and link_accessible are the only IA2 hyperlinks.
  EXPECT_HRESULT_FAILED(
      root_accessible->GetCOM()->QueryInterface(IID_PPV_ARGS(&hyperlink)));
  hyperlink.Reset();
  EXPECT_HRESULT_SUCCEEDED(
      div_accessible->GetCOM()->QueryInterface(IID_PPV_ARGS(&hyperlink)));
  hyperlink.Reset();
  EXPECT_HRESULT_FAILED(
      text_accessible->GetCOM()->QueryInterface(IID_PPV_ARGS(&hyperlink)));
  hyperlink.Reset();
  EXPECT_HRESULT_SUCCEEDED(
      link_accessible->GetCOM()->QueryInterface(IID_PPV_ARGS(&hyperlink)));
  hyperlink.Reset();

  EXPECT_HRESULT_SUCCEEDED(root_accessible->GetCOM()->nActions(&n_actions));
  EXPECT_EQ(0, n_actions);
  EXPECT_HRESULT_SUCCEEDED(div_accessible->GetCOM()->nActions(&n_actions));
  EXPECT_EQ(1, n_actions);
  EXPECT_HRESULT_SUCCEEDED(text_accessible->GetCOM()->nActions(&n_actions));
  EXPECT_EQ(0, n_actions);
  EXPECT_HRESULT_SUCCEEDED(link_accessible->GetCOM()->nActions(&n_actions));
  EXPECT_EQ(1, n_actions);

  EXPECT_HRESULT_FAILED(
      root_accessible->GetCOM()->get_anchor(0, anchor.Receive()));
  anchor.Reset();
  HRESULT hr = div_accessible->GetCOM()->get_anchor(0, anchor.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(VT_BSTR, anchor.type());
  bstr.Reset(V_BSTR(anchor.ptr()));
  EXPECT_STREQ(div_hypertext.c_str(), bstr);
  bstr.Reset();
  anchor.Reset();
  EXPECT_HRESULT_FAILED(
      text_accessible->GetCOM()->get_anchor(0, anchor.Receive()));
  anchor.Reset();
  hr = link_accessible->GetCOM()->get_anchor(0, anchor.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(VT_BSTR, anchor.type());
  bstr.Reset(V_BSTR(anchor.ptr()));
  EXPECT_STREQ(L"here", bstr);
  bstr.Reset();
  anchor.Reset();
  EXPECT_HRESULT_FAILED(
      div_accessible->GetCOM()->get_anchor(1, anchor.Receive()));
  anchor.Reset();
  EXPECT_HRESULT_FAILED(
      link_accessible->GetCOM()->get_anchor(1, anchor.Receive()));
  anchor.Reset();

  EXPECT_HRESULT_FAILED(
      root_accessible->GetCOM()->get_anchorTarget(0, anchor_target.Receive()));
  anchor_target.Reset();
  hr = div_accessible->GetCOM()->get_anchorTarget(0, anchor_target.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(VT_BSTR, anchor_target.type());
  bstr.Reset(V_BSTR(anchor_target.ptr()));
  // Target should be empty.
  EXPECT_STREQ(L"", bstr);
  bstr.Reset();
  anchor_target.Reset();
  EXPECT_HRESULT_FAILED(
      text_accessible->GetCOM()->get_anchorTarget(0, anchor_target.Receive()));
  anchor_target.Reset();
  hr = link_accessible->GetCOM()->get_anchorTarget(0, anchor_target.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(VT_BSTR, anchor_target.type());
  bstr.Reset(V_BSTR(anchor_target.ptr()));
  EXPECT_STREQ(L"example.com", bstr);
  bstr.Reset();
  anchor_target.Reset();
  EXPECT_HRESULT_FAILED(
      div_accessible->GetCOM()->get_anchorTarget(1, anchor_target.Receive()));
  anchor_target.Reset();
  EXPECT_HRESULT_FAILED(
      link_accessible->GetCOM()->get_anchorTarget(1, anchor_target.Receive()));
  anchor_target.Reset();

  EXPECT_HRESULT_FAILED(
      root_accessible->GetCOM()->get_startIndex(&start_index));
  EXPECT_HRESULT_SUCCEEDED(
      div_accessible->GetCOM()->get_startIndex(&start_index));
  EXPECT_EQ(0, start_index);
  EXPECT_HRESULT_FAILED(
      text_accessible->GetCOM()->get_startIndex(&start_index));
  EXPECT_HRESULT_SUCCEEDED(
      link_accessible->GetCOM()->get_startIndex(&start_index));
  EXPECT_EQ(6, start_index);

  EXPECT_HRESULT_FAILED(root_accessible->GetCOM()->get_endIndex(&end_index));
  EXPECT_HRESULT_SUCCEEDED(div_accessible->GetCOM()->get_endIndex(&end_index));
  EXPECT_EQ(1, end_index);
  EXPECT_HRESULT_FAILED(text_accessible->GetCOM()->get_endIndex(&end_index));
  EXPECT_HRESULT_SUCCEEDED(link_accessible->GetCOM()->get_endIndex(&end_index));
  EXPECT_EQ(7, end_index);
}

TEST_F(BrowserAccessibilityWinTest, TestTextAttributesInContentEditables) {
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
  div_editable.child_ids.push_back(text_before.id);
  div_editable.child_ids.push_back(link.id);
  div_editable.child_ids.push_back(text_after.id);
  link.child_ids.push_back(link_text.id);

  ui::AXTreeUpdate update = MakeAXTreeUpdate(root, div_editable, text_before,
                                             link, link_text, text_after);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update, test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* ax_root =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibilityWin* ax_div =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(3U, ax_div->PlatformChildCount());

  BrowserAccessibilityWin* ax_before =
      ToBrowserAccessibilityWin(ax_div->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_before);
  BrowserAccessibilityWin* ax_link =
      ToBrowserAccessibilityWin(ax_div->PlatformGetChild(1));
  ASSERT_NE(nullptr, ax_link);
  ASSERT_EQ(1U, ax_link->PlatformChildCount());
  BrowserAccessibilityWin* ax_after =
      ToBrowserAccessibilityWin(ax_div->PlatformGetChild(2));
  ASSERT_NE(nullptr, ax_after);

  BrowserAccessibilityWin* ax_link_text =
      ToBrowserAccessibilityWin(ax_link->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_link_text);

  HRESULT hr;
  LONG n_characters, start_offset, end_offset;
  base::win::ScopedBstr text_attributes;

  ASSERT_HRESULT_SUCCEEDED(ax_root->GetCOM()->get_nCharacters(&n_characters));
  ASSERT_EQ(1, n_characters);
  ASSERT_HRESULT_SUCCEEDED(ax_div->GetCOM()->get_nCharacters(&n_characters));
  ASSERT_EQ(15, n_characters);

  // Test the style of the root.
  hr = ax_root->GetCOM()->get_attributes(0, &start_offset, &end_offset,
                                         text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(1, end_offset);
  EXPECT_NE(base::string16::npos,
            base::string16(text_attributes).find(L"font-family:Helvetica"));
  text_attributes.Reset();

  // Test the style of text_before.
  for (LONG offset = 0; offset < 7; ++offset) {
    hr = ax_div->GetCOM()->get_attributes(0, &start_offset, &end_offset,
                                          text_attributes.Receive());
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(7, end_offset);
    base::string16 attributes(text_attributes);
    EXPECT_NE(base::string16::npos, attributes.find(L"font-family:Helvetica"));
    EXPECT_NE(base::string16::npos, attributes.find(L"font-weight:bold"));
    EXPECT_NE(base::string16::npos, attributes.find(L"font-style:italic"));
    text_attributes.Reset();
  }

  // Test the style of the link.
  hr = ax_link->GetCOM()->get_attributes(0, &start_offset, &end_offset,
                                         text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(3, end_offset);
  EXPECT_NE(base::string16::npos,
            base::string16(text_attributes).find(L"font-family:Helvetica"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"font-weight:"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"font-style:"));
  EXPECT_NE(
      base::string16::npos,
      base::string16(text_attributes).find(L"text-underline-style:solid"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"text-underline-type:"));
  // For compatibility with Firefox, spelling attributes should also be
  // propagated to the parent of static text leaves.
  EXPECT_NE(base::string16::npos,
            base::string16(text_attributes).find(L"invalid:spelling"));
  text_attributes.Reset();

  hr = ax_link_text->GetCOM()->get_attributes(2, &start_offset, &end_offset,
                                              text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(3, end_offset);
  EXPECT_NE(base::string16::npos,
            base::string16(text_attributes).find(L"font-family:Helvetica"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"font-weight:"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"font-style:"));
  EXPECT_NE(
      base::string16::npos,
      base::string16(text_attributes).find(L"text-underline-style:solid"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"text-underline-type:"));
  EXPECT_NE(base::string16::npos,
            base::string16(text_attributes).find(L"invalid:spelling"));
  text_attributes.Reset();

  // Test the style of text_after.
  for (LONG offset = 8; offset < 15; ++offset) {
    hr = ax_div->GetCOM()->get_attributes(offset, &start_offset, &end_offset,
                                          text_attributes.Receive());
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(8, start_offset);
    EXPECT_EQ(15, end_offset);
    base::string16 attributes(text_attributes);
    EXPECT_NE(base::string16::npos, attributes.find(L"font-family:Helvetica"));
    EXPECT_EQ(base::string16::npos, attributes.find(L"font-weight:"));
    EXPECT_EQ(base::string16::npos, attributes.find(L"font-style:"));
    EXPECT_EQ(
        base::string16::npos,
        base::string16(text_attributes).find(L"text-underline-style:solid"));
    EXPECT_EQ(base::string16::npos,
              base::string16(text_attributes).find(L"text-underline-type:"));
    EXPECT_EQ(base::string16::npos, attributes.find(L"invalid:spelling"));
    text_attributes.Reset();
  }

  // Test the style of the static text nodes.
  hr = ax_before->GetCOM()->get_attributes(6, &start_offset, &end_offset,
                                           text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(7, end_offset);
  EXPECT_NE(base::string16::npos,
            base::string16(text_attributes).find(L"font-family:Helvetica"));
  EXPECT_NE(base::string16::npos,
            base::string16(text_attributes).find(L"font-weight:bold"));
  EXPECT_NE(base::string16::npos,
            base::string16(text_attributes).find(L"font-style:italic"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"invalid:spelling"));
  text_attributes.Reset();

  hr = ax_after->GetCOM()->get_attributes(6, &start_offset, &end_offset,
                                          text_attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(7, end_offset);
  EXPECT_NE(base::string16::npos,
            base::string16(text_attributes).find(L"font-family:Helvetica"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"font-weight:"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"font-style:"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"text-underline-style:"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"text-underline-type:"));
  EXPECT_EQ(base::string16::npos,
            base::string16(text_attributes).find(L"invalid:spelling"));
  text_attributes.Reset();

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest,
       TestExistingMisspellingsInSimpleTextFields) {
  std::string value1("Testing .");
  // The word "helo" is misspelled.
  std::string value2("Helo there.");

  LONG value1_length = static_cast<LONG>(value1.length());
  LONG value2_length = static_cast<LONG>(value2.length());
  LONG combo_box_value_length = value1_length + value2_length;

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
  BrowserAccessibilityWin* ax_root =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibilityWin* ax_combo_box =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_combo_box);

  HRESULT hr;
  LONG start_offset, end_offset;
  base::win::ScopedBstr text_attributes;

  // Ensure that the first part of the value is not marked misspelled.
  for (LONG offset = 0; offset < value1_length; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(base::string16(text_attributes).empty());
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(value1_length, end_offset);
    text_attributes.Reset();
  }

  // Ensure that "helo" is marked misspelled.
  for (LONG offset = value1_length; offset < value1_length + 4; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(value1_length, start_offset);
    EXPECT_EQ(value1_length + 4, end_offset);
    EXPECT_NE(base::string16::npos,
              base::string16(text_attributes).find(L"invalid:spelling"));
    text_attributes.Reset();
  }

  // Ensure that the last part of the value is not marked misspelled.
  for (LONG offset = value1_length + 4; offset < combo_box_value_length;
       ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(base::string16(text_attributes).empty());
    EXPECT_EQ(value1_length + 4, start_offset);
    EXPECT_EQ(combo_box_value_length, end_offset);
    text_attributes.Reset();
  }

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestNewMisspellingsInSimpleTextFields) {
  std::string value1("Testing .");
  // The word "helo" is misspelled.
  std::string value2("Helo there.");

  LONG value1_length = static_cast<LONG>(value1.length());
  LONG value2_length = static_cast<LONG>(value2.length());
  LONG combo_box_value_length = value1_length + value2_length;

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
  BrowserAccessibilityWin* ax_root =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibilityWin* ax_combo_box =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_combo_box);

  HRESULT hr;
  LONG start_offset, end_offset;
  base::win::ScopedBstr text_attributes;

  // Ensure that nothing is marked misspelled.
  for (LONG offset = 0; offset < combo_box_value_length; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(base::string16(text_attributes).empty());
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(combo_box_value_length, end_offset);
    text_attributes.Reset();
  }

  // Add the spelling markers on "helo".
  std::vector<int32_t> marker_types{
      static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)};
  std::vector<int32_t> marker_starts{0};
  std::vector<int32_t> marker_ends{4};
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes,
                                   marker_types);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                   marker_starts);
  static_text2.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                   marker_ends);
  ui::AXTree* tree = const_cast<ui::AXTree*>(manager->ax_tree());
  ASSERT_NE(nullptr, tree);
  ASSERT_TRUE(tree->Unserialize(MakeAXTreeUpdate(static_text2)));

  // Ensure that value1 is still not marked misspelled.
  for (LONG offset = 0; offset < value1_length; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(base::string16(text_attributes).empty());
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(value1_length, end_offset);
    text_attributes.Reset();
  }

  // Ensure that "helo" is now marked misspelled.
  for (LONG offset = value1_length; offset < value1_length + 4; ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(value1_length, start_offset);
    EXPECT_EQ(value1_length + 4, end_offset);
    EXPECT_NE(base::string16::npos,
              base::string16(text_attributes).find(L"invalid:spelling"));
    text_attributes.Reset();
  }

  // Ensure that the last part of the value is not marked misspelled.
  for (LONG offset = value1_length + 4; offset < combo_box_value_length;
       ++offset) {
    hr = ax_combo_box->GetCOM()->get_attributes(
        offset, &start_offset, &end_offset, text_attributes.Receive());
    EXPECT_TRUE(base::string16(text_attributes).empty());
    EXPECT_EQ(value1_length + 4, start_offset);
    EXPECT_EQ(combo_box_value_length, end_offset);
    text_attributes.Reset();
  }

  manager.reset();
}

TEST_F(BrowserAccessibilityWinTest, TestDeepestFirstLastChild) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  ui::AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(2);

  ui::AXNodeData child2;
  child2.id = 3;
  child2.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(3);

  ui::AXNodeData child2_child1;
  child2_child1.id = 4;
  child2_child1.role = ax::mojom::Role::kInlineTextBox;
  child2.child_ids.push_back(4);

  ui::AXNodeData child2_child2;
  child2_child2.id = 5;
  child2_child2.role = ax::mojom::Role::kInlineTextBox;
  child2.child_ids.push_back(5);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, child1, child2, child2_child1, child2_child2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());
  BrowserAccessibility* child1_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, child1_accessible);
  BrowserAccessibility* child2_accessible =
      root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, child2_accessible);
  ASSERT_EQ(0U, child2_accessible->PlatformChildCount());
  ASSERT_EQ(2U, child2_accessible->InternalChildCount());
  BrowserAccessibility* child2_child1_accessible =
      child2_accessible->InternalGetChild(0);
  ASSERT_NE(nullptr, child2_child1_accessible);
  BrowserAccessibility* child2_child2_accessible =
      child2_accessible->InternalGetChild(1);
  ASSERT_NE(nullptr, child2_child2_accessible);

  EXPECT_EQ(child1_accessible, root_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(child1_accessible, root_accessible->InternalDeepestFirstChild());

  EXPECT_EQ(child2_accessible, root_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(child2_child2_accessible,
            root_accessible->InternalDeepestLastChild());

  EXPECT_EQ(nullptr, child1_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(nullptr, child1_accessible->InternalDeepestFirstChild());

  EXPECT_EQ(nullptr, child1_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(nullptr, child1_accessible->InternalDeepestLastChild());

  EXPECT_EQ(nullptr, child2_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(child2_child1_accessible,
            child2_accessible->InternalDeepestFirstChild());

  EXPECT_EQ(nullptr, child2_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(child2_child2_accessible,
            child2_accessible->InternalDeepestLastChild());

  EXPECT_EQ(nullptr, child2_child1_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child1_accessible->InternalDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child1_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(nullptr, child2_child1_accessible->InternalDeepestLastChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->InternalDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->InternalDeepestLastChild());
}

TEST_F(BrowserAccessibilityWinTest, TestInheritedStringAttributes) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "en-US");
  root.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily, "Helvetica");

  ui::AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(2);

  ui::AXNodeData child2;
  child2.id = 3;
  child2.role = ax::mojom::Role::kStaticText;
  child2.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr");
  child2.AddStringAttribute(ax::mojom::StringAttribute::kFontFamily, "Arial");
  root.child_ids.push_back(3);

  ui::AXNodeData child2_child1;
  child2_child1.id = 4;
  child2_child1.role = ax::mojom::Role::kInlineTextBox;
  child2.child_ids.push_back(4);

  ui::AXNodeData child2_child2;
  child2_child2.id = 5;
  child2_child2.role = ax::mojom::Role::kInlineTextBox;
  child2.child_ids.push_back(5);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, child1, child2, child2_child1, child2_child2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* child1_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, child1_accessible);
  BrowserAccessibility* child2_accessible =
      root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, child2_accessible);
  BrowserAccessibility* child2_child1_accessible =
      child2_accessible->InternalGetChild(0);
  ASSERT_NE(nullptr, child2_child1_accessible);
  BrowserAccessibility* child2_child2_accessible =
      child2_accessible->InternalGetChild(1);
  ASSERT_NE(nullptr, child2_child2_accessible);

  // Test GetInheritedString16Attribute(attribute).
  EXPECT_EQ(base::UTF8ToUTF16("en-US"),
            root_accessible->GetInheritedString16Attribute(
                ax::mojom::StringAttribute::kLanguage));
  EXPECT_EQ(base::UTF8ToUTF16("en-US"),
            child1_accessible->GetInheritedString16Attribute(
                ax::mojom::StringAttribute::kLanguage));
  EXPECT_EQ(base::UTF8ToUTF16("fr"),
            child2_accessible->GetInheritedString16Attribute(
                ax::mojom::StringAttribute::kLanguage));
  EXPECT_EQ(base::UTF8ToUTF16("fr"),
            child2_child1_accessible->GetInheritedString16Attribute(
                ax::mojom::StringAttribute::kLanguage));
  EXPECT_EQ(base::UTF8ToUTF16("fr"),
            child2_child2_accessible->GetInheritedString16Attribute(
                ax::mojom::StringAttribute::kLanguage));

  // Test GetInheritedStringAttribute(attribute).
  EXPECT_EQ("Helvetica", root_accessible->GetInheritedStringAttribute(
                             ax::mojom::StringAttribute::kFontFamily));
  EXPECT_EQ("Helvetica", child1_accessible->GetInheritedStringAttribute(
                             ax::mojom::StringAttribute::kFontFamily));
  EXPECT_EQ("Arial", child2_accessible->GetInheritedStringAttribute(
                         ax::mojom::StringAttribute::kFontFamily));
  EXPECT_EQ("Arial", child2_child1_accessible->GetInheritedStringAttribute(
                         ax::mojom::StringAttribute::kFontFamily));
  EXPECT_EQ("Arial", child2_child2_accessible->GetInheritedStringAttribute(
                         ax::mojom::StringAttribute::kFontFamily));
}

TEST_F(BrowserAccessibilityWinTest, UniqueIdWinInvalidAfterDeletingTree) {
  ui::AXNodeData root_node;
  root_node.id = 1;
  root_node.role = ax::mojom::Role::kRootWebArea;

  ui::AXNodeData child_node;
  child_node.id = 2;
  root_node.child_ids.push_back(2);

  std::unique_ptr<BrowserAccessibilityManagerWin> manager(
      new BrowserAccessibilityManagerWin(
          MakeAXTreeUpdate(root_node, child_node),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root = manager->GetRoot();
  int32_t root_unique_id = GetUniqueId(root);
  BrowserAccessibility* child = root->PlatformGetChild(0);
  int32_t child_unique_id = GetUniqueId(child);

  // Now destroy that original tree and create a new tree.
  manager.reset(new BrowserAccessibilityManagerWin(
      MakeAXTreeUpdate(root_node, child_node),
      test_browser_accessibility_delegate_.get(),
      new BrowserAccessibilityFactory()));
  root = manager->GetRoot();
  int32_t root_unique_id_2 = GetUniqueId(root);
  child = root->PlatformGetChild(0);
  int32_t child_unique_id_2 = GetUniqueId(child);

  // The nodes in the new tree should not have the same ids.
  EXPECT_NE(root_unique_id, root_unique_id_2);
  EXPECT_NE(child_unique_id, child_unique_id_2);

  // Trying to access the unique IDs of the old, deleted objects should fail.
  base::win::ScopedVariant old_root_variant(-root_unique_id);
  Microsoft::WRL::ComPtr<IDispatch> old_root_dispatch;
  HRESULT hr = ToBrowserAccessibilityWin(root)->GetCOM()->get_accChild(
      old_root_variant, old_root_dispatch.GetAddressOf());
  EXPECT_EQ(E_INVALIDARG, hr);

  base::win::ScopedVariant old_child_variant(-child_unique_id);
  Microsoft::WRL::ComPtr<IDispatch> old_child_dispatch;
  hr = ToBrowserAccessibilityWin(root)->GetCOM()->get_accChild(
      old_child_variant, old_child_dispatch.GetAddressOf());
  EXPECT_EQ(E_INVALIDARG, hr);

  // Trying to access the unique IDs of the new objects should succeed.
  base::win::ScopedVariant new_root_variant(-root_unique_id_2);
  Microsoft::WRL::ComPtr<IDispatch> new_root_dispatch;
  hr = ToBrowserAccessibilityWin(root)->GetCOM()->get_accChild(
      new_root_variant, new_root_dispatch.GetAddressOf());
  EXPECT_EQ(S_OK, hr);

  base::win::ScopedVariant new_child_variant(-child_unique_id_2);
  Microsoft::WRL::ComPtr<IDispatch> new_child_dispatch;
  hr = ToBrowserAccessibilityWin(root)->GetCOM()->get_accChild(
      new_child_variant, new_child_dispatch.GetAddressOf());
  EXPECT_EQ(S_OK, hr);
}

TEST_F(BrowserAccessibilityWinTest, AccChildOnlyReturnsDescendants) {
  ui::AXNodeData root_node;
  root_node.id = 1;
  root_node.role = ax::mojom::Role::kRootWebArea;

  ui::AXNodeData child_node;
  child_node.id = 2;
  root_node.child_ids.push_back(2);

  std::unique_ptr<BrowserAccessibilityManagerWin> manager(
      new BrowserAccessibilityManagerWin(
          MakeAXTreeUpdate(root_node, child_node),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root = manager->GetRoot();
  BrowserAccessibility* child = root->PlatformGetChild(0);

  base::win::ScopedVariant root_unique_id_variant(-GetUniqueId(root));
  Microsoft::WRL::ComPtr<IDispatch> result;
  EXPECT_EQ(E_INVALIDARG,
            ToBrowserAccessibilityWin(child)->GetCOM()->get_accChild(
                root_unique_id_variant, result.GetAddressOf()));

  base::win::ScopedVariant child_unique_id_variant(-GetUniqueId(child));
  EXPECT_EQ(S_OK, ToBrowserAccessibilityWin(root)->GetCOM()->get_accChild(
                      child_unique_id_variant, result.GetAddressOf()));
}

// TODO(crbug.com/929563): Disabled due to flakiness.
TEST_F(BrowserAccessibilityWinTest, DISABLED_TestIAccessible2Relations) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  // Reflexive relations should be ignored.
  std::vector<int32_t> describedby_ids = {1, 2, 3};
  root.AddIntListAttribute(ax::mojom::IntListAttribute::kDescribedbyIds,
                           describedby_ids);

  ui::AXNodeData child1;
  child1.id = 2;
  child1.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(2);

  ui::AXNodeData child2;
  child2.id = 3;
  child2.role = ax::mojom::Role::kStaticText;
  root.child_ids.push_back(3);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, child1, child2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibilityWin* ax_root =
      ToBrowserAccessibilityWin(manager->GetRoot());
  ASSERT_NE(nullptr, ax_root);
  BrowserAccessibilityWin* ax_child1 =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(0));
  ASSERT_NE(nullptr, ax_child1);
  BrowserAccessibilityWin* ax_child2 =
      ToBrowserAccessibilityWin(ax_root->PlatformGetChild(1));
  ASSERT_NE(nullptr, ax_child2);

  LONG n_relations = 0;
  LONG n_targets = 0;
  LONG unique_id = 0;
  base::win::ScopedBstr relation_type;
  Microsoft::WRL::ComPtr<IAccessibleRelation> describedby_relation;
  Microsoft::WRL::ComPtr<IAccessibleRelation> description_for_relation;
  Microsoft::WRL::ComPtr<IUnknown> target;
  Microsoft::WRL::ComPtr<IAccessible2> ax_target;

  EXPECT_HRESULT_SUCCEEDED(ax_root->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(1, n_relations);

  EXPECT_HRESULT_SUCCEEDED(
      ax_root->GetCOM()->get_relation(0, describedby_relation.GetAddressOf()));
  EXPECT_HRESULT_SUCCEEDED(
      describedby_relation->get_relationType(relation_type.Receive()));
  EXPECT_EQ(L"describedBy", base::string16(relation_type));
  relation_type.Reset();

  EXPECT_HRESULT_SUCCEEDED(describedby_relation->get_nTargets(&n_targets));
  EXPECT_EQ(2, n_targets);

  EXPECT_HRESULT_SUCCEEDED(
      describedby_relation->get_target(0, target.GetAddressOf()));
  target.CopyTo(ax_target.GetAddressOf());
  EXPECT_HRESULT_SUCCEEDED(ax_target->get_uniqueID(&unique_id));
  EXPECT_EQ(-GetUniqueId(ax_child1), unique_id);
  ax_target.Reset();
  target.Reset();

  EXPECT_HRESULT_SUCCEEDED(
      describedby_relation->get_target(1, target.GetAddressOf()));
  target.CopyTo(ax_target.GetAddressOf());
  EXPECT_HRESULT_SUCCEEDED(ax_target->get_uniqueID(&unique_id));
  EXPECT_EQ(-GetUniqueId(ax_child2), unique_id);
  ax_target.Reset();
  target.Reset();
  describedby_relation.Reset();

  // Test the reverse relations.
  EXPECT_HRESULT_SUCCEEDED(ax_child1->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(1, n_relations);

  EXPECT_HRESULT_SUCCEEDED(ax_child1->GetCOM()->get_relation(
      0, description_for_relation.GetAddressOf()));
  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_relationType(relation_type.Receive()));
  EXPECT_EQ(L"descriptionFor", base::string16(relation_type));
  relation_type.Reset();

  EXPECT_HRESULT_SUCCEEDED(description_for_relation->get_nTargets(&n_targets));
  EXPECT_EQ(1, n_targets);

  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_target(0, target.GetAddressOf()));
  target.CopyTo(ax_target.GetAddressOf());
  EXPECT_HRESULT_SUCCEEDED(ax_target->get_uniqueID(&unique_id));
  EXPECT_EQ(-GetUniqueId(ax_root), unique_id);
  ax_target.Reset();
  target.Reset();
  description_for_relation.Reset();

  EXPECT_HRESULT_SUCCEEDED(ax_child2->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(1, n_relations);

  EXPECT_HRESULT_SUCCEEDED(ax_child2->GetCOM()->get_relation(
      0, description_for_relation.GetAddressOf()));
  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_relationType(relation_type.Receive()));
  EXPECT_EQ(L"descriptionFor", base::string16(relation_type));
  relation_type.Reset();

  EXPECT_HRESULT_SUCCEEDED(description_for_relation->get_nTargets(&n_targets));
  EXPECT_EQ(1, n_targets);

  EXPECT_HRESULT_SUCCEEDED(
      description_for_relation->get_target(0, target.GetAddressOf()));
  target.CopyTo(ax_target.GetAddressOf());
  EXPECT_HRESULT_SUCCEEDED(ax_target->get_uniqueID(&unique_id));
  EXPECT_EQ(-GetUniqueId(ax_root), unique_id);
  ax_target.Reset();
  target.Reset();

  // Try adding one more relation.
  std::vector<int32_t> labelledby_ids = {3};
  child1.AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                             labelledby_ids);
  AXEventNotificationDetails event_bundle;
  event_bundle.updates.resize(1);
  event_bundle.updates[0].nodes.push_back(child1);
  ASSERT_TRUE(manager->OnAccessibilityEvents(event_bundle));

  EXPECT_HRESULT_SUCCEEDED(ax_child1->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(2, n_relations);
  EXPECT_HRESULT_SUCCEEDED(ax_child2->GetCOM()->get_nRelations(&n_relations));
  EXPECT_EQ(2, n_relations);
}

TEST_F(BrowserAccessibilityWinTest, TestUIASwitch) {
  EXPECT_FALSE(::switches::IsExperimentalAccessibilityPlatformUIAEnabled());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalUIAutomation);

  EXPECT_TRUE(::switches::IsExperimentalAccessibilityPlatformUIAEnabled());
}

}  // namespace content
