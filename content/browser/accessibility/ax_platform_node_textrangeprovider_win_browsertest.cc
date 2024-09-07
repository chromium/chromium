// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_content_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_com_win.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"

using Microsoft::WRL::ComPtr;

namespace content {

#define ASSERT_UIA_ELEMENTNOTAVAILABLE(expr) \
  ASSERT_EQ(static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE), (expr))

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

#define EXPECT_UIA_SAFEARRAY_EQ(safearray, expected_property_values)   \
  {                                                                    \
    using T = typename decltype(expected_property_values)::value_type; \
    EXPECT_EQ(sizeof(T), ::SafeArrayGetElemsize(safearray));           \
    EXPECT_EQ(1u, SafeArrayGetDim(safearray));                         \
    LONG array_lower_bound;                                            \
    EXPECT_HRESULT_SUCCEEDED(                                          \
        SafeArrayGetLBound(safearray, 1, &array_lower_bound));         \
    LONG array_upper_bound;                                            \
    EXPECT_HRESULT_SUCCEEDED(                                          \
        SafeArrayGetUBound(safearray, 1, &array_upper_bound));         \
    const size_t count = array_upper_bound - array_lower_bound + 1;    \
    EXPECT_EQ(expected_property_values.size(), count);                 \
    if (sizeof(T) == ::SafeArrayGetElemsize(safearray) &&              \
        count == expected_property_values.size()) {                    \
      T* array_data;                                                   \
      EXPECT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                  \
          safearray, reinterpret_cast<void**>(&array_data)));          \
      for (size_t i = 0; i < count; ++i) {                             \
        EXPECT_EQ(array_data[i], expected_property_values[i]);         \
      }                                                                \
      EXPECT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(safearray));    \
    }                                                                  \
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
  const std::wstring kEmbeddedCharacterAsString{
      base::as_wcstr(&ui::AXPlatformNodeBase::kEmbeddedCharacter), 1};

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");
  }

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

  ui::BrowserAccessibilityManager* GetManager() const {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

  void GetTextRangeProviderFromTextNode(
      ui::BrowserAccessibility& target_node,
      ITextRangeProvider** text_range_provider) {
    ui::BrowserAccessibilityComWin* target_node_com =
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
    GetTextRangeProviderFromTextNode(
        *GetManager()->GetBrowserAccessibilityRoot(), text_range_provider);
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
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_end_role,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fend)() const,
      const bool align_to_top) {
    ui::BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();

    ui::BrowserAccessibility* browser_accessibility_start =
        (root_browser_accessibility->*fstart)();
    ASSERT_NE(nullptr, browser_accessibility_start);
    ASSERT_EQ(expected_start_role, browser_accessibility_start->GetRole());

    ui::BrowserAccessibility* browser_accessibility_end =
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
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fstart)(size_t)
          const,
      const size_t fstart_arg,
      const ax::mojom::Role expected_end_role,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fend)(size_t) const,
      const size_t fend_arg,
      const bool align_to_top) {
    ui::BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();

    ui::BrowserAccessibility* browser_accessibility_start =
        (root_browser_accessibility->*fstart)(fstart_arg);
    ASSERT_NE(nullptr, browser_accessibility_start);
    ASSERT_EQ(expected_start_role, browser_accessibility_start->GetRole());

    ui::BrowserAccessibility* browser_accessibility_end =
        (root_browser_accessibility->*fend)(fend_arg);
    ASSERT_NE(nullptr, browser_accessibility_end);
    ASSERT_EQ(expected_end_role, browser_accessibility_end->GetRole());

    AssertScrollIntoView(root_browser_accessibility,
                         browser_accessibility_start, browser_accessibility_end,
                         align_to_top);
  }

  void ScrollIntoViewFromIframeBrowserTestTemplate(
      const ax::mojom::Role expected_start_role,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_end_role,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fend)() const,
      const bool align_to_top) {
    ui::BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();
    ui::BrowserAccessibility* leaf_iframe_browser_accessibility =
        root_browser_accessibility->InternalDeepestLastChild();
    ASSERT_NE(nullptr, leaf_iframe_browser_accessibility);
    ASSERT_EQ(ax::mojom::Role::kIframe,
              leaf_iframe_browser_accessibility->GetRole());

    ui::AXTreeID iframe_tree_id = ui::AXTreeID::FromString(
        leaf_iframe_browser_accessibility->GetStringAttribute(
            ax::mojom::StringAttribute::kChildTreeId));
    ui::BrowserAccessibilityManager* iframe_browser_accessibility_manager =
        ui::BrowserAccessibilityManager::FromID(iframe_tree_id);
    ASSERT_NE(nullptr, iframe_browser_accessibility_manager);
    ui::BrowserAccessibility* root_iframe_browser_accessibility =
        iframe_browser_accessibility_manager->GetBrowserAccessibilityRoot();
    ASSERT_NE(nullptr, root_iframe_browser_accessibility);
    ASSERT_EQ(ax::mojom::Role::kRootWebArea,
              root_iframe_browser_accessibility->GetRole());

    ui::BrowserAccessibility* browser_accessibility_start =
        (root_iframe_browser_accessibility->*fstart)();
    ASSERT_NE(nullptr, browser_accessibility_start);
    ASSERT_EQ(expected_start_role, browser_accessibility_start->GetRole());

    ui::BrowserAccessibility* browser_accessibility_end =
        (root_iframe_browser_accessibility->*fend)();
    ASSERT_NE(nullptr, browser_accessibility_end);
    ASSERT_EQ(expected_end_role, browser_accessibility_end->GetRole());

    AssertScrollIntoView(root_iframe_browser_accessibility,
                         browser_accessibility_start, browser_accessibility_end,
                         align_to_top);
  }

  void AssertScrollIntoView(
      ui::BrowserAccessibility* root_browser_accessibility,
      ui::BrowserAccessibility* browser_accessibility_start,
      ui::BrowserAccessibility* browser_accessibility_end,
      const bool align_to_top) {
    ui::BrowserAccessibility::AXPosition start =
        browser_accessibility_start->CreateTextPositionAt(0);
    ui::BrowserAccessibility::AXPosition end =
        browser_accessibility_end->CreateTextPositionAt(0)
            ->CreatePositionAtEndOfAnchor();

    ui::BrowserAccessibilityComWin* start_browser_accessibility_com_win =
        ToBrowserAccessibilityWin(browser_accessibility_start)->GetCOM();
    ASSERT_NE(nullptr, start_browser_accessibility_com_win);

    ComPtr<ITextRangeProvider> text_range_provider;
    ui::AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
        std::move(start), std::move(end), &text_range_provider);
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
    ASSERT_TRUE(location_changed_waiter.WaitForNotification());

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
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*f)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role, f, expected_role, f, true);
  }

  void ScrollIntoViewTopBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_role_end,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fend)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart,
                                      expected_role_end, fend, true);
  }

  void ScrollIntoViewTopBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fstart)(size_t)
          const,
      const size_t fstart_arg,
      const ax::mojom::Role expected_role_end,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fend)(size_t) const,
      const size_t fend_arg) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart, fstart_arg,
                                      expected_role_end, fend, fend_arg, true);
  }

  void ScrollIntoViewBottomBrowserTestTemplate(
      const ax::mojom::Role expected_role,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*f)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role, f, expected_role, f,
                                      false);
  }

  void ScrollIntoViewBottomBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_role_end,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fend)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart,
                                      expected_role_end, fend, false);
  }

  void ScrollIntoViewBottomBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fstart)(size_t)
          const,
      const size_t fstart_arg,
      const ax::mojom::Role expected_role_end,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*fend)(size_t) const,
      const size_t fend_arg) {
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_{::features::kUiaProvider};
};

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetChildren) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div>
            <span>Text1</span>
            <span>Text2</span>
            <span>Text3</span>
          </div>
          <p>Before link</p>
          <a href="#">
            Link text 1
            <span>Link text 2</span>
            <span>Link text 3</span>
            Link text 4
          </a>
          <p>After link</p>
          <p>Before img</p>
          <img alt="Image description">
          <p>After img</p>
        </body>
      </html>
  )HTML");

  ui::BrowserAccessibility* text1_node =
      FindNode(ax::mojom::Role::kStaticText, "Text1");
  ASSERT_NE(nullptr, text1_node);
  ComPtr<IRawElementProviderSimple> text1_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(text1_node);

  ui::BrowserAccessibility* before_link_text_node =
      FindNode(ax::mojom::Role::kStaticText, "Before link");
  ASSERT_NE(nullptr, before_link_text_node);
  ComPtr<IRawElementProviderSimple> before_link_text_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(before_link_text_node);

  ui::BrowserAccessibility* link_node =
      FindNode(ax::mojom::Role::kLink,
               "Link text 1 Link text 2 Link text 3 Link text 4");
  ASSERT_NE(nullptr, link_node);
  ComPtr<IRawElementProviderSimple> link_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(link_node);

  ui::BrowserAccessibility* link_text2_node =
      FindNode(ax::mojom::Role::kStaticText, "Link text 2");
  ASSERT_NE(nullptr, link_text2_node);
  ComPtr<IRawElementProviderSimple> link_text2_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(link_text2_node);

  ui::BrowserAccessibility* link_text3_node =
      FindNode(ax::mojom::Role::kStaticText, "Link text 3");
  ASSERT_NE(nullptr, link_text3_node);
  ComPtr<IRawElementProviderSimple> link_text3_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(link_text3_node);

  ui::BrowserAccessibility* after_link_text_node =
      FindNode(ax::mojom::Role::kStaticText, "After link");
  ASSERT_NE(nullptr, after_link_text_node);
  ComPtr<IRawElementProviderSimple> after_link_text_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(after_link_text_node);

  ui::BrowserAccessibility* image_node =
      FindNode(ax::mojom::Role::kImage, "Image description");
  ASSERT_NE(nullptr, image_node);
  ComPtr<IRawElementProviderSimple> image_raw =
      QueryInterfaceFromNode<IRawElementProviderSimple>(image_node);

  // 1. Validate that no children are returned when the range doesn't include
  // any UIA embedded objects.
  ComPtr<ITextRangeProvider> text_range;
  GetTextRangeProviderFromTextNode(*text1_node, &text_range);
  ASSERT_NE(nullptr, text_range.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range, L"Text1");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range, TextPatternRangeEndpoint_End,
                                   TextUnit_Paragraph,
                                   /*count*/ 1,
                                   /*expected_text*/ L"Text1 Text2 Text3\n",
                                   /*expected_count*/ 1);

  base::win::ScopedSafearray children;
  std::vector<ComPtr<IRawElementProviderSimple>> expected_values = {};

  EXPECT_HRESULT_SUCCEEDED(text_range->GetChildren(children.Receive()));
  EXPECT_UIA_SAFEARRAY_EQ(children.Get(), expected_values);

  // 2. Validate that both the link and image objects are returned when the
  // range spans the document.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range, TextPatternRangeEndpoint_End, TextUnit_Document,
      /*count*/ 1,
      /*expected_text*/
      L"Text1 Text2 Text3\nBefore link\nLink text 1 Link text 2 Link text 3 "
      L"Link text 4\nAfter link\nBefore img\n\xFFFC\nAfter img",
      /*expected_count*/ 1);

  EXPECT_HRESULT_SUCCEEDED(text_range->GetChildren(children.Receive()));

  expected_values = {link_raw, image_raw};
  EXPECT_UIA_SAFEARRAY_EQ(children.Get(), expected_values);

  // 3. Validate that no object is returned when the range is inside the textual
  // content of an embedded object.
  GetTextRangeProviderFromTextNode(*link_text2_node, &text_range);
  ASSERT_NE(nullptr, text_range.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range, L"Link text 2");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range, TextPatternRangeEndpoint_End,
                                   TextUnit_Character,
                                   /*count*/ 12,
                                   /*expected_text*/ L"Link text 2 Link text 3",
                                   /*expected_count*/ 12);

  EXPECT_HRESULT_SUCCEEDED(text_range->GetChildren(children.Receive()));

  expected_values = {};
  EXPECT_UIA_SAFEARRAY_EQ(children.Get(), expected_values);

  // 4. Validate that the link object is returned when the text range contains
  // a link object.
  GetTextRangeProviderFromTextNode(*before_link_text_node, &text_range);
  ASSERT_NE(nullptr, text_range.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range, L"Before link");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range, TextPatternRangeEndpoint_End,
                                   TextUnit_Paragraph,
                                   /*count*/ 2,
                                   /*expected_text*/
                                   L"Before link\nLink text 1 Link text 2 Link "
                                   L"text 3 Link text 4\nAfter link\n",
                                   /*expected_count*/ 2);

  EXPECT_HRESULT_SUCCEEDED(text_range->GetChildren(children.Receive()));

  expected_values = {link_raw};
  EXPECT_UIA_SAFEARRAY_EQ(children.Get(), expected_values);

  // 5. Validate that the link object is included even if it is partially
  // included in the range.
  GetTextRangeProviderFromTextNode(*link_text2_node, &text_range);
  ASSERT_NE(nullptr, text_range.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range, L"Link text 2");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 2,
      /*expected_text*/ L"Link text 2 Link text 3 Link text 4\nAfter link\n",
      /*expected_count*/ 2);

  EXPECT_HRESULT_SUCCEEDED(text_range->GetChildren(children.Receive()));

  expected_values = {link_raw};
  EXPECT_UIA_SAFEARRAY_EQ(children.Get(), expected_values);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetTextEmptyButtonWithAriaLabelRangeAnchoredInSpans) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
          <span>before</span>
          <button aria-label="middle"><svg aria-hidden="true"></svg></button>
          <span>after</span>
  )HTML");

  ui::BrowserAccessibility* root = GetRootAndAssertNonNull();

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*root, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"before\nmiddle\nafter");
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 6,
      /*expected_text*/ L"\nmiddle\nafter",
      /*expected_count*/ 6);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 7,
      /*expected_text*/ L"\nafter",
      /*expected_count*/ 7);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetTextEmptyButtonWithAriaLabel) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
          <button aria-label="middle"><svg aria-hidden="true"></svg></button>
  )HTML");

  ui::BrowserAccessibility* root = GetRootAndAssertNonNull();

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*root, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"middle");
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 3,
      /*expected_text*/ L"dle",
      /*expected_count*/ 3);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetTextEmptyButtonWithAriaLabelStartAnchoredInSpan) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
          <span>before</span>
          <button aria-label="middle"><svg aria-hidden="true"></svg></button>
  )HTML");

  ui::BrowserAccessibility* root = GetRootAndAssertNonNull();

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*root, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"before\nmiddle");
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 8,
      /*expected_text*/ L"iddle",
      /*expected_count*/ 8);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetTextEmptyButtonWithAriaLabelButtonEndAnchoredInSpan) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
          <button aria-label="middle"><svg aria-hidden="true"></svg></button>
          <span>after</span>
  )HTML");

  ui::BrowserAccessibility* root = GetRootAndAssertNonNull();

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*root, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"middle\nafter");
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 6,
      /*expected_text*/ L"\nafter",
      /*expected_count*/ 6);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetTextEmptyTextfieldWithAriaLabel) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
          <input type="text" aria-label="before">
          <span>after</span>
  )HTML");

  ui::BrowserAccessibility* root = GetRootAndAssertNonNull();

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*root, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"before\nafter");
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 6,
      /*expected_text*/ L"\nafter",
      /*expected_count*/ 6);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetTextNonEmptyTextfieldWithAriaLabel) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
          <input type="text" aria-label="before" value="go blue">
          <span>after</span>
  )HTML");

  ui::BrowserAccessibility* root = GetRootAndAssertNonNull();

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*root, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"go blue\nafter");
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 6,
      /*expected_text*/ L"e\nafter",
      /*expected_count*/ 6);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetAttributeValue) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div style="font-size: 16pt">
            <span style="font-size: 12pt">Text1</span>
            Text2
          </div>
        </body>
      </html>
  )HTML");

  ComPtr<IUnknown> mix_attribute_value;
  EXPECT_HRESULT_SUCCEEDED(
      UiaGetReservedMixedAttributeValue(&mix_attribute_value));

  ui::BrowserAccessibility* node =
      FindNode(ax::mojom::Role::kStaticText, "Text1");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->IsLeaf());
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

