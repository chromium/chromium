// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_content_browsertest.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

using Microsoft::WRL::ComPtr;

namespace content {

#define EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(safearray, expected_property_values) \
  {                                                                         \
    EXPECT_EQ(sizeof(V_R8(LPVARIANT(NULL))),                                \
              ::SafeArrayGetElemsize(safearray));                           \
    ASSERT_EQ(1u, SafeArrayGetDim(safearray));                              \
    LONG array_lower_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetLBound(safearray, 1, &array_lower_bound));              \
    LONG array_upper_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetUBound(safearray, 1, &array_upper_bound));              \
    double* array_data;                                                     \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                         \
        safearray, reinterpret_cast<void**>(&array_data)));                 \
    size_t count = array_upper_bound - array_lower_bound + 1;               \
    ASSERT_EQ(expected_property_values.size(), count);                      \
    for (size_t i = 0; i < count; ++i) {                                    \
      EXPECT_EQ(array_data[i], expected_property_values[i]);                \
    }                                                                       \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(safearray));           \
  }

#define EXPECT_UIA_TEXTRANGE_EQ(provider, expected_content) \
  {                                                         \
    base::win::ScopedBstr provider_content;                 \
    ASSERT_HRESULT_SUCCEEDED(                               \
        provider->GetText(-1, provider_content.Receive())); \
    EXPECT_STREQ(expected_content, provider_content.Get()); \
  }

#define EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider, endpoint, unit,  \
                                         count, expected_text, expected_count) \
  {                                                                            \
    int result_count;                                                          \
    EXPECT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(          \
        endpoint, unit, count, &result_count));                                \
    EXPECT_EQ(expected_count, result_count);                                   \
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, expected_text);               \
  }

#define EXPECT_UIA_MOVE(text_range_provider, unit, count, expected_text, \
                        expected_count)                                  \
  {                                                                      \
    int result_count;                                                    \
    EXPECT_HRESULT_SUCCEEDED(                                            \
        text_range_provider->Move(unit, count, &result_count));          \
    EXPECT_EQ(expected_count, result_count);                             \
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, expected_text);         \
  }

