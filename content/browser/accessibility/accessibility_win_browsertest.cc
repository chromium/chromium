// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/strings/escape.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_browsertest.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/context_menu_interceptor.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "third_party/isimpledom/ISimpleDOMNode.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/browser_accessibility_manager_win.h"
#include "ui/accessibility/platform/browser_accessibility_win.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_win.h"
#include "ui/accessibility/platform/uia_registrar_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace content {

using ui::IAccessible2RoleToString;
using ui::IAccessibleRoleToString;
using ui::IAccessibleStateToString;

namespace {

// AccessibilityWinBrowserTest ------------------------------------------------

class AccessibilityWinBrowserTest : public AccessibilityBrowserTest {
 public:
  AccessibilityWinBrowserTest();

  AccessibilityWinBrowserTest(const AccessibilityWinBrowserTest&) = delete;
  AccessibilityWinBrowserTest& operator=(const AccessibilityWinBrowserTest&) =
      delete;

  ~AccessibilityWinBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AccessibilityBrowserTest::SetUpCommandLine(command_line);
    // Some of these tests assume a device scale factor of 1.0.
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
  }

 protected:
  class AccessibleChecker;
  std::string PrintAXTree() const;
  void SetUpInputField(Microsoft::WRL::ComPtr<IAccessibleText>* input_text);
  void SetUpScrollableInputField(
      Microsoft::WRL::ComPtr<IAccessibleText>* input_text);
  void SetUpScrollableInputTypeSearchField(
      Microsoft::WRL::ComPtr<IAccessibleText>* input_text);
  void SetUpSingleCharInputField(
      Microsoft::WRL::ComPtr<IAccessibleText>* input_text);
  void SetUpSingleCharInputFieldWithPlaceholder(
      Microsoft::WRL::ComPtr<IAccessibleText>* input_text);
  void SetUpSingleCharTextarea(
      Microsoft::WRL::ComPtr<IAccessibleText>* input_text);
  void SetUpSingleCharContenteditable(
      Microsoft::WRL::ComPtr<IAccessibleText>* input_text);
  void SetUpSingleCharRtlInputField(
      Microsoft::WRL::ComPtr<IAccessibleText>* input_text);
  void SetUpTextareaField(
      Microsoft::WRL::ComPtr<IAccessibleText>* textarea_text);
  void SetUpSampleParagraph(
      Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text,
      ui::AXMode accessibility_mode = ui::kAXModeComplete);
  void SetUpSampleParagraphInScrollableDocument(
      Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text,
      ui::AXMode accessibility_mode = ui::kAXModeComplete);
  void SetUpVeryTallPadding(
      Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text,
      ui::AXMode accessibility_mode = ui::kAXModeComplete);
  void SetUpVeryTallMargin(
      Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text,
      ui::AXMode accessibility_mode = ui::kAXModeComplete);
  void SetUpSampleParagraphInScrollableEditable(
      Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text);
  ui::BrowserAccessibility* FindNode(ax::mojom::Role role,
                                     const std::string& name_or_value);
  ui::BrowserAccessibilityManager* GetManager();
  static Microsoft::WRL::ComPtr<IAccessible> GetAccessibleFromVariant(
      IAccessible* parent,
      VARIANT* var);
  static HRESULT QueryIAccessible2(IAccessible* accessible,
                                   IAccessible2** accessible2);
  static void FindNodeInAccessibilityTree(IAccessible* node,
                                          int32_t expected_role,
                                          const std::wstring& expected_name,
                                          int32_t depth,
                                          bool* found);
  static void CheckTextAtOffset(
      const Microsoft::WRL::ComPtr<IAccessibleText>& object,
      LONG offset,
      IA2TextBoundaryType boundary_type,
      LONG expected_start_offset,
      LONG expected_end_offset,
      const std::wstring& expected_text);
  static std::vector<base::win::ScopedVariant> GetAllAccessibleChildren(
      IAccessible* element);

 private:
  void SetUpInputFieldHelper(
      Microsoft::WRL::ComPtr<IAccessibleText>* input_text);
  void SetUpSampleParagraphHelper(
      Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text);
  ui::BrowserAccessibility* FindNodeInSubtree(ui::BrowserAccessibility& node,
                                              ax::mojom::Role role,
                                              const std::string& name_or_value);
};

AccessibilityWinBrowserTest::AccessibilityWinBrowserTest() = default;

AccessibilityWinBrowserTest::~AccessibilityWinBrowserTest() = default;

std::string AccessibilityWinBrowserTest::PrintAXTree() const {
  std::unique_ptr<ui::AXTreeFormatter> formatter(
      AXInspectFactory::CreatePlatformFormatter());
  DCHECK(formatter);
  formatter->set_show_ids(true);
  formatter->SetPropertyFilters(
      {ui::AXPropertyFilter("*", ui::AXPropertyFilter::ALLOW)});

  return formatter->Format(GetRootAccessibilityNode(shell()->web_contents()));
}

// Loads a page with  an input text field and places sample text in it. Also,
// places the caret before the last character.
void AccessibilityWinBrowserTest::SetUpInputField(
    Microsoft::WRL::ComPtr<IAccessibleText>* input_text) {
  ASSERT_NE(nullptr, input_text);
  LoadInputField();
  SetUpInputFieldHelper(input_text);
}

// Loads a page with  an input text field and places sample text in it that
// overflows its width. Also, places the caret before the last character.
void AccessibilityWinBrowserTest::SetUpScrollableInputField(
    Microsoft::WRL::ComPtr<IAccessibleText>* input_text) {
  ASSERT_NE(nullptr, input_text);

  LoadScrollableInputField("text");
  SetUpInputFieldHelper(input_text);
}

// Loads a page with  an input text field and places sample text in it that
// overflows its width. Also, places the caret before the last character.
void AccessibilityWinBrowserTest::SetUpScrollableInputTypeSearchField(
    Microsoft::WRL::ComPtr<IAccessibleText>* input_text) {
  ASSERT_NE(nullptr, input_text);
  LoadScrollableInputField("search");
  SetUpInputFieldHelper(input_text);
}

// Loads a page with an input text field and places a single character in it.
// Also tests with padding, in order to ensure character extent of empty field
// does not erroneously include padding.
void AccessibilityWinBrowserTest::SetUpSingleCharInputField(
    Microsoft::WRL::ComPtr<IAccessibleText>* input_text) {
  ASSERT_NE(nullptr, input_text);
  LoadInitialAccessibilityTreeFromHtml(std::string(
      R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <form>
              <input type="text" value="x" style="padding:3px">
            </form>
          </body>
          </html>)HTML"));
  SetUpInputFieldHelper(input_text);
}

void AccessibilityWinBrowserTest::SetUpSingleCharInputFieldWithPlaceholder(
    Microsoft::WRL::ComPtr<IAccessibleText>* input_text) {
  ASSERT_NE(nullptr, input_text);
  LoadInitialAccessibilityTreeFromHtml(std::string(
      R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <form>
              <input type="text" value="x" placeholder="placeholder">
            </form>
          </body>
          </html>)HTML"));
  SetUpInputFieldHelper(input_text);
}

void AccessibilityWinBrowserTest::SetUpSingleCharTextarea(
    Microsoft::WRL::ComPtr<IAccessibleText>* input_text) {
  ASSERT_NE(nullptr, input_text);
  LoadInitialAccessibilityTreeFromHtml(std::string(
      R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <form>
              <textarea rows="3" cols="10">x</textarea>
            </form>
          </body>
          </html>)HTML"));
  SetUpInputFieldHelper(input_text);
}

// Loads a page with a right-to-left input text field and places a single
// character in it.
void AccessibilityWinBrowserTest::SetUpSingleCharRtlInputField(
    Microsoft::WRL::ComPtr<IAccessibleText>* input_text) {
  ASSERT_NE(nullptr, input_text);
  LoadInitialAccessibilityTreeFromHtml(std::string(
      R"HTML(<!DOCTYPE html>
          <html>
          <body>
            <form>
              <input type="text" id="textField" name="name" dir="rtl" value="x">
            </form>
          </body>
          </html>)HTML"));
  SetUpInputFieldHelper(input_text);
}

void AccessibilityWinBrowserTest::SetUpInputFieldHelper(
    Microsoft::WRL::ComPtr<IAccessibleText>* input_text) {
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> div;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &div));
  std::vector<base::win::ScopedVariant> div_children =
      GetAllAccessibleChildren(div.Get());
  ASSERT_LT(0u, div_children.size());

  // The input field is always the last child.
  Microsoft::WRL::ComPtr<IAccessible2> input;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(div.Get(),
                               div_children[div_children.size() - 1].AsInput())
          .Get(),
      &input));
  LONG input_role = 0;
  ASSERT_HRESULT_SUCCEEDED(input->role(&input_role));
  ASSERT_EQ(ROLE_SYSTEM_TEXT, input_role);

  // Retrieve the IAccessibleText interface for the field.
  input_text->Reset();
  ASSERT_HRESULT_SUCCEEDED(input.As(input_text));

  // Set the caret before the last character.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  std::wstring caret_offset =
      base::NumberToWString(InputContentsString().size() - 1);
  ExecuteScript(base::WideToUTF16(
      base::StrCat({L"let textField = document.querySelector('input,textarea');"
                    L"textField.focus();"
                    L"textField.setSelectionRange(",
                    caret_offset, L",", caret_offset,
                    L");"
                    L"textField.scrollLeft = 1000;"})));
  ASSERT_TRUE(waiter.WaitForNotification());
}

// Loads a page with  a textarea text field and places sample text in it. Also,
// places the caret before the last character.
void AccessibilityWinBrowserTest::SetUpTextareaField(
    Microsoft::WRL::ComPtr<IAccessibleText>* textarea_text) {
  ASSERT_NE(nullptr, textarea_text);
  LoadTextareaField();

  // Retrieve the IAccessible interface for the web page.
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> section;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &section));
  std::vector<base::win::ScopedVariant> section_children =
      GetAllAccessibleChildren(section.Get());
  ASSERT_EQ(1u, section_children.size());

  // Find the textarea text field.
  Microsoft::WRL::ComPtr<IAccessible2> textarea;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(section.Get(), section_children[0].AsInput())
          .Get(),
      &textarea));
  LONG textarea_role = 0;
  ASSERT_HRESULT_SUCCEEDED(textarea->role(&textarea_role));
  ASSERT_EQ(ROLE_SYSTEM_TEXT, textarea_role);

  // Retrieve the IAccessibleText interface for the field.
  textarea_text->Reset();
  ASSERT_HRESULT_SUCCEEDED(textarea.As(textarea_text));

  // Set the caret before the last character.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  std::wstring caret_offset =
      base::NumberToWString(InputContentsString().size() - 1);
  ExecuteScript(base::WideToUTF16(
      base::StrCat({L"var textField = document.querySelector('textarea');"
                    L"textField.focus();"
                    L"textField.setSelectionRange(",
                    caret_offset, L",", caret_offset, L");"})));
  ASSERT_TRUE(waiter.WaitForNotification());
}

// Loads a page with  a paragraph of sample text.
void AccessibilityWinBrowserTest::SetUpSampleParagraph(
    Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text,
    ui::AXMode accessibility_mode) {
  LoadSampleParagraph(accessibility_mode);
  SetUpSampleParagraphHelper(accessible_text);
}

// Loads a page with a paragraph of sample text which is below the
// bottom of the screen.
void AccessibilityWinBrowserTest::SetUpSampleParagraphInScrollableDocument(
    Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text,
    ui::AXMode accessibility_mode) {
  LoadSampleParagraphInScrollableDocument(accessibility_mode);
  SetUpSampleParagraphHelper(accessible_text);
}

// Loads a page with a paragraph of sample text which is below the
// bottom of the screen, where the bounds of the paragraph
// are longer than one screenful.
void AccessibilityWinBrowserTest::SetUpVeryTallPadding(
    Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text,
    ui::AXMode accessibility_mode) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <p style="padding-top:200vh; padding-bottom:200vh">ABC<br/>DEF</p>
      </body>
      </html>)HTML",
      accessibility_mode);

  SetUpSampleParagraphHelper(accessible_text);
}

// Loads a page with a paragraph of sample text which is below the
// bottom of the screen, where the bounds of the paragraph
// are longer than one screenful.
void AccessibilityWinBrowserTest::SetUpVeryTallMargin(
    Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text,
    ui::AXMode accessibility_mode) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <p style="margin-top:200vh; margin-bottom:200vh">ABC<br/>DEF</p>
      </body>
      </html>)HTML",
      accessibility_mode);

  SetUpSampleParagraphHelper(accessible_text);
}

void AccessibilityWinBrowserTest::SetUpSampleParagraphInScrollableEditable(
    Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text) {
  LoadSampleParagraphInScrollableEditable();
  SetUpSampleParagraphHelper(accessible_text);
}

void AccessibilityWinBrowserTest::SetUpSampleParagraphHelper(
    Microsoft::WRL::ComPtr<IAccessibleText>* accessible_text) {
  ASSERT_NE(nullptr, accessible_text);

  // Retrieve the IAccessible interface for the web page.
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &paragraph));

  LONG paragraph_role = 0;
  ASSERT_HRESULT_SUCCEEDED(paragraph->role(&paragraph_role));
  ASSERT_EQ(IA2_ROLE_PARAGRAPH, paragraph_role);

  accessible_text->Reset();
  ASSERT_HRESULT_SUCCEEDED(paragraph.As(accessible_text));
}

// Retrieve the accessibility node, starting from the root node, that matches
// the accessibility role, name or value.
ui::BrowserAccessibility* AccessibilityWinBrowserTest::FindNode(
    ax::mojom::Role role,
    const std::string& name_or_value) {
  ui::BrowserAccessibility* root = GetManager()->GetBrowserAccessibilityRoot();
  CHECK(root);
  return FindNodeInSubtree(*root, role, name_or_value);
}

// Retrieve the browser accessibility manager object for the current web
// contents.
ui::BrowserAccessibilityManager* AccessibilityWinBrowserTest::GetManager() {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  return web_contents->GetOrCreateRootBrowserAccessibilityManager();
}

// Retrieve the accessibility node in the subtree that matches the accessibility
// role, name or value.
ui::BrowserAccessibility* AccessibilityWinBrowserTest::FindNodeInSubtree(
    ui::BrowserAccessibility& node,
    ax::mojom::Role role,
    const std::string& name_or_value) {
  const std::string& name =
      node.GetStringAttribute(ax::mojom::StringAttribute::kName);
  // Note that in the case of a text field,
  // "BrowserAccessibility::GetValueForControl" has the added functionality of
  // computing the value of an ARIA text box from its inner text.
  //
  // <div contenteditable="true" role="textbox">Hello world.</div>
  // Will expose no HTML value attribute, but some screen readers, such as Jaws,
  // VoiceOver and Talkback, require one to be computed.
  const std::string value = base::UTF16ToUTF8(node.GetValueForControl());
  if (node.GetRole() == role &&
      (name == name_or_value || value == name_or_value)) {
    return &node;
  }

  for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
    ui::BrowserAccessibility* result =
        FindNodeInSubtree(*node.PlatformGetChild(i), role, name_or_value);
    if (result) {
      return result;
    }
  }
  return nullptr;
}
// Static helpers ------------------------------------------------

Microsoft::WRL::ComPtr<IAccessible>
AccessibilityWinBrowserTest::GetAccessibleFromVariant(IAccessible* parent,
                                                      VARIANT* var) {
  Microsoft::WRL::ComPtr<IAccessible> ptr;
  switch (V_VT(var)) {
    case VT_DISPATCH: {
      IDispatch* dispatch = V_DISPATCH(var);
      if (dispatch) {
        dispatch->QueryInterface(IID_PPV_ARGS(&ptr));
      }
      break;
    }

    case VT_I4: {
      Microsoft::WRL::ComPtr<IDispatch> dispatch;
      HRESULT hr = parent->get_accChild(*var, &dispatch);
      EXPECT_TRUE(SUCCEEDED(hr));
      if (dispatch.Get()) {
        dispatch.As(&ptr);
      }
      break;
    }
  }
  return ptr;
}

HRESULT AccessibilityWinBrowserTest::QueryIAccessible2(
    IAccessible* accessible,
    IAccessible2** accessible2) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving IAccessible2.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  return SUCCEEDED(hr)
             ? service_provider->QueryService(IID_IAccessible2, accessible2)
             : hr;
}

// Recursively search through all of the descendants reachable from an
// IAccessible node and return true if we find one with the given role
// and name.
void AccessibilityWinBrowserTest::FindNodeInAccessibilityTree(
    IAccessible* node,
    int32_t expected_role,
    const std::wstring& expected_name,
    int32_t depth,
    bool* found) {
  base::win::ScopedBstr name_bstr;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  node->get_accName(childid_self, name_bstr.Receive());
  std::wstring name(name_bstr.Get(), name_bstr.Length());
  base::win::ScopedVariant role;
  node->get_accRole(childid_self, role.Receive());
  ASSERT_EQ(VT_I4, role.type());

  // Print the accessibility tree as we go, because if this test fails
  // on the bots, this is really helpful in figuring out why.
  for (int i = 0; i < depth; i++) {
    printf("  ");
  }
  printf("role=%s name=%s\n",
         base::WideToUTF8(IAccessibleRoleToString(V_I4(role.ptr()))).c_str(),
         base::WideToUTF8(name).c_str());

  if (expected_role == V_I4(role.ptr()) && expected_name == name) {
    *found = true;
    return;
  }

  std::vector<base::win::ScopedVariant> children =
      GetAllAccessibleChildren(node);
  for (size_t i = 0; i < children.size(); ++i) {
    Microsoft::WRL::ComPtr<IAccessible> child_accessible(
        GetAccessibleFromVariant(node, children[i].AsInput()));
    if (child_accessible) {
      FindNodeInAccessibilityTree(child_accessible.Get(), expected_role,
                                  expected_name, depth + 1, found);
      if (*found) {
        return;
      }
    }
  }
}