// An empty atomic text field, such as an empty <input type="text">, should
// expose an embedded object replacement character in its text representation.
IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetAttributeValueInReadonlyEmptyAtomicTextField) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <input readonly type="text">
          <input type="search">
        </body>
      </html>
  )HTML");

  ui::BrowserAccessibility* root = GetRootAndAssertNonNull();

  ui::BrowserAccessibility* input_text_node =
      root->InternalGetFirstChild()->InternalGetFirstChild();
  ASSERT_NE(nullptr, input_text_node);
  EXPECT_TRUE(input_text_node->IsLeaf());
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
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_TRUE);
  text_range_provider.Reset();
  value.Reset();

  ui::BrowserAccessibility* input_search_node =
      input_text_node->InternalGetNextSibling();
  ASSERT_NE(nullptr, input_search_node);
  EXPECT_TRUE(input_search_node->IsLeaf());
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

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetAttributeValueInReadonlyAtomicTextField) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <input type="text" aria-label="input_text" value="text">
          <input readonly type="search" aria-label="input_search"
              value="search">
        </body>
      </html>
  )HTML");

  ui::BrowserAccessibility* input_text_node =
      FindNode(ax::mojom::Role::kTextField, "input_text");
  ASSERT_NE(nullptr, input_text_node);
  EXPECT_TRUE(input_text_node->IsLeaf());
  EXPECT_EQ(0u, input_text_node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*input_text_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"text");

  base::win::ScopedVariant value;
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_FALSE);
  text_range_provider.Reset();
  value.Reset();

  ui::BrowserAccessibility* input_search_node =
      FindNode(ax::mojom::Role::kSearchBox, "input_search");
  ASSERT_NE(nullptr, input_search_node);
  EXPECT_TRUE(input_search_node->IsLeaf());
  EXPECT_EQ(0u, input_search_node->PlatformChildCount());

  GetTextRangeProviderFromTextNode(*input_search_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"search");

  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_TRUE);
  text_range_provider.Reset();
  value.Reset();
}

