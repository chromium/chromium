// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_HIT_TESTING_WIN_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_HIT_TESTING_WIN_BROWSERTEST_H_

#include "content/browser/accessibility/hit_testing_browsertest.h"

#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/accessibility_switches.h"

#include <objbase.h>
#include <uiautomation.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace content {

#define EXPECT_ACCESSIBILITY_WIN_HIT_TEST_RESULT(css_point, expected_unknown, \
                                                 hit_unknown)                 \
  SCOPED_TRACE(GetScopedTrace(css_point));                                    \
  EXPECT_EQ(expected_unknown, hit_unknown);

class AccessibilityHitTestingWinBrowserTest
    : public AccessibilityHitTestingBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AccessibilityHitTestingBrowserTest::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalUIAutomation);
  }

  ComPtr<IAccessible> GetWebContentRootIAccessible() {
    ComPtr<IAccessible> content_root;
    GetRootBrowserAccessibilityManager()
        ->GetRoot()
        ->GetNativeViewAccessible()
        ->QueryInterface(IID_PPV_ARGS(&content_root));
    return content_root;
  }

  ComPtr<IRawElementProviderFragmentRoot> GetWebContentFragmentRoot() {
    ComPtr<IRawElementProviderFragment> content_root;
    GetWebContentRootIAccessible().As(&content_root);
    ComPtr<IRawElementProviderFragmentRoot> fragment_root;
    content_root->get_FragmentRoot(&fragment_root);
    return fragment_root;
  }

  ComPtr<ITextProvider> GetWebContentRootTextProvider() {
    ComPtr<IRawElementProviderSimple> content_root;
    GetWebContentRootIAccessible().As(&content_root);
    ComPtr<ITextProvider> text_provider;
    content_root->GetPatternProvider(UIA_TextPatternId, &text_provider);
    return text_provider;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AccessibilityHitTestingWinBrowserTest,
    ::testing::Combine(::testing::Values(1, 2), ::testing::Bool()),
    AccessibilityHitTestingBrowserTest::TestPassToString());

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingWinBrowserTest, AccHitTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  ComPtr<IAccessible> root_accessible = GetWebContentRootIAccessible();

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    gfx::Point rect_2_point_physical = CSSToPhysicalPixelPoint(rect_2_point);
    base::win::ScopedVariant hit_variant;
    ASSERT_HRESULT_SUCCEEDED(root_accessible->accHitTest(
        rect_2_point_physical.x(), rect_2_point_physical.y(),
        hit_variant.Receive()));
    ASSERT_EQ(hit_variant.type(), VT_DISPATCH);
    ComPtr<IAccessible> hit_accessible;
    ASSERT_HRESULT_SUCCEEDED(hit_variant.ptr()->pdispVal->QueryInterface(
        IID_PPV_ARGS(&hit_accessible)));
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    ComPtr<IAccessible> expected_accessible;
    ASSERT_HRESULT_SUCCEEDED(
        expected_node->GetNativeViewAccessible()->QueryInterface(
            IID_PPV_ARGS(&expected_accessible)));
    EXPECT_ACCESSIBILITY_WIN_HIT_TEST_RESULT(
        rect_2_point, expected_accessible.Get(), hit_accessible.Get());
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    gfx::Point rect_b_point_physical = CSSToPhysicalPixelPoint(rect_b_point);
    base::win::ScopedVariant hit_variant;
    ASSERT_HRESULT_SUCCEEDED(root_accessible->accHitTest(
        rect_b_point_physical.x(), rect_b_point_physical.y(),
        hit_variant.Receive()));
    ASSERT_EQ(hit_variant.type(), VT_DISPATCH);
    ComPtr<IAccessible> hit_accessible;
    ASSERT_HRESULT_SUCCEEDED(hit_variant.ptr()->pdispVal->QueryInterface(
        IID_PPV_ARGS(&hit_accessible)));
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    ComPtr<IAccessible> expected_accessible;
    ASSERT_HRESULT_SUCCEEDED(
        expected_node->GetNativeViewAccessible()->QueryInterface(
            IID_PPV_ARGS(&expected_accessible)));
    EXPECT_ACCESSIBILITY_WIN_HIT_TEST_RESULT(
        rect_b_point, expected_accessible.Get(), hit_accessible.Get());
  }
}

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingWinBrowserTest,
                       ElementProviderFromPoint) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  ComPtr<IRawElementProviderFragmentRoot> fragment_root =
      GetWebContentFragmentRoot();

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    gfx::Point rect_2_point_physical = CSSToPhysicalPixelPoint(rect_2_point);
    ComPtr<IRawElementProviderFragment> hit_fragment;
    ASSERT_HRESULT_SUCCEEDED(fragment_root->ElementProviderFromPoint(
        rect_2_point_physical.x(), rect_2_point_physical.y(), &hit_fragment));
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    ComPtr<IRawElementProviderFragment> expected_fragment;
    ASSERT_HRESULT_SUCCEEDED(
        expected_node->GetNativeViewAccessible()->QueryInterface(
            IID_PPV_ARGS(&expected_fragment)));
    EXPECT_ACCESSIBILITY_WIN_HIT_TEST_RESULT(
        rect_2_point, expected_fragment.Get(), hit_fragment.Get());
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    gfx::Point rect_b_point_physical = CSSToPhysicalPixelPoint(rect_b_point);
    ComPtr<IRawElementProviderFragment> hit_fragment;
    ASSERT_HRESULT_SUCCEEDED(fragment_root->ElementProviderFromPoint(
        rect_b_point_physical.x(), rect_b_point_physical.y(), &hit_fragment));
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    ComPtr<IRawElementProviderFragment> expected_fragment;
    ASSERT_HRESULT_SUCCEEDED(
        expected_node->GetNativeViewAccessible()->QueryInterface(
            IID_PPV_ARGS(&expected_fragment)));
    EXPECT_EQ(hit_fragment.Get(), expected_fragment.Get());
    EXPECT_ACCESSIBILITY_WIN_HIT_TEST_RESULT(
        rect_b_point, expected_fragment.Get(), hit_fragment.Get());
  }
}

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingWinBrowserTest,
                       TextProviderRangeFromPoint) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/text_ranges.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  ComPtr<ITextProvider> text_provider = GetWebContentRootTextProvider();

  // Test a hit on a text in the main frame.
  {
    gfx::Point rect_2_point(70, 20);
    gfx::Point rect_2_point_physical = CSSToPhysicalPixelPoint(rect_2_point);
    UiaPoint uia_point;
    uia_point.x = rect_2_point_physical.x();
    uia_point.y = rect_2_point_physical.y();
    ComPtr<ITextRangeProvider> hit_text_range;
    ASSERT_HRESULT_SUCCEEDED(
        text_provider->RangeFromPoint(uia_point, &hit_text_range));
    ASSERT_HRESULT_SUCCEEDED(
        hit_text_range->ExpandToEnclosingUnit(TextUnit_Character));
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    ComPtr<IRawElementProviderSimple> expected_provider;
    ASSERT_HRESULT_SUCCEEDED(
        expected_node->GetNativeViewAccessible()->QueryInterface(
            IID_PPV_ARGS(&expected_provider)));
    ComPtr<ITextRangeProvider> expected_text_range;
    ASSERT_HRESULT_SUCCEEDED(text_provider->RangeFromChild(
        expected_provider.Get(), &expected_text_range));

    SCOPED_TRACE(GetScopedTrace(rect_2_point));
    BOOL compare_result;
    ASSERT_HRESULT_SUCCEEDED(
        hit_text_range->Compare(expected_text_range.Get(), &compare_result));
    EXPECT_TRUE(compare_result);
  }

  // Test a hit on a text in the iframe.
  {
    gfx::Point rect_b_point(100, 100);
    gfx::Point rect_b_point_physical = CSSToPhysicalPixelPoint(rect_b_point);
    UiaPoint uia_point;
    uia_point.x = rect_b_point_physical.x();
    uia_point.y = rect_b_point_physical.y();
    ComPtr<ITextRangeProvider> hit_text_range;
    ASSERT_HRESULT_SUCCEEDED(
        text_provider->RangeFromPoint(uia_point, &hit_text_range));
    ASSERT_HRESULT_SUCCEEDED(
        hit_text_range->ExpandToEnclosingUnit(TextUnit_Character));
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    ComPtr<IRawElementProviderSimple> expected_provider;
    ASSERT_HRESULT_SUCCEEDED(
        expected_node->GetNativeViewAccessible()->QueryInterface(
            IID_PPV_ARGS(&expected_provider)));
    ComPtr<ITextRangeProvider> expected_text_range;
    ASSERT_HRESULT_SUCCEEDED(text_provider->RangeFromChild(
        expected_provider.Get(), &expected_text_range));

    SCOPED_TRACE(GetScopedTrace(rect_b_point));
    BOOL compare_result;
    ASSERT_HRESULT_SUCCEEDED(
        hit_text_range->Compare(expected_text_range.Get(), &compare_result));
    EXPECT_TRUE(compare_result);
  }
}

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_HIT_TESTING_WIN_BROWSERTEST_H_
