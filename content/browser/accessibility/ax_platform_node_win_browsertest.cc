// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win.h"

#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_content_browsertest.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_com_win.h"
#include "ui/accessibility/platform/uia_registrar_win.h"

using base::win::ScopedVariant;
using Microsoft::WRL::ComPtr;

namespace content {

#define EXPECT_UIA_INT_EQ(node, property_id, expected)              \
  {                                                                 \
    base::win::ScopedVariant expectedVariant(expected);             \
    ASSERT_EQ(VT_I4, expectedVariant.type());                       \
    base::win::ScopedVariant actual;                                \
    ASSERT_HRESULT_SUCCEEDED(                                       \
        node->GetPropertyValue(property_id, actual.Receive()));     \
    EXPECT_EQ(expectedVariant.ptr()->intVal, actual.ptr()->intVal); \
  }

#define EXPECT_UIA_BSTR_EQ(node, property_id, expected)                  \
  {                                                                      \
    ScopedVariant expectedVariant(expected);                             \
    ASSERT_EQ(VT_BSTR, expectedVariant.type());                          \
    ASSERT_NE(nullptr, expectedVariant.ptr()->bstrVal);                  \
    ScopedVariant actual;                                                \
    ASSERT_HRESULT_SUCCEEDED(                                            \
        node->GetPropertyValue(property_id, actual.Receive()));          \
    ASSERT_EQ(VT_BSTR, actual.type());                                   \
    ASSERT_NE(nullptr, actual.ptr()->bstrVal);                           \
    EXPECT_STREQ(expectedVariant.ptr()->bstrVal, actual.ptr()->bstrVal); \
  }

class AXPlatformNodeWinBrowserTest : public AccessibilityContentBrowserTest {
 protected:
  template <typename T>
  ComPtr<T> QueryInterfaceFromNode(
      ui::BrowserAccessibility* browser_accessibility) {
    ComPtr<T> result;
    EXPECT_HRESULT_SUCCEEDED(
        browser_accessibility->GetNativeViewAccessible()->QueryInterface(
            __uuidof(T), &result));
    return result;
  }

  ComPtr<IAccessible> IAccessibleFromNode(
      ui::BrowserAccessibility* browser_accessibility) {
    return QueryInterfaceFromNode<IAccessible>(browser_accessibility);
  }

  ComPtr<IAccessible2> ToIAccessible2(ComPtr<IAccessible> accessible) {
    CHECK(accessible);
    ComPtr<IServiceProvider> service_provider;
    accessible.As(&service_provider);
    ComPtr<IAccessible2> result;
    CHECK(SUCCEEDED(service_provider->QueryService(IID_IAccessible2,
                                                   IID_PPV_ARGS(&result))));
    return result;
  }

  ui::BrowserAccessibility* FindNodeAfter(ui::BrowserAccessibility* begin,
                                          const std::string& name) {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    ui::BrowserAccessibilityManager* manager =
        web_contents->GetRootBrowserAccessibilityManager();
    ui::BrowserAccessibility* node = begin;
    while (node && (node->GetName() != name)) {
      node = manager->NextInTreeOrder(node);
    }

    return node;
  }

