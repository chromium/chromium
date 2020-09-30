// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_content_browsertest.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

void AccessibilityContentBrowserTest::LoadInitialAccessibilityTreeFromUrl(
    const GURL& url,
    ui::AXMode accessibility_mode) {
  AccessibilityNotificationWaiter waiter(GetWebContentsAndAssertNonNull(),
                                         accessibility_mode,
                                         ax::mojom::Event::kLoadComplete);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();
}

void AccessibilityContentBrowserTest::
    LoadInitialAccessibilityTreeFromHtmlFilePath(
        const std::string& html_file_path,
        ui::AXMode accessibility_mode) {
  if (!embedded_test_server()->Started())
    ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Started());
  LoadInitialAccessibilityTreeFromUrl(
      embedded_test_server()->GetURL(html_file_path), accessibility_mode);
}

void AccessibilityContentBrowserTest::LoadInitialAccessibilityTreeFromHtml(
    const std::string& html,
    ui::AXMode accessibility_mode) {
  LoadInitialAccessibilityTreeFromUrl(
      GURL("data:text/html," + net::EscapeQueryParamValue(html, false)),
      accessibility_mode);
}

WebContents* AccessibilityContentBrowserTest::GetWebContentsAndAssertNonNull()
    const {
  // Perform the null pointer validation inside a void function to allow for a
  // return type.
  auto GetWebContentsAndAssertNonNull = [this](WebContents** result) {
    WebContents* web_contents = shell()->web_contents();
    ASSERT_NE(nullptr, web_contents);
    *result = web_contents;
  };

  WebContents* web_contents;
  GetWebContentsAndAssertNonNull(&web_contents);
  return web_contents;
}

WebContentsImpl*
AccessibilityContentBrowserTest::GetWebContentsImplAndAssertNonNull() const {
  return static_cast<WebContentsImpl*>(GetWebContentsAndAssertNonNull());
}

BrowserAccessibilityManager*
AccessibilityContentBrowserTest::GetManagerAndAssertNonNull() const {
  // Perform the null pointer validation inside a void function to allow for a
  // return type.
  auto GetManagerAndAssertNonNull =
      [this](BrowserAccessibilityManager** result) {
        BrowserAccessibilityManager* browser_accessibility_manager =
            GetWebContentsImplAndAssertNonNull()
                ->GetRootBrowserAccessibilityManager();
        ASSERT_NE(nullptr, browser_accessibility_manager);
        *result = browser_accessibility_manager;
      };

  BrowserAccessibilityManager* browser_accessibility_manager;
  GetManagerAndAssertNonNull(&browser_accessibility_manager);
  return browser_accessibility_manager;
}

BrowserAccessibility* AccessibilityContentBrowserTest::GetRootAndAssertNonNull()
    const {
  // Perform the null pointer validation inside a void function to allow for a
  // return type.
  auto GetRootAndAssertNonNull = [this](BrowserAccessibility** result) {
    BrowserAccessibility* root_browser_accessibility =
        GetManagerAndAssertNonNull()->GetRoot();
    ASSERT_NE(nullptr, result);
    *result = root_browser_accessibility;
  };

  BrowserAccessibility* root_browser_accessibility;
  GetRootAndAssertNonNull(&root_browser_accessibility);
  return root_browser_accessibility;
}

BrowserAccessibility* AccessibilityContentBrowserTest::FindNode(
    const ax::mojom::Role role,
    const std::string& name_or_value) const {
  return FindNodeInSubtree(GetRootAndAssertNonNull(), role, name_or_value);
}

BrowserAccessibility* AccessibilityContentBrowserTest::FindNodeInSubtree(
    BrowserAccessibility* node,
    const ax::mojom::Role role,
    const std::string& name_or_value) const {
  const std::string& name =
      node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  // Note that in the case of a text field, "BrowserAccessibility::GetValue" has
  // the added functionality of computing the value of an ARIA text box from its
  // inner text.
  //
  // <div contenteditable="true" role="textbox">Hello world.</div>
  // Will expose no HTML value attribute, but some screen readers, such as Jaws,
  // VoiceOver and Talkback, require one to be computed.
  const std::string& value = base::UTF16ToUTF8(node->GetValue());
  if (node->GetRole() == role &&
      (name == name_or_value || value == name_or_value)) {
    return node;
  }

  for (uint32_t i = 0; i < node->PlatformChildCount(); ++i) {
    BrowserAccessibility* result =
        FindNodeInSubtree(node->PlatformGetChild(i), role, name_or_value);
    if (result)
      return result;
  }

  return nullptr;
}

}  // namespace content
