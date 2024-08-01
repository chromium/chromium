// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_IOS_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_IOS_H_

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT BrowserAccessibilityManagerIOS
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerIOS(const ui::AXTreeUpdate& initial_tree,
                                 ui::AXNodeIdDelegate& node_id_delegate,
                                 ui::AXPlatformTreeManagerDelegate* delegate);

  BrowserAccessibilityManagerIOS(const BrowserAccessibilityManagerIOS&) =
      delete;
  BrowserAccessibilityManagerIOS& operator=(
      const BrowserAccessibilityManagerIOS&) = delete;

  ~BrowserAccessibilityManagerIOS() override;

  static ui::AXTreeUpdate GetEmptyDocument();

  // BrowserAccessibilityManager methods.
  gfx::Rect GetViewBoundsInScreenCoordinates() const override;

 private:
  // AXTreeObserver methods.
  void OnAtomicUpdateFinished(ui::AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_IOS_H_