// Ensures that the text and the start and end offsets retrieved using
// get_textAtOffset match the expected values.
void AccessibilityWinBrowserTest::CheckTextAtOffset(
    const Microsoft::WRL::ComPtr<IAccessibleText>& object,
    LONG offset,
    IA2TextBoundaryType boundary_type,
    LONG expected_start_offset,
    LONG expected_end_offset,
    const std::wstring& expected_text) {
  ::testing::Message message;
  message << "While checking for \'" << expected_text << "\' at "
          << expected_start_offset << '-' << expected_end_offset << '.';
  SCOPED_TRACE(message);

  LONG start_offset = 0;
  LONG end_offset = 0;
  base::win::ScopedBstr text;
  HRESULT hr = object->get_textAtOffset(offset, boundary_type, &start_offset,
                                        &end_offset, text.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(expected_start_offset, start_offset);
  EXPECT_EQ(expected_end_offset, end_offset);
  EXPECT_STREQ(expected_text.c_str(), text.Get());
}

std::vector<base::win::ScopedVariant>
AccessibilityWinBrowserTest::GetAllAccessibleChildren(IAccessible* element) {
  LONG child_count = 0;
  HRESULT hr = element->get_accChildCount(&child_count);
  EXPECT_EQ(S_OK, hr);
  if (child_count <= 0) {
    return std::vector<base::win::ScopedVariant>();
  }

  auto children_array = base::HeapArray<VARIANT>::WithSize(child_count);
  LONG obtained_count = 0;
  hr = AccessibleChildren(element, 0, child_count, children_array.data(),
                          &obtained_count);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(child_count, obtained_count);

  std::vector<base::win::ScopedVariant> children(
      static_cast<size_t>(child_count));
  for (size_t i = 0; i < children.size(); i++) {
    children[i].Reset(children_array[i]);
  }

  return children;
}

// AccessibleChecker ----------------------------------------------------------

class AccessibilityWinBrowserTest::AccessibleChecker {
 public:
  // This constructor can be used if the IA2 role will be the same as the MSAA
  // role.
  AccessibleChecker(const std::wstring& expected_name,
                    int32_t expected_role,
                    const std::wstring& expected_value);
  AccessibleChecker(const std::wstring& expected_name,
                    int32_t expected_role,
                    int32_t expected_ia2_role,
                    const std::wstring& expected_value);

  // Append an AccessibleChecker that verifies accessibility information for
  // a child IAccessible. Order is important.
  void AppendExpectedChild(AccessibleChecker* expected_child);

  // Check that the name and role of the given IAccessible instance and its
  // descendants match the expected names and roles that this object was
  // initialized with.
  void CheckAccessible(IAccessible* accessible);

  // Set the expected value for this AccessibleChecker.
  void SetExpectedValue(const std::wstring& expected_value);

  // Set the expected state for this AccessibleChecker.
  void SetExpectedState(LONG expected_state);

 private:
  typedef std::vector<raw_ptr<AccessibleChecker, VectorExperimental>>
      AccessibleCheckerVector;

  void CheckAccessibleName(IAccessible* accessible);
  void CheckAccessibleRole(IAccessible* accessible);
  void CheckIA2Role(IAccessible* accessible);
  void CheckAccessibleValue(IAccessible* accessible);
  void CheckAccessibleState(IAccessible* accessible);
  void CheckAccessibleChildren(IAccessible* accessible);

  // Expected accessible name. Checked against IAccessible::get_accName.
  std::wstring name_;

  // Expected accessible role. Checked against IAccessible::get_accRole.
  int32_t role_;

  // Expected IAccessible2 role. Checked against IAccessible2::role.
  int32_t ia2_role_;

  // Expected accessible value. Checked against IAccessible::get_accValue.
  std::wstring value_;

  // Expected accessible state. Checked against IAccessible::get_accState.
  LONG state_;

  // Expected accessible children. Checked using IAccessible::get_accChildCount
  // and ::AccessibleChildren.
  AccessibleCheckerVector children_;
};

AccessibilityWinBrowserTest::AccessibleChecker::AccessibleChecker(
    const std::wstring& expected_name,
    int32_t expected_role,
    const std::wstring& expected_value)
    : name_(expected_name),
      role_(expected_role),
      ia2_role_(expected_role),
      value_(expected_value),
      state_(-1) {}

AccessibilityWinBrowserTest::AccessibleChecker::AccessibleChecker(
    const std::wstring& expected_name,
    int32_t expected_role,
    int32_t expected_ia2_role,
    const std::wstring& expected_value)
    : name_(expected_name),
      role_(expected_role),
      ia2_role_(expected_ia2_role),
      value_(expected_value),
      state_(-1) {}

void AccessibilityWinBrowserTest::AccessibleChecker::AppendExpectedChild(
    AccessibleChecker* expected_child) {
  children_.push_back(expected_child);
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessible(
    IAccessible* accessible) {
  SCOPED_TRACE("While checking " +
               base::WideToUTF8(IAccessibleRoleToString(role_)));
  CheckAccessibleName(accessible);
  CheckAccessibleRole(accessible);
  CheckIA2Role(accessible);
  CheckAccessibleValue(accessible);
  CheckAccessibleState(accessible);
  CheckAccessibleChildren(accessible);
}

void AccessibilityWinBrowserTest::AccessibleChecker::SetExpectedValue(
    const std::wstring& expected_value) {
  value_ = expected_value;
}

void AccessibilityWinBrowserTest::AccessibleChecker::SetExpectedState(
    LONG expected_state) {
  state_ = expected_state;
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleName(
    IAccessible* accessible) {
  base::win::ScopedBstr name;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accName(childid_self, name.Receive());

  if (name_.empty()) {
    // If the object doesn't have name S_FALSE should be returned.
    EXPECT_EQ(S_FALSE, hr);
  } else {
    // Test that the correct string was returned.
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(name_, std::wstring(name.Get(), name.Length()));
  }
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleRole(
    IAccessible* accessible) {
  base::win::ScopedVariant role;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accRole(childid_self, role.Receive());
  ASSERT_EQ(S_OK, hr);

  ASSERT_EQ(VT_I4, role.type());
  ASSERT_EQ(role_, V_I4(role.ptr()))
      << "Expected role: " << IAccessibleRoleToString(role_)
      << "\nGot role: " << IAccessibleRoleToString(V_I4(role.ptr()));
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckIA2Role(
    IAccessible* accessible) {
  Microsoft::WRL::ComPtr<IAccessible2> accessible2;
  HRESULT hr = QueryIAccessible2(accessible, &accessible2);
  ASSERT_EQ(S_OK, hr);
  LONG ia2_role = 0;
  hr = accessible2->role(&ia2_role);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(ia2_role_, ia2_role)
      << "Expected ia2 role: " << IAccessible2RoleToString(ia2_role_)
      << "\nGot ia2 role: " << IAccessible2RoleToString(ia2_role);
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleValue(
    IAccessible* accessible) {
  // Don't check the value if if's a DOCUMENT role, because the value
  // is supposed to be the url (and we don't keep track of that in the
  // test expectations).
  base::win::ScopedVariant role;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accRole(childid_self, role.Receive());
  ASSERT_EQ(S_OK, hr);
  if (role.type() == VT_I4 && V_I4(role.ptr()) == ROLE_SYSTEM_DOCUMENT) {
    return;
  }

  // Get the value.
  base::win::ScopedBstr value;
  hr = accessible->get_accValue(childid_self, value.Receive());
  EXPECT_EQ(S_OK, hr);

  // Test that the correct string was returned.
  EXPECT_EQ(value_, std::wstring(value.Get(), value.Length()));
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleState(
    IAccessible* accessible) {
  if (state_ < 0) {
    return;
  }

  base::win::ScopedVariant state;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = accessible->get_accState(childid_self, state.Receive());
  EXPECT_EQ(S_OK, hr);
  ASSERT_EQ(VT_I4, state.type());
  LONG obj_state = V_I4(state.ptr());
  // Avoid flakiness. The "offscreen" state depends on whether the browser
  // window is frontmost or not, and "hottracked" depends on whether the
  // mouse cursor happens to be over the element.
  obj_state &= ~(STATE_SYSTEM_OFFSCREEN | STATE_SYSTEM_HOTTRACKED);
  EXPECT_EQ(state_, obj_state)
      << "Expected state: " << IAccessibleStateToString(state_)
      << "\nGot state: " << IAccessibleStateToString(obj_state);
}

void AccessibilityWinBrowserTest::AccessibleChecker::CheckAccessibleChildren(
    IAccessible* parent) {
  std::vector<base::win::ScopedVariant> obtained_children =
      GetAllAccessibleChildren(parent);
  size_t child_count = obtained_children.size();
  ASSERT_EQ(child_count, children_.size());

  AccessibleCheckerVector::iterator child_checker;
  std::vector<base::win::ScopedVariant>::iterator child;
  for (child_checker = children_.begin(), child = obtained_children.begin();
       child_checker != children_.end() && child != obtained_children.end();
       ++child_checker, ++child) {
    Microsoft::WRL::ComPtr<IAccessible> child_accessible(
        GetAccessibleFromVariant(parent, child->AsInput()));
    ASSERT_TRUE(child_accessible.Get());
    (*child_checker)->CheckAccessible(child_accessible.Get());
  }
}

// Helper class that listens to native Windows events using
// AccessibilityEventRecorder, and blocks until the pretty-printed
// event string matches the given match pattern.
class NativeWinEventWaiter {
 public:
  NativeWinEventWaiter(ui::BrowserAccessibilityManager* manager,
                       const std::string& match_pattern,
                       ui::AXApiType::Type type = ui::AXApiType::kWinIA2)
      : event_recorder_(AXInspectFactory::CreateRecorder(
            type,
            manager,
            base::GetCurrentProcId(),
            ui::AXTreeSelector(manager->GetBrowserAccessibilityRoot()
                                   ->GetTargetForNativeAccessibilityEvent()))),
        match_pattern_(match_pattern),
        browser_accessibility_manager_(manager) {
    event_recorder_->ListenToEvents(base::BindRepeating(
        &NativeWinEventWaiter::OnEvent, base::Unretained(this)));
  }

  void OnEvent(const std::string& event_str) {
    DLOG(INFO) << "Got event " + event_str;
    if (base::MatchPattern(event_str, match_pattern_)) {
      run_loop_.Quit();
    }
  }

  void Wait() { run_loop_.Run(); }

  ~NativeWinEventWaiter() {
    browser_accessibility_manager_->SignalEndOfTest();
    event_recorder_->WaitForDoneRecording();
  }

 private:
  std::unique_ptr<ui::AXEventRecorder> event_recorder_;
  std::string match_pattern_;
  base::RunLoop run_loop_;
  raw_ptr<ui::BrowserAccessibilityManager> browser_accessibility_manager_;
};

// Helper class that reproduces a specific crash when UIA parent navigation
// is performed during the destruction of its WebContents.
class WebContentsUIAParentNavigationInDestroyedWatcher
    : public WebContentsObserver {
 public:
  explicit WebContentsUIAParentNavigationInDestroyedWatcher(
      WebContents* web_contents,
      IUIAutomationElement* root,
      IUIAutomationTreeWalker* tree_walker)
      : WebContentsObserver(web_contents),
        root_(root),
        tree_walker_(tree_walker) {
    CHECK(web_contents);
  }

  ~WebContentsUIAParentNavigationInDestroyedWatcher() override {}

  // Waits until the WebContents is destroyed.
  void Wait() { run_loop_.Run(); }

 private:
  // Overridden WebContentsObserver methods.
  void WebContentsDestroyed() override {
    // Test navigating to the parent node via UIA.
    Microsoft::WRL::ComPtr<IUIAutomationElement> parent;
    tree_walker_->GetParentElement(root_.Get(), &parent);

    // The original bug resulted in a crash when making this call.
    parent.Get();

    run_loop_.Quit();
  }

  Microsoft::WRL::ComPtr<IUIAutomationElement> root_;
  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker_;
  base::RunLoop run_loop_;
};

}  // namespace

//
// Tests ----------------------------------------------------------------------
//

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestAlwaysFireFocusEventAfterNavigationComplete) {
  ScopedAccessibilityModeOverride ax_mode_override(ui::kAXModeBasic.flags());

  ASSERT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Users of Jaws or NVDA screen readers might not realize that the virtual
  // buffer has been loaded, if focus hasn't been placed in the document after
  // navigating to a new page. If a focus event is not received by the screen
  // reader, it might "think" that focus is outside the web contents.
  //
  // We can't use "LoadInitialAccessibilityTreeFromHtml" because it waits for
  // the "kLoadComplete" event, and the "kFocus" and "kLoadComplete" events are
  // not guaranteed to be sent in the same order every time, neither do we need
  // to enforce such an ordering. However, we do need to ensure that at the
  // point when the "kFocus" event is sent, the root object is present.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::FOCUS_CHANGED);
  GURL html_data_url("data:text/html,<p>Hello world.</p>");
  ASSERT_TRUE(NavigateToURL(shell(), html_data_url));
  // TODO(crbug.com/40844856): Investigate why this does not return
  // true.
  ASSERT_TRUE(waiter.WaitForNotification());

  // Check that at least the root of the page has indeed loaded and that it is
  // focused.
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  ASSERT_TRUE(document);
  base::win::ScopedVariant focus;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_HRESULT_SUCCEEDED(document->get_accFocus(focus.Receive()));
  EXPECT_EQ(VT_I4, focus.type());
  EXPECT_EQ(CHILDID_SELF, V_I4(focus.ptr()));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestLoadingAccessibilityTree) {
  ScopedAccessibilityModeOverride ax_mode_override(ui::kAXModeBasic.flags());

  AccessibleChecker document1_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                      std::wstring());

  {
    AccessibilityNotificationWaiter preload_waiter(
        shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kNone);

    GURL html_data_url(
        "data:text/html," +
        base::EscapeQueryParamValue(R"HTML(<body></body>)HTML", false));
    ASSERT_TRUE(NavigateToURL(shell(), html_data_url));

    // It's possible to receive accessibility data from the new document before
    // NavigateToURL returns, in which case it's too late to verify anything
    // about the initial state of browser accessibility.
    if (!preload_waiter.notification_received()) {
      // The initial accessible returned should have state STATE_SYSTEM_BUSY
      // while the accessibility tree is being requested from the renderer.
      document1_checker.SetExpectedState(
          STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSED | STATE_SYSTEM_BUSY);
      document1_checker.CheckAccessible(GetRendererAccessible());
    }
  }

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  ASSERT_TRUE(waiter.WaitForNotification());
  document1_checker.SetExpectedState(
      STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED);
  document1_checker.CheckAccessible(GetRendererAccessible());
}

// Periodically failing.  See crbug.com/145537
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_TestNotificationActiveDescendantChanged) {
  LoadInitialAccessibilityTreeFromHtml(
      "<ul tabindex='-1' role='radiogroup' aria-label='ul'>"
      "<li id='li'>li</li></ul>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker list_marker_checker(L"\x2022", ROLE_SYSTEM_TEXT,
                                        std::wstring());
  AccessibleChecker static_text_checker(L"li", ROLE_SYSTEM_TEXT,
                                        std::wstring());
  AccessibleChecker list_item_checker(std::wstring(), ROLE_SYSTEM_LISTITEM,
                                      std::wstring());
  list_item_checker.SetExpectedState(STATE_SYSTEM_READONLY);
  AccessibleChecker radio_group_checker(L"ul", ROLE_SYSTEM_GROUPING,
                                        IA2_ROLE_SECTION, std::wstring());
  radio_group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  list_item_checker.AppendExpectedChild(&list_marker_checker);
  list_item_checker.AppendExpectedChild(&static_text_checker);
  radio_group_checker.AppendExpectedChild(&list_item_checker);
  document_checker.AppendExpectedChild(&radio_group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set focus to the radio group.
  auto waiter = std::make_unique<AccessibilityNotificationWaiter>(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  ExecuteScript(u"document.body.children[0].focus();");
  ASSERT_TRUE(waiter->WaitForNotification());

  // Check that the accessibility tree of the browser has been updated.
  radio_group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE |
                                       STATE_SYSTEM_FOCUSED);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set the active descendant of the radio group
  waiter = std::make_unique<AccessibilityNotificationWaiter>(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  ExecuteScript(
      u"document.body.children[0].setAttribute('aria-activedescendant', 'li')");
  ASSERT_TRUE(waiter->WaitForNotification());

  // Check that the accessibility tree of the browser has been updated.
  list_item_checker.SetExpectedState(STATE_SYSTEM_READONLY |
                                     STATE_SYSTEM_FOCUSED);
  radio_group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationCheckedStateChanged) {
  LoadInitialAccessibilityTreeFromHtml(
      "<body><input type='checkbox' /></body>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker checkbox_checker(std::wstring(), ROLE_SYSTEM_CHECKBUTTON,
                                     std::wstring());
  checkbox_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker body_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                 IA2_ROLE_SECTION, std::wstring());
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  body_checker.AppendExpectedChild(&checkbox_checker);
  document_checker.AppendExpectedChild(&body_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Check the checkbox.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kCheckedStateChanged);
  ExecuteScript(u"document.body.children[0].checked=true");
  ASSERT_TRUE(waiter.WaitForNotification());

  // Check that the accessibility tree of the browser has been updated.
  checkbox_checker.SetExpectedState(STATE_SYSTEM_CHECKED |
                                    STATE_SYSTEM_FOCUSABLE);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationChildrenChanged) {
  // The role attribute causes the node to be in the accessibility tree.
  LoadInitialAccessibilityTreeFromHtml("<body role=group></body>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker group_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                  std::wstring());
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  document_checker.AppendExpectedChild(&group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Change the children of the document body.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::CHILDREN_CHANGED);
  ExecuteScript(u"document.body.innerHTML='<b>new text</b>'");
  ASSERT_TRUE(waiter.WaitForNotification());

  // Check that the accessibility tree of the browser has been updated.
  AccessibleChecker text_checker(L"new text", ROLE_SYSTEM_STATICTEXT,
                                 std::wstring());
  group_checker.AppendExpectedChild(&text_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationChildrenChanged2) {
  // The role attribute causes the node to be in the accessibility tree.
  LoadInitialAccessibilityTreeFromHtml(
      "<div role=group style='visibility: hidden'>text</div>");

  // Check the accessible tree of the browser.
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  document_checker.CheckAccessible(GetRendererAccessible());

  // Change the children of the document body.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::CHILDREN_CHANGED);
  ExecuteScript(u"document.body.children[0].style.visibility='visible'");
  ASSERT_TRUE(waiter.WaitForNotification());

  // Check that the accessibility tree of the browser has been updated.
  AccessibleChecker static_text_checker(L"text", ROLE_SYSTEM_STATICTEXT,
                                        std::wstring());
  AccessibleChecker group_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                  std::wstring());
  document_checker.AppendExpectedChild(&group_checker);
  group_checker.AppendExpectedChild(&static_text_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationFocusChanged) {
  // The role attribute causes the node to be in the accessibility tree.
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <div role="group" tabindex="-1">
      </div>)HTML");

  // Check the browser's copy of the renderer accessibility tree.
  SCOPED_TRACE("Check initial tree");
  AccessibleChecker group_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                  std::wstring());
  group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  document_checker.AppendExpectedChild(&group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  {
    // Focus the div in the document
    AccessibilityNotificationWaiter waiter(
        shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
    ExecuteScript(u"document.body.children[0].focus();");
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  // Check that the accessibility tree of the browser has been updated.
  SCOPED_TRACE("Check updated tree after focusing div");
  group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED);
  document_checker.CheckAccessible(GetRendererAccessible());

  {
    // Focus the document accessible. This will un-focus the current node.
    AccessibilityNotificationWaiter waiter(
        shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kBlur);
    Microsoft::WRL::ComPtr<IAccessible> document_accessible(
        GetRendererAccessible());
    ASSERT_NE(nullptr, document_accessible.Get());
    base::win::ScopedVariant childid_self(CHILDID_SELF);
    HRESULT hr =
        document_accessible->accSelect(SELFLAG_TAKEFOCUS, childid_self);
    ASSERT_EQ(S_OK, hr);
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  // Check that the accessibility tree of the browser has been updated.
  SCOPED_TRACE("Check updated tree after focusing document again");
  group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, FocusEventOnPageLoad) {
  // Some screen readers, such as older versions of Jaws, require a focus event
  // on the top document after the page loads, if there is no focused element on
  // the page.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  {
    base::RunLoop run_loop;
    GURL html_data_url(
        "data:text/html," +
        base::EscapeQueryParamValue(R"HTML(<p> Hello</ p>)HTML", false));
    ui::BrowserAccessibilityManager::SetFocusChangeCallbackForTesting(
        run_loop.QuitClosure());
    EXPECT_TRUE(NavigateToURL(shell(), html_data_url));
    run_loop.Run();  // Wait for the focus change.
  }
  // TODO(crbug.com/40844856): Investigate why this does not return
  // true.
  ASSERT_TRUE(waiter.WaitForNotification());

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  ASSERT_TRUE(document);
  base::win::ScopedVariant focus;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_HRESULT_SUCCEEDED(document->get_accFocus(focus.Receive()));
  EXPECT_EQ(VT_I4, focus.type());
  EXPECT_EQ(CHILDID_SELF, V_I4(focus.ptr()));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, NoFocusEventOnRootChange) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <p>Hello</p>
      )HTML");

  // Adding an iframe as a direct descendant of the root will reserialize the
  // root node.
  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  ExecuteScript(
      u"let iframe = document.createElement('iframe');"
      u"iframe.srcdoc = '<button>Button</button>';"
      u"document.body.appendChild(iframe);");
  ASSERT_TRUE(waiter.WaitForNotification());

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  ASSERT_TRUE(document);
  base::win::ScopedVariant focus;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_HRESULT_SUCCEEDED(document->get_accFocus(focus.Receive()));
  EXPECT_EQ(VT_I4, focus.type());
  EXPECT_EQ(CHILDID_SELF, V_I4(focus.ptr()));
}

// Flaky on win crbug.com/979741
#if BUILDFLAG(IS_WIN)
#define MAYBE_FocusEventOnFocusedIframeAddedAndRemoved \
  DISABLED_FocusEventOnFocusedIframeAddedAndRemoved
#else
#define MAYBE_FocusEventOnFocusedIframeAddedAndRemoved \
  FocusEventOnFocusedIframeAddedAndRemoved
#endif
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       MAYBE_FocusEventOnFocusedIframeAddedAndRemoved) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <button autofocus>Outer button</button>
      )HTML");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  ASSERT_TRUE(document);

  {
    AccessibilityNotificationWaiter iframe_waiter(
        shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
    ExecuteScript(
        u"let iframe = document.createElement('iframe');"
        u"iframe.srcdoc = '<button autofocus>Inner button</button>';"
        u"document.body.appendChild(iframe);");
    WaitForAccessibilityFocusChange();
    ASSERT_TRUE(iframe_waiter.WaitForNotification());

    const ui::BrowserAccessibility* inner_button =
        FindNode(ax::mojom::Role::kButton, "Inner button");
    ASSERT_NE(nullptr, inner_button);
    const auto* inner_button_win =
        ToBrowserAccessibilityWin(inner_button)->GetCOM();
    ASSERT_NE(nullptr, inner_button_win);

    base::win::ScopedVariant focus;
    base::win::ScopedVariant childid_self(CHILDID_SELF);
    ASSERT_HRESULT_SUCCEEDED(document->get_accFocus(focus.Receive()));
    EXPECT_EQ(VT_DISPATCH, focus.type());
    EXPECT_EQ(inner_button_win, V_DISPATCH(focus.ptr()));
  }

  {
    AccessibilityNotificationWaiter iframe_waiter(shell()->web_contents());
    ExecuteScript(u"document.body.removeChild(iframe);");
    WaitForAccessibilityFocusChange();
    ASSERT_TRUE(iframe_waiter.WaitForNotification());

    base::win::ScopedVariant focus;
    base::win::ScopedVariant childid_self(CHILDID_SELF);
    ASSERT_HRESULT_SUCCEEDED(document->get_accFocus(focus.Receive()));
    EXPECT_EQ(VT_I4, focus.type());
    EXPECT_EQ(CHILDID_SELF, V_I4(focus.ptr()));
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       NoFocusEventOnIframeAddedAndRemoved) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <button autofocus>Outer button</button>
      )HTML");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  ASSERT_TRUE(document);

  const ui::BrowserAccessibility* outer_button =
      FindNode(ax::mojom::Role::kButton, "Outer button");
  ASSERT_NE(nullptr, outer_button);
  const auto* outer_button_win =
      ToBrowserAccessibilityWin(outer_button)->GetCOM();
  ASSERT_NE(nullptr, outer_button_win);

  {
    AccessibilityNotificationWaiter waiter(shell()->web_contents());
    ExecuteScript(
        u"let iframe = document.createElement('iframe');"
        u"iframe.srcdoc = '<button>Inner button</button>';"
        u"document.body.appendChild(iframe);");
    ASSERT_TRUE(waiter.WaitForNotification());

    base::win::ScopedVariant focus;
    base::win::ScopedVariant childid_self(CHILDID_SELF);
    ASSERT_HRESULT_SUCCEEDED(document->get_accFocus(focus.Receive()));
    EXPECT_EQ(VT_DISPATCH, focus.type());
    EXPECT_EQ(outer_button_win, V_DISPATCH(focus.ptr()));
  }

  {
    AccessibilityNotificationWaiter waiter(shell()->web_contents());
    ExecuteScript(u"document.body.removeChild(iframe);");
    ASSERT_TRUE(waiter.WaitForNotification());

    base::win::ScopedVariant focus;
    base::win::ScopedVariant childid_self(CHILDID_SELF);
    ASSERT_HRESULT_SUCCEEDED(document->get_accFocus(focus.Receive()));
    EXPECT_EQ(VT_DISPATCH, focus.type());
    EXPECT_EQ(outer_button_win, V_DISPATCH(focus.ptr()));
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationValueChanged) {
  LoadInitialAccessibilityTreeFromHtml(
      "<body><input type='text' value='old value'/></body>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker text_field_checker(std::wstring(), ROLE_SYSTEM_TEXT,
                                       L"old value");
  text_field_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker body_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                 IA2_ROLE_SECTION, std::wstring());
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  body_checker.AppendExpectedChild(&text_field_checker);
  document_checker.AppendExpectedChild(&body_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set the value of the text control
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  ExecuteScript(u"document.body.children[0].value='new value'");
  ASSERT_TRUE(waiter.WaitForNotification());

  // Check that the accessibility tree of the browser has been updated.
  text_field_checker.SetExpectedValue(L"new value");
  document_checker.CheckAccessible(GetRendererAccessible());
}

// This test verifies that the web content's accessibility tree is a
// descendant of the main browser window's accessibility tree, so that
// tools like AccExplorer32 or AccProbe can be used to examine Chrome's
// accessibility support.
//
// If you made a change and this test now fails, check that the NativeViewHost
// that wraps the tab contents returns the IAccessible implementation
// provided by RenderWidgetHostViewWin in GetNativeViewAccessible().
// flaky: http://crbug.com/402190
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_ContainsRendererAccessibilityTree) {
  LoadInitialAccessibilityTreeFromHtml(
      "<html><head><title>MyDocument</title></head>"
      "<body>Content</body></html>");

  // Get the accessibility object for the window tree host.
  aura::Window* window = shell()->window();
  CHECK(window);
  aura::WindowTreeHost* window_tree_host = window->GetHost();
  CHECK(window_tree_host);
  HWND hwnd = window_tree_host->GetAcceleratedWidget();
  CHECK(hwnd);
  Microsoft::WRL::ComPtr<IAccessible> browser_accessible;
  HRESULT hr = AccessibleObjectFromWindow(hwnd, OBJID_WINDOW,
                                          IID_PPV_ARGS(&browser_accessible));
  ASSERT_EQ(S_OK, hr);

  bool found = false;
  FindNodeInAccessibilityTree(browser_accessible.Get(), ROLE_SYSTEM_DOCUMENT,
                              L"MyDocument", 0, &found);
  ASSERT_EQ(found, true);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, SupportsISimpleDOM) {
  LoadInitialAccessibilityTreeFromHtml("<body><input type='checkbox'></body>");

  // Get the IAccessible object for the document.
  Microsoft::WRL::ComPtr<IAccessible> document_accessible(
      GetRendererAccessible());
  ASSERT_NE(document_accessible.Get(), reinterpret_cast<IAccessible*>(NULL));

  // Get the ISimpleDOM object for the document.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = static_cast<IAccessible*>(document_accessible.Get())
                   ->QueryInterface(IID_PPV_ARGS(&service_provider));
  ASSERT_EQ(S_OK, hr);
  const GUID refguid = {0x0c539790,
                        0x12e4,
                        0x11cf,
                        {0xb6, 0x61, 0x00, 0xaa, 0x00, 0x4c, 0xd6, 0xd8}};
  Microsoft::WRL::ComPtr<ISimpleDOMNode> document_isimpledomnode;
  hr = service_provider->QueryService(refguid,
                                      IID_PPV_ARGS(&document_isimpledomnode));
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedBstr node_name;
  short name_space_id;  // NOLINT
  base::win::ScopedBstr node_value;
  unsigned int num_children;
  unsigned int unique_id;
  unsigned short node_type;  // NOLINT
  hr = document_isimpledomnode->get_nodeInfo(
      node_name.Receive(), &name_space_id, node_value.Receive(), &num_children,
      &unique_id, &node_type);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(NODETYPE_DOCUMENT, node_type);
  EXPECT_EQ(1u, num_children);
  node_name.Reset();
  node_value.Reset();

  Microsoft::WRL::ComPtr<ISimpleDOMNode> body_isimpledomnode;
  hr = document_isimpledomnode->get_firstChild(&body_isimpledomnode);
  ASSERT_EQ(S_OK, hr);
  hr = body_isimpledomnode->get_nodeInfo(node_name.Receive(), &name_space_id,
                                         node_value.Receive(), &num_children,
                                         &unique_id, &node_type);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"body", std::wstring(node_name.Get(), node_name.Length()));
  EXPECT_EQ(NODETYPE_ELEMENT, node_type);
  EXPECT_EQ(1u, num_children);
  node_name.Reset();
  node_value.Reset();

  Microsoft::WRL::ComPtr<ISimpleDOMNode> checkbox_isimpledomnode;
  hr = body_isimpledomnode->get_firstChild(&checkbox_isimpledomnode);
  ASSERT_EQ(S_OK, hr);
  hr = checkbox_isimpledomnode->get_nodeInfo(
      node_name.Receive(), &name_space_id, node_value.Receive(), &num_children,
      &unique_id, &node_type);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"input", std::wstring(node_name.Get(), node_name.Length()));
  EXPECT_EQ(NODETYPE_ELEMENT, node_type);
  EXPECT_EQ(0u, num_children);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestRoleGroup) {
  LoadInitialAccessibilityTreeFromHtml(
      "<fieldset></fieldset><div role=group></div>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker grouping1_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                      std::wstring());
  AccessibleChecker grouping2_checker(std::wstring(), ROLE_SYSTEM_GROUPING,
                                      std::wstring());
  AccessibleChecker document_checker(std::wstring(), ROLE_SYSTEM_DOCUMENT,
                                     std::wstring());
  document_checker.AppendExpectedChild(&grouping1_checker);
  document_checker.AppendExpectedChild(&grouping2_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestAccSelectionWithNoSelectedItems) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
<div role="listbox" aria-expanded="true">
<div role="option" aria-selected="false">
Option 1
</div>
<div role="option" aria-selected="false">
Option 2
</div>
<div aria-selected="false">
Option 3
</div>
</div>
      )HTML");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  ASSERT_TRUE(document);
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> listbox;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &listbox));
  LONG listbox_role = 0;
  ASSERT_HRESULT_SUCCEEDED(listbox->role(&listbox_role));
  ASSERT_EQ(ROLE_SYSTEM_LIST, listbox_role);

  base::win::ScopedVariant selected;
  ASSERT_HRESULT_SUCCEEDED(listbox->get_accSelection(selected.Receive()));
  EXPECT_EQ(VT_EMPTY, selected.type());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestAccSelectionWithOneSelectedItem) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
<div role="listbox" aria-expanded="true">
<div role="option" aria-selected="false">
Option 1
</div>
<div role="option" aria-selected="true">
Option 2
</div>
<div aria-selected="false">
Option 3
</div>
</div>
      )HTML");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  ASSERT_TRUE(document);
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> listbox;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &listbox));
  LONG listbox_role = 0;
  ASSERT_HRESULT_SUCCEEDED(listbox->role(&listbox_role));
  ASSERT_EQ(ROLE_SYSTEM_LIST, listbox_role);

  base::win::ScopedVariant selected;
  ASSERT_HRESULT_SUCCEEDED(listbox->get_accSelection(selected.Receive()));
  ASSERT_EQ(VT_DISPATCH, selected.type());

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  Microsoft::WRL::ComPtr<IAccessible2> option;
  ASSERT_HRESULT_SUCCEEDED(
      V_DISPATCH(selected.AsInput())->QueryInterface(IID_PPV_ARGS(&option)));
  LONG option_role = 0;
  EXPECT_HRESULT_SUCCEEDED(option->role(&option_role));
  EXPECT_EQ(ROLE_SYSTEM_LISTITEM, option_role);
  base::win::ScopedBstr option_name;
  EXPECT_HRESULT_SUCCEEDED(
      option->get_accName(childid_self, option_name.Receive()));
  EXPECT_STREQ(L"Option 2", option_name.Get());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestAccSelectionWithMultipleSelectedItems) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