  void UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      const ui::BrowserAccessibility* target_browser_accessibility,
      const std::vector<std::string>& expected_names) {
    ASSERT_NE(nullptr, target_browser_accessibility);

    auto* target_browser_accessibility_com_win =
        ToBrowserAccessibilityWin(target_browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, target_browser_accessibility_com_win);

    base::win::ScopedVariant flows_from_variant;
    target_browser_accessibility_com_win->GetPropertyValue(
        UIA_FlowsFromPropertyId, flows_from_variant.Receive());
    ASSERT_EQ(VT_ARRAY | VT_UNKNOWN, flows_from_variant.type());
    ASSERT_EQ(1u, SafeArrayGetDim(V_ARRAY(flows_from_variant.ptr())));

    LONG lower_bound, upper_bound, size;
    ASSERT_HRESULT_SUCCEEDED(
        SafeArrayGetLBound(V_ARRAY(flows_from_variant.ptr()), 1, &lower_bound));
    ASSERT_HRESULT_SUCCEEDED(
        SafeArrayGetUBound(V_ARRAY(flows_from_variant.ptr()), 1, &upper_bound));
    size = upper_bound - lower_bound + 1;
    ASSERT_EQ(static_cast<LONG>(expected_names.size()), size);

    std::vector<std::string> names;
    for (LONG i = 0; i < size; ++i) {
      ComPtr<IUnknown> unknown_element;
      ASSERT_HRESULT_SUCCEEDED(
          SafeArrayGetElement(V_ARRAY(flows_from_variant.ptr()), &i,
                              static_cast<void**>(&unknown_element)));
      ASSERT_NE(nullptr, unknown_element);

      ComPtr<IRawElementProviderSimple> raw_element_provider_simple = nullptr;
      ASSERT_HRESULT_SUCCEEDED(
          unknown_element.As(&raw_element_provider_simple));
      ASSERT_NE(nullptr, raw_element_provider_simple);

      base::win::ScopedVariant name;
      ASSERT_HRESULT_SUCCEEDED(raw_element_provider_simple->GetPropertyValue(
          UIA_NamePropertyId, name.Receive()));
      ASSERT_EQ(VT_BSTR, name.type());
      names.push_back(base::WideToUTF8(
          std::wstring(V_BSTR(name.ptr()), SysStringLen(V_BSTR(name.ptr())))));
    }

    ASSERT_THAT(names, ::testing::UnorderedElementsAreArray(expected_names));
  }

  void UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role expected_role,
      ui::BrowserAccessibility* (ui::BrowserAccessibility::*f)(size_t) const,
      size_t index_arg,
      bool expected_is_modal,
      bool expected_is_window_provider_available) {
    ui::BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();
    ui::BrowserAccessibilityComWin* root_browser_accessibility_com_win =
        ToBrowserAccessibilityWin(root_browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, root_browser_accessibility_com_win);

    ui::BrowserAccessibility* browser_accessibility =
        (root_browser_accessibility->*f)(index_arg);
    ASSERT_NE(nullptr, browser_accessibility);
    ASSERT_EQ(expected_role, browser_accessibility->GetRole());
    ui::BrowserAccessibilityComWin* browser_accessibility_com_win =
        ToBrowserAccessibilityWin(browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, browser_accessibility_com_win);

    ComPtr<IWindowProvider> window_provider = nullptr;
    ASSERT_HRESULT_SUCCEEDED(browser_accessibility_com_win->GetPatternProvider(
        UIA_WindowPatternId, &window_provider));
    if (expected_is_window_provider_available) {
      ASSERT_NE(nullptr, window_provider.Get());

      BOOL is_modal = FALSE;
      ASSERT_HRESULT_SUCCEEDED(window_provider->get_IsModal(&is_modal));
      ASSERT_EQ(expected_is_modal, is_modal);
    } else {
      ASSERT_EQ(nullptr, window_provider.Get());
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kEnableAccessibilityAriaVirtualContent};
};

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       IA2ScrollToPointIframeText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/iframe-text.html");
  WaitForAccessibilityTreeToContainNodeWithName(
      shell()->web_contents(),
      "Game theory is \"the study of Mathematical model mathematical models of "
      "conflict and cooperation between intelligent rational decision-makers."
      "\"");

  ui::BrowserAccessibility* browser_accessibility =
      GetRootAndAssertNonNull()->PlatformDeepestLastChild();
  ASSERT_NE(nullptr, browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kStaticText, browser_accessibility->GetRole());

  ui::BrowserAccessibility* iframe_browser_accessibility =
      browser_accessibility->manager()->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, iframe_browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea,
            iframe_browser_accessibility->GetRole());

  gfx::Rect iframe_screen_bounds = iframe_browser_accessibility->GetBoundsRect(
      ui::AXCoordinateSystem::kScreenDIPs, ui::AXClippingBehavior::kUnclipped);

  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);
  ComPtr<IAccessible2> root_iaccessible2 =
      ToIAccessible2(IAccessibleFromNode(browser_accessibility));
  ASSERT_EQ(S_OK, root_iaccessible2->scrollToPoint(
                      IA2_COORDTYPE_SCREEN_RELATIVE, iframe_screen_bounds.x(),
                      iframe_screen_bounds.y()));
  ASSERT_TRUE(location_changed_waiter.WaitForNotification());

  gfx::Rect bounds = browser_accessibility->GetBoundsRect(
      ui::AXCoordinateSystem::kScreenDIPs, ui::AXClippingBehavior::kUnclipped);
  ASSERT_EQ(iframe_screen_bounds.y(), bounds.y());
}

