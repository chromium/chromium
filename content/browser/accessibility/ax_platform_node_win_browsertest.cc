// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win.h"

#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_content_browsertest.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

using Microsoft::WRL::ComPtr;

namespace content {
class AXPlatformNodeWinBrowserTest : public AccessibilityContentBrowserTest {
 protected:
  template <typename T>
  ComPtr<T> QueryInterfaceFromNode(
      BrowserAccessibility* browser_accessibility) {
    ComPtr<T> result;
    EXPECT_HRESULT_SUCCEEDED(
        browser_accessibility->GetNativeViewAccessible()->QueryInterface(
            __uuidof(T), &result));
    return result;
  }

  ComPtr<IAccessible> IAccessibleFromNode(
      BrowserAccessibility* browser_accessibility) {
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

  void UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      const BrowserAccessibility* target_browser_accessibility,
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
      names.push_back(base::UTF16ToUTF8(
          std::wstring(V_BSTR(name.ptr()), SysStringLen(V_BSTR(name.ptr())))));
    }

    ASSERT_THAT(names, testing::UnorderedElementsAreArray(expected_names));
  }

  void UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role expected_role,
      content::BrowserAccessibility* (content::BrowserAccessibility::*f)(
          uint32_t) const,
      uint32_t index_arg,
      bool expected_is_modal,
      bool expected_is_window_provider_available) {
    BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();
    BrowserAccessibilityComWin* root_browser_accessibility_com_win =
        ToBrowserAccessibilityWin(root_browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, root_browser_accessibility_com_win);

    BrowserAccessibility* browser_accessibility =
        (root_browser_accessibility->*f)(index_arg);
    ASSERT_NE(nullptr, browser_accessibility);
    ASSERT_EQ(expected_role, browser_accessibility->GetRole());
    BrowserAccessibilityComWin* browser_accessibility_com_win =
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

  BrowserAccessibility* browser_accessibility =
      GetRootAndAssertNonNull()->PlatformDeepestLastChild();
  ASSERT_NE(nullptr, browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kStaticText, browser_accessibility->GetRole());

  BrowserAccessibility* iframe_browser_accessibility =
      browser_accessibility->manager()->GetRoot();
  ASSERT_NE(nullptr, iframe_browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea,
            iframe_browser_accessibility->GetRole());

  gfx::Rect iframe_screen_bounds = iframe_browser_accessibility->GetBoundsRect(
      ui::AXCoordinateSystem::kScreen, ui::AXClippingBehavior::kUnclipped);

  AccessibilityNotificationWaiter location_changed_waiter(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLocationChanged);
  ComPtr<IAccessible2> root_iaccessible2 =
      ToIAccessible2(IAccessibleFromNode(browser_accessibility));
  ASSERT_EQ(S_OK, root_iaccessible2->scrollToPoint(
                      IA2_COORDTYPE_SCREEN_RELATIVE, iframe_screen_bounds.x(),
                      iframe_screen_bounds.y()));
  location_changed_waiter.WaitForNotification();

  gfx::Rect bounds = browser_accessibility->GetBoundsRect(
      ui::AXCoordinateSystem::kScreen, ui::AXClippingBehavior::kUnclipped);
  ASSERT_EQ(iframe_screen_bounds.y(), bounds.y());
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
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

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAGetPropertyValueFlowsFromSingle) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/aria/aria-flowto.html");

  UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      FindNode(ax::mojom::Role::kFooter, "next"), {"current"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAGetPropertyValueFlowsFromMultiple) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/aria/aria-flowto-multiple.html");

  UIAGetPropertyValueFlowsFromBrowserTestTemplate(
      FindNode(ax::mojom::Role::kGenericContainer, "b3"), {"a3", "c3"});
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDialog) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <dialog open>Example Text</dialog>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDialogAriaModalFalse) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <dialog open aria-modal="false">Example Text</dialog>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAIWindowProviderGetIsModalOnDialogAriaModalTrue) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <dialog open aria-modal="true">Example Text</dialog>
        </body>
      </html>
  )HTML"));

  UIAIWindowProviderGetIsModalBrowserTestTemplate(
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      true, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
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
      &BrowserAccessibility::PlatformGetChild, 0, false, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
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
      &BrowserAccessibility::PlatformGetChild, 0, false, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
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
      &BrowserAccessibility::PlatformGetChild, 0, false, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
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
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
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
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
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
      ax::mojom::Role::kDialog, &BrowserAccessibility::PlatformGetChild, 0,
      true, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
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
      ax::mojom::Role::kAlertDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeWinBrowserTest,
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
      ax::mojom::Role::kAlertDialog, &BrowserAccessibility::PlatformGetChild, 0,
      false, true);
}

IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeWinBrowserTest,
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
      ax::mojom::Role::kAlertDialog, &BrowserAccessibility::PlatformGetChild, 0,
      true, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeWinBrowserTest,
                       UIAGetPropertyValueAutomationId) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        </body>
          <div id="id"></div>
        </body>
      </html>
  )HTML"));

  BrowserAccessibility* root_browser_accessibility = GetRootAndAssertNonNull();
  BrowserAccessibilityComWin* root_browser_accessibility_com_win =
      ToBrowserAccessibilityWin(root_browser_accessibility)->GetCOM();
  ASSERT_NE(nullptr, root_browser_accessibility_com_win);

  BrowserAccessibility* browser_accessibility =
      root_browser_accessibility->PlatformDeepestLastChild();
  ASSERT_NE(nullptr, browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer,
            browser_accessibility->GetRole());
  BrowserAccessibilityComWin* browser_accessibility_com_win =
      ToBrowserAccessibilityWin(browser_accessibility)->GetCOM();
  ASSERT_NE(nullptr, browser_accessibility_com_win);

  base::win::ScopedVariant expected_scoped_variant;
  expected_scoped_variant.Set(SysAllocString(L"id"));
  base::win::ScopedVariant scoped_variant;
  EXPECT_HRESULT_SUCCEEDED(browser_accessibility_com_win->GetPropertyValue(
      UIA_AutomationIdPropertyId, scoped_variant.Receive()));
  EXPECT_EQ(0, expected_scoped_variant.Compare(scoped_variant));
}
}  // namespace content