<div role="listbox" aria-expanded="true">
<div role="option" aria-selected="true">
Option 1
</div>
<div role="option" aria-selected="true">
Option 2
</div>
<div aria-selected="false">
Option 3
</div>
</div>
      )HTML");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  ASSERT_TRUE(document);
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> listbox;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &listbox));
  LONG listbox_role = 0;
  ASSERT_HRESULT_SUCCEEDED(listbox->role(&listbox_role));
  ASSERT_EQ(ROLE_SYSTEM_LIST, listbox_role);

  base::win::ScopedVariant selected;
  ASSERT_HRESULT_SUCCEEDED(listbox->get_accSelection(selected.Receive()));
  ASSERT_EQ(VT_UNKNOWN, selected.type());

  Microsoft::WRL::ComPtr<IEnumVARIANT> enum_variant;
  ASSERT_HRESULT_SUCCEEDED(V_UNKNOWN(selected.AsInput())
                               ->QueryInterface(IID_PPV_ARGS(&enum_variant)));

  selected.Release();
  ASSERT_HRESULT_SUCCEEDED(enum_variant->Next(1, selected.Receive(), nullptr));
  ASSERT_EQ(VT_DISPATCH, selected.type());

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  {
    Microsoft::WRL::ComPtr<IAccessible2> option;
    ASSERT_HRESULT_SUCCEEDED(
        V_DISPATCH(selected.AsInput())->QueryInterface(IID_PPV_ARGS(&option)));
    LONG option_role = 0;
    EXPECT_HRESULT_SUCCEEDED(option->role(&option_role));
    EXPECT_EQ(ROLE_SYSTEM_LISTITEM, option_role);
    base::win::ScopedBstr option_name;
    EXPECT_HRESULT_SUCCEEDED(
        option->get_accName(childid_self, option_name.Receive()));
    EXPECT_STREQ(L"Option 1", option_name.Get());
  }

  selected.Release();
  ASSERT_HRESULT_SUCCEEDED(enum_variant->Next(1, selected.Receive(), nullptr));
  ASSERT_EQ(VT_DISPATCH, selected.type());

  {
    Microsoft::WRL::ComPtr<IAccessible2> option;
    ASSERT_HRESULT_SUCCEEDED(
        V_DISPATCH(selected.AsInput())->QueryInterface(IID_PPV_ARGS(&option)));
    LONG option_role = 0;
    EXPECT_HRESULT_SUCCEEDED(option->role(&option_role));
    EXPECT_EQ(ROLE_SYSTEM_LISTITEM, option_role);
    base::win::ScopedBstr option_name;
    EXPECT_HRESULT_SUCCEEDED(
        option->get_accName(childid_self, option_name.Receive()));
    EXPECT_STREQ(L"Option 2", option_name.Get());
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsWithInvalidArguments) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);

  LONG invalid_offset = -3;
  LONG x = -1, y = -1;
  LONG width = -1, height = -1;

  HRESULT hr = paragraph_text->get_characterExtents(
      invalid_offset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height);
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(-1, x);
  EXPECT_EQ(-1, y);
  EXPECT_EQ(-1, width);
  EXPECT_EQ(-1, height);
  hr = paragraph_text->get_characterExtents(
      invalid_offset, IA2_COORDTYPE_PARENT_RELATIVE, &x, &y, &width, &height);
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(-1, x);
  EXPECT_EQ(-1, y);
  EXPECT_EQ(-1, width);
  EXPECT_EQ(-1, height);

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_nCharacters(&n_characters));
  ASSERT_LT(0, n_characters);

  invalid_offset = n_characters + 1;
  hr = paragraph_text->get_characterExtents(
      invalid_offset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height);
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(-1, x);
  EXPECT_EQ(-1, y);
  EXPECT_EQ(-1, width);
  EXPECT_EQ(-1, height);
  hr = paragraph_text->get_characterExtents(
      invalid_offset, IA2_COORDTYPE_PARENT_RELATIVE, &x, &y, &width, &height);
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(-1, x);
  EXPECT_EQ(-1, y);
  EXPECT_EQ(-1, width);
  EXPECT_EQ(-1, height);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsInEditable) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);

  constexpr LONG newline_offset = 46;
  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_nCharacters(&n_characters));
  ASSERT_EQ(105, n_characters);

  LONG x, y, width, height;
  LONG previous_x, previous_y, previous_height;
  for (int coordinate = IA2_COORDTYPE_SCREEN_RELATIVE;
       coordinate <= IA2_COORDTYPE_PARENT_RELATIVE; ++coordinate) {
    auto coordinate_type = static_cast<IA2CoordinateType>(coordinate);
    EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
        0, coordinate_type, &x, &y, &width, &height));
    EXPECT_LT(0, x) << "at offset 0";
    EXPECT_LT(0, y) << "at offset 0";
    EXPECT_LT(1, width) << "at offset 0";
    EXPECT_LT(1, height) << "at offset 0";

    for (LONG offset = 1; offset < newline_offset; ++offset) {
      previous_x = x;
      previous_y = y;
      previous_height = height;

      EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }

    EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
        newline_offset + 1, coordinate_type, &x, &y, &width, &height));
    EXPECT_LE(0, x) << "at offset " << newline_offset + 1;
    EXPECT_GT(previous_x, x) << "at offset " << newline_offset + 1;
    EXPECT_LT(previous_y, y) << "at offset " << newline_offset + 1;
    EXPECT_LT(1, width) << "at offset " << newline_offset + 1;
    EXPECT_EQ(previous_height, height) << "at offset " << newline_offset + 1;

    for (LONG offset = newline_offset + 2; offset < n_characters; ++offset) {
      previous_x = x;
      previous_y = y;
      previous_height = height;

      EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }

    // Past end of text.
    EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
        n_characters, coordinate_type, &x, &y, &width, &height));
    EXPECT_LT(previous_x, x) << "at final offset " << n_characters;
    EXPECT_EQ(previous_y, y) << "at final offset " << n_characters;
    // Last character width past end should be 1, the width of a caret.
    EXPECT_EQ(1, width) << "at final offset " << n_characters;
    EXPECT_EQ(previous_height, height) << "at final offset " << n_characters;
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsInScrollableEditable) {
  Microsoft::WRL::ComPtr<IAccessibleText> editable_container;
  // By construction, only the first line of the content editable is visible.
  SetUpSampleParagraphInScrollableEditable(&editable_container);

  constexpr LONG first_line_end = 5;
  constexpr LONG last_line_start = 8;
  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(editable_container->get_nCharacters(&n_characters));
  ASSERT_EQ(13, n_characters);
  LONG caret_offset;
  ASSERT_HRESULT_SUCCEEDED(editable_container->get_caretOffset(&caret_offset));
  ASSERT_EQ(last_line_start, caret_offset);

  LONG x, y, width, height;
  LONG previous_x, previous_y, previous_height;
  for (int coordinate = IA2_COORDTYPE_SCREEN_RELATIVE;
       coordinate <= IA2_COORDTYPE_PARENT_RELATIVE; ++coordinate) {
    auto coordinate_type = static_cast<IA2CoordinateType>(coordinate);

    // Test that non offscreen characters have increasing x coordinates and a
    // height that is greater than 1px.
    EXPECT_HRESULT_SUCCEEDED(editable_container->get_characterExtents(
        0, coordinate_type, &x, &y, &width, &height));
    EXPECT_LT(0, x) << "at offset 0";
    EXPECT_LT(0, y) << "at offset 0";
    EXPECT_LT(1, width) << "at offset 0";
    EXPECT_LT(1, height) << "at offset 0";

    for (LONG offset = 1; offset < first_line_end; ++offset) {
      previous_x = x;
      previous_y = y;
      previous_height = height;

      EXPECT_HRESULT_SUCCEEDED(editable_container->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }

    EXPECT_HRESULT_SUCCEEDED(editable_container->get_characterExtents(
        last_line_start, coordinate_type, &x, &y, &width, &height));
    EXPECT_LT(0, x) << "at offset " << last_line_start;
    EXPECT_LT(previous_y, y) << "at offset " << last_line_start;
    EXPECT_LT(1, width) << "at offset " << last_line_start;
    EXPECT_EQ(previous_height, height) << "at offset " << last_line_start;

    for (LONG offset = last_line_start + 1; offset < n_characters; ++offset) {
      previous_x = x;
      previous_y = y;

      EXPECT_HRESULT_SUCCEEDED(editable_container->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsInScrollableInputField) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpScrollableInputField(&input_text);

  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  constexpr LONG visible_characters_start = 21;
  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(contents_string_length, n_characters);
  LONG caret_offset;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_caretOffset(&caret_offset));
  ASSERT_EQ(contents_string_length - 1, caret_offset);

  LONG x, y, width, height;
  LONG previous_x, previous_y, previous_height;
  for (int coordinate = IA2_COORDTYPE_SCREEN_RELATIVE;
       coordinate <= IA2_COORDTYPE_PARENT_RELATIVE; ++coordinate) {
    auto coordinate_type = static_cast<IA2CoordinateType>(coordinate);

    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        0, coordinate_type, &x, &y, &width, &height));
    EXPECT_GT(0, x + width) << "at offset 0";
    EXPECT_LT(0, y) << "at offset 0";
    EXPECT_LT(1, width) << "at offset 0";
    EXPECT_LT(1, height) << "at offset 0";

    for (LONG offset = 1; offset < (visible_characters_start - 1); ++offset) {
      previous_x = x;
      previous_y = y;
      previous_height = height;

      EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }

    // Test that non offscreen characters have increasing x coordinates and a
    // width that is greater than 1px.
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        visible_characters_start, coordinate_type, &x, &y, &width, &height));
    EXPECT_LT(previous_x, x) << "at offset " << visible_characters_start;
    EXPECT_EQ(previous_y, y) << "at offset " << visible_characters_start;
    EXPECT_LT(1, width) << "at offset " << visible_characters_start;
    EXPECT_EQ(previous_height, height)
        << "at offset " << visible_characters_start;

    // Exclude the dot at the end of the text field, because it has a width of
    // one anyway.
    for (LONG offset = visible_characters_start + 1;
         offset < (n_characters - 1); ++offset) {
      previous_x = x;
      previous_y = y;
      previous_height = height;

      EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }
    // Past end of text.
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        n_characters, coordinate_type, &x, &y, &width, &height));
    EXPECT_LT(previous_x, x) << "at final offset " << n_characters;
    EXPECT_EQ(previous_y, y) << "at final offset " << n_characters;
    // Last character width past end should be 1, the width of a caret.
    EXPECT_EQ(1, width) << "at final offset " << n_characters;
    EXPECT_EQ(previous_height, height) << "at final offset " << n_characters;
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsInEmptyInputField) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpSingleCharInputField(&input_text);

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(1, n_characters);

  // Get the rect for the only character.
  LONG prev_x, prev_y, prev_width, prev_height;
  EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
      0, IA2_COORDTYPE_SCREEN_RELATIVE, &prev_x, &prev_y, &prev_width,
      &prev_height));

  EXPECT_LT(1, prev_width);
  EXPECT_LT(1, prev_height);

  // Delete the character in the input field.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  ExecuteScript(u"document.querySelector('input').value='';");
  ASSERT_TRUE(waiter.WaitForNotification());

  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(0, n_characters);
  LONG caret_offset;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_caretOffset(&caret_offset));
  ASSERT_EQ(0, caret_offset);

  {
    base::win::ScopedBstr text;
    ASSERT_HRESULT_SUCCEEDED(
        input_text->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
    EXPECT_EQ(nullptr, text.Get());
  }

  // Now that input is completely empty, the position of the caret should be
  // returned for character 0. The x,y position and height should be the same as
  // it was as when there was single character, but the width should now be 1.
  LONG x, y, width, height;
  for (int offset = IA2_TEXT_OFFSET_CARET; offset <= 0; ++offset) {
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        offset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
    EXPECT_EQ(prev_x, x);
    EXPECT_EQ(prev_y, y);
    EXPECT_EQ(1, width);
    EXPECT_EQ(prev_height, height);
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsInEmptyInputFieldWithPlaceholder) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpSingleCharInputFieldWithPlaceholder(&input_text);

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(1, n_characters);

  // Get the rect for the only character.
  LONG prev_x, prev_y, prev_width, prev_height;
  EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
      0, IA2_COORDTYPE_SCREEN_RELATIVE, &prev_x, &prev_y, &prev_width,
      &prev_height));

  EXPECT_LT(1, prev_width);
  EXPECT_LT(1, prev_height);

  {
    base::win::ScopedBstr text;
    ASSERT_HRESULT_SUCCEEDED(
        input_text->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
    EXPECT_STREQ(L"x", text.Get());
  }

  // Delete the character in the input field.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  ExecuteScript(u"document.querySelector('input').value='';");
  ASSERT_TRUE(waiter.WaitForNotification());

  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  // When the text field is empty, the placeholder text should become visible.
  ASSERT_EQ(0, n_characters);
  LONG caret_offset;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_caretOffset(&caret_offset));
  ASSERT_EQ(0, caret_offset);

  base::win::ScopedBstr text;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_text(0, -1, text.Receive()));

  // Now that input is completely empty, the position of the caret should be
  // returned for character 0. The x,y position and height should be the same as
  // it was as when there was single character, but the width should now be 1.
  for (int offset = IA2_TEXT_OFFSET_CARET; offset <= 0; ++offset) {
    LONG x, y, width, height;
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        offset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
    EXPECT_EQ(prev_x, x);
    EXPECT_EQ(prev_y, y);
    EXPECT_EQ(1, width);
    EXPECT_EQ(prev_height, height);
  }

  {
    LONG x, y, width, height;
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        IA2_TEXT_OFFSET_LENGTH, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width,
        &height));
    EXPECT_EQ(prev_x, x);
    EXPECT_EQ(prev_y, y);
    EXPECT_EQ(1, width);
    EXPECT_EQ(prev_height, height);
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsInScrollableInputTypeSearchField) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpScrollableInputTypeSearchField(&input_text);

  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  constexpr LONG visible_characters_start = 21;
  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(contents_string_length, n_characters);
  LONG caret_offset;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_caretOffset(&caret_offset));
  ASSERT_EQ(contents_string_length - 1, caret_offset);

  LONG x, y, width, height;
  LONG previous_x, previous_y, previous_height;
  for (int coordinate = IA2_COORDTYPE_SCREEN_RELATIVE;
       coordinate <= IA2_COORDTYPE_PARENT_RELATIVE; ++coordinate) {
    auto coordinate_type = static_cast<IA2CoordinateType>(coordinate);

    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        0, coordinate_type, &x, &y, &width, &height));
    EXPECT_GT(0, x + width) << "at offset 0";
    EXPECT_LT(0, y) << "at offset 0";
    EXPECT_LT(1, width) << "at offset 0";
    EXPECT_LT(1, height) << "at offset 0";

    for (LONG offset = 1; offset < (visible_characters_start - 1); ++offset) {
      previous_x = x;
      previous_y = y;
      previous_height = height;

      EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }

    // Test that non offscreen characters have increasing x coordinates and a
    // width that is greater than 1px.
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        visible_characters_start, coordinate_type, &x, &y, &width, &height));
    EXPECT_LT(previous_x, x) << "at offset " << visible_characters_start;
    EXPECT_EQ(previous_y, y) << "at offset " << visible_characters_start;
    EXPECT_LT(1, width) << "at offset " << visible_characters_start;
    EXPECT_EQ(previous_height, height)
        << "at offset " << visible_characters_start;

    // Exclude the dot at the end of the text field, because it has a width of
    // one anyway.
    for (LONG offset = visible_characters_start + 1;
         offset < (n_characters - 1); ++offset) {
      previous_x = x;
      previous_y = y;
      previous_height = height;

      EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
          offset, coordinate_type, &x, &y, &width, &height));
      EXPECT_LT(previous_x, x) << "at offset " << offset;
      EXPECT_EQ(previous_y, y) << "at offset " << offset;
      EXPECT_LT(1, width) << "at offset " << offset;
      EXPECT_EQ(previous_height, height) << "at offset " << offset;
    }
    // Past end of text.
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        n_characters, coordinate_type, &x, &y, &width, &height));
    EXPECT_LT(previous_x, x) << "at final offset " << n_characters;
    EXPECT_EQ(previous_y, y) << "at final offset " << n_characters;
    // Last character width past end should be 1, the width of a caret.
    EXPECT_EQ(1, width) << "at final offset " << n_characters;
    EXPECT_EQ(previous_height, height) << "at final offset " << n_characters;
  }
}

// TODO(accessibility) empty contenteditable gets height of entire
// contenteditable instead of just 1 line. Maybe we are able to use the
// following in Blink to get the height of a line -- it's at least close:
// layout_object->Style()->GetFont().PrimaryFont()->GetFontMetrics().Height()
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_TestCharacterExtentsInEmptyContenteditable) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpSampleParagraphInScrollableEditable(&input_text);

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_LT(0, n_characters);

  // Get the rect for the only character.
  LONG prev_x, prev_y, prev_width, prev_height;
  EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
      0, IA2_COORDTYPE_SCREEN_RELATIVE, &prev_x, &prev_y, &prev_width,
      &prev_height));

  EXPECT_LT(1, prev_width);
  EXPECT_LT(1, prev_height);

  {
    base::win::ScopedBstr text;
    ASSERT_HRESULT_SUCCEEDED(
        input_text->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
    EXPECT_STREQ(L"hello\n\n\nhello", text.Get());
  }

  // Delete the character in the input field.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::CHILDREN_CHANGED);
  ExecuteScript(u"document.querySelector('[contenteditable]').innerText='';");
  ASSERT_TRUE(waiter.WaitForNotification());

  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(0, n_characters);
  LONG caret_offset;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_caretOffset(&caret_offset));
  ASSERT_EQ(0, caret_offset);

  {
    base::win::ScopedBstr text;
    ASSERT_HRESULT_SUCCEEDED(
        input_text->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
    EXPECT_EQ(nullptr, text.Get());
  }

  // Now that input is completely empty, the position of the caret should be
  // returned for character 0. The x,y position and height should be the same as
  // it was as when there was single character, but the width should now be 1.
  LONG x, y, width, height;
  for (int offset = IA2_TEXT_OFFSET_CARET; offset <= 0; ++offset) {
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        offset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
    EXPECT_EQ(prev_x, x);
    EXPECT_EQ(prev_y, y);
    EXPECT_EQ(1, width);
    EXPECT_EQ(prev_height, height);
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsInEmptyTextarea) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpSingleCharTextarea(&input_text);

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(1, n_characters);

  // Get the rect for the only character.
  LONG prev_x, prev_y, prev_width, prev_height;
  EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
      0, IA2_COORDTYPE_SCREEN_RELATIVE, &prev_x, &prev_y, &prev_width,
      &prev_height));

  EXPECT_LT(1, prev_width);
  EXPECT_LT(1, prev_height);

  {
    base::win::ScopedBstr text;
    ASSERT_HRESULT_SUCCEEDED(
        input_text->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
    EXPECT_STREQ(L"x", text.Get());
  }

  // Delete the character in the input field.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  ExecuteScript(u"document.querySelector('textarea').innerText='';");
  ASSERT_TRUE(waiter.WaitForNotification());

  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(0, n_characters);
  LONG caret_offset;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_caretOffset(&caret_offset));
  ASSERT_EQ(0, caret_offset);

  {
    base::win::ScopedBstr text;
    ASSERT_HRESULT_SUCCEEDED(
        input_text->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
    EXPECT_EQ(nullptr, text.Get());
  }

  // Now that input is completely empty, the position of the caret should be
  // returned for character 0. The x,y position and height should be the same as
  // it was as when there was single character, but the width should now be 1.
  LONG x, y, width, height;
  for (int offset = IA2_TEXT_OFFSET_CARET; offset <= 0; ++offset) {
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        offset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
    EXPECT_EQ(prev_x, x);
    EXPECT_EQ(prev_y, y);
    EXPECT_EQ(1, width);
    EXPECT_EQ(prev_height, height);
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsInEmptyRtlInputField) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpSingleCharRtlInputField(&input_text);

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(1, n_characters);

  // Get the rect for the only character.
  LONG prev_x, prev_y, prev_width, prev_height;
  EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
      0, IA2_COORDTYPE_SCREEN_RELATIVE, &prev_x, &prev_y, &prev_width,
      &prev_height));

  EXPECT_LT(1, prev_width);
  EXPECT_LT(1, prev_height);

  {
    base::win::ScopedBstr text;
    ASSERT_HRESULT_SUCCEEDED(
        input_text->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
    EXPECT_STREQ(L"x", text.Get());
  }

  // Delete the character in the input field.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  ExecuteScript(
      u"const input = document.querySelector('input');"
      u"input.value='';");
  ASSERT_TRUE(waiter.WaitForNotification());

  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  ASSERT_EQ(0, n_characters);
  LONG caret_offset;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_caretOffset(&caret_offset));
  ASSERT_EQ(0, caret_offset);

  {
    base::win::ScopedBstr text;
    ASSERT_HRESULT_SUCCEEDED(
        input_text->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
    EXPECT_EQ(nullptr, text.Get());
  }

  // Now that input is completely empty, the position of the caret should be
  // returned for character 0. The x,y position and height should be the same as
  // it was as when there was single character, but the width should now be 1.
  LONG x, y, width, height;
  for (int offset = IA2_TEXT_OFFSET_CARET; offset <= 0; ++offset) {
    EXPECT_HRESULT_SUCCEEDED(input_text->get_characterExtents(
        offset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
    // TODO(accessibility) Why do results keep changing on each run?
    EXPECT_GE(prev_x + prev_width - 1, x);
    EXPECT_EQ(prev_y, y);
    EXPECT_EQ(1, width);
    EXPECT_EQ(prev_height, height);
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestCharacterExtentsWithAccessibilityModeChange) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text, ui::AXMode::kNativeAPIs |
                                            ui::AXMode::kWebContents |
                                            ui::AXMode::kScreenReader);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents |
          ui::AXMode::kScreenReader | ui::AXMode::kInlineTextBoxes,
      ax::mojom::Event::kLoadComplete);

  // Calling `get_characterExtents` will enable `ui::AXMode::kInlineTextBoxes`
  // as well.
  LONG x, y, width, height;
  EXPECT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
      0, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  // X and y coordinates should be available without
  // |ui::AXMode::kInlineTextBoxes|.
  EXPECT_LT(0, x);
  EXPECT_LT(0, y);
  // Width and height should be unavailable at this point.
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);

  ASSERT_TRUE(waiter.WaitForNotification());

  // Inline text boxes should have been enabled by this point but since the tree
  // has been updated, any previously retrieved IAccessibles would have been
  // invalidated.
  ui::BrowserAccessibility* updated_paragraph_text =
      FindNode(ax::mojom::Role::kParagraph, "");
  ASSERT_NE(nullptr, updated_paragraph_text);
  auto* updated_paragraph_text_win =
      ToBrowserAccessibilityWin(updated_paragraph_text)->GetCOM();
  ASSERT_NE(nullptr, updated_paragraph_text_win);

  EXPECT_HRESULT_SUCCEEDED(updated_paragraph_text_win->get_characterExtents(
      0, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  EXPECT_LT(0, x);
  EXPECT_LT(0, y);
  EXPECT_LT(1, width);
  EXPECT_LT(1, height);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestBasicMSAAAccessibilityModeChange) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <p>Hello world.</p>
      )HTML");
  // Get the accessibility object for the window tree host.
  aura::Window* window = shell()->window();
  CHECK(window);
  aura::WindowTreeHost* window_tree_host = window->GetHost();
  CHECK(window_tree_host);
  HWND hwnd = window_tree_host->GetAcceleratedWidget();
  CHECK(hwnd);
  Microsoft::WRL::ComPtr<IAccessible> browser_accessible;
  HRESULT hr = AccessibleObjectFromWindow(hwnd, OBJID_WINDOW,
                                          IID_PPV_ARGS(&browser_accessible));
  ASSERT_EQ(S_OK, hr);

  // Ensure that we can find accessibility nodes in web contents.
  bool found = false;
  FindNodeInAccessibilityTree(browser_accessible.Get(), ROLE_SYSTEM_STATICTEXT,
                              L"Hello world.", 0, &found);
  EXPECT_TRUE(found);

  // Remove all accessibility modes.
  content::BrowserAccessibilityState::GetInstance()->ResetAccessibilityMode();

  // Ensure accessibility is not enabled before we begin the test.
  EXPECT_TRUE(content::BrowserAccessibilityStateImpl::GetInstance()
                  ->GetAccessibilityMode()
                  .is_mode_off());

  // Search for the document, we should be able to find it.
  found = false;
  FindNodeInAccessibilityTree(browser_accessible.Get(), ROLE_SYSTEM_DOCUMENT,
                              L"", 0, &found);
  EXPECT_TRUE(found);

  // The act of searching for the document should enable kNativeAPIs
  EXPECT_EQ(ui::AXMode(ui::AXMode::kNativeAPIs),
            content::BrowserAccessibilityStateImpl::GetInstance()
                ->GetAccessibilityMode());

  // Even with kNativeAPIs, we still shouldn't be able to find the node in web
  // contents.
  found = false;
  FindNodeInAccessibilityTree(browser_accessible.Get(), ROLE_SYSTEM_STATICTEXT,
                              L"Hello world.", 0, &found);
  EXPECT_FALSE(found);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestScrollToPoint) {
  Microsoft::WRL::ComPtr<IAccessibleText> accessible_text;
  SetUpSampleParagraphInScrollableDocument(&accessible_text);
  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(accessible_text.As(&paragraph));

  LONG prev_x, prev_y, x, y, width, height;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_HRESULT_SUCCEEDED(
      paragraph->accLocation(&prev_x, &prev_y, &width, &height, childid_self));
  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);
  EXPECT_HRESULT_SUCCEEDED(
      paragraph->scrollToPoint(IA2_COORDTYPE_PARENT_RELATIVE, 0, 0));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());

  ASSERT_HRESULT_SUCCEEDED(
      paragraph->accLocation(&x, &y, &width, &height, childid_self));
  EXPECT_EQ(prev_x, x);
  EXPECT_GT(prev_y, y);

  constexpr int kScrollToY = 0;
  EXPECT_HRESULT_SUCCEEDED(
      paragraph->scrollToPoint(IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToY));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(
      paragraph->accLocation(&x, &y, &width, &height, childid_self));
  EXPECT_EQ(kScrollToY, y);

  constexpr int kScrollToY_2 = 243;
  EXPECT_HRESULT_SUCCEEDED(
      paragraph->scrollToPoint(IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToY_2));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(
      paragraph->accLocation(&x, &y, &width, &height, childid_self));
  EXPECT_EQ(kScrollToY_2, y);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestScrollSubstringToPoint) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraphInScrollableDocument(&paragraph_text);
  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text.As(&paragraph));

  LONG x, y, width, height;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);

  constexpr int kScrollToY = 187;
  constexpr int kCharOffset = 10;
  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      kCharOffset, kCharOffset + 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0,
      kScrollToY));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
      kCharOffset, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  EXPECT_EQ(kScrollToY, y);

  constexpr int kScrollToY_2 = -133;
  constexpr int kCharOffset_2 = 30;
  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      kCharOffset_2, kCharOffset_2 + 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0,
      kScrollToY_2));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
      kCharOffset_2, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  EXPECT_EQ(kScrollToY_2, y);
}