class AXPlatformNodeTextRangeProviderWinBrowserTest
    : public AccessibilityContentBrowserTest {
 protected:
  const base::string16 kEmbeddedCharacterAsString = {
      ui::AXPlatformNodeBase::kEmbeddedCharacter};

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void SynchronizeThreads() {
    MainThreadFrameObserver observer(GetWidgetHost());
    observer.Wait();
  }

  BrowserAccessibilityManager* GetManager() const {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

  void GetTextRangeProviderFromTextNode(
      const BrowserAccessibility& target_node,
      ITextRangeProvider** text_range_provider) {
    BrowserAccessibilityComWin* target_node_com =
        ToBrowserAccessibilityWin(&target_node)->GetCOM();
    ASSERT_NE(nullptr, target_node_com);

    ComPtr<ITextProvider> text_provider;
    ASSERT_HRESULT_SUCCEEDED(
        target_node_com->GetPatternProvider(UIA_TextPatternId, &text_provider));
    ASSERT_NE(nullptr, text_provider.Get());

    ASSERT_HRESULT_SUCCEEDED(
        text_provider->get_DocumentRange(text_range_provider));
  }

  void GetDocumentRangeForMarkup(const std::string& html_markup,
                                 ITextRangeProvider** text_range_provider) {
    LoadInitialAccessibilityTreeFromHtml(html_markup);
    GetTextRangeProviderFromTextNode(*GetManager()->GetRoot(),
                                     text_range_provider);
  }

  // Run through ITextRangeProvider::ScrollIntoView top tests. It's assumed that
  // the browser has already loaded an HTML document's accessibility tree.
  // Assert the text range generated for an accessibility node is scrolled to be
  // flush with the top of the viewport.
  //   expected_start_role: the expected accessibility role of the text range
  //                        start node under test
  //   fstart:              the function to retrieve the accessibility text
  //                        range start node under test from the root
  //                        accessibility node
  //   expected_end_role:   the expected accessibility role of the text range
  //                        end node under test
  //   fend:                the function to retrieve the accessibility text
  //                        range end node under test from the root
  //                        accessibility node
  //   align_to_top:        true to test top viewport alignment, otherwise test
  //                        bottom viewport alignment
  void ScrollIntoViewBrowserTestTemplate(
      const ax::mojom::Role expected_start_role,
      BrowserAccessibility* (BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_end_role,
      BrowserAccessibility* (BrowserAccessibility::*fend)() const,
      const bool align_to_top) {
    BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();

    BrowserAccessibility* browser_accessibility_start =
        (root_browser_accessibility->*fstart)();
    ASSERT_NE(nullptr, browser_accessibility_start);
    ASSERT_EQ(expected_start_role, browser_accessibility_start->GetRole());

    BrowserAccessibility* browser_accessibility_end =
        (root_browser_accessibility->*fend)();
    ASSERT_NE(nullptr, browser_accessibility_end);
    ASSERT_EQ(expected_end_role, browser_accessibility_end->GetRole());

    AssertScrollIntoView(root_browser_accessibility,
                         browser_accessibility_start, browser_accessibility_end,
                         align_to_top);
  }

  // Run through ITextRangeProvider::ScrollIntoView top tests. It's assumed that
  // the browser has already loaded an HTML document's accessibility tree.
  // Assert the text range generated for an accessibility node is scrolled to be
  // flush with the top of the viewport.
  //   expected_start_role: the expected accessibility role of the text range
  //                        start node under test
  //   fstart:              the function to retrieve the accessibility text
  //                        range start node under test from the root
  //                        accessibility node
  //   fstart_arg:          an index argument for fstart
  //   expected_end_role:   the expected accessibility role of the text range
  //                        end node under test
  //   fend:                the function to retrieve the accessibility text
  //                        range end node under test from the root
  //                        accessibility node
  //   fend_arg:            an index argument for fend
  //   align_to_top:        true to test top viewport alignment, otherwise test
  //                        bottom viewport alignment
  void ScrollIntoViewBrowserTestTemplate(
      const ax::mojom::Role expected_start_role,
      BrowserAccessibility* (BrowserAccessibility::*fstart)(uint32_t) const,
      const uint32_t fstart_arg,
      const ax::mojom::Role expected_end_role,
      BrowserAccessibility* (BrowserAccessibility::*fend)(uint32_t) const,
      const uint32_t fend_arg,
      const bool align_to_top) {
    BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();

    BrowserAccessibility* browser_accessibility_start =
        (root_browser_accessibility->*fstart)(fstart_arg);
    ASSERT_NE(nullptr, browser_accessibility_start);
    ASSERT_EQ(expected_start_role, browser_accessibility_start->GetRole());

    BrowserAccessibility* browser_accessibility_end =
        (root_browser_accessibility->*fend)(fend_arg);
    ASSERT_NE(nullptr, browser_accessibility_end);
    ASSERT_EQ(expected_end_role, browser_accessibility_end->GetRole());

    AssertScrollIntoView(root_browser_accessibility,
                         browser_accessibility_start, browser_accessibility_end,
                         align_to_top);
  }

  void ScrollIntoViewFromIframeBrowserTestTemplate(
      const ax::mojom::Role expected_start_role,
      BrowserAccessibility* (BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_end_role,
      BrowserAccessibility* (BrowserAccessibility::*fend)() const,
      const bool align_to_top) {
    BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();
    BrowserAccessibility* leaf_iframe_browser_accessibility =
        root_browser_accessibility->InternalDeepestLastChild();
    ASSERT_NE(nullptr, leaf_iframe_browser_accessibility);
    ASSERT_EQ(ax::mojom::Role::kIframe,
              leaf_iframe_browser_accessibility->GetRole());

    AXTreeID iframe_tree_id = AXTreeID::FromString(
        leaf_iframe_browser_accessibility->GetStringAttribute(
            ax::mojom::StringAttribute::kChildTreeId));
    BrowserAccessibilityManager* iframe_browser_accessibility_manager =
        BrowserAccessibilityManager::FromID(iframe_tree_id);
    ASSERT_NE(nullptr, iframe_browser_accessibility_manager);
    BrowserAccessibility* root_iframe_browser_accessibility =
        iframe_browser_accessibility_manager->GetRoot();
    ASSERT_NE(nullptr, root_iframe_browser_accessibility);
    ASSERT_EQ(ax::mojom::Role::kRootWebArea,
              root_iframe_browser_accessibility->GetRole());

    BrowserAccessibility* browser_accessibility_start =
        (root_iframe_browser_accessibility->*fstart)();
    ASSERT_NE(nullptr, browser_accessibility_start);
    ASSERT_EQ(expected_start_role, browser_accessibility_start->GetRole());

    BrowserAccessibility* browser_accessibility_end =
        (root_iframe_browser_accessibility->*fend)();
    ASSERT_NE(nullptr, browser_accessibility_end);
    ASSERT_EQ(expected_end_role, browser_accessibility_end->GetRole());

    AssertScrollIntoView(root_iframe_browser_accessibility,
                         browser_accessibility_start, browser_accessibility_end,
                         align_to_top);
  }

  void AssertScrollIntoView(BrowserAccessibility* root_browser_accessibility,
                            BrowserAccessibility* browser_accessibility_start,
                            BrowserAccessibility* browser_accessibility_end,
                            const bool align_to_top) {
    ui::AXNodePosition::AXPositionInstance start =
        browser_accessibility_start->CreateTextPositionAt(0);
    ui::AXNodePosition::AXPositionInstance end =
        browser_accessibility_end->CreateTextPositionAt(0)
            ->CreatePositionAtEndOfAnchor();

    BrowserAccessibilityComWin* start_browser_accessibility_com_win =
        ToBrowserAccessibilityWin(browser_accessibility_start)->GetCOM();
    ASSERT_NE(nullptr, start_browser_accessibility_com_win);

    ComPtr<ITextRangeProvider> text_range_provider =
        ui::AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
            start_browser_accessibility_com_win, std::move(start),
            std::move(end));
    ASSERT_NE(nullptr, text_range_provider);

    gfx::Rect previous_range_bounds =
        align_to_top ? browser_accessibility_start->GetBoundsRect(
                           ui::AXCoordinateSystem::kFrame,
                           ui::AXClippingBehavior::kUnclipped)
                     : browser_accessibility_end->GetBoundsRect(
                           ui::AXCoordinateSystem::kFrame,
                           ui::AXClippingBehavior::kUnclipped);

    AccessibilityNotificationWaiter location_changed_waiter(
        GetWebContentsAndAssertNonNull(), ui::kAXModeComplete,
        ax::mojom::Event::kLocationChanged);
    ASSERT_HRESULT_SUCCEEDED(text_range_provider->ScrollIntoView(align_to_top));
    location_changed_waiter.WaitForNotification();

    gfx::Rect root_page_bounds = root_browser_accessibility->GetBoundsRect(
        ui::AXCoordinateSystem::kFrame, ui::AXClippingBehavior::kUnclipped);
    if (align_to_top) {
      gfx::Rect range_bounds = browser_accessibility_start->GetBoundsRect(
          ui::AXCoordinateSystem::kFrame, ui::AXClippingBehavior::kUnclipped);
      ASSERT_NE(previous_range_bounds.y(), range_bounds.y());
      ASSERT_NEAR(root_page_bounds.y(), range_bounds.y(), 1);
    } else {
      gfx::Rect range_bounds = browser_accessibility_end->GetBoundsRect(
          ui::AXCoordinateSystem::kFrame, ui::AXClippingBehavior::kUnclipped);
      gfx::Size viewport_size =
          gfx::Size(root_page_bounds.width(), root_page_bounds.height());
      ASSERT_NE(previous_range_bounds.y(), range_bounds.y());
      ASSERT_NEAR(root_page_bounds.y() + viewport_size.height(),
                  range_bounds.y() + range_bounds.height(), 1);
    }
  }

  void ScrollIntoViewTopBrowserTestTemplate(
      const ax::mojom::Role expected_role,
      BrowserAccessibility* (BrowserAccessibility::*f)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role, f, expected_role, f, true);
  }

  void ScrollIntoViewTopBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      BrowserAccessibility* (BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_role_end,
      BrowserAccessibility* (BrowserAccessibility::*fend)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart,
                                      expected_role_end, fend, true);
  }

  void ScrollIntoViewTopBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      BrowserAccessibility* (BrowserAccessibility::*fstart)(uint32_t) const,
      const uint32_t fstart_arg,
      const ax::mojom::Role expected_role_end,
      BrowserAccessibility* (BrowserAccessibility::*fend)(uint32_t) const,
      const uint32_t fend_arg) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart, fstart_arg,
                                      expected_role_end, fend, fend_arg, true);
  }

  void ScrollIntoViewBottomBrowserTestTemplate(
      const ax::mojom::Role expected_role,
      BrowserAccessibility* (BrowserAccessibility::*f)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role, f, expected_role, f,
                                      false);
  }

  void ScrollIntoViewBottomBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      BrowserAccessibility* (BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_role_end,
      BrowserAccessibility* (BrowserAccessibility::*fend)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart,
                                      expected_role_end, fend, false);
  }

  void ScrollIntoViewBottomBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      BrowserAccessibility* (BrowserAccessibility::*fstart)(uint32_t) const,
      const uint32_t fstart_arg,
      const ax::mojom::Role expected_role_end,
      BrowserAccessibility* (BrowserAccessibility::*fend)(uint32_t) const,
      const uint32_t fend_arg) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart, fstart_arg,
                                      expected_role_end, fend, fend_arg, false);
  }

  void AssertMoveByUnitForMarkup(
      const TextUnit& unit,
      const std::string& html_markup,
      const std::vector<const wchar_t*>& expected_text) {
    ComPtr<ITextRangeProvider> text_range;

    GetDocumentRangeForMarkup(html_markup, &text_range);
    ASSERT_NE(nullptr, text_range.Get());
    text_range->ExpandToEnclosingUnit(unit);

    size_t index = 0;
    int count_moved = 1;
    while (count_moved == 1 && index < expected_text.size()) {
      EXPECT_UIA_TEXTRANGE_EQ(text_range, expected_text[index++]);
      ASSERT_HRESULT_SUCCEEDED(text_range->Move(unit, 1, &count_moved));
    }
    EXPECT_EQ(expected_text.size(), index);
    EXPECT_EQ(0, count_moved);

    count_moved = -1;
    index = expected_text.size();
    while (count_moved == -1 && index > 0) {
      EXPECT_UIA_TEXTRANGE_EQ(text_range, expected_text[--index]);
      ASSERT_HRESULT_SUCCEEDED(text_range->Move(unit, -1, &count_moved));
    }
    EXPECT_EQ(0, count_moved);
    EXPECT_EQ(0u, index);
  }
};

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetAttributeValue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div style="font-size: 16pt">
            <span style="font-size: 12pt">Text1</span>
            Text2
          </div>
        </body>
      </html>
  )HTML"));

  ComPtr<IUnknown> mix_attribute_value;
  EXPECT_HRESULT_SUCCEEDED(
      UiaGetReservedMixedAttributeValue(&mix_attribute_value));

  auto* node = FindNode(ax::mojom::Role::kStaticText, "Text1");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Text1");

  base::win::ScopedVariant value;
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_FontSizeAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_R8);
  EXPECT_EQ(V_R8(value.ptr()), 12.0);

  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Paragraph));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Text1 Text2");

  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_FontSizeAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_UNKNOWN);
  EXPECT_EQ(V_UNKNOWN(value.ptr()), mix_attribute_value.Get())
      << "expected 'mixed attribute value' interface pointer";
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetAttributeValueIsReadonlyEmptyTextInputs) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <input type='text' aria-label='input_text'>
          <input type='search' aria-label='input_search'>
        </body>
      </html>
  )HTML"));

  auto* input_text_node = FindNode(ax::mojom::Role::kTextField, "input_text");
  ASSERT_NE(nullptr, input_text_node);
  EXPECT_TRUE(input_text_node->PlatformIsLeaf());
  EXPECT_EQ(0u, input_text_node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*input_text_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                          kEmbeddedCharacterAsString.c_str());

  base::win::ScopedVariant value;
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_FALSE);
  text_range_provider.Reset();
  value.Reset();

  auto* input_search_node =
      FindNode(ax::mojom::Role::kSearchBox, "input_search");
  ASSERT_NE(nullptr, input_search_node);
  EXPECT_TRUE(input_search_node->PlatformIsLeaf());
  EXPECT_EQ(0u, input_search_node->PlatformChildCount());

  GetTextRangeProviderFromTextNode(*input_search_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                          kEmbeddedCharacterAsString.c_str());

  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_FALSE);
  text_range_provider.Reset();
  value.Reset();
}

