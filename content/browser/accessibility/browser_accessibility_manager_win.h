// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_

#include <oleacc.h>

#include <map>
#include <memory>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

namespace content {
class BrowserAccessibilityWin;

// {3761326A-34B2-465A-835D-7A3D8F4EFB92}
static const GUID kUiaTestCompleteSentinelGuid = {
    0x3761326a,
    0x34b2,
    0x465a,
    {0x83, 0x5d, 0x7a, 0x3d, 0x8f, 0x4e, 0xfb, 0x92}};
static const wchar_t kUiaTestCompleteSentinel[] = L"kUiaTestCompleteSentinel";

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
  BrowserAccessibility* GetFocus() const override;
  bool CanFireEvents() const override;
  gfx::Rect GetViewBounds() override;

  void FireFocusEvent(BrowserAccessibility* node) override;
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node) override;
  void FireGeneratedEvent(ui::AXEventGenerator::Event event_type,
                          BrowserAccessibility* node) override;

  void FireWinAccessibilityEvent(LONG win_event, BrowserAccessibility* node);
  void FireUiaAccessibilityEvent(LONG uia_event, BrowserAccessibility* node);
  void FireUiaPropertyChangedEvent(LONG uia_property,
                                   BrowserAccessibility* node);
  void FireUiaStructureChangedEvent(StructureChangeType change_type,
                                    BrowserAccessibility* node);
  void FireUiaTextContainerEvent(LONG uia_event, BrowserAccessibility* node);

  // Do event pre-processing
  void BeforeAccessibilityEvents() override;

  // Do event post-processing
  void FinalizeAccessibilityEvents() override;

  // Track this object and post a VISIBLE_DATA_CHANGED notification when
  // its container scrolls.
  // TODO(dmazzoni): remove once http://crbug.com/113483 is fixed.
  void TrackScrollingObject(BrowserAccessibilityWin* node);

  // Called when |accessible_hwnd_| is deleted by its parent.
  void OnAccessibleHwndDeleted();

 protected:
  // AXTreeObserver methods.
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;

  bool ShouldFireEventForNode(BrowserAccessibility* node) const;

 private:
  void HandleSelectedStateChanged(BrowserAccessibility* node);

  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;

  // Keep track of if we got a "load complete" event but were unable to fire
  // it because of no HWND, because otherwise JAWS can get very confused.
  // TODO(dmazzoni): a better fix would be to always have an HWND.
  // http://crbug.com/521877
  bool load_complete_pending_;

  // Since there could be multiple aria property changes on a node and we only
  // want to fire UIA_AriaPropertiesPropertyId once for that node, we use the
  // unordered set here to keep track of the unique nodes that had aria property
  // changes, so we only fire the event once for every node.
  std::unordered_set<BrowserAccessibility*> aria_properties_events_;

  // When the ignored state changes for a node, we only want to fire the
  // events relevant to the ignored state change (e.g. show / hide).
  // This set keeps track of what nodes should suppress superfluous events.
  std::set<BrowserAccessibility*> ignored_changed_nodes_;

  // Keep track of selection changes so we can optimize UIA event firing.
  // Pointers are only stored for the duration of |OnAccessibilityEvents|, and
  // the map is cleared in |FinalizeAccessibilityEvents|.
  struct SelectionEvents {
    std::vector<BrowserAccessibility*> added;
    std::vector<BrowserAccessibility*> removed;
    SelectionEvents();
    ~SelectionEvents();
  };
  std::map<BrowserAccessibility*, SelectionEvents> selection_events_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