// https://crbug.com/948612
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestScrollVeryTallPaddingNearBottom) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpVeryTallPadding(&paragraph_text);
  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text.As(&paragraph));

  LONG x, y, width, height;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);

  constexpr int kScrollToY = 500;
  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      1, 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToY));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
      1, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  EXPECT_EQ(kScrollToY, y);
}

// https://crbug.com/948612
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestScrollVeryTallPaddingNearTopOnce) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpVeryTallPadding(&paragraph_text);
  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text.As(&paragraph));

  LONG x, y, width, height;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);

  constexpr int kScrollToY = 30;
  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      1, 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToY));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
      1, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  EXPECT_EQ(kScrollToY, y);
}

// https://crbug.com/948612
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestScrollVeryTallPaddingNearTopTwice) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpVeryTallPadding(&paragraph_text);
  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text.As(&paragraph));

  LONG x, y, width, height;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);

  constexpr int kScrollToY = 30;
  constexpr int kScrollToYInitial = kScrollToY - 1;
  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      1, 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToYInitial));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());

  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      1, 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToY));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
      1, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  EXPECT_EQ(kScrollToY, y);
}

// https://crbug.com/948612
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestScrollVeryTallMarginNearBottom) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpVeryTallMargin(&paragraph_text);
  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text.As(&paragraph));

  LONG x, y, width, height;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);

  constexpr int kScrollToY = 500;
  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      1, 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToY));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
      1, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  EXPECT_EQ(kScrollToY, y);
}

// https://crbug.com/948612
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestScrollVeryTallMarginNearTopOnce) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpVeryTallMargin(&paragraph_text);
  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text.As(&paragraph));

  LONG x, y, width, height;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);

  constexpr int kScrollToY = 30;
  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      1, 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToY));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
      1, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  EXPECT_EQ(kScrollToY, y);
}