// With a non-atomic text field, the read-only attribute should be determined
// based on the content editable root node's editable state.
IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetAttributeValueInReadonlyNonAtomicTextField) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
      <html>
        <style>
          .non-atomic-text-field::before {
              content: attr(data-placeholder);
              pointer-events: none;
          }
        </style>
        <body>
          <div contenteditable="true" data-placeholder="@mention or comment"
              role="textbox" aria-readonly="false" aria-label="text_field_1"
              class="non-atomic-text-field">
              <p>value1</p>
          </div>
          <div contenteditable="true" data-placeholder="@mention or comment"
              role="textbox" aria-readonly="true" aria-label="text_field_2"
              class="non-atomic-text-field">
              <p>value2</p>
          </div>
          <div contenteditable="false" data-placeholder="@mention or comment"
              role="textbox" aria-readonly="true" aria-label="text_field_3"
              class="non-atomic-text-field">
              <p>value3</p>
          </div>
        </body>
      </html>
  )HTML");

  ui::BrowserAccessibility* text_field_node_1 =
      FindNode(ax::mojom::Role::kTextField, "text_field_1");
  ASSERT_NE(nullptr, text_field_node_1);
  ui::BrowserAccessibility* text_field_node_2 =
      FindNode(ax::mojom::Role::kTextField, "text_field_2");
  ASSERT_NE(nullptr, text_field_node_2);
  ui::BrowserAccessibility* text_field_node_3 =
      FindNode(ax::mojom::Role::kTextField, "text_field_3");
  ASSERT_NE(nullptr, text_field_node_3);

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*text_field_node_1, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"@mention or comment\nvalue1");

  base::win::ScopedVariant value;
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_FALSE);
  text_range_provider.Reset();
  value.Reset();

  GetTextRangeProviderFromTextNode(*text_field_node_2, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"@mention or comment\nvalue2");

  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_FALSE);
  text_range_provider.Reset();
  value.Reset();

  GetTextRangeProviderFromTextNode(*text_field_node_3, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"@mention or comment\nvalue3");

  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsReadOnlyAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), VARIANT_TRUE);
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
            <input type="text" aria-label="input_text"><span
              style="font-size: 12pt">Text1</span>
          </div>
          <div contenteditable="true">
            <ul><li>item</li></ul>3.14
          </div>
        </body>
      </html>
  )HTML"));

  // Case 1: Inside of an atomic text field, NormalizeTextRange shouldn't modify
  // the text range endpoints. An atomic text field does not expose its internal
  // implementation to assistive software, appearing as a single leaf node in
  // the accessibility tree. It includes <input>, <textarea> and Views-based
  // text fields.
  //
  // In order for the test harness to effectively simulate typing in a text
  // input, first change the value of the text input and then focus it. Only
  // editing the value won't show the cursor and only focusing will put the
  // cursor at the beginning of the text input, so both steps are necessary.
  ui::BrowserAccessibility* input_text_node =
      FindNode(ax::mojom::Role::kTextField, "input_text");
  ASSERT_NE(nullptr, input_text_node);
  EXPECT_TRUE(input_text_node->IsLeaf());
  EXPECT_EQ(0u, input_text_node->PlatformChildCount());

  AccessibilityNotificationWaiter edit_waiter(shell()->web_contents(),
                                              ui::kAXModeComplete,
                                              ax::mojom::Event::kValueChanged);
  ui::AXActionData edit_data;
  edit_data.target_node_id = input_text_node->GetId();
  edit_data.action = ax::mojom::Action::kSetValue;
  edit_data.value = "test";
  input_text_node->AccessibilityPerformAction(edit_data);
  ASSERT_TRUE(edit_waiter.WaitForNotification());

  AccessibilityNotificationWaiter focus_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  ui::AXActionData focus_data;
  focus_data.target_node_id = input_text_node->GetId();
  focus_data.action = ax::mojom::Action::kFocus;
  input_text_node->AccessibilityPerformAction(focus_data);
  ASSERT_TRUE(focus_waiter.WaitForNotification());

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
  // change the result of CompareEndpoints below since the range is inside an
  // atomic text field.
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
  ui::BrowserAccessibility* node =
      FindNode(ax::mojom::Role::kStaticText, "item");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->IsLeaf());
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
      /*count*/ 2,
      /*expected_text*/ L"\n3",
      /*expected_count*/ 2);

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
                       CompareAriaInvalidTextRange) {
  // This test is needed since there was a bug with this scenario, and it
  // differs from others since the "aria-invalid" attribute causes the tree to
  // be different, with an extra generic container that we do not have in the
  // case without aria-invalid="spelling".
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <div contentEditable="true">x <span aria-invalid="spelling">He</span></div>
      </html>
  )HTML"));

  ui::BrowserAccessibility* static_text_node_1 =
      FindNode(ax::mojom::Role::kStaticText, "He");
  ASSERT_NE(nullptr, static_text_node_1);
  ui::BrowserAccessibility* static_text_node_2 =
      FindNode(ax::mojom::Role::kStaticText, "x ");
  ASSERT_NE(nullptr, static_text_node_2);

  ComPtr<ITextRangeProvider> text_range_provider_1;
  GetTextRangeProviderFromTextNode(*static_text_node_1, &text_range_provider_1);

  // We are moving the endpoints to replicate a bug where the text ranges looked
  // like:
  // 1. H<e>
  // 2. x <>
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider_1, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 1,
      /*expected_text*/ L"e",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider_1, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ -1,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
  ASSERT_NE(nullptr, text_range_provider_1.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider_1, L"");

  ComPtr<ITextRangeProvider> text_range_provider_2;
  GetTextRangeProviderFromTextNode(*static_text_node_2, &text_range_provider_2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider_2, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 2,
      /*expected_text*/ L"",
      /*expected_count*/ 2);
  ASSERT_NE(nullptr, text_range_provider_2.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider_2, L"");

  BOOL are_same;
  text_range_provider_1->Compare(text_range_provider_2.Get(), &are_same);
  EXPECT_FALSE(are_same);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       TextInputWithNewline) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div aria-value='wrapper'>
            <input type='text' aria-label='input_text'><br>
          </div>
        </body>
      </html>
  )HTML");

  // This test validates an important scenario for editing. UIA clients such as
  // Narrator expect newlines to be contained within their adjacent nodes.
  // This test validates this scenario for GetEnclosingElement and
  // GetAttributeValue, both of which are essential for text editing scenarios.
  //
  // In order for the test harness to effectively simulate typing in a text
  // input, first change the value of the text input and then focus it. Only
  // editing the value won't show the cursor and only focusing will put the
  // cursor at the beginning of the text input, so both steps are necessary.
  ui::BrowserAccessibility* input_text_node =
      FindNode(ax::mojom::Role::kTextField, "input_text");
  ASSERT_NE(nullptr, input_text_node);
  EXPECT_TRUE(input_text_node->IsLeaf());
  EXPECT_EQ(0u, input_text_node->PlatformChildCount());

  AccessibilityNotificationWaiter edit_waiter(shell()->web_contents(),
                                              ui::kAXModeComplete,
                                              ax::mojom::Event::kValueChanged);
  ui::AXActionData edit_data;
  edit_data.target_node_id = input_text_node->GetId();
  edit_data.action = ax::mojom::Action::kSetValue;
  edit_data.value = "test";
  input_text_node->AccessibilityPerformAction(edit_data);
  ASSERT_TRUE(edit_waiter.WaitForNotification());

  AccessibilityNotificationWaiter focus_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  ui::AXActionData focus_data;
  focus_data.target_node_id = input_text_node->GetId();
  focus_data.action = ax::mojom::Action::kFocus;
  input_text_node->AccessibilityPerformAction(focus_data);
  ASSERT_TRUE(focus_waiter.WaitForNotification());

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
  EXPECT_TRUE(node->IsLeaf());
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
  gfx::Vector2dF view_offset(
      node->manager()->GetViewBoundsInScreenCoordinates().OffsetFromOrigin());
  std::vector<double> expected_values = {
      8 + view_offset.x(), 16 + view_offset.y(), 49, 17,
      8 + view_offset.x(), 34 + view_offset.y(), 44, 17};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       RemoveNode) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
        <div id="wrapper">
          <p id="node_1">Node 1</p>
          <p>Node 2</p>
        </div>
  )HTML");

  ui::BrowserAccessibility* node =
      FindNode(ax::mojom::Role::kStaticText, "Node 1");
  ASSERT_NE(nullptr, node);
  EXPECT_EQ(0u, node->PlatformChildCount());

  // Create the text range on "Node 1".
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Node 1");

  // Move the text range to "Node 2".
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Word,
                  /*count*/ 2,
                  /*expected_text*/ L"Node ",
                  /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ 1,
                                   /*expected_text*/ L"Node 2",
                                   /*expected_count*/ 1);

  // Now remove "Node 1" from the DOM and verify the text range created from
  // "Node 1" is still functional.
  {
    AccessibilityNotificationWaiter waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        ui::AXEventGenerator::Event::CHILDREN_CHANGED);
    EXPECT_TRUE(
        ExecJs(shell()->web_contents(),
               "document.getElementById('wrapper').removeChild(document."
               "getElementById('node_1'));"));

    ASSERT_TRUE(waiter.WaitForNotification());
    EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
        text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
        /*count*/ -1,
        /*expected_text*/ L"Node ",
        /*expected_count*/ -1);
  }

  // Now remove all children from the DOM and verify the text range created from
  // "Node 1" is still valid (it got moved to a non-deleted ancestor node).
  {
    AccessibilityNotificationWaiter waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        ui::AXEventGenerator::Event::CHILDREN_CHANGED);
    EXPECT_TRUE(ExecJs(shell()->web_contents(),
                       "while(document.body.childElementCount > 0) {"
                       "  document.body.removeChild(document.body.firstChild);"
                       "}"));

    ASSERT_TRUE(waiter.WaitForNotification());

    EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
        text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
        /*count*/ 1,
        /*expected_text*/ L"",
        /*expected_count*/ 0);

    EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
        text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
        /*count*/ -1,
        /*expected_text*/ L"",
        /*expected_count*/ 0);
  }
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopStaticText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/text.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestFirstChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomStaticText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/text.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestFirstChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopEmbeddedText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomEmbeddedText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopEmbeddedTextCrossNode) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomEmbeddedTextCrossNode) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopTable) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kTable, &ui::BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kTable, &ui::BrowserAccessibility::PlatformGetChild, 0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomTable) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kTable, &ui::BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kTable, &ui::BrowserAccessibility::PlatformGetChild, 0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopTableText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomTableText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopLinkText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomLinkText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopLinkContainer) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &ui::BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kGenericContainer,
      &ui::BrowserAccessibility::PlatformGetChild, 0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomLinkContainer) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &ui::BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kGenericContainer,
      &ui::BrowserAccessibility::PlatformGetChild, 0);
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
      &ui::BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild, true);
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
      &ui::BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &ui::BrowserAccessibility::PlatformDeepestLastChild, false);
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
  EXPECT_TRUE(node->IsLeaf());
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
      /*expected_text*/ L"plain 1\nplain 2\nplain heading",
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
                       MoveEndpointByLineInlineBlockSpan) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <div contenteditable="true" style="outline: 1px solid;" aria-label="canvas">
        <div>first line</div>
        <div><span>this line </span><span style="display: inline-block" id="IB"><span style="display: block;">is</span></span><span> broken.</span></div>
        <div>last line</div>
      </div>
      </html>)HTML");
  auto* node = FindNode(ax::mojom::Role::kStaticText, "first line");
  ASSERT_NE(nullptr, node);

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"first line");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Line,
      /*count*/ 1,
      /*expected_text*/ L"first line\nthis line \nis\n broken.",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Line,
      /*count*/ 1,
      /*expected_text*/
      L"this line \nis\n broken.",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Line,
                                   /*count*/ 1,
                                   /*expected_text*/
                                   L"this line \nis\n broken.\nlast line",
                                   /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Line,
      /*count*/ 1,
      /*expected_text*/ L"last line",
      /*expected_count*/ 1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByLineLinkInTwoLines) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(<!DOCTYPE html>
      <div contenteditable style="width: 70px">
        Hello
        <a href="#">this is a</a>
        test.
      </div>
      )HTML");
  auto* node = FindNode(ax::mojom::Role::kStaticText, "Hello ");
  ASSERT_NE(nullptr, node);

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Hello ");
  text_range_provider->ExpandToEnclosingUnit(TextUnit_Line);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Hello this ");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Line,
                                   /*count*/ 1,
                                   /*expected_text*/ L"Hello this is a test.",
                                   /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Line,
      /*count*/ 1,
      /*expected_text*/
      L"is a test.",
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
        <div aria-invalid="spelling">spelling 1</div>
        <div aria-invalid="spelling">spelling two</div> <!-- different length string on purpose -->
        <div aria-invalid="grammar">grammar 1</div>
        <div aria-invalid="grammar">grammar two</div> <!-- different length string on purpose -->
      </body>
      </html>)HTML");
  auto* node = FindNode(ax::mojom::Role::kStaticText, "plain 1");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->IsLeaf());
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
      /*expected_text*/ L"plain 1\nplain 2",
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
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2",
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
      L"1\ncolor 2",
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
      L"1\ncolor 2\noverline 1\noverline 2",
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
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through 2",
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
      L"2\nsup 1\nsup 2",
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
      L"2\nsup 1\nsup 2\nbold 1\nbold 2",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2\nfont-family 1\nfont-family "
      L"2\nspelling 1\nspelling two",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2\nfont-family 1\nfont-family 2",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2\nfont-family 1\nfont-family "
      L"2\nspelling 1\nspelling two\ngrammar 1\ngrammar two",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2\nfont-family 1\nfont-family "
      L"2\nspelling 1\nspelling two",
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
  ui::BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  ui::BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  std::vector<std::wstring> paragraphs = {
      L"start",
      L"text with [:before] and [:after]content, then a\n\xFFFC",
      L"bold element with a [block]before content then a italic\n\xFFFC",
      L"element with a [block] after content",
      L"end",
  };

  // FORWARD NAVIGATION
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, paragraphs[0].c_str());

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
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + L'\n' + paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + L'\n' + paragraphs[3] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[3] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[3] + L'\n' + paragraphs[4]).c_str(),
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

  //
  // REVERSE NAVIGATION
  //

  GetTextRangeProviderFromTextNode(*end_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, paragraphs[4].c_str());

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
      /*expected_text*/ (paragraphs[3] + L'\n' + paragraphs[4]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[3] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + L'\n' + paragraphs[3] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + L'\n' + paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
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
  ui::BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  ui::BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  // The three <br> elements should be merged with the previous paragraph,
  // because according to MSDN, in UI Automation, any trailing whitespace should
  // be part of the previous paragraph.
  std::vector<std::wstring> paragraphs = {
      L"start",
      L"some text\n\n\n",
      L"more text",
      L"end",
  };

  // FORWARD NAVIGATION
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, paragraphs[0].c_str());

  // There is no trailing '\n' because the second paragraph already has merged
  // trailing whitespace in it, and in such cases we made the design decision
  // not to add an extra line break.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1]).c_str(),
      /*expected_count*/ 1);
  // There is no trailing '\n' because the second paragraph already has merged
  // trailing whitespace in it, and in such cases we made the design decision
  // not to add an extra line break.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + L'\n' + paragraphs[3]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ L"",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ L"",
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ L"",
      /*expected_count*/ 0);

  //
  // REVERSE NAVIGATION
  //

  GetTextRangeProviderFromTextNode(*end_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, paragraphs[3].c_str());

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + L'\n' + paragraphs[3]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ -1);
  // There is no trailing '\n' because the second paragraph already has merged
  // trailing whitespace in it.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1]).c_str(),
      /*expected_count*/ -1);
  // There is no trailing '\n' because the second paragraph already has merged
  // trailing whitespace in it.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ L"",
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ L"",
      /*expected_count*/ 0);
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
  ui::BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);

  std::vector<std::wstring> paragraphs = {
      L"start",
      L"text with [:before] and [:after]content, then a\n\xFFFC",
      L"bold element",
  };

  // Forward navigation.
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, paragraphs[0].c_str());

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/
      (paragraphs[0] + L'\n' + paragraphs[1] + L'\n' + paragraphs[2]).c_str(),
      /*expected_count*/ 1);

  //
  // Reverse navigation.
  //

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
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
  ui::BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  ui::BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  ComPtr<ITextRangeProvider> text_range_provider;

  // According to MSDN, empty paragraphs should be merged with the previous
  // paragraph. However, paragraphs containing only spaces are not considered
  // empty. Otherwise, deleting all the text except a few spaces from an
  // existing paragraph while editing a document will confusingly make the
  // paragraph disappear.
  //
  // Note that two out of the three empty paragraphs span two lines, hence the
  // '\n' suffix.
  std::vector<std::wstring> paragraphs = {
      L"start",
      L"          First Paragraph",
      L"          Second Paragraph",
      L"        \n",  // Empty paragraph 1.
      L"Third Paragraph",
      L"Fourth Paragraph\n",
      L"          Fifth               Paragraph",
      L"          Sixth               Paragraph",
      L"        \n",  // Empty paragraph 2.
      L"          Seventh             Paragraph",
      L"          Eighth              Paragraph",
      L"        ",  // Empty paragraph 3.
      L"end",
  };

  // FORWARD NAVIGATION
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, paragraphs[0].c_str());

  // The first paragraph extends beyond the end of the "start" node, because
  // the preserved whitespace node begins with a line break, so
  // move once to capture that.
  //
  // Also, some paragraphs end with a line break, '\n', and in those cases
  // `AXRange::GetText()` does not add an additional line break. Hence you might
  // see some expectations that at first glance appear to be inconsistent with
  // one another.

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + L'\n' + paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ 1);
  // Since paragraphs[3] ends with a line break, '\n', `AXRange::GetText()` does
  // not add an additional line break.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + L'\n' + paragraphs[3]).c_str(),
      /*expected_count*/ 1);
  // Since paragraphs[3] ends with a line break, '\n', `AXRange::GetText()` does
  // not add an additional line break.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ 1);
  // Since paragraphs[3] ends with a line break, '\n', `AXRange::GetText()` does
  // not add an additional line break.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[3] + paragraphs[4] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[4] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[4] + L'\n' + paragraphs[5] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[5] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[5] + L'\n' + paragraphs[6] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[6] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[6] + L'\n' + paragraphs[7] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[7] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[7] + L'\n' + paragraphs[8]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[8].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[8] + paragraphs[9] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[9] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/
      (paragraphs[9] + L'\n' + paragraphs[10] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[10] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[10] + L'\n' + paragraphs[11]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[11] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[11] + L'\n' + paragraphs[12]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[12].c_str(),
      /*expected_count*/ 1);

  //
  // REVERSE NAVIGATION
  //

  GetTextRangeProviderFromTextNode(*end_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, paragraphs[12].c_str());

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[11] + L'\n' + paragraphs[12]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[11] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[10] + L'\n' + paragraphs[11]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[10] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/
      (paragraphs[9] + L'\n' + paragraphs[10] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[9] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[8] + paragraphs[9] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[8].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[7] + L'\n' + paragraphs[8]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[7] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[6] + L'\n' + paragraphs[7] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[6] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[5] + L'\n' + paragraphs[6] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[5] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[4] + L'\n' + paragraphs[5] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[4] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[3] + paragraphs[4] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + L'\n' + paragraphs[3]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + L'\n' + paragraphs[2] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveByUnitParagraphWithAriaHiddenNodes) {
  const std::string html_markup = R"HTML(<!DOCTYPE html>
  <html>   <!-- aria=describedby on #body causes hidden nodes to be included -->
    <body id="body" aria-describedby="body">
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
      L"start\n",
      L"1. Paragraph with hidden inline in between\n",
      L"2. Paragraph parts wrapped by span with hidden inline in between\n",
      L"3. Paragraph before hidden block\n",
      L"4. Paragraph after hidden block\n",
      L"5. Paragraph with leading and trailing hidden span\n",
      L"6. Paragraph with leading and trailing hidden block\n",
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
        <svg aria-label="middle"></svg>
        <span>end</span>
      </body>
      </html>)HTML");
  ui::BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  ui::BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  std::vector<std::wstring> paragraphs = {
      L"start",
      kEmbeddedCharacterAsString,
      L"end",
  };

  // FORWARD NAVIGATION
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, paragraphs[0].c_str());

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
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + L'\n' + paragraphs[2]).c_str(),
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

  //
  // REVERSE NAVIGATION
  //

  GetTextRangeProviderFromTextNode(*end_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, paragraphs[2].c_str());

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
      /*expected_text*/ (paragraphs[1] + L'\n' + paragraphs[2]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n' + paragraphs[1] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L'\n').c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -2,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByUnitLineInertSpan) {
  // Spans need to be in the same line: see https://crbug.com/1511390.
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <div>
        <div>first line</div>
        <span id="span1">go </span><span inert>inert1</span><span inert>inert2</span><span>blue</span>
        <div>last line</div>
      </div>)HTML");
  ui::BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "first line");
  ASSERT_NE(nullptr, start_node);

  ui::BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "last line");
  ASSERT_NE(nullptr, start_node);

  // Navigating forward to the next line.
  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"first line");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Line,
                                   /*count*/ 1,
                                   /*expected_text*/ L"first line\ngo blue",
                                   /*expected_count*/ 1);

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Line,
      /*count*/ 1,
      /*expected_text*/ L"go blue",
      /*expected_count*/ 1);

  // Navigating to the previous line.
  GetTextRangeProviderFromTextNode(*end_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"last line");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Line,
      /*count*/ -1,
      /*expected_text*/ L"go blue\nlast line",
      /*expected_count*/ -1);

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Line,
                                   /*count*/ -1,
                                   /*expected_text*/ L"go blue",
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
  EXPECT_TRUE(node->IsLeaf());
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
      /*count*/ -18,
      /*expected_text*/ L"iframe\nAfter frame",
      /*expected_count*/ -18);
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
      /*count*/ 2,
      /*expected_text*/ L"frame\nT",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 7,
                  /*expected_text*/ L"e",
                  /*expected_count*/ 7);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 20,
                  /*expected_text*/ L"f",
                  /*expected_count*/ 20);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ -8,
                  /*expected_text*/ L"e",
                  /*expected_count*/ -8);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Line,
                  /*count*/ 1,
                  /*expected_text*/ L"After frame",
                  /*expected_count*/ 1);

  text_range_provider->ExpandToEnclosingUnit(TextUnit_Document);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                          L"Before frame\nText in iframe\nAfter frame");
}

