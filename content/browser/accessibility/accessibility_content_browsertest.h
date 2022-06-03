// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CONTENT_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CONTENT_BROWSERTEST_H_

#include "content/public/test/content_browser_test.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

class BrowserAccessibility;
class BrowserAccessibilityManager;
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

  BrowserAccessibilityManager* GetManagerAndAssertNonNull() const;

  BrowserAccessibility* GetRootAndAssertNonNull() const;

  BrowserAccessibility* FindNode(const ax::mojom::Role role,
                                 const std::string& name_or_value) const;

 private:
  BrowserAccessibility* FindNodeInSubtree(
      BrowserAccessibility* node,
      const ax::mojom::Role role,
      const std::string& name_or_value) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CONTENT_BROWSERTEST_H_