// https://crbug.com/948612
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestScrollVeryTallMarginNearTopTwice) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpVeryTallMargin(&paragraph_text);
  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text.As(&paragraph));

  LONG x, y, width, height;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);

  constexpr int kScrollToY = 30;
  constexpr int kScrollToYInitial = kScrollToY - 1;
  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      1, 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToYInitial));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());

  EXPECT_HRESULT_SUCCEEDED(paragraph_text->scrollSubstringToPoint(
      1, 1, IA2_COORDTYPE_SCREEN_RELATIVE, 0, kScrollToY));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_characterExtents(
      1, IA2_COORDTYPE_SCREEN_RELATIVE, &x, &y, &width, &height));
  EXPECT_EQ(kScrollToY, y);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestPutAccValueInInputField) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  Microsoft::WRL::ComPtr<IAccessible2> input;
  ASSERT_HRESULT_SUCCEEDED(input_text.As(&input));

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr new_value(L"New value");
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED);
  EXPECT_HRESULT_SUCCEEDED(input->put_accValue(childid_self, new_value.Get()));
  ASSERT_TRUE(waiter.WaitForNotification());

  base::win::ScopedBstr value;
  EXPECT_HRESULT_SUCCEEDED(input->get_accValue(childid_self, value.Receive()));
  ASSERT_NE(nullptr, value.Get());
  EXPECT_STREQ(new_value.Get(), value.Get());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestPutAccValueInTextarea) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  Microsoft::WRL::ComPtr<IAccessible2> textarea;
  ASSERT_HRESULT_SUCCEEDED(textarea_text.As(&textarea));

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr new_value(L"New value");
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  EXPECT_HRESULT_SUCCEEDED(
      textarea->put_accValue(childid_self, new_value.Get()));
  ASSERT_TRUE(waiter.WaitForNotification());

  base::win::ScopedBstr value;
  EXPECT_HRESULT_SUCCEEDED(
      textarea->get_accValue(childid_self, value.Receive()));
  ASSERT_NE(nullptr, value.Get());
  EXPECT_STREQ(new_value.Get(), value.Get());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestPutAccValueInEditable) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraphInScrollableEditable(&paragraph_text);

  Microsoft::WRL::ComPtr<IAccessible2> paragraph;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text.As(&paragraph));

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr new_value(L"New value");
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::CHILDREN_CHANGED);
  EXPECT_HRESULT_SUCCEEDED(
      paragraph->put_accValue(childid_self, new_value.Get()));
  ASSERT_TRUE(waiter.WaitForNotification());

  base::win::ScopedBstr value;
  EXPECT_HRESULT_SUCCEEDED(
      paragraph->get_accValue(childid_self, value.Receive()));
  ASSERT_NE(nullptr, value.Get());
  EXPECT_STREQ(new_value.Get(), value.Get());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestSetCaretOffset) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  LONG caret_offset = 0;
  HRESULT hr = input_text->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(static_cast<LONG>(InputContentsString().size() - 1), caret_offset);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  caret_offset = 0;
  hr = input_text->setCaretOffset(caret_offset);
  EXPECT_EQ(S_OK, hr);
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = input_text->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, caret_offset);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineSetCaretOffset) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  LONG caret_offset = 0;
  HRESULT hr = textarea_text->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(static_cast<LONG>(InputContentsString().size() - 1), caret_offset);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  caret_offset = 0;
  hr = textarea_text->setCaretOffset(caret_offset);
  EXPECT_EQ(S_OK, hr);
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = textarea_text->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, caret_offset);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestSetSelection) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  LONG start_offset, end_offset;
  EXPECT_HRESULT_FAILED(
      input_text->get_selection(1, &start_offset, &end_offset));
  HRESULT hr = input_text->get_selection(0, &start_offset, &end_offset);
  // There is no selection, just a caret.
  EXPECT_EQ(E_INVALIDARG, hr);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  start_offset = 0;
  end_offset = contents_string_length;
  EXPECT_HRESULT_FAILED(input_text->setSelection(1, start_offset, end_offset));
  EXPECT_HRESULT_SUCCEEDED(
      input_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = input_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(contents_string_length, end_offset);

  start_offset = contents_string_length;
  end_offset = 1;
  EXPECT_HRESULT_SUCCEEDED(
      input_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = input_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  // Start and end offsets should always be swapped to be in ascending order
  // according to the IA2 Spec.
  EXPECT_EQ(1, start_offset);
  EXPECT_EQ(contents_string_length, end_offset);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestSetSelectionRanges) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);
  Microsoft::WRL::ComPtr<IAccessible2_4> ax_input;
  ASSERT_HRESULT_SUCCEEDED(input_text.As(&ax_input));

  LONG n_ranges = 1;
  IA2Range* ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemAlloc(sizeof(IA2Range)));
  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  ranges[0].anchor = ax_input.Get();
  ranges[0].anchorOffset = -1;
  ranges[0].active = ax_input.Get();
  ranges[0].activeOffset = contents_string_length;
  EXPECT_HRESULT_FAILED(ax_input->setSelectionRanges(n_ranges, ranges));
  ranges[0].anchorOffset = 0;
  ranges[0].activeOffset = contents_string_length + 1;
  EXPECT_HRESULT_FAILED(ax_input->setSelectionRanges(n_ranges, ranges));

  ranges[0].activeOffset = contents_string_length;
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  EXPECT_HRESULT_SUCCEEDED(ax_input->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());
  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  HRESULT hr = ax_input->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  EXPECT_EQ(ax_input.Get(), ranges[0].anchor);
  EXPECT_EQ(0, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  EXPECT_EQ(ax_input.Get(), ranges[0].active);
  EXPECT_EQ(contents_string_length, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  n_ranges = 1;
  ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemRealloc(ranges, sizeof(IA2Range)));
  ranges[0].anchor = ax_input.Get();
  ranges[0].anchorOffset = contents_string_length;
  ranges[0].active = ax_input.Get();
  ranges[0].activeOffset = 1;
  EXPECT_HRESULT_SUCCEEDED(ax_input->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());
  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  hr = ax_input->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  EXPECT_EQ(ax_input.Get(), ranges[0].anchor);
  EXPECT_EQ(contents_string_length, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  EXPECT_EQ(ax_input.Get(), ranges[0].active);
  EXPECT_EQ(1, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  CoTaskMemFree(ranges);
  ranges = nullptr;
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestSetSelectionRangesIFrame) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(
      "data:text/html,"
      "<!doctype html><html><body>"
      "Text before iframe"
      "<iframe src='data:text/html,"
      "<!doctype html><html><body>"
      "<button>Text in iframe</button></body></html>"
      "'></iframe>"
      "<button>Text after iframe</button>"
      "</body></html>");
  ASSERT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());
  Microsoft::WRL::ComPtr<IAccessible2> body_iaccessible2;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &body_iaccessible2));

  std::vector<base::win::ScopedVariant> body_children =
      GetAllAccessibleChildren(body_iaccessible2.Get());
  ASSERT_EQ(3u, body_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> iframe;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), body_children[1].AsInput())
          .Get(),
      &iframe));

  std::vector<base::win::ScopedVariant> iframe_children =
      GetAllAccessibleChildren(iframe.Get());
  ASSERT_EQ(1u, iframe_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> iframe_body;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), iframe_children[0].AsInput())
          .Get(),
      &iframe_body));

  std::vector<base::win::ScopedVariant> iframe_body_children =
      GetAllAccessibleChildren(iframe_body.Get());
  ASSERT_EQ(1u, iframe_body_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> text_in_iframe;
  ASSERT_HRESULT_SUCCEEDED(
      QueryIAccessible2(GetAccessibleFromVariant(
                            document.Get(), iframe_body_children[0].AsInput())
                            .Get(),
                        &text_in_iframe));

  Microsoft::WRL::ComPtr<IAccessible2> text_after_iframe;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), body_children[2].AsInput())
          .Get(),
      &text_after_iframe));

  Microsoft::WRL::ComPtr<IAccessible2_4> text_after_iframe_iaccessible2_4;
  ASSERT_HRESULT_SUCCEEDED(
      text_after_iframe.As(&text_after_iframe_iaccessible2_4));

  LONG n_ranges = 1;
  IA2Range* cross_tree_ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemAlloc(sizeof(IA2Range)));
  cross_tree_ranges[0].anchor = text_in_iframe.Get();
  cross_tree_ranges[0].anchorOffset = 0;
  cross_tree_ranges[0].active = text_after_iframe.Get();
  cross_tree_ranges[0].activeOffset = 2;

  // This is expected to fail because the anchor and focus nodes are in
  // different trees, which Blink doesn't support.
  EXPECT_HRESULT_FAILED(text_after_iframe_iaccessible2_4->setSelectionRanges(
      n_ranges, cross_tree_ranges));

  CoTaskMemFree(cross_tree_ranges);
  cross_tree_ranges = nullptr;

  // Now test a variation where the selection start and end are in the same
  // tree as each other, but a different tree than the caller.
  IA2Range* same_tree_ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemAlloc(sizeof(IA2Range)));
  same_tree_ranges[0].anchor = text_in_iframe.Get();
  same_tree_ranges[0].anchorOffset = 0;
  same_tree_ranges[0].active = text_in_iframe.Get();
  same_tree_ranges[0].activeOffset = 1;

  // This should succeed, however the selection will need to be queried from
  // a node in the iframe tree.
  AccessibilityNotificationWaiter selection_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);
  ASSERT_HRESULT_SUCCEEDED(text_after_iframe_iaccessible2_4->setSelectionRanges(
      n_ranges, same_tree_ranges));
  ASSERT_TRUE(selection_waiter.WaitForNotification());

  Microsoft::WRL::ComPtr<IAccessible2_4> text_in_iframe_iaccessible2_4;
  ASSERT_HRESULT_SUCCEEDED(text_in_iframe.As(&text_in_iframe_iaccessible2_4));

  IA2Range* result_ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemAlloc(sizeof(IA2Range)));
  HRESULT hr = text_in_iframe_iaccessible2_4->get_selectionRanges(
      &result_ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, result_ranges);
  ASSERT_NE(nullptr, result_ranges[0].anchor);
  EXPECT_EQ(text_in_iframe.Get(), result_ranges[0].anchor);
  EXPECT_EQ(0, result_ranges[0].anchorOffset);
  ASSERT_NE(nullptr, result_ranges[0].active);
  EXPECT_EQ(text_in_iframe.Get(), result_ranges[0].active);
  EXPECT_EQ(1, result_ranges[0].activeOffset);

  same_tree_ranges[0].anchor->Release();
  same_tree_ranges[0].active->Release();
  CoTaskMemFree(same_tree_ranges);
  same_tree_ranges = nullptr;

  CoTaskMemFree(result_ranges);
  result_ranges = nullptr;
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestMultiLineSetSelection) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  LONG start_offset, end_offset;
  EXPECT_HRESULT_FAILED(
      textarea_text->get_selection(1, &start_offset, &end_offset));
  HRESULT hr = textarea_text->get_selection(0, &start_offset, &end_offset);
  // There is no selection, just a caret.
  EXPECT_EQ(E_INVALIDARG, hr);

  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  start_offset = 0;
  end_offset = contents_string_length;
  EXPECT_HRESULT_FAILED(
      textarea_text->setSelection(1, start_offset, end_offset));
  EXPECT_HRESULT_SUCCEEDED(
      textarea_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = textarea_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(contents_string_length, end_offset);

  start_offset = contents_string_length - 1;
  end_offset = 0;
  EXPECT_HRESULT_SUCCEEDED(
      textarea_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = textarea_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  // Start and end offsets are always swapped to be in ascending order.
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(contents_string_length - 1, end_offset);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineSetSelectionRanges) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);
  Microsoft::WRL::ComPtr<IAccessible2_4> ax_textarea;
  ASSERT_HRESULT_SUCCEEDED(textarea_text.As(&ax_textarea));

  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  LONG n_ranges = 1;
  IA2Range* ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemAlloc(sizeof(IA2Range)));
  ranges[0].anchor = ax_textarea.Get();
  ranges[0].anchorOffset = -1;
  ranges[0].active = ax_textarea.Get();
  ranges[0].activeOffset = contents_string_length;
  EXPECT_HRESULT_FAILED(ax_textarea->setSelectionRanges(n_ranges, ranges));
  ranges[0].anchorOffset = 0;
  ranges[0].activeOffset = contents_string_length + 1;
  EXPECT_HRESULT_FAILED(ax_textarea->setSelectionRanges(n_ranges, ranges));

  ranges[0].activeOffset = contents_string_length;
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  EXPECT_HRESULT_SUCCEEDED(ax_textarea->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());
  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  HRESULT hr = ax_textarea->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  EXPECT_EQ(ax_textarea.Get(), ranges[0].anchor);
  EXPECT_EQ(0, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  EXPECT_EQ(ax_textarea.Get(), ranges[0].active);
  EXPECT_EQ(contents_string_length, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  n_ranges = 1;
  ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemRealloc(ranges, sizeof(IA2Range)));

  ranges[0].anchor = ax_textarea.Get();
  ranges[0].anchorOffset = contents_string_length - 1;
  ranges[0].active = ax_textarea.Get();
  ranges[0].activeOffset = 0;
  EXPECT_HRESULT_SUCCEEDED(ax_textarea->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());

  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  hr = ax_textarea->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  EXPECT_EQ(ax_textarea.Get(), ranges[0].anchor);
  EXPECT_EQ(contents_string_length - 1, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  EXPECT_EQ(ax_textarea.Get(), ranges[0].active);
  EXPECT_EQ(0, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  CoTaskMemFree(ranges);
  ranges = nullptr;
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestStaticTextSetSelection) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_nCharacters(&n_characters));
  ASSERT_LT(0, n_characters);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);
  LONG start_offset = 0;
  LONG end_offset = n_characters;
  EXPECT_HRESULT_FAILED(
      paragraph_text->setSelection(1, start_offset, end_offset));
  EXPECT_HRESULT_SUCCEEDED(
      paragraph_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  HRESULT hr = paragraph_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(n_characters, end_offset);

  start_offset = n_characters - 1;
  end_offset = 0;
  EXPECT_HRESULT_SUCCEEDED(
      paragraph_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = paragraph_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  // Start and end offsets are always swapped to be in ascending order.
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(n_characters - 1, end_offset);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestStaticTextSetSelectionRanges) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);
  Microsoft::WRL::ComPtr<IAccessible2_4> ax_paragraph;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text.As(&ax_paragraph));

  LONG child_count = 0;
  ASSERT_HRESULT_SUCCEEDED(ax_paragraph->get_accChildCount(&child_count));
  ASSERT_LT(0, child_count);

  // IAccessible retrieves children using an one-based index.
  base::win::ScopedVariant one_variant(1);
  base::win::ScopedVariant child_count_variant(child_count);

  Microsoft::WRL::ComPtr<IDispatch> ax_first_static_text_child;
  ASSERT_HRESULT_SUCCEEDED(
      ax_paragraph->get_accChild(one_variant, &ax_first_static_text_child));
  ASSERT_NE(nullptr, ax_first_static_text_child);

  Microsoft::WRL::ComPtr<IDispatch> ax_last_static_text_child;
  ASSERT_HRESULT_SUCCEEDED(ax_paragraph->get_accChild(
      child_count_variant, &ax_last_static_text_child));
  ASSERT_NE(nullptr, ax_last_static_text_child);

  LONG n_ranges = 1;
  IA2Range* ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemAlloc(sizeof(IA2Range)));
  ranges[0].anchor = ax_paragraph.Get();
  ranges[0].anchorOffset = -1;
  ranges[0].active = ax_paragraph.Get();
  ranges[0].activeOffset = child_count;
  EXPECT_HRESULT_FAILED(ax_paragraph->setSelectionRanges(n_ranges, ranges));
  ranges[0].anchorOffset = 0;
  ranges[0].activeOffset = child_count + 1;
  EXPECT_HRESULT_FAILED(ax_paragraph->setSelectionRanges(n_ranges, ranges));

  // Select the entire paragraph's contents but not the paragraph itself, i.e.
  // in the selected HTML the "p" tag will not be included.
  ranges[0].activeOffset = child_count;
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);
  EXPECT_HRESULT_SUCCEEDED(ax_paragraph->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());
  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  HRESULT hr = ax_paragraph->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  EXPECT_EQ(ax_first_static_text_child.Get(), ranges[0].anchor);
  EXPECT_EQ(0, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  EXPECT_EQ(ax_paragraph.Get(), ranges[0].active);
  EXPECT_EQ(child_count, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  n_ranges = 1;
  ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemRealloc(ranges, sizeof(IA2Range)));

  // Select from the beginning of the paragraph's text up to the start of the
  // last static text child.
  ranges[0].anchor = ax_paragraph.Get();
  ranges[0].anchorOffset = child_count - 1;
  ranges[0].active = ax_paragraph.Get();
  ranges[0].activeOffset = 0;
  EXPECT_HRESULT_SUCCEEDED(ax_paragraph->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());
  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  hr = ax_paragraph->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  EXPECT_EQ(ax_last_static_text_child.Get(), ranges[0].anchor);
  EXPECT_EQ(0, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  EXPECT_EQ(ax_first_static_text_child.Get(), ranges[0].active);
  EXPECT_EQ(0, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  CoTaskMemFree(ranges);
  ranges = nullptr;
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       SetSelectionWithIgnoredObjects) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(<!DOCTYPE html>
      <html>
        <body>
          <ul>
            <li>
              <div role="presentation"></div>
              <p role="presentation">
                <span>Banana</span>
              </p>
              <span>fruit.</span>
            </li>
          </ul>
        </body>
      </html>)HTML");

  ui::BrowserAccessibility* list_item =
      FindNode(ax::mojom::Role::kListItem, "");
  ASSERT_NE(nullptr, list_item);
  gfx::NativeViewAccessible list_item_win =
      list_item->GetNativeViewAccessible();
  ASSERT_NE(nullptr, list_item_win);

  Microsoft::WRL::ComPtr<IAccessibleText> list_item_text;
  ASSERT_HRESULT_SUCCEEDED(
      list_item_win->QueryInterface(IID_PPV_ARGS(&list_item_text)));

  // The hypertext expose by "list_item_text" includes a bullet character
  // (U+2022) followed by a space for the list bullet and the joined word
  // "Bananafruit.". The word "Banana" is exposed as text because its container
  // paragraph is ignored.
  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(list_item_text->get_nCharacters(&n_characters));
  ASSERT_EQ(14, n_characters);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);

  // First select the whole of the text found in the hypertext.
  LONG start_offset = 0;
  LONG end_offset = n_characters;
  EXPECT_HRESULT_SUCCEEDED(
      list_item_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  HRESULT hr = list_item_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(n_characters, end_offset);

  // Select only the list bullet.
  start_offset = 0;
  end_offset = 2;
  EXPECT_HRESULT_SUCCEEDED(
      list_item_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = list_item_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(2, end_offset);

  // Select the word "Banana" in the ignored paragraph.
  start_offset = 2;
  end_offset = 8;
  EXPECT_HRESULT_SUCCEEDED(
      list_item_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = list_item_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2, start_offset);
  EXPECT_EQ(8, end_offset);

  // Select both the list bullet and the word "Banana" in the ignored paragraph.
  start_offset = 0;
  end_offset = 8;
  EXPECT_HRESULT_SUCCEEDED(
      list_item_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = list_item_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(8, end_offset);

  // Select the joined word "Bananafruit." both in the ignored paragraph and in
  // the unignored span.
  start_offset = 2;
  end_offset = n_characters;
  EXPECT_HRESULT_SUCCEEDED(
      list_item_text->setSelection(0, start_offset, end_offset));
  ASSERT_TRUE(waiter.WaitForNotification());

  hr = list_item_text->get_selection(0, &start_offset, &end_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2, start_offset);
  EXPECT_EQ(n_characters, end_offset);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       SetSelectionRangesWithIgnoredObjects) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(<!DOCTYPE html>
      <html>
        <body>
          <ul>
            <li>
              <div role="presentation"></div>
              <p role="presentation">
                <span>Banana</span>
              </p>
              <span>fruit.</span>
            </li>
          </ul>
        </body>
      </html>)HTML");

  ui::BrowserAccessibility* list_item =
      FindNode(ax::mojom::Role::kListItem, "");
  ASSERT_NE(nullptr, list_item);
  ui::BrowserAccessibility* list = list_item->PlatformGetParent();
  ASSERT_NE(nullptr, list);

  gfx::NativeViewAccessible list_item_win =
      list_item->GetNativeViewAccessible();
  ASSERT_NE(nullptr, list_item_win);
  gfx::NativeViewAccessible list_win = list->GetNativeViewAccessible();
  ASSERT_NE(nullptr, list_win);

  Microsoft::WRL::ComPtr<IAccessible2_4> ax_list_item;
  ASSERT_HRESULT_SUCCEEDED(
      list_item_win->QueryInterface(IID_PPV_ARGS(&ax_list_item)));

  Microsoft::WRL::ComPtr<IAccessible2_4> ax_list;
  ASSERT_HRESULT_SUCCEEDED(list_win->QueryInterface(IID_PPV_ARGS(&ax_list)));

  // The list item should contain the list bullet and two static text objects
  // containing the word "Banana" and the word "fruit". The first static text's
  // immediate parent, i.e. the paragraph object, is ignored.
  LONG child_count = 0;
  ASSERT_HRESULT_SUCCEEDED(ax_list_item->get_accChildCount(&child_count));
  ASSERT_EQ(3, child_count);

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);
  LONG n_ranges = 1;
  IA2Range* ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemAlloc(sizeof(IA2Range)));

  // First select the whole of the list item.
  ranges[0].anchor = ax_list_item.Get();
  ranges[0].anchorOffset = 0;
  ranges[0].active = ax_list_item.Get();
  ranges[0].activeOffset = child_count;
  EXPECT_HRESULT_SUCCEEDED(ax_list_item->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());

  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  HRESULT hr = ax_list->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  // The list bullet is not included in the DOM tree, so a DOM equivalent
  // position at the beginning of the list (before the <li>) is computed by
  // Blink.
  EXPECT_EQ(ax_list.Get(), ranges[0].anchor);
  EXPECT_EQ(0, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  EXPECT_EQ(ax_list_item.Get(), ranges[0].active);
  EXPECT_EQ(child_count, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  n_ranges = 1;
  ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemRealloc(ranges, sizeof(IA2Range)));

  // Select only the list bullet.
  ranges[0].anchor = ax_list_item.Get();
  ranges[0].anchorOffset = 0;
  ranges[0].active = ax_list_item.Get();
  ranges[0].activeOffset = 1;
  EXPECT_HRESULT_SUCCEEDED(ax_list_item->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());

  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  hr = ax_list->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  // The list bullet is not included in the DOM tree, so a DOM equivalent
  // position at the beginning of the list (before the <li>) is computed by
  // Blink.
  EXPECT_EQ(ax_list.Get(), ranges[0].anchor);
  EXPECT_EQ(0, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  // Child 1 is the static text node with the word "Banana", so this is a
  // "before text" position on that node.
  //
  // This is returned instead of an equivalent position anchored on the list
  // item, in order to ensure that both a tree position before the first child
  // and a "before text"position on the first child would always compare as
  // equal.
  EXPECT_EQ(list_item->PlatformGetChild(1)->GetNativeViewAccessible(),
            ranges[0].active);
  EXPECT_EQ(0, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  n_ranges = 1;
  ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemRealloc(ranges, sizeof(IA2Range)));

  // Select the word "Banana" in the ignored paragraph.
  ranges[0].anchor = ax_list_item.Get();
  ranges[0].anchorOffset = 1;
  ranges[0].active = ax_list_item.Get();
  ranges[0].activeOffset = 2;
  EXPECT_HRESULT_SUCCEEDED(ax_list_item->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());

  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  hr = ax_list->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  // Child 1 is the static text node with the word "Banana", so this is a
  // "before text" position on that node.
  EXPECT_EQ(list_item->PlatformGetChild(1)->GetNativeViewAccessible(),
            ranges[0].anchor);
  EXPECT_EQ(0, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  // Child 2 is the static text node with the word "fruit.", so this is a
  // "before text" position on that node.
  //
  // This is returned instead of an equivalent position anchored on the list
  // item, in order to ensure that both a tree position before the second child
  // and a "before text"position on the second child would always compare as
  // equal.
  EXPECT_EQ(list_item->PlatformGetChild(2)->GetNativeViewAccessible(),
            ranges[0].active);
  EXPECT_EQ(0, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  n_ranges = 1;
  ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemRealloc(ranges, sizeof(IA2Range)));

  // Select the joined word "Bananafruit." both in the ignored paragraph and in
  // the unignored span.
  ranges[0].anchor = ax_list_item.Get();
  ranges[0].anchorOffset = 1;
  ranges[0].active = ax_list_item.Get();
  ranges[0].activeOffset = child_count;
  EXPECT_HRESULT_SUCCEEDED(ax_list_item->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());

  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  hr = ax_list->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  // Child 1 is the static text node with the word "Banana", so this is a
  // "before text" position on that node.
  EXPECT_EQ(list_item->PlatformGetChild(1)->GetNativeViewAccessible(),
            ranges[0].anchor);
  EXPECT_EQ(0, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  EXPECT_EQ(ax_list_item.Get(), ranges[0].active);
  EXPECT_EQ(child_count, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  n_ranges = 1;
  ranges =
      reinterpret_cast<IA2Range*>(CoTaskMemRealloc(ranges, sizeof(IA2Range)));

  // Select both the list bullet and the word "Banana" in the ignored paragraph.
  ranges[0].anchor = ax_list_item.Get();
  ranges[0].anchorOffset = 0;
  ranges[0].active = ax_list_item.Get();
  ranges[0].activeOffset = 2;
  EXPECT_HRESULT_SUCCEEDED(ax_list_item->setSelectionRanges(n_ranges, ranges));
  ASSERT_TRUE(waiter.WaitForNotification());

  CoTaskMemFree(ranges);
  ranges = nullptr;
  n_ranges = 0;

  hr = ax_list->get_selectionRanges(&ranges, &n_ranges);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1, n_ranges);
  ASSERT_NE(nullptr, ranges);
  ASSERT_NE(nullptr, ranges[0].anchor);
  // The list bullet is not included in the DOM tree, so a DOM equivalent
  // position at the beginning of the list (before the <li>) is computed by
  // Blink.
  EXPECT_EQ(ax_list.Get(), ranges[0].anchor);
  EXPECT_EQ(0, ranges[0].anchorOffset);
  ASSERT_NE(nullptr, ranges[0].active);
  // Child 2 is the static text node with the word "fruit.", so this is a
  // "before text" position on that node.
  EXPECT_EQ(list_item->PlatformGetChild(2)->GetNativeViewAccessible(),
            ranges[0].active);
  EXPECT_EQ(0, ranges[0].activeOffset);

  ranges[0].anchor->Release();
  ranges[0].active->Release();
  CoTaskMemFree(ranges);
  ranges = nullptr;
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithInvalidArguments) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);
  HRESULT hr =
      input_text->get_textAtOffset(0, IA2_TEXT_BOUNDARY_CHAR, NULL, NULL, NULL);
  EXPECT_EQ(E_INVALIDARG, hr);

  // Test invalid offset.
  LONG start_offset = 0;
  LONG end_offset = 0;
  base::win::ScopedBstr text;
  LONG invalid_offset = -5;
  hr = input_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_CHAR,
                                    &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  invalid_offset = InputContentsString().size() + 1;
  hr = input_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_WORD,
                                    &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());

  // According to the IA2 Spec, only line boundaries should succeed when
  // the offset is one past the end of the text.
  invalid_offset = InputContentsString().size();
  hr = input_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_CHAR,
                                    &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = input_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_WORD,
                                    &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = input_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_SENTENCE,
                                    &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = input_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_LINE,
                                    &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(46, end_offset);
  EXPECT_STREQ(L"Moz/5.0 (ST 6.x; WWW33) WebKit  \"KHTML, like\".", text.Get());
  text.Reset();
  hr = input_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_ALL,
                                    &start_offset, &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());

  // The same behavior should be observed when the special offset
  // IA2_TEXT_OFFSET_LENGTH is used.
  hr = input_text->get_textAtOffset(IA2_TEXT_OFFSET_LENGTH,
                                    IA2_TEXT_BOUNDARY_CHAR, &start_offset,
                                    &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = input_text->get_textAtOffset(IA2_TEXT_OFFSET_LENGTH,
                                    IA2_TEXT_BOUNDARY_WORD, &start_offset,
                                    &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = input_text->get_textAtOffset(IA2_TEXT_OFFSET_LENGTH,
                                    IA2_TEXT_BOUNDARY_SENTENCE, &start_offset,
                                    &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = input_text->get_textAtOffset(IA2_TEXT_OFFSET_LENGTH,
                                    IA2_TEXT_BOUNDARY_LINE, &start_offset,
                                    &end_offset, text.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(46, end_offset);
  EXPECT_STREQ(L"Moz/5.0 (ST 6.x; WWW33) WebKit  \"KHTML, like\".", text.Get());
  text.Reset();
  hr = input_text->get_textAtOffset(IA2_TEXT_OFFSET_LENGTH,
                                    IA2_TEXT_BOUNDARY_ALL, &start_offset,
                                    &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineTextAtOffsetWithInvalidArguments) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);
  HRESULT hr = textarea_text->get_textAtOffset(0, IA2_TEXT_BOUNDARY_CHAR, NULL,
                                               NULL, NULL);
  EXPECT_EQ(E_INVALIDARG, hr);

  // Test invalid offset.
  LONG start_offset = 0;
  LONG end_offset = 0;
  base::win::ScopedBstr text;
  LONG invalid_offset = -5;
  hr = textarea_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_CHAR,
                                       &start_offset, &end_offset,
                                       text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  invalid_offset = InputContentsString().size() + 1;
  hr = textarea_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_WORD,
                                       &start_offset, &end_offset,
                                       text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());

  // According to the IA2 Spec, only line boundaries should succeed when
  // the offset is one past the end of the text.
  invalid_offset = InputContentsString().size();
  hr = textarea_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_CHAR,
                                       &start_offset, &end_offset,
                                       text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = textarea_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_WORD,
                                       &start_offset, &end_offset,
                                       text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = textarea_text->get_textAtOffset(
      invalid_offset, IA2_TEXT_BOUNDARY_SENTENCE, &start_offset, &end_offset,
      text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = textarea_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_LINE,
                                       &start_offset, &end_offset,
                                       text.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(32, start_offset);
  EXPECT_EQ(46, end_offset);
  EXPECT_STREQ(L"\"KHTML, like\".", text.Get());
  text.Reset();
  hr = textarea_text->get_textAtOffset(invalid_offset, IA2_TEXT_BOUNDARY_ALL,
                                       &start_offset, &end_offset,
                                       text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());

  // The same behavior should be observed when the special offset
  // IA2_TEXT_OFFSET_LENGTH is used.
  hr = textarea_text->get_textAtOffset(IA2_TEXT_OFFSET_LENGTH,
                                       IA2_TEXT_BOUNDARY_CHAR, &start_offset,
                                       &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = textarea_text->get_textAtOffset(IA2_TEXT_OFFSET_LENGTH,
                                       IA2_TEXT_BOUNDARY_WORD, &start_offset,
                                       &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = textarea_text->get_textAtOffset(
      IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_SENTENCE, &start_offset,
      &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
  hr = textarea_text->get_textAtOffset(IA2_TEXT_OFFSET_LENGTH,
                                       IA2_TEXT_BOUNDARY_LINE, &start_offset,
                                       &end_offset, text.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(32, start_offset);
  EXPECT_EQ(46, end_offset);
  EXPECT_STREQ(L"\"KHTML, like\".", text.Get());
  text.Reset();
  hr = textarea_text->get_textAtOffset(IA2_TEXT_OFFSET_LENGTH,
                                       IA2_TEXT_BOUNDARY_ALL, &start_offset,
                                       &end_offset, text.Receive());
  EXPECT_EQ(E_INVALIDARG, hr);
  EXPECT_EQ(0, start_offset);
  EXPECT_EQ(0, end_offset);
  EXPECT_EQ(nullptr, text.Get());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundaryCharacter) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  for (LONG offset = 0; offset < contents_string_length; ++offset) {
    std::wstring expected_text(1, InputContentsString()[offset]);
    LONG expected_start_offset = offset;
    LONG expected_end_offset = offset + 1;
    CheckTextAtOffset(input_text, offset, IA2_TEXT_BOUNDARY_CHAR,
                      expected_start_offset, expected_end_offset,
                      expected_text);
  }

  for (LONG offset = contents_string_length - 1; offset >= 0; --offset) {
    std::wstring expected_text(1, InputContentsString()[offset]);
    LONG expected_start_offset = offset;
    LONG expected_end_offset = offset + 1;
    CheckTextAtOffset(input_text, offset, IA2_TEXT_BOUNDARY_CHAR,
                      expected_start_offset, expected_end_offset,
                      expected_text);
  }

  CheckTextAtOffset(input_text, IA2_TEXT_OFFSET_CARET, IA2_TEXT_BOUNDARY_CHAR,
                    contents_string_length - 1, contents_string_length, L".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineTextAtOffsetWithBoundaryCharacter) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  for (LONG offset = 0; offset < contents_string_length; ++offset) {
    std::wstring expected_text(1, TextAreaContentsString()[offset]);
    LONG expected_start_offset = offset;
    LONG expected_end_offset = offset + 1;
    CheckTextAtOffset(textarea_text, offset, IA2_TEXT_BOUNDARY_CHAR,
                      expected_start_offset, expected_end_offset,
                      expected_text);
  }

  for (LONG offset = contents_string_length - 1; offset >= 0; --offset) {
    std::wstring expected_text(1, TextAreaContentsString()[offset]);
    LONG expected_start_offset = offset;
    LONG expected_end_offset = offset + 1;
    CheckTextAtOffset(textarea_text, offset, IA2_TEXT_BOUNDARY_CHAR,
                      expected_start_offset, expected_end_offset,
                      expected_text);
  }

  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_CARET,
                    IA2_TEXT_BOUNDARY_CHAR, contents_string_length - 1,
                    contents_string_length, L".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultilingualTextAtOffsetWithBoundaryCharacter) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  // Place an e acute, and two emoticons in the text field.
  ExecuteScript(
      uR"SCRIPT(
      const input = document.querySelector('input');
      input.value =
          'e\u0301\uD83D\uDC69\u200D\u2764\uFE0F\u200D\uD83D\uDC69\uD83D\uDC36';
      )SCRIPT");
  ASSERT_TRUE(waiter.WaitForNotification());

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(input_text->get_nCharacters(&n_characters));
  // "n_characters" is the number of valid text offsets.
  //
  // Ordinarily, the number of valid text offsets should equal the number of
  // actual characters which are only three in this case. However, this is
  // harder to implement given our current UTF16-based representation of IA2
  // hyptertext.
  // TODO(nektar): Implement support for base::OffsetAdjuster in AXPosition.
  ASSERT_EQ(12, n_characters);

  // The expected text consists of an e acute, and two emoticons.
  const std::vector<std::wstring> expected_text = {
      L"e\x0301", L"\xD83D\xDC69\x200D\x2764\xFE0F\x200D\xD83D\xDC69",
      L"\xD83D\xDC36"};
  LONG offset = 0;
  for (const std::wstring& expected_character : expected_text) {
    LONG expected_start_offset = offset;
    LONG expected_end_offset =
        expected_start_offset + static_cast<LONG>(expected_character.length());
    for (size_t code_unit_offset = 0;
         code_unit_offset < expected_character.length(); ++code_unit_offset) {
      CheckTextAtOffset(input_text, offset, IA2_TEXT_BOUNDARY_CHAR,
                        expected_start_offset, expected_end_offset,
                        expected_character);
      ++offset;
    }
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundaryCharacterAndEmbeddedObject) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(<!DOCTYPE html>
      <div contenteditable>
        Before<img alt="image">after.
      </div>
      )HTML");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> contenteditable;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &contenteditable));

  Microsoft::WRL::ComPtr<IAccessibleText> contenteditable_text;
  ASSERT_HRESULT_SUCCEEDED(contenteditable.As(&contenteditable_text));

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(
      contenteditable_text->get_nCharacters(&n_characters));
  ASSERT_EQ(13, n_characters);

  const std::u16string embedded_character(
      1, ui::AXPlatformNodeBase::kEmbeddedCharacter);
  const std::wstring expected_hypertext =
      L"Before" + base::UTF16ToWide(embedded_character) + L"after.";

  // "Before".
  //
  // The embedded object character representing the image is at offset 6.
  for (LONG i = 0; i < 6; ++i) {
    CheckTextAtOffset(contenteditable_text, i, IA2_TEXT_BOUNDARY_CHAR, i,
                      (i + 1), std::wstring(1, expected_hypertext[i]));
  }

  // "after.".
  //
  // Note that according to the IA2 Spec, an offset that is equal to
  // "n_characters" is not permitted.
  for (LONG i = 7; i < n_characters; ++i) {
    CheckTextAtOffset(contenteditable_text, i, IA2_TEXT_BOUNDARY_CHAR, i,
                      (i + 1), std::wstring(1, expected_hypertext[i]));
  }

  std::vector<base::win::ScopedVariant> contenteditable_children =
      GetAllAccessibleChildren(contenteditable.Get());
  ASSERT_EQ(3u, contenteditable_children.size());
  // The image is the second child.
  Microsoft::WRL::ComPtr<IAccessible2> image;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(contenteditable.Get(),
                               contenteditable_children[1].AsInput())
          .Get(),
      &image));
  LONG image_role = 0;
  ASSERT_HRESULT_SUCCEEDED(image->role(&image_role));
  ASSERT_EQ(ROLE_SYSTEM_GRAPHIC, image_role);

  // The alt text of the image is not navigable as text.
  Microsoft::WRL::ComPtr<IAccessibleText> image_text;
  EXPECT_HRESULT_FAILED(image.As(&image_text));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundaryWord) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  // Trailing punctuation should not be included as part of the previous word.
  CheckTextAtOffset(input_text, 0, IA2_TEXT_BOUNDARY_WORD, 0, 3, L"Moz");
  CheckTextAtOffset(input_text, 2, IA2_TEXT_BOUNDARY_WORD, 0, 3, L"Moz");

  // If the offset is at the punctuation, it should return
  // the punctuation as a word.
  CheckTextAtOffset(input_text, 3, IA2_TEXT_BOUNDARY_WORD, 3, 4, L"/");

  // Numbers with a decimal point ("." for U.S), should be treated as one word.
  // Also, trailing punctuation that occurs after empty space should not be part
  // of the word. ("5.0 " and not "5.0 (".)
  CheckTextAtOffset(input_text, 4, IA2_TEXT_BOUNDARY_WORD, 4, 8, L"5.0 ");
  CheckTextAtOffset(input_text, 5, IA2_TEXT_BOUNDARY_WORD, 4, 8, L"5.0 ");
  CheckTextAtOffset(input_text, 6, IA2_TEXT_BOUNDARY_WORD, 4, 8, L"5.0 ");
  CheckTextAtOffset(input_text, 7, IA2_TEXT_BOUNDARY_WORD, 4, 8, L"5.0 ");

  // Leading punctuation should not be included with the word after it.
  CheckTextAtOffset(input_text, 8, IA2_TEXT_BOUNDARY_WORD, 8, 9, L"(");
  CheckTextAtOffset(input_text, 11, IA2_TEXT_BOUNDARY_WORD, 9, 12, L"ST ");

  // Numbers separated from letters with trailing punctuation should
  // be split into multiple words. Same for abbreviations like "i.e.".
  CheckTextAtOffset(input_text, 12, IA2_TEXT_BOUNDARY_WORD, 12, 13, L"6");
  CheckTextAtOffset(input_text, 13, IA2_TEXT_BOUNDARY_WORD, 13, 14, L".");
  CheckTextAtOffset(input_text, 14, IA2_TEXT_BOUNDARY_WORD, 14, 15, L"x");
  CheckTextAtOffset(input_text, 15, IA2_TEXT_BOUNDARY_WORD, 15, 17, L"; ");

  // Words with numbers should be treated like ordinary words.
  CheckTextAtOffset(input_text, 17, IA2_TEXT_BOUNDARY_WORD, 17, 22, L"WWW33");
  CheckTextAtOffset(input_text, 23, IA2_TEXT_BOUNDARY_WORD, 22, 24, L") ");

  // Multiple trailing empty spaces should be part of the word preceding it.
  CheckTextAtOffset(input_text, 28, IA2_TEXT_BOUNDARY_WORD, 24, 32,
                    L"WebKit  ");
  CheckTextAtOffset(input_text, 31, IA2_TEXT_BOUNDARY_WORD, 24, 32,
                    L"WebKit  ");
  CheckTextAtOffset(input_text, 32, IA2_TEXT_BOUNDARY_WORD, 32, 33, L"\"");

  // Leading and trailing punctuation such as quotation marks should not be part
  // of the word.
  CheckTextAtOffset(input_text, 33, IA2_TEXT_BOUNDARY_WORD, 33, 38, L"KHTML");
  CheckTextAtOffset(input_text, 38, IA2_TEXT_BOUNDARY_WORD, 38, 40, L", ");
  CheckTextAtOffset(input_text, 39, IA2_TEXT_BOUNDARY_WORD, 38, 40, L", ");

  // Trailing final punctuation should not be part of the last word.
  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  CheckTextAtOffset(input_text, 40, IA2_TEXT_BOUNDARY_WORD, 40, 44, L"like");
  CheckTextAtOffset(input_text, 41, IA2_TEXT_BOUNDARY_WORD, 40, 44, L"like");
  CheckTextAtOffset(input_text, 44, IA2_TEXT_BOUNDARY_WORD, 44,
                    contents_string_length, L"\".");
  CheckTextAtOffset(input_text, 45, IA2_TEXT_BOUNDARY_WORD, 44,
                    contents_string_length, L"\".");

  // Test special offsets.
  CheckTextAtOffset(input_text, IA2_TEXT_OFFSET_CARET, IA2_TEXT_BOUNDARY_WORD,
                    44, contents_string_length, L"\".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineTextAtOffsetWithBoundaryWord) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  // Trailing punctuation should not be included as part of the previous word.
  CheckTextAtOffset(textarea_text, 0, IA2_TEXT_BOUNDARY_WORD, 0, 3, L"Moz");
  CheckTextAtOffset(textarea_text, 2, IA2_TEXT_BOUNDARY_WORD, 0, 3, L"Moz");

  // If the offset is at the punctuation, it should return
  // the punctuation as a word.
  CheckTextAtOffset(textarea_text, 3, IA2_TEXT_BOUNDARY_WORD, 3, 4, L"/");

  // Numbers with a decimal point ("." for U.S), should be treated as one word.
  // Also, trailing punctuation that occurs after empty space should not be part
  // of the word. ("5.0 " and not "5.0 (".)
  CheckTextAtOffset(textarea_text, 4, IA2_TEXT_BOUNDARY_WORD, 4, 8, L"5.0 ");
  CheckTextAtOffset(textarea_text, 5, IA2_TEXT_BOUNDARY_WORD, 4, 8, L"5.0 ");
  CheckTextAtOffset(textarea_text, 6, IA2_TEXT_BOUNDARY_WORD, 4, 8, L"5.0 ");
  CheckTextAtOffset(textarea_text, 7, IA2_TEXT_BOUNDARY_WORD, 4, 8, L"5.0 ");

  // Leading punctuation should not be included with the word after it.
  CheckTextAtOffset(textarea_text, 8, IA2_TEXT_BOUNDARY_WORD, 8, 9, L"(");
  CheckTextAtOffset(textarea_text, 11, IA2_TEXT_BOUNDARY_WORD, 9, 12, L"ST ");

  // Numbers separated from letters with trailing punctuation should
  // be split into multiple words. Same for abbreviations like "i.e.".
  CheckTextAtOffset(textarea_text, 12, IA2_TEXT_BOUNDARY_WORD, 12, 13, L"6");
  CheckTextAtOffset(textarea_text, 13, IA2_TEXT_BOUNDARY_WORD, 13, 14, L".");
  CheckTextAtOffset(textarea_text, 14, IA2_TEXT_BOUNDARY_WORD, 14, 15, L"x");
  CheckTextAtOffset(textarea_text, 15, IA2_TEXT_BOUNDARY_WORD, 15, 17, L"; ");

  // Words with numbers should be treated like ordinary words.
  CheckTextAtOffset(textarea_text, 17, IA2_TEXT_BOUNDARY_WORD, 17, 22,
                    L"WWW33");
  CheckTextAtOffset(textarea_text, 23, IA2_TEXT_BOUNDARY_WORD, 22, 24, L")\n");

  // Multiple trailing empty spaces should be part of the word preceding it.
  CheckTextAtOffset(textarea_text, 28, IA2_TEXT_BOUNDARY_WORD, 24, 32,
                    L"WebKit \n");
  CheckTextAtOffset(textarea_text, 31, IA2_TEXT_BOUNDARY_WORD, 24, 32,
                    L"WebKit \n");
  CheckTextAtOffset(textarea_text, 32, IA2_TEXT_BOUNDARY_WORD, 32, 33, L"\"");

  // Leading and trailing punctuation such as quotation marks should not be part
  // of the word.
  CheckTextAtOffset(textarea_text, 33, IA2_TEXT_BOUNDARY_WORD, 33, 38,
                    L"KHTML");
  CheckTextAtOffset(textarea_text, 38, IA2_TEXT_BOUNDARY_WORD, 38, 40, L", ");
  CheckTextAtOffset(textarea_text, 39, IA2_TEXT_BOUNDARY_WORD, 38, 40, L", ");

  // Trailing final punctuation should not be part of the last word.
  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  CheckTextAtOffset(textarea_text, 40, IA2_TEXT_BOUNDARY_WORD, 40, 44, L"like");
  CheckTextAtOffset(textarea_text, 41, IA2_TEXT_BOUNDARY_WORD, 40, 44, L"like");
  CheckTextAtOffset(textarea_text, 44, IA2_TEXT_BOUNDARY_WORD, 44,
                    contents_string_length, L"\".");
  CheckTextAtOffset(textarea_text, 45, IA2_TEXT_BOUNDARY_WORD, 44,
                    contents_string_length, L"\".");

  // Test special offsets.
  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_CARET,
                    IA2_TEXT_BOUNDARY_WORD, 44, contents_string_length, L"\".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestStaticTextAtOffsetWithBoundaryWord) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);
  std::wstring embedded_character = base::UTF16ToWide(
      std::u16string(1, ui::AXPlatformNodeBase::kEmbeddedCharacter));
  std::vector<std::wstring> words = {
      L"Game ",    L"theory ",      L"is ",       L"\"",
      L"the ",     L"study ",       L"of ",       embedded_character,
      L"of ",      L"conflict ",    L"and\n",     L"cooperation ",
      L"between ", L"intelligent ", L"rational ", L"decision",
      L"-",        L"makers",       L".\""};

  // Try to retrieve one word after another.
  LONG word_start_offset = 0;
  for (auto& word : words) {
    LONG word_end_offset = word_start_offset + word.size();
    CheckTextAtOffset(paragraph_text, word_start_offset, IA2_TEXT_BOUNDARY_WORD,
                      word_start_offset, word_end_offset, word);
    word_start_offset = word_end_offset;
    // If the word boundary is inside an embedded object, |word_end_offset|
    // should be one past the embedded object character. To get to the start of
    // the next word, we have to skip the space between the embedded object
    // character and the next word.
    if (word == embedded_character) {
      ++word_start_offset;
    }
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_TestTextAtOffsetWithBoundarySentence) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  const LONG contents_string_length =
      static_cast<LONG>(InputContentsString().size());
  const std::wstring expected_text = base::SysUTF8ToWide(InputContentsString());
  for (LONG offset = 0; offset < contents_string_length; ++offset) {
    CheckTextAtOffset(input_text, offset, IA2_TEXT_BOUNDARY_SENTENCE, 0,
                      contents_string_length, expected_text);
  }

  // Test special offsets.
  CheckTextAtOffset(input_text, IA2_TEXT_OFFSET_CARET,
                    IA2_TEXT_BOUNDARY_SENTENCE, 0, contents_string_length,
                    expected_text);
  {
    LONG start_offset = 0;
    LONG end_offset = 0;
    base::win::ScopedBstr text;
    HRESULT hr = input_text->get_textAtOffset(
        IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_SENTENCE, &start_offset,
        &end_offset, text.Receive());
    EXPECT_EQ(E_INVALIDARG, hr);
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(0, end_offset);
    EXPECT_EQ(nullptr, text.Get());
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_TestMultiLineTextAtOffsetWithBoundarySentence) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  const LONG contents_string_length =
      static_cast<LONG>(TextAreaContentsString().size());
  const std::vector<LONG> sentence_starts{0, 23, 24, 31, 32};
  const std::vector<LONG> sentence_ends{23, 24, 31, 32, contents_string_length};
  size_t sentence_index = 0;
  for (LONG offset = 0; offset < contents_string_length &&
                        sentence_index < sentence_starts.size();
       ++offset) {
    if (offset == sentence_starts[sentence_index + 1]) {
      ++sentence_index;
    }
    LONG expected_start_offset = sentence_starts[sentence_index];
    LONG expected_end_offset = sentence_ends[sentence_index];
    const std::wstring expected_text =
        base::SysUTF8ToWide(TextAreaContentsString().substr(
            sentence_starts[sentence_index],
            (sentence_ends[sentence_index] - sentence_starts[sentence_index])));
    CheckTextAtOffset(textarea_text, offset, IA2_TEXT_BOUNDARY_SENTENCE,
                      expected_start_offset, expected_end_offset,
                      expected_text);
  }

  // Test special offsets.
  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_CARET,
                    IA2_TEXT_BOUNDARY_SENTENCE, 32, contents_string_length,
                    base::SysUTF8ToWide(TextAreaContentsString().substr(32)));
  {
    LONG start_offset = 0;
    LONG end_offset = 0;
    base::win::ScopedBstr text;
    HRESULT hr = textarea_text->get_textAtOffset(
        IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_SENTENCE, &start_offset,
        &end_offset, text.Receive());
    EXPECT_EQ(E_INVALIDARG, hr);
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(0, end_offset);
    EXPECT_EQ(nullptr, text.Get());
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundaryLine) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  // Single line text fields should return the whole text.
  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  CheckTextAtOffset(input_text, 0, IA2_TEXT_BOUNDARY_LINE, 0,
                    contents_string_length,
                    base::SysUTF8ToWide(InputContentsString()));

  // Test special offsets.
  CheckTextAtOffset(input_text, IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_LINE,
                    0, contents_string_length,
                    base::SysUTF8ToWide(InputContentsString()));
  CheckTextAtOffset(input_text, IA2_TEXT_OFFSET_CARET, IA2_TEXT_BOUNDARY_LINE,
                    0, contents_string_length,
                    base::SysUTF8ToWide(InputContentsString()));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineTextAtOffsetWithBoundaryLine) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  CheckTextAtOffset(textarea_text, 0, IA2_TEXT_BOUNDARY_LINE, 0, 24,
                    L"Moz/5.0 (ST 6.x; WWW33)\n");

  // If the offset is at the newline, return the line preceding it.
  CheckTextAtOffset(textarea_text, 31, IA2_TEXT_BOUNDARY_LINE, 24, 32,
                    L"WebKit \n");

  // Last line does not have a trailing newline.
  LONG contents_string_length = static_cast<LONG>(InputContentsString().size());
  CheckTextAtOffset(textarea_text, 32, IA2_TEXT_BOUNDARY_LINE, 32,
                    contents_string_length, L"\"KHTML, like\".");

  // An offset one past the last character should return the last line.
  CheckTextAtOffset(textarea_text, contents_string_length,
                    IA2_TEXT_BOUNDARY_LINE, 32, contents_string_length,
                    L"\"KHTML, like\".");

  // Test special offsets.
  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_LENGTH,
                    IA2_TEXT_BOUNDARY_LINE, 32, contents_string_length,
                    L"\"KHTML, like\".");
  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_CARET,
                    IA2_TEXT_BOUNDARY_LINE, 32, contents_string_length,
                    L"\"KHTML, like\".");
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestBlankLineTextAtOffsetWithBoundaryLine) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kValueChanged);
  // Add a blank line at the end of the textarea.
  ExecuteScript(
      uR"SCRIPT(
      const textarea = document.querySelector('textarea');
      textarea.value += '\n';
      )SCRIPT");
  ASSERT_TRUE(waiter.WaitForNotification());

  // The second last line should have an additional trailing newline. Also,
  // Blink represents the blank line with a newline character, so in total there
  // should be two more newlines. The second newline is not part of the HTML
  // value attribute however.
  LONG contents_string_length =
      static_cast<LONG>(InputContentsString().size()) + 1;
  CheckTextAtOffset(textarea_text, 32, IA2_TEXT_BOUNDARY_LINE, 32,
                    contents_string_length, L"\"KHTML, like\".\n");
  CheckTextAtOffset(textarea_text, 46, IA2_TEXT_BOUNDARY_LINE, 32,
                    contents_string_length, L"\"KHTML, like\".\n");

  // An offset one past the last character should return the last line which is
  // blank. This is represented by Blink with yet another line break.
  CheckTextAtOffset(textarea_text, contents_string_length,
                    IA2_TEXT_BOUNDARY_LINE, contents_string_length,
                    (contents_string_length + 1), L"\n");

  {
    // There should be no text after the blank line.
    LONG start_offset = 0;
    LONG end_offset = 0;
    base::win::ScopedBstr text;
    EXPECT_EQ(S_FALSE, textarea_text->get_textAtOffset(
                           (contents_string_length + 1), IA2_TEXT_BOUNDARY_LINE,
                           &start_offset, &end_offset, text.Receive()));

    // Test special offsets.
    EXPECT_EQ(S_FALSE, textarea_text->get_textAtOffset(
                           IA2_TEXT_OFFSET_LENGTH, IA2_TEXT_BOUNDARY_LINE,
                           &start_offset, &end_offset, text.Receive()));
  }

  // The caret should have moved to the blank line.
  CheckTextAtOffset(textarea_text, IA2_TEXT_OFFSET_CARET,
                    IA2_TEXT_BOUNDARY_LINE, contents_string_length,
                    (contents_string_length + 1), L"\n");
}