// TODO(crbug.com/40848898): This test is flaky.
IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       DISABLED_OutOfProcessIFrameTraversal) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/iframe-cross-process.html"));
  LoadInitialAccessibilityTreeFromUrl(main_url);

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
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
    EXPECT_TRUE(NavigateToURLFromRenderer(iframe_node, iframe_url));
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  SynchronizeThreads();
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  WaitForHitTestData(iframe_node->current_frame_host());
  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(*root));

  auto* node = FindNode(ax::mojom::Role::kStaticText, "After frame");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->IsLeaf());
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
  EXPECT_TRUE(node->IsLeaf());
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
      /*count*/ 4,
      /*expected_text*/ L"plain\ntext\nita",
      /*expected_count*/ 4);

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 12,
      /*expected_text*/ L"ta",
      /*expected_count*/ 12);

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
        <p tabindex="0" aria-label="space">&nbsp;</p>
        <p>3.14</p>
      </body>
      </html>)HTML");

  // Case 1: test on degenerate range before whitespace.
  auto* node = FindNode(ax::mojom::Role::kParagraph, "space")
                   ->PlatformDeepestFirstChild();
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->IsLeaf());
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
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"\xA0");

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
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"\xA0");

  // Case 3: test on degenerate range after whitespace.
  node = FindNode(ax::mojom::Role::kStaticText, "3.14");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->IsLeaf());
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

  AssertMoveByUnitForMarkup(TextUnit_Word,
                            "<ul><li>item one</li><li>item two</li></ul>",
                            {L"• ", L"item ", L"one", L"• ", L"item ", L"two"});
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

  // This makes sure that inline block is not adding a line break
  AssertMoveByUnitForMarkup(
      TextUnit_Line, "a<div style='display:inline-block'>b</div>c", {L"abc"});
  AssertMoveByUnitForMarkup(TextUnit_Line,
                            "a<div style='display:block'>b</div>c",
                            {L"a", L"b", L"c"});

  AssertMoveByUnitForMarkup(
      TextUnit_Line,
      "<h1>line one</h1><ul><li>line two</li><li>line three</li></ul>",
      {L"line one", L"• line two", L"• line three"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ExpandToLineCrossingBoundary) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <body>
          plain text <b>on <i>line</i></b><i><b> one<br>
          <span>next</span> <span>text</span> </b> on </i>line two<br>
          line three,
        </body>
      </html>)HTML");

  ui::BrowserAccessibility* start_of_second_line =
      FindNode(ax::mojom::Role::kStaticText, "next");
  ASSERT_NE(nullptr, start_of_second_line);

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*start_of_second_line, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());

  // Ensure ExpandToEnclosingUnit by Line both moves the start and end endpoints
  // appropriately (doesn't move to previous line for start and spans multiple
  // elements).
  text_range_provider->ExpandToEnclosingUnit(TextUnit_Line);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"next text on line two");

  ui::BrowserAccessibility* text_on_second_line =
      FindNode(ax::mojom::Role::kStaticText, "next");
  ASSERT_NE(nullptr, text_on_second_line);

  GetTextRangeProviderFromTextNode(*text_on_second_line, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());

  // Ensure ExpandToEnclosingUnit by Line moves the start past the anchor
  // boundary (but not past the start of the line).
  text_range_provider->ExpandToEnclosingUnit(TextUnit_Line);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"next text on line two");
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ExpandToParagraphCrossingBoundary) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <body>
          plain text <b>on <i>line</i></b><i><b> one<br>
          <span>next</span> <span>text</span> </b> on </i>line two<br>
          line three,
        </body>
      </html>)HTML");

  ui::BrowserAccessibility* first_bold_text =
      FindNode(ax::mojom::Role::kStaticText, "line two");
  ASSERT_NE(nullptr, first_bold_text);

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*first_bold_text, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());

  // Ensure ExpandToEnclosingUnit by Line both moves the start and end endpoints
  // appropriately.
  text_range_provider->ExpandToEnclosingUnit(TextUnit_Paragraph);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"next text on line two\n");
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ExpandToPageCrossingBoundary) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <body>
          plain text <b>on <i>line</i></b><i><b> one<br>
          <span>next</span> <span>text</span> </b> on </i>line two<br>
          line three,
        </body>
      </html>)HTML");

  ui::BrowserAccessibility* first_bold_text =
      FindNode(ax::mojom::Role::kStaticText, "next");
  ASSERT_NE(nullptr, first_bold_text);

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*first_bold_text, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());

  // Ensure ExpandToEnclosingUnit by Line both moves the start and end endpoints
  // appropriately.
  text_range_provider->ExpandToEnclosingUnit(TextUnit_Page);
  EXPECT_UIA_TEXTRANGE_EQ(
      text_range_provider,
      L"plain text on line one\nnext text on line two\nline three,");
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
      L"S",
      L"o",
      L"m",
      L"e",
      L" ",
      L"t",
      L"e",
      L"x",
      L"t",
      L"\n",
      kEmbeddedCharacterAsString.c_str(),
      L"\n",
      L"a",
      L"f",
      L"t",
      L"e",
      L"r"};

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
                       MoveByWordWithDialogAndButton) {
  const std::string html_markup = R"HTML(<!DOCTYPE html>
  <html>
    <body>
      <h1>One</h1>
      <h1>Two</h1>
      <div style="visibility:hidden">
        <div role="dialog">
          <div>
            <div>
              <button aria-label="close" type="button"></button>
            </div>
          </div>
        </div>
      </div>
      <h1>Three</h1>
    </body>
  </html>)HTML";

  const std::vector<const wchar_t*> words = {L"One", L"Two", L"Three"};

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

  ui::BrowserAccessibility* text_before_list =
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

  gfx::Vector2dF view_offset(text_before_list->manager()
                                 ->GetViewBoundsInScreenCoordinates()
                                 .OffsetFromOrigin());
  std::vector<double> expected_values = {85 + view_offset.x(),
                                         16 + view_offset.y(), 20, 17};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 19,
                  /*expected_text*/ L"t",
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
      L"First Heading", L"\nParagraph One", L"Second Heading",
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
      L"First Heading", L"\nParagraph One", L"Second Heading",
      L"\nParagraph Two"};

  AssertMoveByUnitForMarkup(TextUnit_Format, html_markup, format_units);
}