// With a rich text field, the read-only attribute should be determined based on
// the editable root node's editable state.
IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetAttributeValueIsReadonlyRichTextField) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <style>
          .myDiv::before {
              content: attr(data-placeholder);
              pointer-events: none;
          }
        </style>
        <body>
          <div contenteditable="true" data-placeholder="@mention or comment"
          role="textbox" aria-readonly="false" aria-label="text_field"
          class="myDiv"><p>3.14</p></div>
        </body>
      </html>
  )HTML"));

  auto* text_field_node = FindNode(ax::mojom::Role::kTextField, "text_field");
  ASSERT_NE(nullptr, text_field_node);
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*text_field_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());

  base::win::ScopedVariant value;
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_FALSE);
  text_range_provider.Reset();
  value.Reset();
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       DoNotNormalizeRangeWithVisibleCaretOrSelection) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div aria-value="wrapper">
            <input type='text' aria-label='input_text'><span
              style="font-size: 12pt">Text1</span>
          </div>
          <div contenteditable="true">
            <ul><li>item</li></ul>3.14
          </div>
        </body>
      </html>
  )HTML"));

  // Case 1: Inside of a plain text field, NormalizeTextRange shouldn't modify
  //         the text range endpoints.
  //
  // In order for the test harness to effectively simulate typing in a text
  // input, first change the value of the text input and then focus it. Only
  // editing the value won't show the cursor and only focusing will put the
  // cursor at the beginning of the text input, so both steps are necessary.
  auto* input_text_node = FindNode(ax::mojom::Role::kTextField, "input_text");
  ASSERT_NE(nullptr, input_text_node);
  EXPECT_TRUE(input_text_node->PlatformIsLeaf());
  EXPECT_EQ(0u, input_text_node->PlatformChildCount());

  AccessibilityNotificationWaiter edit_waiter(shell()->web_contents(),
                                              ui::kAXModeComplete,
                                              ax::mojom::Event::kValueChanged);
  ui::AXActionData edit_data;
  edit_data.target_node_id = input_text_node->GetId();
  edit_data.action = ax::mojom::Action::kSetValue;
  edit_data.value = "test";
  input_text_node->AccessibilityPerformAction(edit_data);
  edit_waiter.WaitForNotification();

  AccessibilityNotificationWaiter focus_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  ui::AXActionData focus_data;
  focus_data.target_node_id = input_text_node->GetId();
  focus_data.action = ax::mojom::Action::kFocus;
  input_text_node->AccessibilityPerformAction(focus_data);
  focus_waiter.WaitForNotification();

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*input_text_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"test");

  // Move the first position so that both endpoints are at the end of the text
  // input. This is where calls to NormalizeTextRange can be problematic.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 4,
      /*expected_text*/ L"",
      /*expected_count*/ 4);

  // Clone the original text range so we can keep track if NormalizeTextRange
  // causes a change in position.
  ComPtr<ITextRangeProvider> text_range_provider_clone;
  text_range_provider->Clone(&text_range_provider_clone);

  // Since both ranges are identical, the result of CompareEndpoints should be
  // 0.
  int result = 0;
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_End, text_range_provider_clone.Get(),
      TextPatternRangeEndpoint_Start, &result));
  ASSERT_EQ(0, result);

  // Calling GetAttributeValue will call NormalizeTextRange, which shouldn't
  // change the result of CompareEndpoints below since the range is inside a
  // plain text field.
  base::win::ScopedVariant value;
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_FALSE);
  value.Reset();

  EXPECT_HRESULT_SUCCEEDED(text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_End, text_range_provider_clone.Get(),
      TextPatternRangeEndpoint_Start, &result));
  ASSERT_EQ(0, result);

  // Case 2: Inside of a rich text field, NormalizeTextRange should modify the
  //         text range endpoints.
  auto* node = FindNode(ax::mojom::Role::kStaticText, "item");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"item");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 4,
      /*expected_text*/ L"",
      /*expected_count*/ 4);
  // Make the range degenerate.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ 1,
      /*expected_text*/ L"\n3",
      /*expected_count*/ 1);

  // The range should now span two nodes: start: "item<>", end: "<3>.14".
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"\n3");

  // Clone the original text range so we can keep track if NormalizeTextRange
  // causes a change in position.
  text_range_provider->Clone(&text_range_provider_clone);

  // Calling GetAttributeValue will call NormalizeTextRange, which should
  // change the result of CompareEndpoints below since we are in a rich text
  // field.
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_FALSE);
  value.Reset();

  // Since text_range_provider has been modified by NormalizeTextRange, we
  // expect a difference here.
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->CompareEndpoints(
      TextPatternRangeEndpoint_End, text_range_provider_clone.Get(),
      TextPatternRangeEndpoint_Start, &result));
  ASSERT_EQ(1, result);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       TextInputWithNewline) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div aria-value='wrapper'>
            <input type='text' aria-label='input_text'><br>
          </div>
        </body>
      </html>
  )HTML"));

  // This test validates an important scenario for editing. UIA clients such as
  // Narrator expect newlines to be contained within their adjacent nodes.
  // This test validates this scenario for GetEnclosingElement and
  // GetAttributeValue, both of which are essential for text editing scenarios.
  //
  // In order for the test harness to effectively simulate typing in a text
  // input, first change the value of the text input and then focus it. Only
  // editing the value won't show the cursor and only focusing will put the
  // cursor at the beginning of the text input, so both steps are necessary.
  auto* input_text_node = FindNode(ax::mojom::Role::kTextField, "input_text");
  ASSERT_NE(nullptr, input_text_node);
  EXPECT_TRUE(input_text_node->PlatformIsLeaf());
  EXPECT_EQ(0u, input_text_node->PlatformChildCount());

  AccessibilityNotificationWaiter edit_waiter(shell()->web_contents(),
                                              ui::kAXModeComplete,
                                              ax::mojom::Event::kValueChanged);
  ui::AXActionData edit_data;
  edit_data.target_node_id = input_text_node->GetId();
  edit_data.action = ax::mojom::Action::kSetValue;
  edit_data.value = "test";
  input_text_node->AccessibilityPerformAction(edit_data);
  edit_waiter.WaitForNotification();

  AccessibilityNotificationWaiter focus_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  ui::AXActionData focus_data;
  focus_data.target_node_id = input_text_node->GetId();
  focus_data.action = ax::mojom::Action::kFocus;
  input_text_node->AccessibilityPerformAction(focus_data);
  focus_waiter.WaitForNotification();

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*input_text_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"test");

  // Move the first position so that both endpoints are at the end of the text
  // input.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 4,
      /*expected_text*/ L"",
      /*expected_count*/ 4);

  ComPtr<IRawElementProviderSimple> text_input_provider =
      QueryInterfaceFromNode<IRawElementProviderSimple>(input_text_node);

  // Validate that the enclosing element is the text input node and not the
  // parent node that includes the newline.
  ComPtr<IRawElementProviderSimple> enclosing_element;
  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->GetEnclosingElement(&enclosing_element));
  EXPECT_EQ(text_input_provider.Get(), enclosing_element.Get());

  // Calling GetAttributeValue on the editable text input should return false,
  // verifying that the read-only newline is not interfering.
  base::win::ScopedVariant value;
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_FALSE);
  value.Reset();
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetBoundingRectangles) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .break_word {
              width: 50px;
              word-wrap: break-word;
            }
          </style>
        </head>
        <body>
          <p class="break_word">AsdfAsdfAsdf</p>
        </body>
      </html>
  )HTML"));

  auto* node = FindNode(ax::mojom::Role::kStaticText, "AsdfAsdfAsdf");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"AsdfAsdfAsdf");

  base::win::ScopedSafearray rectangles;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));

  // |view_offset| is necessary to account for differences in the shell
  // between platforms (e.g. title bar height) because the results of
  // |GetBoundingRectangles| are in screen coordinates.
  gfx::Vector2d view_offset =
      node->manager()->GetViewBoundsInScreenCoordinates().OffsetFromOrigin();
  std::vector<double> expected_values = {
      8 + view_offset.x(), 16 + view_offset.y(), 49, 17,
      8 + view_offset.x(), 34 + view_offset.y(), 44, 17};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopStaticText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/text.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomStaticText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/text.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopEmbeddedText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomEmbeddedText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopEmbeddedTextCrossNode) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomEmbeddedTextCrossNode) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopTable) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kTable, &BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kTable, &BrowserAccessibility::PlatformGetChild, 0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomTable) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kTable, &BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kTable, &BrowserAccessibility::PlatformGetChild, 0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopTableText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomTableText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopLinkText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomLinkText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopLinkContainer) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewTopBrowserTestTemplate(ax::mojom::Role::kGenericContainer,
                                       &BrowserAccessibility::PlatformGetChild,
                                       0, ax::mojom::Role::kGenericContainer,
                                       &BrowserAccessibility::PlatformGetChild,
                                       0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomLinkContainer) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kGenericContainer,
      &BrowserAccessibility::PlatformGetChild, 0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopTextFromIFrame) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/iframe-text.html");
  WaitForAccessibilityTreeToContainNodeWithName(
      shell()->web_contents(),
      "Game theory is \"the study of Mathematical model mathematical models of "
      "conflict and cooperation between intelligent rational decision-makers."
      "\"");
  ScrollIntoViewFromIframeBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomTextFromIFrame) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/iframe-text.html");
  WaitForAccessibilityTreeToContainNodeWithName(
      shell()->web_contents(),
      "Game theory is \"the study of Mathematical model mathematical models of "
      "conflict and cooperation between intelligent rational decision-makers."
      "\"");
  ScrollIntoViewFromIframeBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByUnitFormat) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div>plain 1</div><div>plain 2</div>
        <div role="heading">plain heading</div>
        <div role="article" style="font-style: italic">italic 1</div>
        <div style="font-style: italic">italic 2</div>
        <h1>heading</h1>
        <h1>heading</h1>
        <div style="font-weight: bold">bold 1</div>
        <div style="font-weight: bold">bold 2</div>
      </body>
      </html>)HTML");
  auto* node = FindNode(ax::mojom::Role::kStaticText, "plain 1");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"plain 1");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/ L"plain 1\nplain 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/
      L"plain 1\nplain 2\nplain heading",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/
      L"plain 1\nplain 2\nplain heading\nitalic 1\nitalic 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/ L"plain 1\nplain 2\nplain heading\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/
      L"plain 1\nplain 2\nplain heading\nitalic 1\nitalic 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/
      L"plain 1\nplain 2\nplain heading\nitalic 1\nitalic 2\nheading",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/
      L"plain 1\nplain 2\nplain heading\nitalic 1\nitalic 2\nheading\nheading",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 5,
      /*expected_text*/
      L"plain 1\nplain 2\nplain heading\nitalic 1\nitalic 2"
      L"\nheading\nheading\nbold 1\nbold 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -8,
      /*expected_text*/ L"",
      /*expected_count*/ -6);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/ L"plain 1\nplain 2",
      /*expected_count*/ 1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByUnitFormatAllFormats) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div>plain 1</div><div>plain 2</div>
        <div style="background-color: red">background-color 1</div>
        <div style="background-color: red">background-color 2</div>
        <div style="color: blue">color 1</div>
        <div style="color: blue">color 2</div>
        <div style="text-decoration: overline">overline 1</div>
        <div style="text-decoration: overline">overline 2</div>
        <div style="text-decoration: line-through">line-through 1</div>
        <div style="text-decoration: line-through">line-through 2</div>
        <div style="vertical-align:super">sup 1</div>
        <div style="vertical-align:super">sup 2</div>
        <div style="font-weight: bold">bold 1</div>
        <div style="font-weight: bold">bold 2</div>
        <div style="font-family: sans-serif">font-family 1</div>
        <div style="font-family: sans-serif">font-family 2</div>
      </body>
      </html>)HTML");
  auto* node = FindNode(ax::mojom::Role::kStaticText, "plain 1");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"plain 1");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/ L"plain 1\nplain 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/ L"plain 1\nplain 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2\nfont-family 1\nfont-family 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2\n",
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByUnitParagraph) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <head><style>
        div {
          width: 100px;
        }
        code::before {
          content: "[";
        }
        code::after {
          content: "]";
        }
        b::before, i::after {
          width: 5px;
          height: 5px;
          content: "";
          display: block;
          background: black;
        }
      </style></head>
      <body>
        <div>start</div>
        <div>
          text with <code>:before</code>
          and <code>:after</code> content,
          then a <b>bold</b> element with a
          <code>block</code> before content
          then a <i>italic</i> element with
          a <code>block</code> after content
        </div>
        <div>end</div>
      </body>
      </html>)HTML");
  BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  std::vector<base::string16> paragraphs = {
      L"start",
      L"text with [:before] and [:after]content, then a",
      L"bold element with a [block]before content then a italic",
      L"element with a [block] after content",
      L"end",
  };

  // FORWARD NAVIGATION
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"start");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -2,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + L"\n" + paragraphs[2]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + L"\n" + paragraphs[3]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[3] + L"\n" + paragraphs[4]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 2,
      /*expected_text*/ L"",
      /*expected_count*/ 1);

  // REVERSE NAVIGATION
  GetTextRangeProviderFromTextNode(*end_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"end");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 2,
      /*expected_text*/ L"",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[3] + L"\n" + paragraphs[4]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + L"\n" + paragraphs[3]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + L"\n" + paragraphs[2]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -2,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeTextRangeProviderWinBrowserTest,
    MoveEndpointByUnitParagraphCollapseTrailingLineBreakingObjects) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div>start</div>
        <div>
          <div>some text</div>
          <div></div>
          <br>
          <span><br><div><br></div></span>
          <div>more text</div>
        </div>
        <div>end</div>
      </body>
      </html>)HTML");
  BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  std::vector<base::string16> paragraphs = {
      L"start",
      L"some text\n\n\n",
      L"more text",
      L"end",
  };

  // FORWARD NAVIGATION
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"start");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + L"\n" + paragraphs[3]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ 1);

  // REVERSE NAVIGATION
  GetTextRangeProviderFromTextNode(*end_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"end");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + L"\n" + paragraphs[3]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeTextRangeProviderWinBrowserTest,
    MoveEndpointByUnitParagraphCollapseConsecutiveParentChildLineBreakingObjects) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <head>
        <style>
          div {
            width: 100px;
          }

          code::before {
            content: "[";
          }

          code::after {
            content: "]";
          }

          /* This will create an empty anonymous layout block before the <b>
             element. */
          b::before {
            content: "";
            display: block;
          }
        </style>
      </head>

      <body>
        <div>start</div>
        <div>
          text with <code>:before</code>
          and <code>:after</code> content,
          then a
          <div>
            <b>bold</b> element
          </div>
        </div>
      </body>
      </html>)HTML");
  BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);

  std::vector<base::string16> paragraphs = {
      L"start",
      L"text with [:before] and [:after]content, then a",
      L"bold element",
  };

  // Forward navigation.
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"start");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/
      (paragraphs[0] + L"\n" + paragraphs[1] + L"\n" + paragraphs[2]).c_str(),
      /*expected_count*/ 1);

  // Reverse navigation.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ -1);

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByUnitParagraphPreservedWhiteSpace) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div>start</div>
        <span style='white-space: pre'>
          First Paragraph
          Second Paragraph
        </span>
        <!--
          Intentional nesting to test that ancestor positions can
          resolve to the newline at the start of preserved whitespace.
        -->
        <div>
          <div style='white-space: pre-line'>
            Third Paragraph
            Fourth Paragraph
          </div>
        </div>
        <div style='white-space: pre-wrap; width: 10em;'>
          Fifth               Paragraph
          Sixth               Paragraph
        </div>
        <div style='white-space: break-spaces; width: 10em;'>
          Seventh             Paragraph
          Eighth              Paragraph
        </div>
        <div>end</div>
      </body>
      </html>)HTML");
  BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  ComPtr<ITextRangeProvider> text_range_provider;

  std::vector<base::string16> paragraphs = {
      L"start\n",
      L"          First Paragraph\n",
      L"          Second Paragraph\n        \n",
      L"Third Paragraph\n",
      L"Fourth Paragraph\n\n          ",
      L"Fifth               Paragraph\n          ",
      L"Sixth               Paragraph\n        \n          ",
      L"Seventh             Paragraph\n          ",
      L"Eighth              Paragraph\n        ",
      L"end",
  };

  // FORWARD NAVIGATION
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"start");

  // The first paragraph extends beyond the end of the "start" node, because
  // the preserved whitespace node begins with a line break, so
  // move once to capture that.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + paragraphs[1]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + paragraphs[3]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[3] + paragraphs[4]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[4] + paragraphs[5]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[5].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[5] + paragraphs[6]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[6].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[6] + paragraphs[7]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[7].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[7] + paragraphs[8]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[8].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[8] + paragraphs[9]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[9].c_str(),
      /*expected_count*/ 1);

  // REVERSE NAVIGATION
  GetTextRangeProviderFromTextNode(*end_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"end");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[8] + paragraphs[9]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[8].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[7] + paragraphs[8]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[7].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[6] + paragraphs[7]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[6].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[5] + paragraphs[6]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[5].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[4] + paragraphs[5]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[3] + paragraphs[4]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + paragraphs[3]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + paragraphs[1]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveByUnitParagraphWithAriaHiddenNodes) {
  const std::string html_markup = R"HTML(<!DOCTYPE html>
  <html>
    <body>
      <div>start</div>
      <div>
        1. Paragraph with hidden <span aria-hidden="true">
          [IGNORED]
        </span> inline in between
      </div>
      <div>
        <span>2. Paragraph parts wrapped by</span> <span aria-hidden="true">
          [IGNORED]
        </span> <span>span with hidden inline in between</span>
      </div>
      <div>
        <span>3. Paragraph before hidden block</span><div aria-hidden="true">
          [IGNORED]
        </div><span>4. Paragraph after hidden block</span>
      </div>
      <div>
        <span aria-hidden="true">[IGNORED]</span><span>5. Paragraph with leading
        and trailing hidden span</span><span aria-hidden="true">[IGNORED]</span>
      </div>
      <div>
        <div aria-hidden="true">[IGNORED]</div><span>6. Paragraph with leading
        and trailing hidden block</span><div aria-hidden="true">[IGNORED]</div>
      </div>
      <div>end</div>
    </body>
  </html>)HTML";

  const std::vector<const wchar_t*> paragraphs = {
      L"start",
      L"1. Paragraph with hidden inline in between",
      L"2. Paragraph parts wrapped by span with hidden inline in between",
      L"3. Paragraph before hidden block",
      L"4. Paragraph after hidden block",
      L"5. Paragraph with leading and trailing hidden span",
      L"6. Paragraph with leading and trailing hidden block",
      L"end",
  };

  AssertMoveByUnitForMarkup(TextUnit_Paragraph, html_markup, paragraphs);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByUnitParagraphWithEmbeddedObject) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <head></head>
      <body>
        <span>start</span>
        <svg></svg>
        <span>end</span>
      </body>
      </html>)HTML");
  BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  std::vector<base::string16> paragraphs = {
      L"start",
      kEmbeddedCharacterAsString,
      L"end",
  };

  // FORWARD NAVIGATION
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"start");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -2,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + paragraphs[1]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 2,
      /*expected_text*/ L"",
      /*expected_count*/ 1);

  // REVERSE NAVIGATION
  GetTextRangeProviderFromTextNode(*end_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"end");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 2,
      /*expected_text*/ L"",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + paragraphs[1]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -2,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       IFrameTraversal) {
  LoadInitialAccessibilityTreeFromUrl(embedded_test_server()->GetURL(
      "/accessibility/html/iframe-cross-process.html"));

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  auto* node = FindNode(ax::mojom::Role::kStaticText, "After frame");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"After frame");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Word,
      /*count*/ -1,
      /*expected_text*/ L"iframe\nAfter frame",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Word,
      /*count*/ -2,
      /*expected_text*/ L"Text in iframe\nAfter frame",
      /*expected_count*/ -2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ -3,
                                   /*expected_text*/ L"Text in ",
                                   /*expected_count*/ -3);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ 2,
                                   /*expected_text*/ L"Text in iframe\nAfter ",
                                   /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Line,
      /*count*/ 1,
      /*expected_text*/ L"Text in iframe\nAfter frame",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Document,
      /*count*/ 1,
      /*expected_text*/ L"",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ -17,
      /*expected_text*/ L"iframe\nAfter frame",
      /*expected_count*/ -17);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Line,
                                   /*count*/ -1,
                                   /*expected_text*/ L"iframe",
                                   /*expected_count*/ -1);

  text_range_provider->ExpandToEnclosingUnit(TextUnit_Line);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Text in iframe");

  text_range_provider->ExpandToEnclosingUnit(TextUnit_Document);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                          L"Before frame\nText in iframe\nAfter frame");

  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Word,
                  /*count*/ 2,
                  /*expected_text*/ L"Text ",
                  /*expected_count*/ 2);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Word,
                  /*count*/ -1,
                  /*expected_text*/ L"frame",
                  /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ 1,
      /*expected_text*/ L"frame\nT",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 6,
                  /*expected_text*/ L"e",
                  /*expected_count*/ 6);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 19,
                  /*expected_text*/ L"f",
                  /*expected_count*/ 19);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ -7,
                  /*expected_text*/ L"e",
                  /*expected_count*/ -7);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Line,
                  /*count*/ 1,
                  /*expected_text*/ L"After frame",
                  /*expected_count*/ 1);

  text_range_provider->ExpandToEnclosingUnit(TextUnit_Document);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                          L"Before frame\nText in iframe\nAfter frame");
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       OutOfProcessIFrameTraversal) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/iframe-cross-process.html"));
  LoadInitialAccessibilityTreeFromUrl(main_url);

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  // Navigate oopif to URL.
  FrameTreeNode* iframe_node = root->child_at(0);
  GURL iframe_url(embedded_test_server()->GetURL(
      "b.com", "/accessibility/html/frame/static_text.html"));
  WebContentsImpl* iframe_web_contents =
      WebContentsImpl::FromFrameTreeNode(iframe_node);
  DCHECK(iframe_web_contents);
  {
    AccessibilityNotificationWaiter waiter(iframe_web_contents,
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLoadComplete);
    NavigateFrameToURL(iframe_node, iframe_url);
    waiter.WaitForNotification();
  }

  SynchronizeThreads();
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  WaitForHitTestData(iframe_node->current_frame_host());
  FrameTreeVisualizer visualizer;
  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      visualizer.DepictFrameTree(root));

  auto* node = FindNode(ax::mojom::Role::kStaticText, "After frame");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"After frame");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Word,
      /*count*/ -1,
      /*expected_text*/ L"iframe\nAfter frame",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Word,
      /*count*/ -2,
      /*expected_text*/ L"Text in iframe\nAfter frame",
      /*expected_count*/ -2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ -3,
                                   /*expected_text*/ L"Text in ",
                                   /*expected_count*/ -3);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ 2,
                                   /*expected_text*/ L"Text in iframe\nAfter ",
                                   /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Line,
      /*count*/ 1,
      /*expected_text*/ L"Text in iframe\nAfter frame",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Document,
      /*count*/ 1,
      /*expected_text*/ L"",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ -17,
      /*expected_text*/ L"iframe\nAfter frame",
      /*expected_count*/ -17);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Line,
                                   /*count*/ -1,
                                   /*expected_text*/ L"iframe",
                                   /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ExpandToEnclosingFormat) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div>plain</div>
        <div>text</div>
        <div style="font-style: italic">italic<div>
        <div style="font-style: italic">text<div>
      </body>
      </html>)HTML");

  auto* node = FindNode(ax::mojom::Role::kStaticText, "plain");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"plain");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 3,
      /*expected_text*/ L"in",
      /*expected_count*/ 3);

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Format));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"plain\ntext");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ 3,
      /*expected_text*/ L"plain\ntext\nita",
      /*expected_count*/ 3);

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 10,
      /*expected_text*/ L"ta",
      /*expected_count*/ 10);

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Format));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"italic\ntext");
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ExpandToEnclosingWordWhenBeforeFirstWordBoundary) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <p aria-label="space">&nbsp;</p>
        <p>3.14</p>
      </body>
      </html>)HTML");

  // Case 1: test on degenerate range before whitespace.
  auto* node = FindNode(ax::mojom::Role::kParagraph, "space")
                   ->PlatformDeepestFirstChild();
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"\xA0");

  // Make the range degenerate.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ -1,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Word));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"\xA0\n");

  // Case 2: test on range that includes the whitespace and the following word.
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"\xA0");
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ 1,
                                   /*expected_text*/ L"\xA0\n3.14",
                                   /*expected_count*/ 1);
  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Word));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"\xA0\n");

  // Case 3: test on degenerate range after whitespace.
  node = FindNode(ax::mojom::Role::kStaticText, "3.14");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"3.14");
  // Make the range degenerate.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ -1,
                                   /*expected_text*/ L"",
                                   /*expected_count*/ -1);
  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Word));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"3.14");
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       EntireMarkupSuccessiveMoveByCharacter) {
  AssertMoveByUnitForMarkup(
      TextUnit_Character, "Test ing.",
      {L"T", L"e", L"s", L"t", L" ", L"i", L"n", L"g", L"."});

  // The text consists of an e acute, and two emoticons.
  const std::string html = R"HTML(<!DOCTYPE html>
      <html>
        <body>
          <input type="text" value="">
          <script>
            document.querySelector('input').value = 'e\u0301' +
                '\uD83D\uDC69\u200D\u2764\uFE0F\u200D\uD83D\uDC69' +
                '\uD83D\uDC36';
          </script>
        </body>
      </html>)HTML";
  AssertMoveByUnitForMarkup(
      TextUnit_Character, html,
      {L"e\x0301", L"\xD83D\xDC69\x200D\x2764\xFE0F\x200D\xD83D\xDC69",
       L"\xD83D\xDC36"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       EntireMarkupSuccessiveMoveByWord) {
  AssertMoveByUnitForMarkup(TextUnit_Word, "This is a test.",
                            {L"This ", L"is ", L"a ", L"test", L"."});

  AssertMoveByUnitForMarkup(TextUnit_Word,
                            "    This    is      a      test.    ",
                            {L"This ", L"is ", L"a ", L"test", L"."});

  AssertMoveByUnitForMarkup(
      TextUnit_Word, "It said: to be continued...",
      {L"It ", L"said", L": ", L"to ", L"be ", L"continued", L"..."});

  AssertMoveByUnitForMarkup(TextUnit_Word,
                            "A <a>link with multiple words</a> and text after.",
                            {L"A ", L"link ", L"with ", L"multiple ", L"words",
                             L"and ", L"text ", L"after", L"."});

  AssertMoveByUnitForMarkup(TextUnit_Word,
                            "A <span aria-hidden='true'>span with ignored "
                            "text</span> and text after.",
                            {L"A ", L"and ", L"text ", L"after", L"."});

  AssertMoveByUnitForMarkup(
      TextUnit_Word, "<ol><li>item one</li><li>item two</li></ol>",
      {L"1", L". ", L"item ", L"one", L"2", L". ", L"item ", L"two"});

  // The following test should be enabled when crbug.com/1028830 is fixed.
  // AssertMoveByUnitForMarkup(TextUnit_Word,
  //                           "<ul><li>item one</li><li>item two</li></ul>",
  //                           {L"• ", L"item ", L"one", L"• ", L"item ",
  //                           L"two"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       EntireMarkupSuccessiveMoveByFormat) {
  AssertMoveByUnitForMarkup(
      TextUnit_Format,
      "plain text <b>bold text <i>bold and italic text</i></b><i><b> more bold "
      "and italic text</b> italic text</i> plain text",
      {L"plain text ", L"bold text ",
       L"bold and italic text more bold and italic text", L" italic text",
       L" plain text"});

  AssertMoveByUnitForMarkup(
      TextUnit_Format, "before <img src='test'> after",
      {L"before ", kEmbeddedCharacterAsString.c_str(), L" after"});

  AssertMoveByUnitForMarkup(TextUnit_Format,
                            "before <a href='test'>link</a> after",
                            {L"before ", L"link", L" after"});

  AssertMoveByUnitForMarkup(TextUnit_Format,
                            "before <a href='test'><b>link </b></a>    after",
                            {L"before ", L"link ", L"after"});

  AssertMoveByUnitForMarkup(TextUnit_Format,
                            "before <b><a href='test'>link </a></b>    after",
                            {L"before ", L"link ", L"after"});

  AssertMoveByUnitForMarkup(
      TextUnit_Format, "before <a href='test'>link </a>    after <b>bold</b>",
      {L"before ", L"link ", L"after ", L"bold"});

  AssertMoveByUnitForMarkup(
      TextUnit_Format,
      "before <a style='font-weight:bold' href='test'>link </a>    after",
      {L"before ", L"link ", L"after"});

  AssertMoveByUnitForMarkup(
      TextUnit_Format,
      "before <a style='font-weight:bold' href='test'>link 1</a><a "
      "style='font-weight:bold' href='test'>link 2</a> after",
      {L"before ", L"link 1link 2", L" after"});

  AssertMoveByUnitForMarkup(
      TextUnit_Format,
      "before <span style='font-weight:bold'>text </span>    after",
      {L"before ", L"text ", L"after"});

  AssertMoveByUnitForMarkup(
      TextUnit_Format,
      "before <span style='font-weight:bold'>text 1</span><span "
      "style='font-weight:bold'>text 2</span> after",
      {L"before ", L"text 1text 2", L" after"});

  AssertMoveByUnitForMarkup(
      TextUnit_Format,
      "before <span style='font-weight:bold'>bold text</span><span "
      "style='font-style: italic'>italic text</span> after",
      {L"before ", L"bold text", L"italic text", L" after"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       EntireMarkupSuccessiveMoveByLine) {
  AssertMoveByUnitForMarkup(TextUnit_Line, "<div style='width:0'>one two</div>",
                            {L"one ", L"two"});

  AssertMoveByUnitForMarkup(TextUnit_Line, "line one<br>line two",
                            {L"line one", L"line two"});

  AssertMoveByUnitForMarkup(TextUnit_Line,
                            "<div>line one</div><div><div>line two</div></div>",
                            {L"line one", L"line two"});

  AssertMoveByUnitForMarkup(
      TextUnit_Line, "<div style='display:inline-block'>a</div>", {L"a"});

  // This tests a weird edge-case; TextUnit_Line breaks at the beginning of an
  // inline-block, but not at the end.
  AssertMoveByUnitForMarkup(TextUnit_Line,
                            "a<div style='display:inline-block'>b</div>c",
                            {L"a", L"b\nc"});

  AssertMoveByUnitForMarkup(
      TextUnit_Line,
      "<h1>line one</h1><ul><li>line two</li><li>line three</li></ul>",
      {L"line one", L"• line two", L"• line three"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveByCharacterWithEmbeddedObject) {
  const std::string html_markup = R"HTML(<!DOCTYPE html>
  <html>
    <body>
      <div>
        <label for="input1">Some text</label>
        <input id="input1">
        <p>after</p>
      </div>
    </body>
  </html>)HTML";

  const std::vector<const wchar_t*> characters = {
      L"S", L"o", L"m", L"e", L" ",
      L"t", L"e", L"x", L"t", kEmbeddedCharacterAsString.c_str(),
      L"a", L"f", L"t", L"e", L"r"};

  AssertMoveByUnitForMarkup(TextUnit_Character, html_markup, characters);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveByWordWithEmbeddedObject) {
  const std::string html_markup = R"HTML(<!DOCTYPE html>
  <html>
    <body>
      <div>
        <label for="input1">Some text </label>
        <input id="input1">
        <p> after input</p>
        <button><div></div></button>
      </div>
    </body>
  </html>)HTML";

  const std::vector<const wchar_t*> words = {
      L"Some ",  L"text ", kEmbeddedCharacterAsString.c_str(),
      L"after ", L"input", kEmbeddedCharacterAsString.c_str()};

  AssertMoveByUnitForMarkup(TextUnit_Word, html_markup, words);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       BoundingRectangleOfWordBeforeListItemMarker) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <body>
          <p>Text before list</p>
          <ul>
            <li>First list item</li>
            <li>Second list item</li>
          </ul>
        </body>
      </html>)HTML");

  BrowserAccessibility* text_before_list =
      FindNode(ax::mojom::Role::kStaticText, "Text before list");
  ASSERT_NE(nullptr, text_before_list);

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*text_before_list, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());

  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 12,
                  /*expected_text*/ L"l",
                  /*expected_count*/ 12);
  text_range_provider->ExpandToEnclosingUnit(TextUnit_Word);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"list");

  base::win::ScopedSafearray rectangles;
  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));

  gfx::Vector2d view_offset = text_before_list->manager()
                                  ->GetViewBoundsInScreenCoordinates()
                                  .OffsetFromOrigin();
  std::vector<double> expected_values = {85 + view_offset.x(),
                                         16 + view_offset.y(), 20, 17};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 19,
                  /*expected_text*/ L"e",
                  /*expected_count*/ 19);
  text_range_provider->ExpandToEnclosingUnit(TextUnit_Word);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"item");

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  expected_values = {105 + view_offset.x(), 50 + view_offset.y(), 28, 17};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveByFormatWithGeneratedContentTableAndSpans) {
  const std::string html_markup =
      "<!DOCTYPE html>"
      "<style>"
      "h2:before, h2:after { content: \" \"; display: table; }"
      "span {white-space: pre; }"
      "</style>"
      "<div><h2>First Heading</h2><span>\nParagraph One</span></div>"
      "<div><h2>Second Heading</h2><span>\nParagraph Two</span></div>";

  const std::vector<const wchar_t*> format_units = {
      L"  \nFirst Heading  ", L"\nParagraph One", L"  \nSecond Heading  ",
      L"\nParagraph Two"};

  AssertMoveByUnitForMarkup(TextUnit_Format, html_markup, format_units);
}