IN_PROC_BROWSER_TEST_F(
    AccessibilityWinBrowserTest,
    TestTextAtOffsetWithBoundaryLineAndMultiLineEmbeddedObject) {
  // There should be two lines in this contenteditable.
  //
  // Half of the link is on the first line, and the other half is on the second
  // line.
  LoadInitialAccessibilityTreeFromHtml(R"HTML(<!DOCTYPE html>
      <div contenteditable style="width: 70px">
        Hello
        <a href="#">this is a</a>
        test.
      </div>
      )HTML");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> contenteditable;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &contenteditable));

  Microsoft::WRL::ComPtr<IAccessibleText> contenteditable_text;
  ASSERT_HRESULT_SUCCEEDED(contenteditable.As(&contenteditable_text));

  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(
      contenteditable_text->get_nCharacters(&n_characters));
  ASSERT_EQ(13, n_characters);

  // Line one.
  //
  // The embedded object character representing the link is at offset 6.
  for (LONG i = 0; i <= 6; ++i) {
    CheckTextAtOffset(contenteditable_text, i, IA2_TEXT_BOUNDARY_LINE, 0, 7,
                      L"Hello \xFFFC");
  }

  // Line two.
  //
  // Note that the caret can also be at the end of the contenteditable, so an
  // offset that is equal to "n_characters" is also permitted.
  for (LONG i = 7; i <= n_characters; ++i) {
    CheckTextAtOffset(contenteditable_text, i, IA2_TEXT_BOUNDARY_LINE, 6,
                      n_characters, L"\xFFFC test.");
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestParagraphTextAtOffsetWithBoundaryLine) {
  Microsoft::WRL::ComPtr<IAccessibleText> paragraph_text;
  SetUpSampleParagraph(&paragraph_text);

  // There should be two lines in this paragraph.
  const LONG newline_offset = 46;
  LONG n_characters;
  ASSERT_HRESULT_SUCCEEDED(paragraph_text->get_nCharacters(&n_characters));
  ASSERT_LT(0, n_characters);
  ASSERT_LT(newline_offset, n_characters);

  for (LONG i = 0; i <= newline_offset; ++i) {
    CheckTextAtOffset(
        paragraph_text, i, IA2_TEXT_BOUNDARY_LINE, 0, newline_offset + 1,
        L"Game theory is \"the study of \xFFFC of conflict and\n");
  }

  // For line boundaries, IA2 Spec allows for the offset to be equal to the
  // text's length.
  for (LONG i = newline_offset + 1; i <= n_characters; ++i) {
    CheckTextAtOffset(
        paragraph_text, i, IA2_TEXT_BOUNDARY_LINE, newline_offset + 1,
        n_characters,
        L"cooperation between intelligent rational decision-makers.\"");
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestTextAtOffsetWithBoundaryAll) {
  Microsoft::WRL::ComPtr<IAccessibleText> input_text;
  SetUpInputField(&input_text);

  CheckTextAtOffset(input_text, 0, IA2_TEXT_BOUNDARY_ALL, 0,
                    InputContentsString().size(),
                    base::SysUTF8ToWide(InputContentsString()));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestMultiLineTextAtOffsetWithBoundaryAll) {
  Microsoft::WRL::ComPtr<IAccessibleText> textarea_text;
  SetUpTextareaField(&textarea_text);

  CheckTextAtOffset(textarea_text, InputContentsString().size() - 1,
                    IA2_TEXT_BOUNDARY_ALL, 0, InputContentsString().size(),
                    base::SysUTF8ToWide(TextAreaContentsString()));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestIAccessibleAction) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <img src="" alt="image"
            onclick="document.querySelector('img').alt = 'clicked';">
      </body>
      </html>)HTML");

  // Retrieve the IAccessible interface for the web page.
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> div;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &div));
  std::vector<base::win::ScopedVariant> div_children =
      GetAllAccessibleChildren(div.Get());
  ASSERT_EQ(1u, div_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> image;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(div.Get(), div_children[0].AsInput()).Get(),
      &image));
  LONG image_role = 0;
  ASSERT_HRESULT_SUCCEEDED(image->role(&image_role));
  ASSERT_EQ(ROLE_SYSTEM_GRAPHIC, image_role);

  Microsoft::WRL::ComPtr<IAccessibleAction> image_action;
  ASSERT_HRESULT_SUCCEEDED(image.As(&image_action));

  LONG n_actions = 0;
  EXPECT_HRESULT_SUCCEEDED(image_action->nActions(&n_actions));
  EXPECT_EQ(2, n_actions);

  base::win::ScopedBstr action_name;
  EXPECT_HRESULT_SUCCEEDED(image_action->get_name(0, action_name.Receive()));
  EXPECT_EQ(L"click", std::wstring(action_name.Get(), action_name.Length()));
  action_name.Release();
  EXPECT_HRESULT_SUCCEEDED(image_action->get_name(1, action_name.Receive()));
  EXPECT_EQ(L"showContextMenu",
            std::wstring(action_name.Get(), action_name.Length()));
  action_name.Release();
  EXPECT_HRESULT_FAILED(image_action->get_name(2, action_name.Receive()));
  EXPECT_EQ(nullptr, action_name.Get());

  base::win::ScopedBstr localized_name;
  EXPECT_HRESULT_SUCCEEDED(
      image_action->get_localizedName(0, localized_name.Receive()));
  EXPECT_EQ(L"click",
            std::wstring(localized_name.Get(), localized_name.Length()));
  localized_name.Release();
  EXPECT_HRESULT_SUCCEEDED(
      image_action->get_localizedName(1, localized_name.Receive()));
  EXPECT_EQ(L"showContextMenu",
            std::wstring(localized_name.Get(), localized_name.Length()));
  localized_name.Release();
  EXPECT_HRESULT_FAILED(
      image_action->get_localizedName(2, localized_name.Receive()));
  EXPECT_EQ(nullptr, localized_name.Get());

  LONG n_key_bindings = 0;
  BSTR* key_bindings = nullptr;
  EXPECT_HRESULT_SUCCEEDED(
      image_action->get_keyBinding(0, 100, &key_bindings, &n_key_bindings));
  EXPECT_EQ(0, n_key_bindings);
  EXPECT_EQ(nullptr, key_bindings);

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr image_name;
  EXPECT_HRESULT_SUCCEEDED(
      image->get_accName(childid_self, image_name.Receive()));
  EXPECT_EQ(L"image", std::wstring(image_name.Get(), image_name.Length()));
  image_name.Release();
  // The action for index 0 is the default one, "click" in this case.
  // Clicking the image will change its name.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::NAME_CHANGED);
  EXPECT_HRESULT_SUCCEEDED(image_action->doAction(0));
  ASSERT_TRUE(waiter.WaitForNotification());
  EXPECT_HRESULT_SUCCEEDED(
      image->get_accName(childid_self, image_name.Receive()));
  EXPECT_EQ(L"clicked", std::wstring(image_name.Get(), image_name.Length()));
  image_name.Release();
  // The action for index 1 is "showContextMenu".
  // We use a ContextMenuInterceptor to intercept the event before
  // RenderFrameHost receives.
  auto context_menu_interceptor = std::make_unique<ContextMenuInterceptor>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      ContextMenuInterceptor::ShowBehavior::kPreventShow);
  EXPECT_HRESULT_SUCCEEDED(image_action->doAction(1));
  // If the context menu event did not happen, the test would time out here:
  context_menu_interceptor->Wait();
  // There are no more actions, calls for indexes >=2 will fail.
  EXPECT_HRESULT_FAILED(image_action->doAction(2));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, HasHWNDAfterNavigation) {
  // This test simulates a scenario where RenderWidgetHostViewAura::SetSize
  // is not called again after its window is added to the root window.
  // Ensure that we still get a legacy HWND for accessibility.
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  WebContentsView* web_contents_view = web_contents->GetView();
  WebContentsViewAura* web_contents_view_aura =
      static_cast<WebContentsViewAura*>(web_contents_view);

  // Set a flag that will cause WebContentsViewAura to initialize a
  // RenderWidgetHostViewAura with a null parent view.
  web_contents_view_aura->set_init_rwhv_with_null_parent_for_testing(true);

  // Enable accessibility.
  AccessibilityNotificationWaiter waiter(web_contents, ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  // Navigate to a new page.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "/accessibility/html/article.html")));

  // At this point the root of the accessibility tree shouldn't have an HWND
  // because we never gave a parent window to the RWHVA.
  ui::BrowserAccessibilityManagerWin* manager =
      static_cast<ui::BrowserAccessibilityManagerWin*>(GetManager());
  ASSERT_EQ(nullptr, manager->GetParentHWND());

  // Now add the RWHVA's window to the root window and ensure that we have
  // an HWND for accessibility now.
  web_contents_view->GetNativeView()->AddChild(
      web_contents->GetRenderWidgetHostView()->GetNativeView());
  // The load event will only fire after the page is attached.
  ASSERT_TRUE(waiter.WaitForNotification());
  ASSERT_NE(nullptr, manager->GetParentHWND());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestAccNavigateInTables) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/accessibility/html/table-spans.html")));
  ASSERT_TRUE(waiter.WaitForNotification());

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  // There are two tables in this test file. Use only the first one.
  ASSERT_EQ(2u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> table;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &table));
  LONG role = 0;
  ASSERT_HRESULT_SUCCEEDED(table->role(&role));
  ASSERT_EQ(ROLE_SYSTEM_TABLE, role);

  // Retrieve the first cell.
  Microsoft::WRL::ComPtr<IAccessibleTable2> table2;
  Microsoft::WRL::ComPtr<IUnknown> cell;
  Microsoft::WRL::ComPtr<IAccessible2> cell1;
  EXPECT_HRESULT_SUCCEEDED(table.As(&table2));
  EXPECT_HRESULT_SUCCEEDED(table2->get_cellAt(0, 0, &cell));
  EXPECT_HRESULT_SUCCEEDED(cell.As(&cell1));

  base::win::ScopedBstr name;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  Microsoft::WRL::ComPtr<IAccessibleTableCell> accessible_cell;
  LONG row_index = -1;
  LONG column_index = -1;
  EXPECT_HRESULT_SUCCEEDED(cell1->role(&role));
  EXPECT_EQ(ROLE_SYSTEM_CELL, role);
  EXPECT_HRESULT_SUCCEEDED(cell1->get_accName(childid_self, name.Receive()));
  EXPECT_STREQ(L"AD", name.Get());
  EXPECT_HRESULT_SUCCEEDED(cell1.As(&accessible_cell));
  EXPECT_HRESULT_SUCCEEDED(accessible_cell->get_rowIndex(&row_index));
  EXPECT_HRESULT_SUCCEEDED(accessible_cell->get_columnIndex(&column_index));
  EXPECT_EQ(0, row_index);
  EXPECT_EQ(0, column_index);
  name.Reset();
  accessible_cell.Reset();

  // The first cell has a rowspan of 2, try navigating down and expect to get
  // at the end of the table.
  base::win::ScopedVariant variant;
  EXPECT_HRESULT_SUCCEEDED(
      cell1->accNavigate(NAVDIR_DOWN, childid_self, variant.Receive()));
  ASSERT_EQ(VT_EMPTY, variant.type());

  // Try navigating to the cell in the first row, 2nd column.
  Microsoft::WRL::ComPtr<IAccessible2> cell2;
  EXPECT_HRESULT_SUCCEEDED(
      cell1->accNavigate(NAVDIR_RIGHT, childid_self, variant.Receive()));
  ASSERT_NE(nullptr, V_DISPATCH(variant.AsInput()));
  ASSERT_EQ(VT_DISPATCH, variant.type());
  V_DISPATCH(variant.AsInput())->QueryInterface(IID_PPV_ARGS(&cell2));
  EXPECT_HRESULT_SUCCEEDED(cell2->role(&role));
  EXPECT_EQ(ROLE_SYSTEM_CELL, role);
  EXPECT_HRESULT_SUCCEEDED(cell2->get_accName(childid_self, name.Receive()));
  EXPECT_STREQ(L"BC", name.Get());
  EXPECT_HRESULT_SUCCEEDED(cell2.As(&accessible_cell));
  EXPECT_HRESULT_SUCCEEDED(accessible_cell->get_rowIndex(&row_index));
  EXPECT_HRESULT_SUCCEEDED(accessible_cell->get_columnIndex(&column_index));
  EXPECT_EQ(0, row_index);
  EXPECT_EQ(1, column_index);
  variant.Reset();
  name.Reset();
  accessible_cell.Reset();

  // Try navigating to the cell in the second row, 2nd column.
  Microsoft::WRL::ComPtr<IAccessible2> cell3;
  EXPECT_HRESULT_SUCCEEDED(
      cell2->accNavigate(NAVDIR_DOWN, childid_self, variant.Receive()));
  ASSERT_NE(nullptr, V_DISPATCH(variant.AsInput()));
  ASSERT_EQ(VT_DISPATCH, variant.type());
  V_DISPATCH(variant.AsInput())->QueryInterface(IID_PPV_ARGS(&cell3));
  EXPECT_HRESULT_SUCCEEDED(cell3->role(&role));
  EXPECT_EQ(ROLE_SYSTEM_CELL, role);
  EXPECT_HRESULT_SUCCEEDED(cell3->get_accName(childid_self, name.Receive()));
  EXPECT_STREQ(L"EF", name.Get());
  EXPECT_HRESULT_SUCCEEDED(cell3.As(&accessible_cell));
  EXPECT_HRESULT_SUCCEEDED(accessible_cell->get_rowIndex(&row_index));
  EXPECT_HRESULT_SUCCEEDED(accessible_cell->get_columnIndex(&column_index));
  EXPECT_EQ(1, row_index);
  EXPECT_EQ(1, column_index);
  variant.Reset();
  name.Reset();
  accessible_cell.Reset();
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestTreegridIsIATable) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(
                                 "/accessibility/aria/aria-treegrid.html")));
  ASSERT_TRUE(waiter.WaitForNotification());

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  // There are three treegrids in this test file. Use only the first one.
  ASSERT_EQ(3u, document_children.size());

  Microsoft::WRL::ComPtr<IAccessible2> table;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &table));
  LONG role = 0;
  ASSERT_HRESULT_SUCCEEDED(table->role(&role));
  ASSERT_EQ(ROLE_SYSTEM_OUTLINE, role);

  // Retrieve the first cell.
  Microsoft::WRL::ComPtr<IAccessibleTable2> table2;
  Microsoft::WRL::ComPtr<IUnknown> cell;
  Microsoft::WRL::ComPtr<IAccessible2> cell1;
  EXPECT_HRESULT_SUCCEEDED(table.As(&table2));
  EXPECT_HRESULT_SUCCEEDED(table2->get_cellAt(0, 0, &cell));
  EXPECT_HRESULT_SUCCEEDED(cell.As(&cell1));

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  Microsoft::WRL::ComPtr<IAccessibleTableCell> accessible_cell;
  LONG row_index = -1;
  LONG column_index = -1;
  EXPECT_HRESULT_SUCCEEDED(cell1->role(&role));
  EXPECT_EQ(ROLE_SYSTEM_CELL, role);
  EXPECT_HRESULT_SUCCEEDED(cell1.As(&accessible_cell));
  EXPECT_HRESULT_SUCCEEDED(accessible_cell->get_rowIndex(&row_index));
  EXPECT_HRESULT_SUCCEEDED(accessible_cell->get_columnIndex(&column_index));
  EXPECT_EQ(0, row_index);
  EXPECT_EQ(0, column_index);
  accessible_cell.Reset();
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestScrollTo) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div style="height: 5000px;"></div>
        <img src="" alt="Target1">
        <div style="height: 5000px;"></div>
        <img src="" alt="Target2">
        <div style="height: 5000px;"></div>
      </body>
      </html>)HTML");

  // Retrieve the IAccessible interface for the document node.
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());

  // Get the dimensions of the document.
  LONG doc_x, doc_y, doc_width, doc_height;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_HRESULT_SUCCEEDED(document->accLocation(&doc_x, &doc_y, &doc_width,
                                                 &doc_height, childid_self));

  // The document should only have two children, both with a role of GRAPHIC.
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(2u, document_children.size());
  Microsoft::WRL::ComPtr<IAccessible2> target;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &target));
  LONG target_role = 0;
  ASSERT_HRESULT_SUCCEEDED(target->role(&target_role));
  ASSERT_EQ(ROLE_SYSTEM_GRAPHIC, target_role);
  Microsoft::WRL::ComPtr<IAccessible2> target2;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[1].AsInput())
          .Get(),
      &target2));
  LONG target2_role = 0;
  ASSERT_HRESULT_SUCCEEDED(target2->role(&target2_role));
  ASSERT_EQ(ROLE_SYSTEM_GRAPHIC, target2_role);

  // Call scrollTo on the first target. Ensure it ends up very near the
  // center of the window.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED);
  ASSERT_HRESULT_SUCCEEDED(target->scrollTo(IA2_SCROLL_TYPE_ANYWHERE));
  ASSERT_TRUE(waiter.WaitForNotification());

  // Don't assume anything about the font size or the exact centering
  // behavior, just assert that the object is (roughly) centered by
  // checking that its top coordinate is between 40% and 60% of the
  // document's height.
  LONG x, y, width, height;
  ASSERT_HRESULT_SUCCEEDED(
      target->accLocation(&x, &y, &width, &height, childid_self));
  EXPECT_GT(y + height / 2, doc_y + 0.4 * doc_height);
  EXPECT_LT(y + height / 2, doc_y + 0.6 * doc_height);

  // Now call scrollTo on the second target. Ensure it ends up very near the
  // center of the window.
  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED);
  ASSERT_HRESULT_SUCCEEDED(target2->scrollTo(IA2_SCROLL_TYPE_ANYWHERE));
  ASSERT_TRUE(waiter2.WaitForNotification());

  // Same as above, make sure it's roughly centered.
  ASSERT_HRESULT_SUCCEEDED(
      target2->accLocation(&x, &y, &width, &height, childid_self));
  EXPECT_GT(y + height / 2, doc_y + 0.4 * doc_height);
  EXPECT_LT(y + height / 2, doc_y + 0.6 * doc_height);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestPageIsAccessibleAfterCancellingReload) {
  if (base::FeatureList::IsEnabled(
          blink::features::kBeforeunloadEventCancelByPreventDefault)) {
    LoadInitialAccessibilityTreeFromHtml(
        "data:text/html,"
        "<script>"
        "window.onbeforeunload = function (e) {"
        "  e.preventDefault()"
        "};"
        "</script>"
        "<input value='Test'>");
  } else {
    LoadInitialAccessibilityTreeFromHtml(
        "data:text/html,"
        "<script>"
        "window.onbeforeunload = function () {"
        "  return 'Not empty string';"
        "};"
        "</script>"
        "<input value='Test'>");
  }

  // When the before unload dialog shows, simulate the user clicking
  // cancel on that dialog.
  SetShouldProceedOnBeforeUnload(shell(), true, false);

  // The beforeunload dialog won't be shown unless the page has at
  // least one user gesture on it.
  auto* main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  main_frame->ExecuteJavaScriptWithUserGestureForTests(
      std::u16string(), base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);

  // Trigger a reload here, which will get cancelled.
  AppModalDialogWaiter dialog_waiter(shell());
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);

  // Wait for the dialog to be triggered and then get cancelled.
  dialog_waiter.Wait();

  // Now set up a listener for native Windows accessibility events.
  // The bug here was that when a page is being reloaded or navigated
  // away, we were suppressing accessibility events. This test ensures
  // that if you cancel a reload, events are no longer suppressed.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();
  NativeWinEventWaiter win_event_waiter(
      manager, "EVENT_OBJECT_FOCUS on <input> role=ROLE_SYSTEM_TEXT*");

  // Get the root accessible element and its children.
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());

  // Get the only child of the root.
  Microsoft::WRL::ComPtr<IAccessible2> group;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &group));
  LONG group_role = 0;
  ASSERT_HRESULT_SUCCEEDED(group->role(&group_role));
  ASSERT_EQ(IA2_ROLE_SECTION, group_role);
  std::vector<base::win::ScopedVariant> group_children =
      GetAllAccessibleChildren(group.Get());
  ASSERT_EQ(2u, group_children.size());

  // Get the second child of that group, the input element.
  Microsoft::WRL::ComPtr<IAccessible2> input;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(group.Get(), group_children[1].AsInput()).Get(),
      &input));
  LONG input_role = 0;
  ASSERT_HRESULT_SUCCEEDED(input->role(&input_role));
  ASSERT_EQ(ROLE_SYSTEM_TEXT, input_role);

  // Try to focus that input element.
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = input->accSelect(SELFLAG_TAKEFOCUS, childid_self);
  ASSERT_EQ(S_OK, hr);

  // Ensure that we get the native focus event on that input element.
  win_event_waiter.Wait();
}