// Flaky.
// TODO(crbug.com/40721846): Re-enable.
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

  // Validate this selection with a waiter.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->Select());

  ASSERT_TRUE(waiter.WaitForNotification());
  ui::AXSelection selection = node->GetUnignoredSelection();
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

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ReplaceStartAndEndEndpointNodeInMultipleTrees) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/html/replaced-node-across-trees.html");
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  WaitForAccessibilityTreeToContainNodeWithName(web_contents, "Text in iframe");

  auto* before_frame_node =
      FindNode(ax::mojom::Role::kStaticText, "Before frame");
  ASSERT_NE(nullptr, before_frame_node);

  // 1. Test when |start_| and |end_| are both not in the iframe.
  {
    ComPtr<ITextRangeProvider> text_range_provider;
    GetTextRangeProviderFromTextNode(*before_frame_node, &text_range_provider);
    ASSERT_NE(nullptr, text_range_provider.Get());
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Before frame");

    AccessibilityNotificationWaiter waiter(web_contents);

    // Updating the style on that particular node is going to invalidate the
    // leaf text node and will replace it with a new one with the updated style.
    // We don't care about the style - we use it to trigger a node replacement.
    EXPECT_TRUE(ExecJs(
        web_contents,
        "document.getElementById('s1').style.outline = '1px solid black';"));

    ASSERT_TRUE(waiter.WaitForNotification());
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Before frame");
  }

  // 2. Test when |start_| is not in the iframe but |end_| is.
  {
    ComPtr<ITextRangeProvider> text_range_provider;
    GetTextRangeProviderFromTextNode(*before_frame_node, &text_range_provider);
    ASSERT_NE(nullptr, text_range_provider.Get());
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Before frame");

    // Move the range from "<B>efore frame<>" to "<B>efore frame/nText< >"
    EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
        text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Word,
        /*count*/ 1,
        /*expected_text*/ L"Before frame\nText ",
        /*expected_count*/ 1);

    AccessibilityNotificationWaiter waiter(web_contents);

    // Updating the style on that particular node is going to invalidate the
    // leaf text node and will replace it with a new one with the updated style.
    // We don't care about the style - we use it to trigger a node replacement.
    EXPECT_TRUE(ExecJs(
        web_contents,
        "document.getElementsByTagName('iframe')[0].contentWindow.document."
        "getElementById('s1').style.outline = '1px solid black';"));

    ASSERT_TRUE(waiter.WaitForNotification());
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Before frame\nText ");
  }

  // 3. Test when |start_| is in the iframe but |end_| is not.
  {
    ComPtr<ITextRangeProvider> text_range_provider;
    auto* after_frame_node =
        FindNode(ax::mojom::Role::kStaticText, "After frame");
    ASSERT_NE(nullptr, after_frame_node);
    GetTextRangeProviderFromTextNode(*after_frame_node, &text_range_provider);
    ASSERT_NE(nullptr, text_range_provider.Get());
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"After frame");

    // Move the range from "<B>efore frame<>" to "<B>efore frame/nText< >"
    EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
        text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Word,
        /*count*/ -1,
        /*expected_text*/ L"iframe\nAfter frame",
        /*expected_count*/ -1);

    AccessibilityNotificationWaiter waiter(web_contents, ui::kAXModeComplete,
                                           ax::mojom::Event::kEndOfTest);

    // Updating the style on that particular node is going to invalidate the
    // leaf text node and will replace it with a new one with the updated style.
    // We don't care about the style - we use it to trigger a node replacement.
    EXPECT_TRUE(ExecJs(
        web_contents,
        "document.getElementById('s2').style.outline = '1px solid black';"));

    GetManager()->SignalEndOfTest();
    ASSERT_TRUE(waiter.WaitForNotification());
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"iframe\nAfter frame");
  }
}