class AXPlatformNodeWinUIABrowserTest : public AXPlatformNodeWinBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{::features::kUiaProvider};
};

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAGetPropertyValueFlowsFromNone) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/aria/aria-label.html");

  base::win::ScopedVariant flows_from_variant;
  ComPtr<IRawElementProviderSimple> node_provider =
      QueryInterfaceFromNode<IRawElementProviderSimple>(
          FindNode(ax::mojom::Role::kCheckBox, "aria label"));
  node_provider->GetPropertyValue(UIA_FlowsFromPropertyId,
                                  flows_from_variant.Receive());
  ASSERT_EQ(VT_ARRAY | VT_UNKNOWN, flows_from_variant.type());
  ASSERT_EQ(nullptr, V_ARRAY(flows_from_variant.ptr()));
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAGetPropertyValueFlowsFromSingle) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/aria/aria-flowto.html");

  UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      FindNode(ax::mojom::Role::kFooter, "next"), {"current"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAGetPropertyValueFlowsFromMultiple) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/aria/aria-flowto-multiple.html");

  UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      FindNode(ax::mojom::Role::kGenericContainer, "b3"), {"a3", "c3"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest, UIANamePropertyValue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <ol>
          <li>list item 1</li>
          <li></li>
          <li>before <div><span>problem</span></div>, after</li>
          <li>before <a href="https://blah.com">problem</a>, after</li>
          <li aria-label="from author">from content</li>
        </ol>
      </html>
  )HTML"));
  ui::BrowserAccessibility* list_node =
      GetRootAndAssertNonNull()->PlatformGetChild(0);
  ui::BrowserAccessibility* item_node = list_node->PlatformGetChild(0);
  ASSERT_NE(nullptr, item_node);
  EXPECT_UIA_BSTR_EQ(ToBrowserAccessibilityWin(item_node)->GetCOM(),
                     UIA_NamePropertyId, L"list item 1");

  // Empty string as name should correspond to empty <li>.
  item_node = list_node->PlatformGetChild(1);
  ASSERT_NE(nullptr, item_node);
  EXPECT_UIA_BSTR_EQ(ToBrowserAccessibilityWin(item_node)->GetCOM(),
                     UIA_NamePropertyId, L"");

  //  <li> with complex structure and text.
  item_node = list_node->PlatformGetChild(2);
  ASSERT_NE(nullptr, item_node);
  EXPECT_UIA_BSTR_EQ(ToBrowserAccessibilityWin(item_node)->GetCOM(),
                     UIA_NamePropertyId, L"beforeproblem, after");

  // <li> with a link inside
  item_node = list_node->PlatformGetChild(3);
  ASSERT_NE(nullptr, item_node);
  EXPECT_UIA_BSTR_EQ(ToBrowserAccessibilityWin(item_node)->GetCOM(),
                     UIA_NamePropertyId, L"before problem, after");

  // <li> with name specified by the author rather than by the contents.
  item_node = list_node->PlatformGetChild(4);
  ASSERT_NE(nullptr, item_node);
  EXPECT_UIA_BSTR_EQ(ToBrowserAccessibilityWin(item_node)->GetCOM(),
                     UIA_NamePropertyId, L"from author");
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDialog) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body role="none">
          <dialog open>Example Text</dialog>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &ui::BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDialogAriaModalFalse) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body role="none">
          <dialog open aria-modal="false">Example Text</dialog>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &ui::BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDialogAriaModalTrue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body role="none">
          <dialog open aria-modal="true">Example Text</dialog>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &ui::BrowserAccessibility::PlatformGetChild, 0,
      true, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDiv) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div>Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &ui::BrowserAccessibility::PlatformGetChild, 0, false, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDivAriaModalFalse) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div aria-modal="false">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &ui::BrowserAccessibility::PlatformGetChild, 0, false, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDivAriaModalTrue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div aria-modal="true">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &ui::BrowserAccessibility::PlatformGetChild, 0, false, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDivDialog) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="dialog">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &ui::BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDivDialogAriaModalFalse) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="dialog" aria-modal="false">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &ui::BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDivDialogAriaModalTrue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="dialog" aria-modal="true">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &ui::BrowserAccessibility::PlatformGetChild, 0,
      true, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAIWindowProviderGetIsModalOnDivAlertDialog) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="alertdialog">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kAlertDialog,
      &ui::BrowserAccessibility::PlatformGetChild, 0, false, true);
}

IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeWinUIABrowserTest,
    UIAIWindowProviderGetIsModalOnDivAlertDialogAriaModalFalse) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="alertdialog" aria-modal="false">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kAlertDialog,
      &ui::BrowserAccessibility::PlatformGetChild, 0, false, true);
}

IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeWinUIABrowserTest,
    UIAIWindowProviderGetIsModalOnDivAlertDialogAriaModalTrue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="alertdialog" aria-modal="true">Example Text</div>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kAlertDialog,
      &ui::BrowserAccessibility::PlatformGetChild, 0, true, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAGetPropertyValueAutomationId) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div id="id"></div>
        </body>
      </html>
  )HTML"));

  ui::BrowserAccessibility* root_browser_accessibility =
      GetRootAndAssertNonNull();
  ui::BrowserAccessibilityComWin* root_browser_accessibility_com_win =
      ToBrowserAccessibilityWin(root_browser_accessibility)->GetCOM();
  ASSERT_NE(nullptr, root_browser_accessibility_com_win);

  ui::BrowserAccessibility* browser_accessibility =
      root_browser_accessibility->PlatformDeepestLastChild();
  ASSERT_NE(nullptr, browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer,
            browser_accessibility->GetRole());
  ui::BrowserAccessibilityComWin* browser_accessibility_com_win =
      ToBrowserAccessibilityWin(browser_accessibility)->GetCOM();
  ASSERT_NE(nullptr, browser_accessibility_com_win);

  base::win::ScopedVariant expected_scoped_variant;
  expected_scoped_variant.Set(SysAllocString(L"id"));
  base::win::ScopedVariant scoped_variant;
  EXPECT_HRESULT_SUCCEEDED(browser_accessibility_com_win->GetPropertyValue(
      UIA_AutomationIdPropertyId, scoped_variant.Receive()));
  EXPECT_EQ(0, expected_scoped_variant.Compare(scoped_variant));
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAGetPropertyValueNonEmptyAutomationIdOnRootWebArea) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <button></button>
        </body>
      </html>
  )HTML"));

  ui::BrowserAccessibility* root_browser_accessibility =
      GetRootAndAssertNonNull();
  ASSERT_NE(nullptr, root_browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea,
            root_browser_accessibility->GetRole());

  ui::BrowserAccessibilityComWin* root_browser_accessibility_com_win =
      ToBrowserAccessibilityWin(root_browser_accessibility)->GetCOM();
  ASSERT_NE(nullptr, root_browser_accessibility_com_win);

  // kRootWebArea nodes should not be empty. Some UIA clients appear to rely on
  // whether it's empty or not. See https://crbug.com/40065516#comment32.
  base::win::ScopedVariant expected_scoped_variant;
  expected_scoped_variant.Set(SysAllocString(L"RootWebArea"));
  base::win::ScopedVariant scoped_variant;
  EXPECT_HRESULT_SUCCEEDED(root_browser_accessibility_com_win->GetPropertyValue(
      UIA_AutomationIdPropertyId, scoped_variant.Receive()));
  EXPECT_EQ(0, expected_scoped_variant.Compare(scoped_variant));
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest, UIAScrollIntoView) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div id="id" style="height: 20px; overflow: scroll;">
            <ul>
              <li>Item 1</li>
              <li>Item 2</li>
              <li>Item 3</li>
              <li>Item 4</li>
              <li>Item 5</li>
              <li>Item 6</li>
            </ul>
          </div>
        </body>
      </html>
  )HTML"));

  ui::BrowserAccessibility* root_browser_accessibility =
      GetRootAndAssertNonNull();
  ui::BrowserAccessibilityComWin* root_browser_accessibility_com_win =
      ToBrowserAccessibilityWin(root_browser_accessibility)->GetCOM();
  ASSERT_NE(nullptr, root_browser_accessibility_com_win);

  ui::BrowserAccessibility* browser_accessibility =
      root_browser_accessibility->PlatformDeepestLastChild();
  ASSERT_NE(nullptr, browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kStaticText, browser_accessibility->GetRole());
  ui::BrowserAccessibilityComWin* browser_accessibility_com_win =
      ToBrowserAccessibilityWin(browser_accessibility)->GetCOM();
  ASSERT_NE(nullptr, browser_accessibility_com_win);

  ui::AXPlatformNodeWin* platform_node = static_cast<ui::AXPlatformNodeWin*>(
      ui::AXPlatformNode::FromNativeViewAccessible(
          browser_accessibility->GetNativeViewAccessible()));
  ASSERT_NE(nullptr, platform_node);

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLocationChanged);
  EXPECT_HRESULT_SUCCEEDED(platform_node->ScrollIntoView());
  ASSERT_TRUE(waiter.WaitForNotification());
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAGetPropertyValueCulture) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div lang='en-us'>en-us</div>
          <div lang='en-gb'>en-gb</div>
          <div lang='ru-ru'>ru-ru</div>
          <div lang='fake'>fake</div>
          <div>no lang</div>
          <div lang=''>empty lang</div>
        </body>
      </html>
  )HTML"));

  ui::BrowserAccessibility* root_node = GetRootAndAssertNonNull();
  ui::BrowserAccessibility* body_node = root_node->PlatformGetFirstChild();
  ASSERT_NE(nullptr, body_node);

  ui::BrowserAccessibility* node = FindNodeAfter(body_node, "en-us");
  ASSERT_NE(nullptr, node);
  ui::BrowserAccessibilityComWin* en_us_node_com_win =
      ToBrowserAccessibilityWin(node)->GetCOM();
  ASSERT_NE(nullptr, en_us_node_com_win);
  constexpr int en_us_lcid = 1033;
  EXPECT_UIA_INT_EQ(en_us_node_com_win, UIA_CulturePropertyId, en_us_lcid);

  node = FindNodeAfter(node, "en-gb");
  ASSERT_NE(nullptr, node);
  ui::BrowserAccessibilityComWin* en_gb_node_com_win =
      ToBrowserAccessibilityWin(node)->GetCOM();
  ASSERT_NE(nullptr, en_gb_node_com_win);
  constexpr int en_gb_lcid = 2057;
  EXPECT_UIA_INT_EQ(en_gb_node_com_win, UIA_CulturePropertyId, en_gb_lcid);

  node = FindNodeAfter(node, "ru-ru");
  ASSERT_NE(nullptr, node);
  ui::BrowserAccessibilityComWin* ru_ru_node_com_win =
      ToBrowserAccessibilityWin(node)->GetCOM();
  ASSERT_NE(nullptr, ru_ru_node_com_win);
  constexpr int ru_ru_lcid = 1049;
  EXPECT_UIA_INT_EQ(ru_ru_node_com_win, UIA_CulturePropertyId, ru_ru_lcid);

  // Setting to an invalid language should return a failed HRESULT.
  node = FindNodeAfter(node, "fake");
  ASSERT_NE(nullptr, node);
  ui::BrowserAccessibilityComWin* fake_lang_node_com_win =
      ToBrowserAccessibilityWin(node)->GetCOM();
  ASSERT_NE(nullptr, fake_lang_node_com_win);
  base::win::ScopedVariant actual_value;
  EXPECT_HRESULT_FAILED(fake_lang_node_com_win->GetPropertyValue(
      UIA_CulturePropertyId, actual_value.Receive()));

  // No lang should default to the page's default language (en-us).
  node = FindNodeAfter(node, "no lang");
  ASSERT_NE(nullptr, node);
  ui::BrowserAccessibilityComWin* no_lang_node_com_win =
      ToBrowserAccessibilityWin(node)->GetCOM();
  ASSERT_NE(nullptr, no_lang_node_com_win);
  EXPECT_UIA_INT_EQ(no_lang_node_com_win, UIA_CulturePropertyId, en_us_lcid);

  // Empty lang should default to the page's default language (en-us).
  node = FindNodeAfter(node, "empty lang");
  ASSERT_NE(nullptr, node);
  ui::BrowserAccessibilityComWin* empty_lang_node_com_win =
      ToBrowserAccessibilityWin(node)->GetCOM();
  ASSERT_NE(nullptr, empty_lang_node_com_win);
  EXPECT_UIA_INT_EQ(empty_lang_node_com_win, UIA_CulturePropertyId, en_us_lcid);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinUIABrowserTest,
                       UIAGetPropertyValueVirtualContent) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role="group" aria-virtualcontent="block-end"
               aria-label="vc">Hello World</div>
        </body>
      </html>
  )HTML"));

  ui::BrowserAccessibility* root_node = GetRootAndAssertNonNull();
  ui::BrowserAccessibility* body_node = root_node->PlatformGetFirstChild();
  ASSERT_NE(nullptr, body_node);

  ui::BrowserAccessibility* node = FindNode(ax::mojom::Role::kGroup, "vc");
  ASSERT_NE(nullptr, node);
  ui::BrowserAccessibilityComWin* node_com_win =
      ToBrowserAccessibilityWin(node)->GetCOM();
  ASSERT_NE(nullptr, node_com_win);

  EXPECT_UIA_BSTR_EQ(
      node_com_win,
      ui::UiaRegistrarWin::GetInstance().GetVirtualContentPropertyId(),
      L"block-end");
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       HitTestOnAncestorOfWebRoot) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Load the page.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html><head><title>Accessibility Test</title></head>"
      "<body>"
      "<button>This is a button</button>"
      "</body></html>";
  GURL url(url_str);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();

  // Find a node to hit test. Note that this is a really simple page,
  // so synchronous hit testing will work fine.
  ui::BrowserAccessibility* node = manager->GetBrowserAccessibilityRoot();
  while (node && node->GetRole() != ax::mojom::Role::kButton) {
    node = manager->NextInTreeOrder(node);
  }
  DCHECK(node);

  // Get the screen bounds of the hit target and find the point in the middle.
  gfx::Rect bounds = node->GetClippedScreenBoundsRect();
  gfx::Point point = bounds.CenterPoint();

  // Get the root AXPlatformNodeWin.
  ui::AXPlatformNodeWin* root_platform_node =
      static_cast<ui::AXPlatformNodeWin*>(
          ui::AXPlatformNode::FromNativeViewAccessible(
              manager->GetBrowserAccessibilityRoot()
                  ->GetNativeViewAccessible()));

  // First test that calling accHitTest on the root node returns the button.
  {
    base::win::ScopedVariant hit_child_variant;
    ASSERT_EQ(S_OK, root_platform_node->accHitTest(
                        point.x(), point.y(), hit_child_variant.Receive()));
    ASSERT_EQ(VT_DISPATCH, hit_child_variant.type());
    ASSERT_NE(nullptr, hit_child_variant.ptr());
    ComPtr<IAccessible> accessible;
    ASSERT_HRESULT_SUCCEEDED(V_DISPATCH(hit_child_variant.ptr())
                                 ->QueryInterface(IID_PPV_ARGS(&accessible)));
    ui::AXPlatformNode* hit_child =
        ui::AXPlatformNode::FromNativeViewAccessible(accessible.Get());
    ASSERT_NE(nullptr, hit_child);
    EXPECT_EQ(node->GetId(), hit_child->GetDelegate()->GetData().id);
  }

  // Now test it again, but this time caliing accHitTest on the parent
  // IAccessible of the web root node.
  {
    RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
    IAccessible* ancestor = rwhva->GetParentNativeViewAccessible();

    base::win::ScopedVariant hit_child_variant;
    ASSERT_EQ(S_OK, ancestor->accHitTest(point.x(), point.y(),
                                         hit_child_variant.Receive()));
    ASSERT_EQ(VT_DISPATCH, hit_child_variant.type());
    ASSERT_NE(nullptr, hit_child_variant.ptr());
    ComPtr<IAccessible> accessible;
    ASSERT_HRESULT_SUCCEEDED(V_DISPATCH(hit_child_variant.ptr())
                                 ->QueryInterface(IID_PPV_ARGS(&accessible)));
    ui::AXPlatformNode* hit_child =
        ui::AXPlatformNode::FromNativeViewAccessible(accessible.Get());
    ASSERT_NE(nullptr, hit_child);
    EXPECT_EQ(node->GetId(), hit_child->GetDelegate()->GetData().id);
  }
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest, IFrameTraversal) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/html/iframe-traversal.html");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  ui::BrowserAccessibility* root_node = GetRootAndAssertNonNull();
  ui::BrowserAccessibility* before_iframe_node =
      FindNodeAfter(root_node, "Before iframe");
  ASSERT_NE(nullptr, before_iframe_node);
  ASSERT_EQ(ax::mojom::Role::kStaticText, before_iframe_node->GetRole());

  ASSERT_EQ(0U, before_iframe_node->PlatformChildCount());
  ASSERT_EQ(1U, before_iframe_node->InternalChildCount());
  before_iframe_node = before_iframe_node->InternalGetFirstChild();
  ASSERT_NE(nullptr, before_iframe_node);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, before_iframe_node->GetRole());

  ui::BrowserAccessibility* inside_iframe_node =
      FindNodeAfter(before_iframe_node, "Text in iframe");
  ASSERT_NE(nullptr, inside_iframe_node);
  ASSERT_EQ(ax::mojom::Role::kStaticText, inside_iframe_node->GetRole());

  ASSERT_EQ(0U, inside_iframe_node->PlatformChildCount());
  ASSERT_EQ(1U, inside_iframe_node->InternalChildCount());
  inside_iframe_node = inside_iframe_node->InternalGetFirstChild();
  ASSERT_NE(nullptr, inside_iframe_node);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, inside_iframe_node->GetRole());

  ui::BrowserAccessibility* after_iframe_node =
      FindNodeAfter(inside_iframe_node, "After iframe");
  ASSERT_NE(nullptr, after_iframe_node);
  ASSERT_EQ(ax::mojom::Role::kStaticText, after_iframe_node->GetRole());

  ASSERT_EQ(0U, after_iframe_node->PlatformChildCount());
  ASSERT_EQ(1U, after_iframe_node->InternalChildCount());
  after_iframe_node = after_iframe_node->InternalGetFirstChild();
  ASSERT_NE(nullptr, after_iframe_node);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, after_iframe_node->GetRole());

  EXPECT_LT(*before_iframe_node->CreateTextPositionAt(0),
            *inside_iframe_node->CreateTextPositionAt(0));
  // The following positions should not be equivalent because they are on two
  // separate lines in the accessibility tree's text representation, i.e. the
  // first has an upstream affinity while the second has a downstream affinity.
  // Note that an iframe boundary is also a line boundary.
  EXPECT_LT(*before_iframe_node->CreateTextPositionAt(13),
            *inside_iframe_node->CreateTextPositionAt(0));
  EXPECT_LT(*inside_iframe_node->CreateTextPositionAt(0),
            *after_iframe_node->CreateTextPositionAt(0));
  // The following positions should not be equivalent because they are on two
  // separate lines in the accessibility tree's text representation, i.e. the
  // first has an upstream affinity while the second has a downstream affinity.
  // Note that an iframe boundary is also a line boundary.
  EXPECT_LT(*inside_iframe_node->CreateTextPositionAt(14),
            *after_iframe_node->CreateTextPositionAt(0));

  // Traverse the leaves of the AXTree forwards.
  ui::BrowserAccessibility::AXPosition tree_position =
      root_node->CreateTextPositionAt(0)->CreateNextLeafTreePosition();
  EXPECT_TRUE(tree_position->IsTreePosition());
  EXPECT_EQ(before_iframe_node->node(), tree_position->GetAnchor());
  tree_position = tree_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(tree_position->IsTreePosition());
  EXPECT_EQ(inside_iframe_node->node(), tree_position->GetAnchor());
  tree_position = tree_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(tree_position->IsTreePosition());
  EXPECT_EQ(after_iframe_node->node(), tree_position->GetAnchor());
  tree_position = tree_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(tree_position->IsNullPosition());

  // Traverse the leaves of the AXTree backwards.
  tree_position = after_iframe_node->CreateTextPositionAt(0)
                      ->CreatePositionAtEndOfAnchor()
                      ->AsLeafTreePosition();
  EXPECT_TRUE(tree_position->IsTreePosition());
  EXPECT_EQ(after_iframe_node->node(), tree_position->GetAnchor());
  tree_position = tree_position->CreatePreviousLeafTreePosition();
  EXPECT_TRUE(tree_position->IsTreePosition());
  EXPECT_EQ(inside_iframe_node->node(), tree_position->GetAnchor());
  tree_position = tree_position->CreatePreviousLeafTreePosition();
  EXPECT_TRUE(tree_position->IsTreePosition());
  EXPECT_EQ(before_iframe_node->node(), tree_position->GetAnchor());
  tree_position = tree_position->CreatePreviousLeafTreePosition();
  EXPECT_TRUE(tree_position->IsNullPosition());
}

}  // namespace content
