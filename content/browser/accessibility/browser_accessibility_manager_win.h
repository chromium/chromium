// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_

#include <oleacc.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

namespace content {

class BrowserAccessibilityWin;
class WebAXPlatformTreeManagerDelegate;

using UiaRaiseActiveTextPositionChangedEventFunction =
    HRESULT(WINAPI*)(IRawElementProviderSimple*, ITextRangeProvider*);

// Manages a tree of BrowserAccessibilityWin objects.
class CONTENT_EXPORT BrowserAccessibilityManagerWin
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerWin(const ui::AXTreeUpdate& initial_tree,
                                 WebAXPlatformTreeManagerDelegate* delegate);

  BrowserAccessibilityManagerWin(const BrowserAccessibilityManagerWin&) =
      delete;
  BrowserAccessibilityManagerWin& operator=(
      const BrowserAccessibilityManagerWin&) = delete;

  ~BrowserAccessibilityManagerWin() override;

  static ui::AXTreeUpdate GetEmptyDocument();
  static bool IsUiaActiveTextPositionChangedEventSupported();

  // Get the closest containing HWND.
  HWND GetParentHWND();

  // BrowserAccessibilityManager methods
  void UserIsReloading() override;
  bool IsIgnoredChangedNode(const BrowserAccessibility* node) const;
  bool CanFireEvents() const override;

  void FireFocusEvent(ui::AXNode* node) override;
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node,
                      int action_request_id) override;
  void FireGeneratedEvent(ui::AXEventGenerator::Event event_type,
                          const ui::AXNode* node) override;

  void FireWinAccessibilityEvent(LONG win_event, BrowserAccessibility* node);
  void FireUiaAccessibilityEvent(LONG uia_event, BrowserAccessibility* node);
  void FireUiaPropertyChangedEvent(LONG uia_property,
                                   BrowserAccessibility* node);
  void FireUiaStructureChangedEvent(StructureChangeType change_type,
                                    BrowserAccessibility* node);
  void FireUiaActiveTextPositionChangedEvent(BrowserAccessibility* node);

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

 private:
  struct SelectionEvents {
    std::vector<BrowserAccessibility*> added;
    std::vector<BrowserAccessibility*> removed;
    SelectionEvents();
    ~SelectionEvents();
  };

  using SelectionEventsMap = std::map<BrowserAccessibility*, SelectionEvents>;
  using IsSelectedPredicate =
      base::RepeatingCallback<bool(BrowserAccessibility*)>;
  using FirePlatformSelectionEventsCallback =
      base::RepeatingCallback<void(BrowserAccessibility*,
                                   BrowserAccessibility*,
                                   const SelectionEvents&)>;
  static bool IsIA2NodeSelected(BrowserAccessibility* node);
  static bool IsUIANodeSelected(BrowserAccessibility* node);

  void FireIA2SelectionEvents(BrowserAccessibility* container,
                              BrowserAccessibility* only_selected_child,
                              const SelectionEvents& changes);
  void FireUIASelectionEvents(BrowserAccessibility* container,
                              BrowserAccessibility* only_selected_child,
                              const SelectionEvents& changes);

  static void HandleSelectedStateChanged(
      SelectionEventsMap& selection_events_map,
      BrowserAccessibility* node,
      bool is_selected);

  static void FinalizeSelectionEvents(
      SelectionEventsMap& selection_events_map,
      IsSelectedPredicate is_selected_predicate,
      FirePlatformSelectionEventsCallback fire_platform_events_callback);

  // Retrieve UIA RaiseActiveTextPositionChangedEvent function if supported.
  static UiaRaiseActiveTextPositionChangedEventFunction
  GetUiaActiveTextPositionChangedEventFunction();

  void HandleAriaPropertiesChangedEvent(BrowserAccessibility& node);
  void EnqueueTextChangedEvent(BrowserAccessibility& node);
  void EnqueueSelectionChangedEvent(BrowserAccessibility& node);

  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;

  // Since there could be multiple aria property changes on a node and we only
  // want to fire UIA_AriaPropertiesPropertyId once for that node, we use the
  // set here to keep track of the unique nodes that had aria property changes,
  // so we only fire the event once for every node.
  std::set<BrowserAccessibility*> aria_properties_events_;

  // Since there could be duplicate selection changed events on a node raised
  // from both EventType::DOCUMENT_SELECTION_CHANGED and
  // EventType::TEXT_SELECTION_CHANGED, we keep track of the unique
  // nodes so we only fire the event once for every node.
  std::set<BrowserAccessibility*> selection_changed_nodes_;

  // Since there could be duplicate text changed events on a node raised from
  // both FireBlinkEvent and FireGeneratedEvent, we use the set here to keep
  // track of the unique nodes that had UIA_Text_TextChangedEventId, so we only
  // fire the event once for every node.
  std::set<BrowserAccessibility*> text_changed_nodes_;

  // When the ignored state changes for a node, we only want to fire the
  // events relevant to the ignored state change (e.g. show / hide).
  // This set keeps track of what nodes should suppress superfluous events.
  std::set<BrowserAccessibility*> ignored_changed_nodes_;

  // Keep track of selection changes so we can optimize UIA event firing.
  // Pointers are only stored for the duration of |OnAccessibilityEvents|, and
  // the map is cleared in |FinalizeAccessibilityEvents|.
  SelectionEventsMap ia2_selection_events_;
  SelectionEventsMap uia_selection_events_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