// Test that a page reload removes the AXTreeObserver from the AXTree's
// observers list. If it doesn't, this test will crash.
IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ReloadTreeShouldRemoveObserverFromTree) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/html/simple_spans.html");
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  WaitForAccessibilityTreeToContainNodeWithName(web_contents, "Some text");

  // 1. Reload the page and trigger a tree update - this should update the tree
  // id without modifying the observers from the tree.
  {
    auto* node = FindNode(ax::mojom::Role::kStaticText, "Some text");
    ASSERT_NE(nullptr, node);

    ComPtr<ITextRangeProvider> text_range_provider;
    GetTextRangeProviderFromTextNode(*node, &text_range_provider);
    ASSERT_NE(nullptr, text_range_provider.Get());
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Some text");
    ui::AXTreeID old_tree_id = GetManager()->GetTreeID();

    // Reloading changes the tree id, triggering an AXTreeManager replacement.
    shell()->Reload();

    AccessibilityNotificationWaiter waiter(
        web_contents, ui::kAXModeComplete,
        ui::AXEventGenerator::Event::FOCUS_CHANGED);

    // We do a style change here only to trigger an AXTree update - apparently,
    // a shell reload doesn't update the tree by itself.
    EXPECT_TRUE(ExecJs(
        web_contents,
        "document.getElementById('s1').style.outline = '1px solid black';"));

    ASSERT_TRUE(waiter.WaitForNotification());
    ASSERT_NE(old_tree_id, GetManager()->GetTreeID());

    // |text_range_provider| should now be invalid since it is using nodes
    // pointing to the previous tree id. If the tree id has not been updated
    // from the page reload, this should fail.
    base::win::ScopedSafearray children;
    ASSERT_UIA_ELEMENTNOTAVAILABLE(
        text_range_provider->GetChildren(children.Receive()));
  }

  // 2. Validate that the observer for the previous range has been removed. Also
  // test that the new observer has been added correctly.
  {
    auto* node = FindNode(ax::mojom::Role::kStaticText, "Some text");
    ASSERT_NE(nullptr, node);

    ComPtr<ITextRangeProvider> text_range_provider;
    GetTextRangeProviderFromTextNode(*node, &text_range_provider);
    ASSERT_NE(nullptr, text_range_provider.Get());
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Some text");

    // Make the range span the entire document.
    EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
        text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
        /*count*/ 1,
        /*expected_text*/ L"Some text 3.14159",
        /*expected_count*/ 1);

    AccessibilityNotificationWaiter waiter(web_contents, ui::kAXModeComplete,
                                           ax::mojom::Event::kEndOfTest);

    // We do a style change here only to trigger an AXTree update.
    EXPECT_TRUE(ExecJs(
        web_contents,
        "document.getElementById('s2').style.outline = '1px solid black';"));

    GetManager()->SignalEndOfTest();
    ASSERT_TRUE(waiter.WaitForNotification());

    // If the previous observer was not removed correctly, this will cause a
    // crash. If it was removed correctly and this EXPECT fails, it's likely
    // because the new observer has not been added as expected.
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Some text 3.14159");
  }
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetAttributeValueNormalizesClonedRange) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
        <body>
          <p>Text before list</p>
          <div role="listbox">
            <div role="option"><i aria-hidden="true">i</i>One</div>
          </div>
        </body>
      </html>)HTML");

  ui::BrowserAccessibility* text_before_list =
      FindNode(ax::mojom::Role::kStaticText, "Text before list");
  ASSERT_NE(nullptr, text_before_list);

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*text_before_list, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());

  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Format,
                  /*count*/ 1,
                  /*expected_text*/ L"One",
                  /*expected_count*/ 1);

  // GetAttributeValue calls NormalizeTextRange but should not modify the
  // internal endpoints. However, the value returned by GetAttributeValue should
  // still be computed from a normalized range.
  base::win::ScopedVariant value;
  EXPECT_HRESULT_SUCCEEDED(text_range_provider->GetAttributeValue(
      UIA_IsItalicAttributeId, value.Receive()));
  EXPECT_EQ(value.type(), VT_BOOL);
  EXPECT_EQ(V_BOOL(value.ptr()), 0);

  // The text should be the same as before since the internal endpoints didn't
  // move.
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"One");
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       DegenerateRangeBoundingRect) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/html/fixed-width-text.html");

  ui::BrowserAccessibility* text_node =
      FindNode(ax::mojom::Role::kStaticText, "Hello,");
  ASSERT_NE(nullptr, text_node);
  EXPECT_TRUE(text_node->IsLeaf());
  EXPECT_EQ(0u, text_node->PlatformChildCount());

  // |view_offset| is necessary to account for differences in the shell
  // between platforms (e.g. title bar height) because the results of
  // |GetBoundingRectangles| are in screen coordinates.
  gfx::Vector2dF view_offset(text_node->manager()
                                 ->GetViewBoundsInScreenCoordinates()
                                 .OffsetFromOrigin());

  // The offset from top based on CSS style absolute position (200px) + viewport
  // offset.
  const double total_top_offset = 216 + view_offset.y();
  // The offset from left based on CSS style absolute position (100px) +
  // viewport offset.
  const double total_left_offset = 100 + view_offset.x();

  // The bounding box for character width and height with font-size: 11px.
  constexpr double bounding_box_char_height = 16;
  constexpr double bounding_box_char_width = 32;

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(*text_node, &text_range_provider);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Hello,");
  base::win::ScopedSafearray rectangles;
  std::vector<double> expected_values = {total_left_offset, total_top_offset,
                                         6 * bounding_box_char_width,
                                         bounding_box_char_height};
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  // Range spans character "H".
  // |-|
  //  H e l l o ,
  //  W o r l d
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ -5,
      /*expected_text*/ L"H",
      /*expected_count*/ -5);

  expected_values = {total_left_offset, total_top_offset,
                     bounding_box_char_width, bounding_box_char_height};
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  // Range is degenerate and position is before "H".
  // ||
  //  H e l l o ,
  //  W o r l d
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ -1,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  expected_values = {total_left_offset, total_top_offset, 1,
                     bounding_box_char_height};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  // Range is degenerate and position is after ",".
  //             ||
  //  H e l l o ,
  //  W o r l d
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 6,
                  /*expected_text*/ L"",
                  /*expected_count*/ 6);
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  expected_values = {total_left_offset + 6 * bounding_box_char_width,
                     total_top_offset, 1, bounding_box_char_height};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  // Range spans character ",".
  //           |-|
  //  H e l l o ,
  //  W o r l d
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ -1,
      /*expected_text*/ L",",
      /*expected_count*/ -1);
  expected_values = {total_left_offset + 5 * bounding_box_char_width,
                     total_top_offset, bounding_box_char_width,
                     bounding_box_char_height};
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  // Range spans character "\n".
  //             |-|
  //  H e l l o ,
  //  W o r l d
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 1,
                  /*expected_text*/ L"\n",
                  /*expected_count*/ 1);
  expected_values = {};
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  // Range spans character "W".
  //  H e l l o ,
  //  W o r l d
  // |-|
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 1,
                  /*expected_text*/ L"W",
                  /*expected_count*/ 1);
  expected_values = {total_left_offset,
                     total_top_offset + bounding_box_char_height,
                     bounding_box_char_width, bounding_box_char_height};
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  // Range is degenerate and position is before "W".
  //  H e l l o ,
  //  W o r l d
  // ||
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ -1,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
  expected_values = {total_left_offset,
                     total_top_offset + bounding_box_char_height, 1,
                     bounding_box_char_height};
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  // Range is degenerate and position is after "d".
  //  H e l l o ,
  //  W o r l d
  //           ||
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 5,
                  /*expected_text*/ L"",
                  /*expected_count*/ 5);
  expected_values = {total_left_offset + 5 * bounding_box_char_width,
                     total_top_offset + bounding_box_char_height, 1,
                     bounding_box_char_height};
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);

  // Range spans character "d".
  //  H e l l o ,
  //  W o r l d
  //         |-|
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ -1,
      /*expected_text*/ L"d",
      /*expected_count*/ -1);
  expected_values = {total_left_offset + 4 * bounding_box_char_width,
                     total_top_offset + bounding_box_char_height,
                     bounding_box_char_width, bounding_box_char_height};
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);
}

