// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CONTENT_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CONTENT_BROWSERTEST_H_

#include "content/public/test/content_browser_test.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_mode.h"

namespace ui {
class BrowserAccessibility;
class BrowserAccessibilityManager;
}  // namespace ui

namespace content {

class WebContents;
class WebContentsImpl;

class AccessibilityContentBrowserTest : public ContentBrowserTest {
 protected:
  void LoadInitialAccessibilityTreeFromUrl(
      const GURL& url,
      ui::AXMode accessibility_mode = ui::kAXModeComplete);

  void LoadInitialAccessibilityTreeFromHtmlFilePath(
      const std::string& html_file_path,
      ui::AXMode accessibility_mode = ui::kAXModeComplete);

  void LoadInitialAccessibilityTreeFromHtml(
      const std::string& html,
      ui::AXMode accessibility_mode = ui::kAXModeComplete);

  WebContents* GetWebContentsAndAssertNonNull() const;

  WebContentsImpl* GetWebContentsImplAndAssertNonNull() const;

  ui::BrowserAccessibilityManager* GetManagerAndAssertNonNull() const;

  ui::BrowserAccessibility* GetRootAndAssertNonNull() const;

  ui::BrowserAccessibility* FindNode(const ax::mojom::Role role,
                                     const std::string& name_or_value) const;

 private:
  ui::BrowserAccessibility* FindNodeInSubtree(
      ui::BrowserAccessibility* node,
      const ax::mojom::Role role,
      const std::string& name_or_value) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CONTENT_BROWSERTEST_H_