class AccessibilityWinUIABrowserTest : public AccessibilityWinBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{::features::kUiaProvider};
};

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest, TestIScrollProvider) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div aria-label='not' role='group'>
            not scrollable
          </div>
          <div style='width:100px; overflow:auto' aria-label='x' role='group'>
              <div style='width:200px; height:100px'></div>
          </div>
          <div style='height:100px; overflow:auto' aria-label='y' role='group'>
              <div style='width:100px; height:200px'></div>
          </div>
        </body>
      </html>
    )HTML");

  struct ScrollTestData {
    std::string node_name;
    bool can_scroll_horizontal;
    bool can_scroll_vertical;
    double size_horizontal;
    double size_vertical;
  };
  double error = 0.01f;
  std::vector<ScrollTestData> all_expected = {{"not", false, false, 0.0, 0.0},
                                              {"x", true, false, 50.0, 0.0},
                                              {"y", false, true, 0.0, 50.0}};
  for (auto& expected : all_expected) {
    ui::BrowserAccessibility* browser_accessibility =
        FindNode(ax::mojom::Role::kGroup, expected.node_name);
    EXPECT_NE(browser_accessibility, nullptr);

    ui::BrowserAccessibilityComWin* browser_accessibility_com_win =
        ToBrowserAccessibilityWin(browser_accessibility)->GetCOM();
    Microsoft::WRL::ComPtr<IScrollProvider> scroll_provider;

    EXPECT_HRESULT_SUCCEEDED(browser_accessibility_com_win->GetPatternProvider(
        UIA_ScrollPatternId, &scroll_provider));

    if (expected.can_scroll_vertical || expected.can_scroll_horizontal) {
      ASSERT_NE(nullptr, scroll_provider);

      BOOL can_scroll_horizontal;
      EXPECT_HRESULT_SUCCEEDED(
          scroll_provider->get_HorizontallyScrollable(&can_scroll_horizontal));
      ASSERT_EQ(expected.can_scroll_horizontal, can_scroll_horizontal);
      if (expected.can_scroll_horizontal) {
        double size_horizontal;
        EXPECT_HRESULT_SUCCEEDED(
            scroll_provider->get_HorizontalViewSize(&size_horizontal));
        EXPECT_NEAR(expected.size_horizontal, size_horizontal, error);

        double x_before;
        EXPECT_HRESULT_SUCCEEDED(
            scroll_provider->get_HorizontalScrollPercent(&x_before));
        EXPECT_NEAR(0, x_before, error);

        AccessibilityNotificationWaiter waiter(
            shell()->web_contents(), ui::kAXModeComplete,
            ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED);

        EXPECT_HRESULT_SUCCEEDED(scroll_provider->Scroll(
            ScrollAmount_SmallIncrement, ScrollAmount_NoAmount));
        ASSERT_TRUE(waiter.WaitForNotification());

        double x_after;
        EXPECT_HRESULT_SUCCEEDED(
            scroll_provider->get_HorizontalScrollPercent(&x_after));
        EXPECT_GT(x_after, x_before);

        AccessibilityNotificationWaiter waiter2(
            shell()->web_contents(), ui::kAXModeComplete,
            ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED);

        EXPECT_HRESULT_SUCCEEDED(scroll_provider->SetScrollPercent(0.0, 0.0));
        ASSERT_TRUE(waiter2.WaitForNotification());

        EXPECT_HRESULT_SUCCEEDED(
            scroll_provider->get_HorizontalScrollPercent(&x_after));
        EXPECT_NEAR(0, x_after, error);
      }

      BOOL can_scroll_vertical;
      EXPECT_HRESULT_SUCCEEDED(
          scroll_provider->get_VerticallyScrollable(&can_scroll_vertical));
      ASSERT_EQ(expected.can_scroll_vertical, can_scroll_vertical);
      if (expected.can_scroll_vertical) {
        double size_vertical;
        EXPECT_HRESULT_SUCCEEDED(
            scroll_provider->get_VerticalViewSize(&size_vertical));
        EXPECT_NEAR(expected.size_vertical, size_vertical, error);

        double y_before;
        EXPECT_HRESULT_SUCCEEDED(
            scroll_provider->get_VerticalScrollPercent(&y_before));
        EXPECT_NEAR(0, y_before, error);

        AccessibilityNotificationWaiter waiter(
            shell()->web_contents(), ui::kAXModeComplete,
            ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED);

        EXPECT_HRESULT_SUCCEEDED(scroll_provider->Scroll(
            ScrollAmount_NoAmount, ScrollAmount_SmallIncrement));
        ASSERT_TRUE(waiter.WaitForNotification());

        double y_after;
        EXPECT_HRESULT_SUCCEEDED(
            scroll_provider->get_VerticalScrollPercent(&y_after));
        EXPECT_GT(y_after, y_before);

        AccessibilityNotificationWaiter waiter2(
            shell()->web_contents(), ui::kAXModeComplete,
            ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED);

        EXPECT_HRESULT_SUCCEEDED(scroll_provider->SetScrollPercent(0.0, 0.0));
        ASSERT_TRUE(waiter2.WaitForNotification());

        EXPECT_HRESULT_SUCCEEDED(
            scroll_provider->get_VerticalScrollPercent(&y_after));
        EXPECT_NEAR(0, y_after, error);
      }
    } else {
      EXPECT_EQ(nullptr, scroll_provider);
    }
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest,
                       TestIsContentElementPropertyId) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <table>
          <tr>
              <th aria-label="header">
                  header
              </th>
              <td> data </td>
          </tr>
        </table>
      </html>)HTML");

  ui::BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kRowHeader, "header");
  EXPECT_NE(nullptr, target);
  ui::BrowserAccessibilityComWin* accessibility_com_win =
      ToBrowserAccessibilityWin(target)->GetCOM();
  EXPECT_NE(nullptr, accessibility_com_win);

  base::win::ScopedVariant result;
  accessibility_com_win->GetPropertyValue(UIA_IsContentElementPropertyId,
                                          result.Receive());

  ui::BrowserAccessibility* child = target->PlatformDeepestFirstChild();
  EXPECT_NE(nullptr, child);
  accessibility_com_win = ToBrowserAccessibilityWin(child)->GetCOM();
  EXPECT_NE(nullptr, accessibility_com_win);

  result.Release();
  accessibility_com_win->GetPropertyValue(UIA_IsContentElementPropertyId,
                                          result.Receive());
  EXPECT_EQ(VT_BOOL, result.type());
  EXPECT_EQ(VARIANT_FALSE, result.ptr()->boolVal);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest,
                       OffscreenNodeNotClickable) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <div style="height:200vh"></div>
        <button>offscreen</button>
      </html>)HTML");

  ui::BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kButton, "offscreen");
  EXPECT_NE(nullptr, target);
  ui::BrowserAccessibilityComWin* accessibility_com_win =
      ToBrowserAccessibilityWin(target)->GetCOM();
  EXPECT_NE(nullptr, accessibility_com_win);

  base::win::ScopedVariant result;

  accessibility_com_win->GetPropertyValue(UIA_IsOffscreenPropertyId,
                                          result.Receive());

  EXPECT_EQ(VARIANT_TRUE, result.ptr()->boolVal);

  result.Release();

  accessibility_com_win->GetPropertyValue(UIA_ClickablePointPropertyId,
                                          result.Receive());

  EXPECT_EQ(VT_EMPTY, result.type());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest, OnscreenNodeClickable) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <button>onscreen</button>
      </html>)HTML");

  ui::BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kButton, "onscreen");
  EXPECT_NE(nullptr, target);
  ui::BrowserAccessibilityComWin* accessibility_com_win =
      ToBrowserAccessibilityWin(target)->GetCOM();
  EXPECT_NE(nullptr, accessibility_com_win);

  base::win::ScopedVariant result;

  accessibility_com_win->GetPropertyValue(UIA_IsOffscreenPropertyId,
                                          result.Receive());

  EXPECT_EQ(VARIANT_FALSE, result.ptr()->boolVal);

  result.Release();

  accessibility_com_win->GetPropertyValue(UIA_ClickablePointPropertyId,
                                          result.Receive());

  EXPECT_NE(VT_EMPTY, result.type());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest,
                       TestIFrameRootNodeChange) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      </html>)HTML");

  // Request an automation element for the legacy window to ensure that the
  // fragment root is created before the iframe content tree shows up.
  Microsoft::WRL::ComPtr<IUIAutomation> uia;
  ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                            CLSCTX_INPROC_SERVER,
                                            IID_IUIAutomation, &uia));

  RenderWidgetHostViewAura* render_widget_host_view_aura =
      static_cast<RenderWidgetHostViewAura*>(
          shell()->web_contents()->GetRenderWidgetHostView());
  ASSERT_NE(nullptr, render_widget_host_view_aura);
  HWND hwnd = render_widget_host_view_aura->AccessibilityGetAcceleratedWidget();
  ASSERT_NE(gfx::kNullAcceleratedWidget, hwnd);
  Microsoft::WRL::ComPtr<IUIAutomationElement> root_element;
  ASSERT_HRESULT_SUCCEEDED(uia->ElementFromHandle(hwnd, &root_element));
  ASSERT_NE(nullptr, root_element.Get());

  // Insert a new iframe and wait for tree update.
  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  ExecuteScript(
      u"let new_frame = document.createElement('iframe');"
      u"new_frame.setAttribute('src', 'about:blank');"
      u"document.body.appendChild(new_frame);");
  ASSERT_TRUE(waiter.WaitForNotification());

  // Content root node's parent's child should still be the content root node.
  Microsoft::WRL::ComPtr<IRawElementProviderFragment> content_root;
  ASSERT_HRESULT_SUCCEEDED(GetManager()
                               ->GetBrowserAccessibilityRoot()
                               ->GetNativeViewAccessible()
                               ->QueryInterface(IID_PPV_ARGS(&content_root)));

  Microsoft::WRL::ComPtr<IRawElementProviderFragment> parent;
  ASSERT_HRESULT_SUCCEEDED(
      content_root->Navigate(NavigateDirection_Parent, &parent));
  ASSERT_NE(nullptr, parent.Get());

  Microsoft::WRL::ComPtr<IRawElementProviderFragment> first_child;
  ASSERT_HRESULT_SUCCEEDED(
      parent->Navigate(NavigateDirection_FirstChild, &first_child));
  EXPECT_EQ(content_root.Get(), first_child.Get());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest, TestGetFragmentRoot) {
  // Verify that we can obtain a fragment root from a fragment without having
  // sent WM_GETOBJECT to the host window.
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      </html>)HTML");

  Microsoft::WRL::ComPtr<IRawElementProviderFragment> content_root;
  ASSERT_HRESULT_SUCCEEDED(GetManager()
                               ->GetBrowserAccessibilityRoot()
                               ->GetNativeViewAccessible()
                               ->QueryInterface(IID_PPV_ARGS(&content_root)));

  Microsoft::WRL::ComPtr<IRawElementProviderFragmentRoot> fragment_root;
  ASSERT_HRESULT_SUCCEEDED(content_root->get_FragmentRoot(&fragment_root));
  ASSERT_NE(nullptr, fragment_root.Get());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest, IA2ElementToUIAElement) {
  // This test validates looking up an UIA element from an IA2 element.
  // We start by retrieving an IA2 element then its corresponding unique id. We
  // then use the unique id to retrieve the corresponding UIA element through
  // IItemContainerProvider::FindItemByProperty().
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(
      <html>
        <div role="button">button</div>
        <div role="listbox" aria-label="listbox"></div>
      </html>
     )HTML");

  // Obtain the fragment root from the top-level HWND.
  HWND hwnd = shell()->window()->GetHost()->GetAcceleratedWidget();
  ASSERT_NE(gfx::kNullAcceleratedWidget, hwnd);
  ui::AXFragmentRootWin* fragment_root =
      ui::AXFragmentRootWin::GetForAcceleratedWidget(hwnd);
  ASSERT_NE(nullptr, fragment_root);

  Microsoft::WRL::ComPtr<IItemContainerProvider> item_container_provider;
  ASSERT_HRESULT_SUCCEEDED(
      fragment_root->GetNativeViewAccessible()->QueryInterface(
          IID_PPV_ARGS(&item_container_provider)));

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());

  // Look up button's UIA element from its IA2 element.
  {
    // Retrieve button's IA2 element.
    Microsoft::WRL::ComPtr<IAccessible2> button_ia2;
    ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
        GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
            .Get(),
        &button_ia2));
    LONG button_role = 0;
    ASSERT_HRESULT_SUCCEEDED(button_ia2->role(&button_role));
    ASSERT_EQ(ROLE_SYSTEM_PUSHBUTTON, button_role);

    // Retrieve button's IA2 unique id.
    LONG button_unique_id;
    button_ia2->get_uniqueID(&button_unique_id);
    base::win::ScopedVariant ia2_unique_id;
    ia2_unique_id.Set(
        SysAllocString(base::NumberToWString(button_unique_id).c_str()));

    // Verify we can find the button's UIA element based on its unique id.
    Microsoft::WRL::ComPtr<IRawElementProviderSimple> button_uia;
    ASSERT_HRESULT_SUCCEEDED(item_container_provider->FindItemByProperty(
        nullptr, ui::UiaRegistrarWin::GetInstance().GetUniqueIdPropertyId(),
        ia2_unique_id, &button_uia));

    // UIA and IA2 elements should have the same unique id.
    base::win::ScopedVariant uia_unique_id;
    ASSERT_HRESULT_SUCCEEDED(button_uia->GetPropertyValue(
        ui::UiaRegistrarWin::GetInstance().GetUniqueIdPropertyId(),
        uia_unique_id.Receive()));
    ASSERT_STREQ(ia2_unique_id.ptr()->bstrVal, uia_unique_id.ptr()->bstrVal);

    // Verify the retrieved UIA element is button through its name property.
    base::win::ScopedVariant name_property;
    ASSERT_HRESULT_SUCCEEDED(button_uia->GetPropertyValue(
        UIA_NamePropertyId, name_property.Receive()));
    ASSERT_EQ(name_property.type(), VT_BSTR);
    BSTR name_bstr = name_property.ptr()->bstrVal;
    std::wstring actual_name(name_bstr, ::SysStringLen(name_bstr));
    ASSERT_EQ(L"button", actual_name);

    // Verify that the button's IA2 element and UIA element are the same through
    // comparing their IUnknown interfaces.
    Microsoft::WRL::ComPtr<IUnknown> iunknown_button_from_uia;
    ASSERT_HRESULT_SUCCEEDED(
        button_uia->QueryInterface(IID_PPV_ARGS(&iunknown_button_from_uia)));

    Microsoft::WRL::ComPtr<IUnknown> iunknown_button_from_ia2;
    ASSERT_HRESULT_SUCCEEDED(
        button_ia2->QueryInterface(IID_PPV_ARGS(&iunknown_button_from_ia2)));

    ASSERT_EQ(iunknown_button_from_uia.Get(), iunknown_button_from_ia2.Get());
  }

  // Look up listbox's UIA element from its IA2 element.
  {
    // Retrieve listbox's IA2 element.
    Microsoft::WRL::ComPtr<IAccessible2> listbox_ia2;
    ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
        GetAccessibleFromVariant(document.Get(), document_children[1].AsInput())
            .Get(),
        &listbox_ia2));
    LONG listbox_role = 0;
    ASSERT_HRESULT_SUCCEEDED(listbox_ia2->role(&listbox_role));
    ASSERT_EQ(ROLE_SYSTEM_LIST, listbox_role);

    // Retrieve listbox's IA2 unique id.
    LONG listbox_unique_id;
    listbox_ia2->get_uniqueID(&listbox_unique_id);
    base::win::ScopedVariant ia2_unique_id;
    ia2_unique_id.Set(
        SysAllocString(base::NumberToWString(listbox_unique_id).c_str()));

    // Verify we can find the listbox's UIA element based on its unique id.
    Microsoft::WRL::ComPtr<IRawElementProviderSimple> listbox_uia;
    ASSERT_HRESULT_SUCCEEDED(item_container_provider->FindItemByProperty(
        nullptr, ui::UiaRegistrarWin::GetInstance().GetUniqueIdPropertyId(),
        ia2_unique_id, &listbox_uia));

    // UIA and IA2 elements should have the same unique id.
    base::win::ScopedVariant uia_unique_id;
    ASSERT_HRESULT_SUCCEEDED(listbox_uia->GetPropertyValue(
        ui::UiaRegistrarWin::GetInstance().GetUniqueIdPropertyId(),
        uia_unique_id.Receive()));
    ASSERT_STREQ(ia2_unique_id.ptr()->bstrVal, uia_unique_id.ptr()->bstrVal);

    // Verify the retrieved UIA element is listbox through its name property.
    base::win::ScopedVariant name_property;
    ASSERT_HRESULT_SUCCEEDED(listbox_uia->GetPropertyValue(
        UIA_NamePropertyId, name_property.Receive()));
    ASSERT_EQ(name_property.type(), VT_BSTR);
    BSTR name_bstr = name_property.ptr()->bstrVal;
    std::wstring actual_name(name_bstr, ::SysStringLen(name_bstr));
    ASSERT_EQ(L"listbox", actual_name);

    // Verify that the listbox's IA2 element and UIA element are the same
    // through comparing their IUnknown interfaces.
    Microsoft::WRL::ComPtr<IUnknown> iunknown_listbox_from_uia;
    ASSERT_HRESULT_SUCCEEDED(
        listbox_uia->QueryInterface(IID_PPV_ARGS(&iunknown_listbox_from_uia)));

    Microsoft::WRL::ComPtr<IUnknown> iunknown_listbox_from_ia2;
    ASSERT_HRESULT_SUCCEEDED(
        listbox_ia2->QueryInterface(IID_PPV_ARGS(&iunknown_listbox_from_ia2)));

    ASSERT_EQ(iunknown_listbox_from_uia.Get(), iunknown_listbox_from_ia2.Get());
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest, UIAElementToIA2Element) {
  // This test validates looking up an IA2 element from an UIA element.
  // We start by retrieving an UIA element then its corresponding unique id. We
  // then use the unique id to retrieve the corresponding IA2 element.
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(
      <html>
        <div role="button">button</div>
      </html>
     )HTML");
  Microsoft::WRL::ComPtr<IRawElementProviderSimple> button_uia =
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          FindNode(ax::mojom::Role::kButton, "button"));

  // Retrieve the UIA element's unique id.
  base::win::ScopedVariant uia_unique_id;
  ASSERT_HRESULT_SUCCEEDED(button_uia->GetPropertyValue(
      ui::UiaRegistrarWin::GetInstance().GetUniqueIdPropertyId(),
      uia_unique_id.Receive()));

  int32_t unique_id_value;
  ASSERT_EQ(VT_BSTR, uia_unique_id.type());
  ASSERT_TRUE(
      base::StringToInt(uia_unique_id.ptr()->bstrVal, &unique_id_value));

  // Retrieve the corresponding IA2 element through the unique id.
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  base::win::ScopedVariant ia2_unique_id(unique_id_value);
  Microsoft::WRL::ComPtr<IAccessible2> button_ia2;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), ia2_unique_id.AsInput()).Get(),
      &button_ia2));

  // Verify that the retrieved IA2 element shares the same unique id as the UIA
  // element.
  LONG button_ia2_unique_id;
  button_ia2->get_uniqueID(&button_ia2_unique_id);
  ASSERT_EQ(unique_id_value, button_ia2_unique_id);

  // Verify that the IA2 element and UIA element are the same through
  // comparing their IUnknown interfaces.
  Microsoft::WRL::ComPtr<IUnknown> iunknown_button_from_uia;
  ASSERT_HRESULT_SUCCEEDED(
      button_uia->QueryInterface(IID_PPV_ARGS(&iunknown_button_from_uia)));

  Microsoft::WRL::ComPtr<IUnknown> iunknown_button_from_ia2;
  ASSERT_HRESULT_SUCCEEDED(
      button_ia2->QueryInterface(IID_PPV_ARGS(&iunknown_button_from_ia2)));

  ASSERT_EQ(iunknown_button_from_uia.Get(), iunknown_button_from_ia2.Get());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest,
                       RootElementPropertyValues) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <title>Page title</title>
      </html>)HTML");

  // Request an automation element for the root element.
  Microsoft::WRL::ComPtr<IUIAutomation> uia;
  ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                            CLSCTX_INPROC_SERVER,
                                            IID_IUIAutomation, &uia));

  HWND hwnd = shell()->window()->GetHost()->GetAcceleratedWidget();
  ASSERT_NE(gfx::kNullAcceleratedWidget, hwnd);
  Microsoft::WRL::ComPtr<IUIAutomationElement> element;
  ASSERT_HRESULT_SUCCEEDED(uia->ElementFromHandle(hwnd, &element));
  ASSERT_NE(nullptr, element.Get());

  // The control type should be Window and the name should be the same as the
  // window title. These values come from UIA's built-in provider; this test
  // validates that we aren't overriding them.
  CONTROLTYPEID control_type;
  ASSERT_HRESULT_SUCCEEDED(element->get_CurrentControlType(&control_type));
  EXPECT_EQ(UIA_WindowControlTypeId, control_type);

  wchar_t window_text[100] = {0};
  ::GetWindowTextW(hwnd, window_text, _countof(window_text));
  std::wstring window_text_str16(window_text);
  base::win::ScopedBstr name;
  ASSERT_HRESULT_SUCCEEDED(element->get_CurrentName(name.Receive()));
  std::wstring name_str16(name.Get(), name.Length());
  EXPECT_EQ(window_text_str16, name_str16);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest,
                       GetFocusFromRootReachesWebContent) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <button>Focus target</button>
        <script>
          document.querySelector('button').focus();
        </script>
      </html>)HTML");

  // Obtain the fragment root from the top-level HWND.
  HWND hwnd = shell()->window()->GetHost()->GetAcceleratedWidget();
  ASSERT_NE(gfx::kNullAcceleratedWidget, hwnd);
  ui::AXFragmentRootWin* fragment_root =
      ui::AXFragmentRootWin::GetForAcceleratedWidget(hwnd);
  ASSERT_NE(nullptr, fragment_root);
  Microsoft::WRL::ComPtr<IRawElementProviderFragmentRoot> uia_fragment_root;
  ASSERT_HRESULT_SUCCEEDED(
      fragment_root->GetNativeViewAccessible()->QueryInterface(
          IID_PPV_ARGS(&uia_fragment_root)));

  // Verify that calling GetFocus on the fragment root reaches web content.
  Microsoft::WRL::ComPtr<IRawElementProviderFragment> focused_fragment;
  ASSERT_HRESULT_SUCCEEDED(uia_fragment_root->GetFocus(&focused_fragment));

  Microsoft::WRL::ComPtr<IRawElementProviderSimple> focused_element;
  ASSERT_HRESULT_SUCCEEDED(focused_fragment.As(&focused_element));

  base::win::ScopedVariant name_property;
  ASSERT_HRESULT_SUCCEEDED(focused_element->GetPropertyValue(
      UIA_NamePropertyId, name_property.Receive()));
  ASSERT_EQ(name_property.type(), VT_BSTR);
  BSTR name_bstr = name_property.ptr()->bstrVal;
  std::wstring actual_name(name_bstr, ::SysStringLen(name_bstr));
  EXPECT_EQ(L"Focus target", actual_name);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest,
                       LegacyWindowIsNotControlElement) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
        <html>
        </html>)HTML");

  // Request an automation element for the legacy window.
  Microsoft::WRL::ComPtr<IUIAutomation> uia;
  ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                            CLSCTX_INPROC_SERVER,
                                            IID_IUIAutomation, &uia));

  RenderWidgetHostViewAura* render_widget_host_view_aura =
      static_cast<RenderWidgetHostViewAura*>(
          shell()->web_contents()->GetRenderWidgetHostView());
  ASSERT_NE(nullptr, render_widget_host_view_aura);
  HWND hwnd = render_widget_host_view_aura->AccessibilityGetAcceleratedWidget();
  ASSERT_NE(gfx::kNullAcceleratedWidget, hwnd);
  Microsoft::WRL::ComPtr<IUIAutomationElement> element;
  ASSERT_HRESULT_SUCCEEDED(uia->ElementFromHandle(hwnd, &element));
  ASSERT_NE(nullptr, element.Get());

  // The legacy window should not be in the control or content trees.
  BOOL result;
  ASSERT_HRESULT_SUCCEEDED(element->get_CurrentIsControlElement(&result));
  EXPECT_EQ(FALSE, result);
  ASSERT_HRESULT_SUCCEEDED(element->get_CurrentIsContentElement(&result));
  EXPECT_EQ(FALSE, result);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest,
                       UIAParentNavigationDuringWebContentsClose) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
        <html>
        </html>)HTML");

  // Request an automation element for UIA tree traversal.
  Microsoft::WRL::ComPtr<IUIAutomation> uia;
  ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                            CLSCTX_INPROC_SERVER,
                                            IID_IUIAutomation, &uia));

  RenderWidgetHostViewAura* view = static_cast<RenderWidgetHostViewAura*>(
      shell()->web_contents()->GetRenderWidgetHostView());
  ASSERT_NE(nullptr, view);

  // Start by getting the root element for the HWND hosting the web content.
  HWND hwnd = view->host()
                  ->GetRootBrowserAccessibilityManager()
                  ->GetBrowserAccessibilityRoot()
                  ->GetTargetForNativeAccessibilityEvent();
  ASSERT_NE(gfx::kNullAcceleratedWidget, hwnd);
  Microsoft::WRL::ComPtr<IUIAutomationElement> root;
  uia->ElementFromHandle(hwnd, &root);
  ASSERT_NE(nullptr, root.Get());

  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker;
  uia->get_RawViewWalker(&tree_walker);
  ASSERT_NE(nullptr, tree_walker.Get());

  // Navigate to the root's first child before closing the WebContents.
  Microsoft::WRL::ComPtr<IUIAutomationElement> first_child;
  tree_walker->GetFirstChildElement(root.Get(), &first_child);
  ASSERT_NE(nullptr, first_child.Get());

  // The bug only reproduces during the WebContentsDestroyed event, so create
  // an observer that will do UIA parent navigation (on the first child that
  // was just obtained) while the WebContents is being destroyed.
  content::WebContentsUIAParentNavigationInDestroyedWatcher destroyed_watcher(
      shell()->web_contents(), first_child.Get(), tree_walker.Get());
  shell()->CloseContents(shell()->web_contents());
  destroyed_watcher.Wait();
}