// TODO(crbug.com/340389557): This test is flaky.
IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       DISABLED_TextDeletedInTextFieldAdjustmentNeeded) {
  // This test, tests a scenario where an AT is used to make a deletion on some
  // text and then manually moves the caret around: On the input "hello world
  // red green<> blue" where the caret position is noted by <>:
  // 1. The AT creates a degenerate text range provider on the caret position
  // 2. The AT selects "world" and then simulates a backspace to delete the
  // word.
  // 3. The AT moves the caret back to the original position (right after
  // "green")
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<input id='input' type='text' value='hello world red green blue'/>)HTML");

  auto* node =
      FindNode(ax::mojom::Role::kTextField, "hello world red green blue");
  ASSERT_NE(nullptr, node);

  ComPtr<ITextRangeProvider> original_text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &original_text_range_provider);
  ASSERT_NE(nullptr, original_text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(original_text_range_provider,
                          L"hello world red green blue");

  ComPtr<ITextRangeProvider> deletion_text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &deletion_text_range_provider);
  ASSERT_NE(nullptr, deletion_text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(deletion_text_range_provider,
                          L"hello world red green blue");

  base::win::ScopedBstr find_string(L"world");
  EXPECT_HRESULT_SUCCEEDED(original_text_range_provider->FindText(
      find_string.Get(), false, false, &deletion_text_range_provider));

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(original_text_range_provider,
                                   TextPatternRangeEndpoint_Start,
                                   TextUnit_Word,
                                   /*count*/ 1,
                                   /*expected_text*/ L"world red green blue",
                                   /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(original_text_range_provider,
                                   TextPatternRangeEndpoint_Start,
                                   TextUnit_Word,
                                   /*count*/ 1,
                                   /*expected_text*/ L"red green blue",
                                   /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(original_text_range_provider,
                                   TextPatternRangeEndpoint_Start,
                                   TextUnit_Word,
                                   /*count*/ 1,
                                   /*expected_text*/ L"green blue",
                                   /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(original_text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ -1,
                                   /*expected_text*/ L"green ",
                                   /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(original_text_range_provider,
                                   TextPatternRangeEndpoint_Start,
                                   TextUnit_Character,
                                   /*count*/ 5,
                                   /*expected_text*/ L" ",
                                   /*expected_count*/ 5);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(original_text_range_provider,
                                   TextPatternRangeEndpoint_End,
                                   TextUnit_Character,
                                   /*count*/ -1,
                                   /*expected_text*/ L"",
                                   /*expected_count*/ -1);

  // At this point, `original_text_range_provider` should be a degenerate range
  // acting as the caret here: "hello red green<> blue" and
  // `deletion_text_range_provider` should have "<w>orld<>".

  EXPECT_HRESULT_SUCCEEDED(deletion_text_range_provider->Select());

  // Now we delete "world".
  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  SimulateKeyPress(shell()->web_contents(), ui::DomKey::BACKSPACE,
                   ui::DomCode::BACKSPACE, ui::VKEY_BACK, false, false, false,
                   false);
  ASSERT_TRUE(waiter.WaitForNotification());

  // Since our deletion text range provider is also an observer to the deletion
  // event, it will be set to nullposition after the deletion, since by design
  // we return nullpositions when the deletion range encompasses the observing
  // endpoints. As such, we create this textrange provider that mimmicks where
  // blink would set the selection after the deletion is done.
  ComPtr<ITextRangeProvider> blink_selection_text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &blink_selection_text_range_provider);
  ASSERT_NE(nullptr, blink_selection_text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(blink_selection_text_range_provider,
                          L"hello  red green blue");
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(blink_selection_text_range_provider,
                                   TextPatternRangeEndpoint_Start,
                                   TextUnit_Character,
                                   /*count*/ 6,
                                   /*expected_text*/ L" red green blue",
                                   /*expected_count*/ 6);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(blink_selection_text_range_provider,
                                   TextPatternRangeEndpoint_End,
                                   TextUnit_Character,
                                   /*count*/ -15,
                                   /*expected_text*/ L"",
                                   /*expected_count*/ -15);

  // Now we move the `deletion` text range to the original, which was the
  // original caret position.
  EXPECT_HRESULT_SUCCEEDED(
      blink_selection_text_range_provider->MoveEndpointByRange(
          TextPatternRangeEndpoint_Start, original_text_range_provider.Get(),
          TextPatternRangeEndpoint_Start));
  EXPECT_HRESULT_SUCCEEDED(
      blink_selection_text_range_provider->MoveEndpointByRange(
          TextPatternRangeEndpoint_End, original_text_range_provider.Get(),
          TextPatternRangeEndpoint_End));

  EXPECT_UIA_TEXTRANGE_EQ(blink_selection_text_range_provider, L"");

  // Since the original caret position was right after "green" if we now expand
  // the resulting range we would expect to have "green" here as well.
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(blink_selection_text_range_provider,
                                   TextPatternRangeEndpoint_Start,
                                   TextUnit_Character,
                                   /*count*/ -5,
                                   /*expected_text*/ L"green",
                                   /*expected_count*/ -5);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       TextDeletedInTextFieldAdjustmentNeededNodeDeleted) {
  // This test, tests a scenario where an AT is used to make a deletion on some
  // text and then manually moves the caret around: On the input "go hello
  // <>blue" where the caret position is noted by <>: This covers the scenario
  // where the deleted text actually deletes a node in the DOM.
  // 1. The AT creates a degenerate text range provider on the caret position
  // 2. The AT selects "hello" and then simulates a backspace to delete the
  // word.
  // 3. The AT moves the caret back to the original position (right before
  // "blue")
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<div id='input' contenteditable='true'>go <span>hello</span> blue<div/>)HTML");

  auto* node = FindNode(ax::mojom::Role::kGenericContainer, "go hello blue");
  ASSERT_NE(nullptr, node);

  ComPtr<ITextRangeProvider> original_text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &original_text_range_provider);
  ASSERT_NE(nullptr, original_text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(original_text_range_provider, L"go hello blue");

  ComPtr<ITextRangeProvider> deletion_text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &deletion_text_range_provider);
  ASSERT_NE(nullptr, deletion_text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(deletion_text_range_provider, L"go hello blue");

  base::win::ScopedBstr find_string(L"hello");
  EXPECT_HRESULT_SUCCEEDED(original_text_range_provider->FindText(
      find_string.Get(), false, false, &deletion_text_range_provider));

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(original_text_range_provider,
                                   TextPatternRangeEndpoint_Start,
                                   TextUnit_Word,
                                   /*count*/ 1,
                                   /*expected_text*/ L"hello blue",
                                   /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(original_text_range_provider,
                                   TextPatternRangeEndpoint_Start,
                                   TextUnit_Word,
                                   /*count*/ 1,
                                   /*expected_text*/ L"blue",
                                   /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(original_text_range_provider,
                                   TextPatternRangeEndpoint_End,
                                   TextUnit_Character,
                                   /*count*/ -4,
                                   /*expected_text*/ L"",
                                   /*expected_count*/ -4);

  // At this point, `original_text_range_provider` should be a degenerate range
  // acting as the caret here: "go hello <>blue" and
  // `deletion_text_range_provider` should have "<h>ello<>".

  AccessibilityNotificationWaiter sel_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kDocumentSelectionChanged);

  EXPECT_HRESULT_SUCCEEDED(deletion_text_range_provider->Select());

  ASSERT_TRUE(sel_waiter.WaitForNotification());

  // Now we delete "hello".
  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  SimulateKeyPress(shell()->web_contents(), ui::DomKey::BACKSPACE,
                   ui::DomCode::BACKSPACE, ui::VKEY_BACK, /* control */ false,
                   /* shift */ false, /* alt */ false,
                   /* command */ false);
  ASSERT_TRUE(waiter.WaitForNotification());

  // Since our deletion text range provider is also an observer to the deletion
  // event, it will be set to nullposition after the deletion, since by design
  // we return nullpositions when the deletion range encompasses the observing
  // endpoints. As such, we create this textrange provider that mimmicks where
  // blink would set the selection after the deletion is done.
  ComPtr<ITextRangeProvider> blink_selection_text_range_provider;
  GetTextRangeProviderFromTextNode(*node, &blink_selection_text_range_provider);
  ASSERT_NE(nullptr, blink_selection_text_range_provider.Get());
  wchar_t text[11] = L"go ";
  wchar_t non_breaking_space[2] = L"\xA0";
  wchar_t blue[5] = L"blue";
  wcscat(text, non_breaking_space);
  wcscat(text, blue);
  wchar_t text_2[7] = L"\xA0";
  wcscat(text_2, blue);
  EXPECT_UIA_TEXTRANGE_EQ(blink_selection_text_range_provider, text);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(blink_selection_text_range_provider,
                                   TextPatternRangeEndpoint_Start,
                                   TextUnit_Character,
                                   /*count*/ 3,
                                   /*expected_text*/ text_2,
                                   /*expected_count*/ 3);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(blink_selection_text_range_provider,
                                   TextPatternRangeEndpoint_End,
                                   TextUnit_Character,
                                   /*count*/ -5,
                                   /*expected_text*/ L"",
                                   /*expected_count*/ -5);

  // Now we move the 'deletion' text range to the original, which was the
  // original caret position.
  EXPECT_HRESULT_SUCCEEDED(
      blink_selection_text_range_provider->MoveEndpointByRange(
          TextPatternRangeEndpoint_Start, original_text_range_provider.Get(),
          TextPatternRangeEndpoint_Start));
  EXPECT_HRESULT_SUCCEEDED(
      blink_selection_text_range_provider->MoveEndpointByRange(
          TextPatternRangeEndpoint_End, original_text_range_provider.Get(),
          TextPatternRangeEndpoint_End));

  EXPECT_UIA_TEXTRANGE_EQ(blink_selection_text_range_provider, L"");

  // Since the original caret position was right after "blue" if we now expand
  // the resulting range we would expect to have "blue" here as well, and the
  // full text would be "go blue".
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(blink_selection_text_range_provider,
                                   TextPatternRangeEndpoint_End,
                                   TextUnit_Character,
                                   /*count*/ 4,
                                   /*expected_text*/ L"blue",
                                   /*expected_count*/ 4);
}

}  // namespace content
