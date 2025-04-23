// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CONTENT_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CONTENT_BROWSERTEST_H_

#include <optional>

#include "content/public/test/content_browser_test.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
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
  AccessibilityContentBrowserTest();
  ~AccessibilityContentBrowserTest() override;

  // ContentBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void LoadInitialAccessibilityTreeFromUrl(const GURL& url);

  void LoadInitialAccessibilityTreeFromHtmlFilePath(
      const std::string& html_file_path);

  void LoadInitialAccessibilityTreeFromHtml(const std::string& html);

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

  std::optional<ScopedAccessibilityModeOverride> accessibility_mode_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CONTENT_BROWSERTEST_H_