class AccessibilityWinUIASelectivelyEnabledBrowserTest
    : public AccessibilityWinUIABrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSelectiveUIAEnablement);

    AccessibilityWinUIABrowserTest::SetUp();
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIASelectivelyEnabledBrowserTest,
                       RequestingTopLevelElementEnablesWebAccessibility) {
  std::string html = R"HTML(<!DOCTYPE html>
        <html>
        <div>some text</div>
        </html>)HTML";
  GURL html_data_url("data:text/html," + html);
  EXPECT_TRUE(NavigateToURL(shell(), html_data_url));

  // Ensure accessibility is not enabled before we begin the test.
  EXPECT_TRUE(content::BrowserAccessibilityStateImpl::GetInstance()
                  ->GetAccessibilityMode()
                  .is_mode_off());

  // Start with AXMode::kWebContents. Later, a UIA call will cause kNativeAPIs
  // to be added to the AXMode.
  ScopedAccessibilityModeOverride ax_mode_override(ui::AXMode::kWebContents);

  // Request an automation element for the top-level window.
  Microsoft::WRL::ComPtr<IUIAutomation> uia;
  ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                            CLSCTX_INPROC_SERVER,
                                            IID_IUIAutomation, &uia));

  HWND hwnd = shell()->window()->GetHost()->GetAcceleratedWidget();
  ASSERT_NE(gfx::kNullAcceleratedWidget, hwnd);
  Microsoft::WRL::ComPtr<IUIAutomationElement> root;
  uia->ElementFromHandle(hwnd, &root);
  ASSERT_NE(nullptr, root.Get());

  // AXMode::kNativeAPIs should now be enabled in addition to kWebContents.
  // (kAXModeBasic includes both kNativeAPIs and kWebContents). Importantly,
  // this combination of AXModes allows RenderFrameHostImpl to create
  // BrowserAccessibilityManagers.
  ui::AXMode expected_mode = ui::kAXModeBasic.flags();
  EXPECT_EQ(expected_mode, content::BrowserAccessibilityStateImpl::GetInstance()
                               ->GetAccessibilityMode());

  // Now get the fragment root's first (only) child.
  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker;
  uia->get_RawViewWalker(&tree_walker);
  Microsoft::WRL::ComPtr<IUIAutomationElement> first_child;
  tree_walker->GetFirstChildElement(root.Get(), &first_child);
  ASSERT_NE(nullptr, first_child.Get());
  base::win::ScopedVariant control_type;
  // Query Property value on non web content.
  ASSERT_HRESULT_SUCCEEDED(first_child->GetCurrentPropertyValue(
      UIA_ControlTypePropertyId, control_type.Receive()));
  // As this is not on web content, this should not cause any enablement.
  EXPECT_EQ(expected_mode, content::BrowserAccessibilityStateImpl::GetInstance()
                               ->GetAccessibilityMode());
  // While no additional enablement is done, the result should still be correct.
  EXPECT_EQ(UIA_PaneControlTypeId, control_type.ptr()->intVal);

  // Now try to get the text content.
  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
  base::win::ScopedVariant control_type_variant(UIA_TextControlTypeId);
  ASSERT_HRESULT_SUCCEEDED(uia->CreatePropertyCondition(
      UIA_ControlTypePropertyId, control_type_variant, &condition));
  EXPECT_NE(nullptr, condition.Get());
  Microsoft::WRL::ComPtr<IUIAutomationElement> text_element;

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::AXMode::kNativeAPIs,
                                         ax::mojom::Event::kLoadComplete);
  ASSERT_HRESULT_SUCCEEDED(
      root->FindFirst(TreeScope_Subtree, condition.Get(), &text_element));
  // This call failed as web contents was not previously enabled.
  EXPECT_EQ(nullptr, text_element.Get());

  // Web content accessibility support should now be enabled.
  expected_mode |= ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents |
                   ui::AXMode::kScreenReader;
  EXPECT_EQ(expected_mode, content::BrowserAccessibilityStateImpl::GetInstance()
                               ->GetAccessibilityMode());
  ASSERT_TRUE(waiter.WaitForNotification());

  // This call should succeed as web contents have been enabled.
  ASSERT_HRESULT_SUCCEEDED(
      root->FindFirst(TreeScope_Subtree, condition.Get(), &text_element));
  ASSERT_NE(nullptr, text_element.Get());

  Microsoft::WRL::ComPtr<IUnknown> text_pattern_unknown;
  ASSERT_HRESULT_SUCCEEDED(text_element->GetCurrentPattern(
      UIA_TextPatternId, &text_pattern_unknown));
  EXPECT_NE(nullptr, text_pattern_unknown.Get());

  // Now check that inline text box support is enabled as well.
  expected_mode |= ui::AXMode::kInlineTextBoxes;
  EXPECT_EQ(expected_mode, content::BrowserAccessibilityStateImpl::GetInstance()
                               ->GetAccessibilityMode());

  {
    base::win::ScopedVariant variant;
    ASSERT_HRESULT_SUCCEEDED(text_element->GetCurrentPropertyValue(
        UIA_LabeledByPropertyId, variant.Receive()));
  }
  // Now check that we have complete accessibility support enabled.
  expected_mode |= ui::AXMode::kScreenReader;
  EXPECT_EQ(expected_mode, content::BrowserAccessibilityStateImpl::GetInstance()
                               ->GetAccessibilityMode());

  {
    base::win::ScopedVariant variant;
    ASSERT_HRESULT_SUCCEEDED(text_element->GetCurrentPropertyValue(
        UIA_AutomationIdPropertyId, variant.Receive()));
  }
  EXPECT_EQ(expected_mode, content::BrowserAccessibilityStateImpl::GetInstance()
                               ->GetAccessibilityMode());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest,
                       TabHeuristicForWindowsNarrator) {
  // Windows Narrator uses certain heuristics to determine where in the UIA
  // tree a "browser tab" begins and ends, in order to contain the search range
  // for commands such as "move to next/previous text input field."
  // This test is used to validate such heuristics.
  // Specifically: The boundary of a browser tab is the element nearest the
  // root with a ControlType of Document and an implementation of TextPattern.
  // In this test, we validate that such an element exists.

  // Load an empty HTML page.
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
        <html>
        </html>)HTML");

  // The element exposed by the legacy window is a fragment root whose first
  // child represents the document root (our tab boundary). First, get the
  // fragment root using the legacy window's HWND.
  RenderWidgetHostViewAura* render_widget_host_view_aura =
      static_cast<RenderWidgetHostViewAura*>(
          shell()->web_contents()->GetRenderWidgetHostView());
  HWND hwnd = render_widget_host_view_aura->AccessibilityGetAcceleratedWidget();
  ASSERT_NE(gfx::kNullAcceleratedWidget, hwnd);
  Microsoft::WRL::ComPtr<IUIAutomation> uia;
  ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                            CLSCTX_INPROC_SERVER,
                                            IID_IUIAutomation, &uia));
  Microsoft::WRL::ComPtr<IUIAutomationElement> fragment_root;
  ASSERT_HRESULT_SUCCEEDED(uia->ElementFromHandle(hwnd, &fragment_root));
  ASSERT_NE(nullptr, fragment_root.Get());

  // Now get the fragment root's first (only) child.
  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker;
  uia->get_RawViewWalker(&tree_walker);
  Microsoft::WRL::ComPtr<IUIAutomationElement> first_child;
  tree_walker->GetFirstChildElement(fragment_root.Get(), &first_child);
  ASSERT_NE(nullptr, first_child.Get());

  // Validate the control type and presence of TextPattern.
  CONTROLTYPEID control_type;
  ASSERT_HRESULT_SUCCEEDED(first_child->get_CurrentControlType(&control_type));
  EXPECT_EQ(control_type, UIA_DocumentControlTypeId);

  Microsoft::WRL::ComPtr<IUnknown> text_pattern_unknown;
  ASSERT_HRESULT_SUCCEEDED(
      first_child->GetCurrentPattern(UIA_TextPatternId, &text_pattern_unknown));
  EXPECT_NE(nullptr, text_pattern_unknown.Get());
}

// TODO(crbug.com/40902845): Fix this failing test.
IN_PROC_BROWSER_TEST_F(AccessibilityWinUIABrowserTest,
                       DISABLED_AsyncContentLoadedEventOnDocumentLoad) {
  // Load the page.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* browser_accessibility_manager =
      web_contents->GetOrCreateRootBrowserAccessibilityManager();

  NativeWinEventWaiter win_event_waiter(
      browser_accessibility_manager,
      "AsyncContentLoaded on role=document, name=Accessibility Test",
      ui::AXApiType::kWinUIA);

  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html><head><title>Accessibility Test</title></head>"
      "<body>"
      "<button>This is a button</button>"
      "</body></html>";
  GURL url(url_str);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  win_event_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestOffsetsOfSelectionAll) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <p>Hello world.</p>
      <p>Another paragraph.</p>
      <p>Goodbye world.</p>
      <script>
      var root = document.documentElement;
      window.getSelection().selectAllChildren(root);
      </script>)HTML");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  ASSERT_TRUE(document);

  auto* node = static_cast<ui::AXPlatformNodeWin*>(
      ui::AXPlatformNode::FromNativeViewAccessible(document.Get()));
  {
    LONG start_offset = 0;
    LONG end_offset = 0;
    HRESULT hr = node->get_selection(0, &start_offset, &end_offset);
    EXPECT_EQ(S_OK, hr);
    EXPECT_EQ(0, start_offset);
    EXPECT_EQ(3, end_offset);
  }

  std::vector<int> expected = {12, 18, 14};  // text length of each child
  std::vector<base::win::ScopedVariant> children =
      GetAllAccessibleChildren(node);
  for (size_t i = 0; i < children.size(); ++i) {
    Microsoft::WRL::ComPtr<IAccessible> child_accessible(
        GetAccessibleFromVariant(node, children[i].AsInput()));
    if (child_accessible) {
      auto* child = static_cast<ui::AXPlatformNodeWin*>(
          ui::AXPlatformNode::FromNativeViewAccessible(child_accessible.Get()));
      LONG start_offset = 0;
      LONG end_offset = 0;
      HRESULT hr = child->get_selection(0, &start_offset, &end_offset);
      EXPECT_EQ(S_OK, hr);
      EXPECT_EQ(0, start_offset);
      EXPECT_EQ(expected[i], end_offset);
    }
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestSetCurrentValue) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <input type="range" min=1 max=10 value=7>
      </body>
      </html>)HTML");

  // Retrieve the IAccessible interface for the document node.
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());

  // The document should have one child, a slider.
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());
  ASSERT_EQ(1u, document_children.size());
  Microsoft::WRL::ComPtr<IAccessible2> section;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &section));
  std::vector<base::win::ScopedVariant> section_children =
      GetAllAccessibleChildren(section.Get());
  Microsoft::WRL::ComPtr<IAccessible2> slider;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(section.Get(), section_children[0].AsInput())
          .Get(),
      &slider));
  LONG slider_role = 0;
  ASSERT_HRESULT_SUCCEEDED(slider->role(&slider_role));
  ASSERT_EQ(ROLE_SYSTEM_SLIDER, slider_role);
  Microsoft::WRL::ComPtr<IAccessibleValue> slider_iavalue;
  slider.As(&slider_iavalue);
  base::win::ScopedVariant slider_value;
  slider_iavalue->get_currentValue(slider_value.Receive());
  EXPECT_EQ(VT_R8, slider_value.type());
  EXPECT_DOUBLE_EQ(7.0, V_R8(slider_value.ptr()));

  // Call setCurrentValue on the slider, wait for the value changed event.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::RANGE_VALUE_CHANGED);
  base::win::ScopedVariant new_value(5.0);
  ASSERT_HRESULT_SUCCEEDED(slider_iavalue->setCurrentValue(new_value));
  ASSERT_TRUE(waiter.WaitForNotification());

  // The value should now be 5.
  slider_iavalue->get_currentValue(slider_value.Receive());
  EXPECT_EQ(VT_R8, slider_value.type());
  EXPECT_DOUBLE_EQ(5.0, V_R8(slider_value.ptr()));
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, FixedRuntimeId) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <p id="target">foo</p>
      <div id="newParent">bar</div>
      )HTML");

  ui::BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kStaticText, "foo");
  Microsoft::WRL::ComPtr<IRawElementProviderFragment> target_as_fragment;
  EXPECT_HRESULT_SUCCEEDED(target->GetNativeViewAccessible()->QueryInterface(
      IID_PPV_ARGS(&target_as_fragment)));

  base::win::ScopedSafearray original_runtime_id;
  EXPECT_HRESULT_SUCCEEDED(
      target_as_fragment->GetRuntimeId(original_runtime_id.Receive()));

  // First verify that the ids of 'target' and 'newParent' are in fact
  // different.
  ui::BrowserAccessibility* new_parent =
      FindNode(ax::mojom::Role::kStaticText, "bar");
  Microsoft::WRL::ComPtr<IRawElementProviderFragment> new_parent_as_fragment;
  EXPECT_HRESULT_SUCCEEDED(
      new_parent->GetNativeViewAccessible()->QueryInterface(
          IID_PPV_ARGS(&new_parent_as_fragment)));
  base::win::ScopedSafearray new_parent_runtime_id;
  EXPECT_HRESULT_SUCCEEDED(
      new_parent_as_fragment->GetRuntimeId(new_parent_runtime_id.Receive()));

  Microsoft::WRL::ComPtr<IUIAutomation> ui_automation;
  EXPECT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, NULL,
                                            CLSCTX_INPROC_SERVER,
                                            IID_PPV_ARGS(&ui_automation)));
  BOOL are_same;
  EXPECT_HRESULT_SUCCEEDED(ui_automation->CompareRuntimeIds(
      original_runtime_id.Get(), new_parent_runtime_id.Get(), &are_same));
  EXPECT_FALSE(are_same);

  ExecuteScript(
      u"let target = document.getElementById('target');"
      u"let parent = document.getElementById('newParent');"
      u"parent.appendChild(target);");

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  ASSERT_TRUE(waiter.WaitForNotification());

  target = FindNode(ax::mojom::Role::kStaticText, "foo");
  EXPECT_HRESULT_SUCCEEDED(target->GetNativeViewAccessible()->QueryInterface(
      IID_PPV_ARGS(&target_as_fragment)));

  base::win::ScopedSafearray new_runtime_id;
  EXPECT_HRESULT_SUCCEEDED(
      target_as_fragment->GetRuntimeId(new_runtime_id.Receive()));

  EXPECT_HRESULT_SUCCEEDED(ui_automation->CompareRuntimeIds(
      original_runtime_id.Get(), new_runtime_id.Get(), &are_same));
  EXPECT_TRUE(are_same);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, UniqueIdIsStable) {
  LoadInitialAccessibilityTreeFromHtml("<h1>Hello</h1>");

  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  std::vector<base::win::ScopedVariant> document_children =
      GetAllAccessibleChildren(document.Get());

  // Retrieve heading's IA2 element and unique id.
  Microsoft::WRL::ComPtr<IAccessible2> heading;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &heading));
  LONG heading_role = 0;
  ASSERT_HRESULT_SUCCEEDED(heading->role(&heading_role));
  ASSERT_EQ(IA2_ROLE_HEADING, heading_role);
  LONG heading_unique_id;
  heading->get_uniqueID(&heading_unique_id);

  // Change the heading to a group. This will cause it to get a new AXObject on
  // the renderer side, but the id will remain the same.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::ROLE_CHANGED);
  ExecuteScript(u"document.querySelector('h1').setAttribute('role', 'group');");
  ASSERT_TRUE(waiter.WaitForNotification());

  // Retrieve group's IA2 element and unique id.
  Microsoft::WRL::ComPtr<IAccessible2> group;
  ASSERT_HRESULT_SUCCEEDED(QueryIAccessible2(
      GetAccessibleFromVariant(document.Get(), document_children[0].AsInput())
          .Get(),
      &group));
  LONG group_role = 0;
  ASSERT_HRESULT_SUCCEEDED(group->role(&group_role));
  ASSERT_EQ(ROLE_SYSTEM_GROUPING, group_role);
  LONG group_unique_id;
  group->get_uniqueID(&group_unique_id);

  // The outgoing id assigned on the browser side, which is unique within the
  // window, remains the same.
  ASSERT_EQ(heading_unique_id, group_unique_id);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       UniqueIdIsStableAfterReset) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(R"HTML(
      data:text/html,<!DOCTYPE html>
      <html>
      <body>
        <button>Button 1</button>
        <iframe srcdoc="
          <!DOCTYPE html>
          <html>
          <body>
            <button>Button 2</button>
          </body>
          </html>
        "></iframe>
        <button>Button 3</button>
      </body>
      </html>)HTML")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  BrowserAccessibilityState::GetInstance()->ResetAccessibilityMode();
  auto accessibility_mode = web_contents->GetAccessibilityMode();
  ASSERT_TRUE(accessibility_mode.is_mode_off());
  EXPECT_EQ(nullptr, GetManager());

  // Turn accessibility on.
  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::kAXModeComplete);
  ASSERT_TRUE(waiter.WaitForNotification());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Button 2");

  // Save unique ids.
  accessibility_mode = web_contents->GetAccessibilityMode();
  ASSERT_TRUE(accessibility_mode.has_mode(ui::AXMode::kNativeAPIs));
  ASSERT_TRUE(accessibility_mode.has_mode(ui::AXMode::kWebContents));
  EXPECT_NE(nullptr, GetManager());
  const ui::BrowserAccessibility* button_1 =
      FindNode(ax::mojom::Role::kButton, "Button 1");
  ASSERT_NE(nullptr, button_1);
  const ui::BrowserAccessibility* button_2 =
      FindNode(ax::mojom::Role::kButton, "Button 2");
  ASSERT_NE(nullptr, button_2);
  int32_t unique_id_1 = button_1->GetAXPlatformNode()->GetUniqueId();
  int32_t unique_id_2 = button_2->GetAXPlatformNode()->GetUniqueId();

  // Turn accessibility off again.
  BrowserAccessibilityState::GetInstance()->ResetAccessibilityMode();
  accessibility_mode = web_contents->GetAccessibilityMode();
  ASSERT_TRUE(accessibility_mode.is_mode_off());
  EXPECT_EQ(nullptr, GetManager());

  // Turn accessibility on again.
  AccessibilityNotificationWaiter waiter_3(web_contents);
  BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::kAXModeBasic);
  ASSERT_TRUE(waiter_3.WaitForNotification());
  WaitForAccessibilityTreeToContainNodeWithName(web_contents, "Button 2");

  // Compare unique id for newly created a11y nodes with previous unique ids.
  accessibility_mode = web_contents->GetAccessibilityMode();
  ASSERT_TRUE(accessibility_mode.has_mode(ui::AXMode::kNativeAPIs));
  ASSERT_TRUE(accessibility_mode.has_mode(ui::AXMode::kWebContents));
  EXPECT_NE(nullptr, GetManager());
  const ui::BrowserAccessibility* button_1_refresh =
      FindNode(ax::mojom::Role::kButton, "Button 1");
  ASSERT_NE(nullptr, button_1_refresh);
  // button_1 is now a dangling pointer for the old button.
  // The pointers are not the same, proving that button_1_refresh is new.
  ASSERT_NE(button_1, button_1_refresh);
  const ui::BrowserAccessibility* button_2_refresh =
      FindNode(ax::mojom::Role::kButton, "Button 2");
  ASSERT_NE(nullptr, button_2_refresh);
  // button_2 is now a dangling pointer for the old button.
  // The pointers are not the same, proving that button_2_refresh is new.
  ASSERT_NE(button_2, button_2_refresh);

  // Test platform node ids have remained the same.
  EXPECT_EQ(unique_id_1, button_1_refresh->GetAXPlatformNode()->GetUniqueId());
  EXPECT_EQ(unique_id_2, button_2_refresh->GetAXPlatformNode()->GetUniqueId());

  // Test get_accChild with the IA2 unique ids.
  Microsoft::WRL::ComPtr<IAccessible> document(GetRendererAccessible());
  base::win::ScopedVariant button_variant_1(-unique_id_1);
  Microsoft::WRL::ComPtr<IDispatch> dispatch_button_1;
  ASSERT_HRESULT_SUCCEEDED(
      document->get_accChild(button_variant_1, &dispatch_button_1));
  Microsoft::WRL::ComPtr<IAccessible2> ia2_button_1;
  ASSERT_HRESULT_SUCCEEDED(
      dispatch_button_1->QueryInterface(IID_PPV_ARGS(&ia2_button_1)));
  LONG role_button_1 = 0;
  ASSERT_HRESULT_SUCCEEDED(ia2_button_1->role(&role_button_1));
  EXPECT_EQ(role_button_1, ROLE_SYSTEM_PUSHBUTTON);

  base::win::ScopedVariant button_variant_2(-unique_id_2);
  Microsoft::WRL::ComPtr<IDispatch> dispatch_button_2;
  ASSERT_HRESULT_SUCCEEDED(
      document->get_accChild(button_variant_2, &dispatch_button_2));
  Microsoft::WRL::ComPtr<IAccessible2> ia2_button_2;
  ASSERT_HRESULT_SUCCEEDED(
      dispatch_button_2->QueryInterface(IID_PPV_ARGS(&ia2_button_2)));
  LONG role_button_2 = 0;
  ASSERT_HRESULT_SUCCEEDED(ia2_button_2->role(&role_button_2));
  EXPECT_EQ(role_button_2, ROLE_SYSTEM_PUSHBUTTON);
}

}  // namespace content
