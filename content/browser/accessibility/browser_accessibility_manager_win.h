// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_

#include <oleacc.h>

#include <memory>

#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

namespace content {
class BrowserAccessibilityWin;

// Manages a tree of BrowserAccessibilityWin objects.
class CONTENT_EXPORT BrowserAccessibilityManagerWin
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerWin(
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  ~BrowserAccessibilityManagerWin() override;

  static ui::AXTreeUpdate GetEmptyDocument();

  // Get the closest containing HWND.
  HWND GetParentHWND();

  // BrowserAccessibilityManager methods
  void UserIsReloading() override;
  BrowserAccessibility* GetFocus() override;
  bool CanFireEvents() override;
  gfx::Rect GetViewBounds() override;

  void FireFocusEvent(BrowserAccessibility* node) override;
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node) override;
  void FireGeneratedEvent(AXEventGenerator::Event event_type,
                          BrowserAccessibility* node) override;

  void FireWinAccessibilityEvent(LONG win_event, BrowserAccessibility* node);

  // Track this object and post a VISIBLE_DATA_CHANGED notification when
  // its container scrolls.
  // TODO(dmazzoni): remove once http://crbug.com/113483 is fixed.
  void TrackScrollingObject(BrowserAccessibilityWin* node);

  // Called when |accessible_hwnd_| is deleted by its parent.
  void OnAccessibleHwndDeleted();

 protected:
  // AXTreeDelegate methods.
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeDelegate::Change>& changes) override;

 private:
  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;

  // Keep track of if we got a "load complete" event but were unable to fire
  // it because of no HWND, because otherwise JAWS can get very confused.
  // TODO(dmazzoni): a better fix would be to always have an HWND.
  // http://crbug.com/521877
  bool load_complete_pending_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