// https://crbug.com/1036397
IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeTextRangeProviderWinBrowserTest,
    DISABLED_MoveByFormatWithGeneratedContentTableAndParagraphs) {
  const std::string html_markup = R"HTML(<!DOCTYPE html>
        <html>
        <style>
            h2:before, h2:after { content: " "; display: table; }
        </style>
        <div><h2>First Heading</h2><p>Paragraph One</p></div>
        <div><h2>Second Heading</h2><p>Paragraph Two</p></div>
        </html>)HTML";

  const std::vector<const wchar_t*> format_units = {
      L"  \nFirst Heading  ", L"\nParagraph One", L"  \nSecond Heading  ",
      L"\nParagraph Two"};

  AssertMoveByUnitForMarkup(TextUnit_Format, html_markup, format_units);
}

// Flaky.
// TODO(https://crbug.com/1132248): Re-enable.
IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       DISABLED_IframeSelect) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/html/iframe-cross-process.html");

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  auto* node = FindNode(ax::mojom::Role::kStaticText, "Text in iframe");
  ASSERT_NE(nullptr, node);

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Text in iframe");

  // First select text entirely in the iframe. To prevent test timeouts, only
  // validate the next selection, which spans outside of the iframe.
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->Select());

  // Move the endpoint so it spans outside of the text range and ensure
  // selection still works.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Document,
      /*count*/ 1,
      /*expected_text*/ L"Text in iframe\nAfter frame",
      /*expected_count*/ 1);

  // Validiate this selection with a waiter.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->Select());

  waiter.WaitForNotification();
  ui::AXTree::Selection selection = node->GetUnignoredSelection();
  EXPECT_EQ(selection.anchor_object_id, node->GetId());
  EXPECT_EQ(selection.anchor_offset, 0);
  EXPECT_EQ(selection.focus_object_id, node->GetId());
  EXPECT_EQ(selection.focus_offset, 14);

  // Now move the start position to outside of the iframe and select.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Document,
      /*count*/ -1,
      /*expected_text*/ L"Before frame\nText in iframe\nAfter frame",
      /*expected_count*/ -1);
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->Select());

  // Now move the end position so it's inside of the iframe.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ -12,
      /*expected_text*/ L"Before frame\nText in ifram",
      /*expected_count*/ -12);
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->Select());
}

}  // namespace content
