// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_win.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_utils_win.h"
#include "ui/accessibility/platform/uia_registrar_win.h"
#include "ui/base/win/atl_module.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate) {
  return new BrowserAccessibilityManagerWin(initial_tree, delegate);
}

BrowserAccessibilityManagerWin*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerWin() {
  return static_cast<BrowserAccessibilityManagerWin*>(this);
}

BrowserAccessibilityManagerWin::BrowserAccessibilityManagerWin(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate)
    : BrowserAccessibilityManager(delegate), load_complete_pending_(false) {
  ui::win::CreateATLModuleIfNeeded();
  Initialize(initial_tree);
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() = default;

// static
ui::AXTreeUpdate BrowserAccessibilityManagerWin::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 1;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  empty_document.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

HWND BrowserAccessibilityManagerWin::GetParentHWND() {
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (!delegate)
    return NULL;
  return delegate->AccessibilityGetAcceleratedWidget();
}

void BrowserAccessibilityManagerWin::UserIsReloading() {
  if (GetRoot())
    FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_RELOAD, GetRoot());
}

BrowserAccessibility* BrowserAccessibilityManagerWin::GetFocus() const {
  BrowserAccessibility* focus = BrowserAccessibilityManager::GetFocus();
  return GetActiveDescendant(focus);
}

void BrowserAccessibilityManagerWin::FireFocusEvent(
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireFocusEvent(node);
  DCHECK(node);
  FireWinAccessibilityEvent(EVENT_OBJECT_FOCUS, node);
  FireUiaAccessibilityEvent(UIA_AutomationFocusChangedEventId, node);
}

void BrowserAccessibilityManagerWin::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node);
  switch (event_type) {
    case ax::mojom::Event::kClicked:
      if (node->GetData().IsInvocable())
        FireUiaAccessibilityEvent(UIA_Invoke_InvokedEventId, node);
      break;
    case ax::mojom::Event::kEndOfTest:
      // Event tests use kEndOfTest as a sentinel to mark the end of the test.
      FireUiaAccessibilityEvent(
          ui::UiaRegistrarWin::GetInstance().GetUiaTestCompleteEventId(), node);
      break;
    case ax::mojom::Event::kLocationChanged:
      FireWinAccessibilityEvent(IA2_EVENT_VISIBLE_DATA_CHANGED, node);
      break;
    case ax::mojom::Event::kScrolledToAnchor:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGSTART, node);
      break;
    case ax::mojom::Event::kTextChanged:
      FireUiaTextContainerEvent(UIA_Text_TextChangedEventId, node);
      break;
    case ax::mojom::Event::kTextSelectionChanged:
      text_selection_changed_events_.insert(node);
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerWin::FireGeneratedEvent(
    ui::AXEventGenerator::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);
  bool can_fire_events = CanFireEvents();

  if (event_type == ui::AXEventGenerator::Event::LOAD_COMPLETE &&
      can_fire_events)
    load_complete_pending_ = false;

  if (load_complete_pending_ && can_fire_events && GetRoot()) {
    load_complete_pending_ = false;
    FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_LOAD_COMPLETE, GetRoot());
  }

  if (!can_fire_events && !load_complete_pending_ &&
      event_type == ui::AXEventGenerator::Event::LOAD_COMPLETE && GetRoot() &&
      !GetRoot()->IsOffscreen() && GetRoot()->PlatformChildCount() > 0) {
    load_complete_pending_ = true;
  }

  switch (event_type) {
    case ui::AXEventGenerator::Event::ACCESS_KEY_CHANGED:
      FireUiaPropertyChangedEvent(UIA_AccessKeyPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_ACTIVE_DESCENDANT_CHANGED, node);
      break;
    case ui::AXEventGenerator::Event::ALERT:
      FireWinAccessibilityEvent(EVENT_SYSTEM_ALERT, node);
      FireUiaAccessibilityEvent(UIA_SystemAlertEventId, node);
      break;
    case ui::AXEventGenerator::Event::ATOMIC_CHANGED:
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::BUSY_CHANGED:
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      // https://www.w3.org/TR/core-aam-1.1/#mapping_state-property_table
      // SelectionItem.IsSelected is set according to the True or False value of
      // aria-checked for 'radio' and 'menuitemradio' roles.
      if (ui::IsRadio(node->GetRole())) {
        HandleSelectedStateChanged(uia_selection_events_, node,
                                   IsUIANodeSelected(node));
      }
      FireUiaPropertyChangedEvent(UIA_ToggleToggleStatePropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::CHILDREN_CHANGED: {
      // If this node is ignored, fire the event on the platform parent since
      // ignored nodes cannot raise events.
      BrowserAccessibility* target_node =
          node->IsIgnored() ? node->PlatformGetParent() : node;
      if (target_node) {
        FireWinAccessibilityEvent(EVENT_OBJECT_REORDER, target_node);
        FireUiaStructureChangedEvent(StructureChangeType_ChildrenReordered,
                                     target_node);
      }
      break;
    }
    case ui::AXEventGenerator::Event::CLASS_NAME_CHANGED:
      FireUiaPropertyChangedEvent(UIA_ClassNamePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::COLLAPSED:
    case ui::AXEventGenerator::Event::EXPANDED:
      FireUiaPropertyChangedEvent(
          UIA_ExpandCollapseExpandCollapseStatePropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_ControllerForPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
      FireUiaPropertyChangedEvent(UIA_DescribedByPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
      FireUiaPropertyChangedEvent(UIA_FullDescriptionPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED: {
      // Fire the event on the object where the focus of the selection is.
      int32_t focus_id = ax_tree()->GetUnignoredSelection().focus_object_id;
      BrowserAccessibility* focus_object = GetFromID(focus_id);
      if (focus_object && focus_object->HasVisibleCaretOrSelection())
        FireWinAccessibilityEvent(IA2_EVENT_TEXT_CARET_MOVED, focus_object);
      text_selection_changed_events_.insert(node);
      break;
    }
    // aria-dropeffect is deprecated in WAI-ARIA 1.1.
    case ui::AXEventGenerator::Event::DROPEFFECT_CHANGED:
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::ENABLED_CHANGED:
      FireUiaPropertyChangedEvent(UIA_IsEnabledPropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::FLOW_FROM_CHANGED:
      FireUiaPropertyChangedEvent(UIA_FlowsFromPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::FLOW_TO_CHANGED:
      FireUiaPropertyChangedEvent(UIA_FlowsToPropertyId, node);
      break;
    // aria-grabbed is deprecated in WAI-ARIA 1.1.
    case ui::AXEventGenerator::Event::GRABBED_CHANGED:
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::HASPOPUP_CHANGED:
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
      FireUiaPropertyChangedEvent(UIA_LevelPropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::IGNORED_CHANGED:
      if (node->IsIgnored()) {
        FireWinAccessibilityEvent(EVENT_OBJECT_HIDE, node);
        FireUiaStructureChangedEvent(StructureChangeType_ChildRemoved, node);
        if (node->GetRole() == ax::mojom::Role::kMenu) {
          FireWinAccessibilityEvent(EVENT_SYSTEM_MENUPOPUPEND, node);
          FireUiaAccessibilityEvent(UIA_MenuClosedEventId, node);
        }
      }
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_NAMECHANGE, node);
      break;
    case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_IsDataValidForFormPropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_AcceleratorKeyPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::LABELED_BY_CHANGED:
      FireUiaPropertyChangedEvent(UIA_LabeledByPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::LANGUAGE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_CulturePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::LIVE_REGION_CREATED:
      FireUiaAccessibilityEvent(UIA_LiveRegionChangedEventId, node);
      break;
    case ui::AXEventGenerator::Event::LIVE_REGION_CHANGED:
      // This event is redundant with the IA2_EVENT_TEXT_INSERTED events;
      // however, JAWS 2018 and earlier do not process the text inserted
      // events when "virtual cursor mode" is turned off (Insert+Z).
      // Fortunately, firing the redudant event does not cause duplicate
      // verbalizations in either screen reader.
      // Future versions of JAWS may process the text inserted event when
      // in focus mode, and so at some point the live region
      // changed events may truly become redundant with the text inserted
      // events. Note: Firefox does not fire this event, but JAWS processes
      // Firefox live region events differently (utilizes MSAA's
      // EVENT_OBJECT_SHOW).
      FireWinAccessibilityEvent(EVENT_OBJECT_LIVEREGIONCHANGED, node);
      FireUiaAccessibilityEvent(UIA_LiveRegionChangedEventId, node);
      break;
    case ui::AXEventGenerator::Event::LIVE_STATUS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_LiveSettingPropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::LOAD_COMPLETE:
      FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_LOAD_COMPLETE, node);
      break;
    case ui::AXEventGenerator::Event::LAYOUT_INVALIDATED:
      FireUiaAccessibilityEvent(UIA_LayoutInvalidatedEventId, node);
      break;
    case ui::AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
    case ui::AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_SelectionCanSelectMultiplePropertyId,
                                  node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::NAME_CHANGED:
      FireUiaPropertyChangedEvent(UIA_NamePropertyId, node);
      // Only fire name changes when the name comes from an attribute, otherwise
      // name changes are redundant with text removed/inserted events.
      if (node->GetData().GetNameFrom() != ax::mojom::NameFrom::kContents)
        FireWinAccessibilityEvent(EVENT_OBJECT_NAMECHANGE, node);
      break;
    case ui::AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED, node);
      // TODO(crbug.com/1108871): Fire UIA event.
      break;
    case ui::AXEventGenerator::Event::PLACEHOLDER_CHANGED:
      FireUiaPropertyChangedEvent(UIA_HelpTextPropertyId, node);
      break;
    case ui::AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
      FireUiaPropertyChangedEvent(UIA_PositionInSetPropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::READONLY_CHANGED:
      if (node->GetData().IsRangeValueSupported())
        FireUiaPropertyChangedEvent(UIA_RangeValueIsReadOnlyPropertyId, node);
      else if (ui::IsValuePatternSupported(node))
        FireUiaPropertyChangedEvent(UIA_ValueIsReadOnlyPropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_IsRequiredForFormPropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::ROLE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_AriaRolePropertyId, node);
      break;
    case ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGEND, node);
      FireUiaPropertyChangedEvent(UIA_ScrollHorizontalScrollPercentPropertyId,
                                  node);
      break;
    case ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGEND, node);
      FireUiaPropertyChangedEvent(UIA_ScrollVerticalScrollPercentPropertyId,
                                  node);
      break;
    case ui::AXEventGenerator::Event::SELECTED_CHANGED:
      HandleSelectedStateChanged(ia2_selection_events_, node,
                                 IsIA2NodeSelected(node));
      HandleSelectedStateChanged(uia_selection_events_, node,
                                 IsUIANodeSelected(node));
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONWITHIN, node);
      break;
    case ui::AXEventGenerator::Event::SET_SIZE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_SizeOfSetPropertyId, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::SORT_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED, node);
      aria_properties_events_.insert(node);
      break;
    case ui::AXEventGenerator::Event::SUBTREE_CREATED:
      FireWinAccessibilityEvent(EVENT_OBJECT_SHOW, node);
      FireUiaStructureChangedEvent(StructureChangeType_ChildAdded, node);
      if (node->GetRole() == ax::mojom::Role::kMenu) {
        FireWinAccessibilityEvent(EVENT_SYSTEM_MENUPOPUPSTART, node);
        FireUiaAccessibilityEvent(UIA_MenuOpenedEventId, node);
      }
      break;
    case ui::AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_TEXT_ATTRIBUTE_CHANGED, node);
      FireUiaTextContainerEvent(UIA_Text_TextChangedEventId, node);
      break;
    case ui::AXEventGenerator::Event::VALUE_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_VALUECHANGE, node);
      if (node->GetData().IsRangeValueSupported()) {
        FireUiaPropertyChangedEvent(UIA_RangeValueValuePropertyId, node);
        aria_properties_events_.insert(node);
      } else if (ui::IsValuePatternSupported(node)) {
        FireUiaPropertyChangedEvent(UIA_ValueValuePropertyId, node);
        FireUiaTextContainerEvent(UIA_Text_TextChangedEventId, node);
      } else if (node->GetData().GetBoolAttribute(
                     ax::mojom::BoolAttribute::kEditableRoot)) {
        FireUiaTextContainerEvent(UIA_Text_TextChangedEventId, node);
      }
      break;
    case ui::AXEventGenerator::Event::VALUE_MAX_CHANGED:
      if (node->GetData().IsRangeValueSupported()) {
        FireUiaPropertyChangedEvent(UIA_RangeValueMaximumPropertyId, node);
        aria_properties_events_.insert(node);
      }
      break;
    case ui::AXEventGenerator::Event::VALUE_MIN_CHANGED:
      if (node->GetData().IsRangeValueSupported()) {
        FireUiaPropertyChangedEvent(UIA_RangeValueMinimumPropertyId, node);
        aria_properties_events_.insert(node);
      }
      break;
    case ui::AXEventGenerator::Event::VALUE_STEP_CHANGED:
      if (node->GetData().IsRangeValueSupported()) {
        FireUiaPropertyChangedEvent(UIA_RangeValueSmallChangePropertyId, node);
        FireUiaPropertyChangedEvent(UIA_RangeValueLargeChangePropertyId, node);
      }
      break;
    case ui::AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_STATECHANGE, node);
      break;
    case ui::AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
    case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
    case ui::AXEventGenerator::Event::FOCUS_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
    case ui::AXEventGenerator::Event::LOAD_START:
    case ui::AXEventGenerator::Event::PORTAL_ACTIVATED:
    case ui::AXEventGenerator::Event::MENU_ITEM_SELECTED:
    case ui::AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::RELATED_NODE_CHANGED:
    case ui::AXEventGenerator::Event::ROW_COUNT_CHANGED:
    case ui::AXEventGenerator::Event::STATE_CHANGED:
      // There are some notifications that aren't meaningful on Windows.
      // It's okay to skip them.
      break;
  }
}

void BrowserAccessibilityManagerWin::FireWinAccessibilityEvent(
    LONG win_event_type,
    BrowserAccessibility* node) {
  if (!ShouldFireEventForNode(node))
    return;
  // Suppress events when |IGNORED_CHANGED| except for related SHOW / HIDE.
  // Also include MENUPOPUPSTART / MENUPOPUPEND since a change in the ignored
  // state may show / hide a popup by exposing it to the tree or not.
  // Also include focus events since a node may become visible at the same time
  // it receives focus It's never good to suppress a po
  if (base::Contains(ignored_changed_nodes_, node)) {
    switch (win_event_type) {
      case EVENT_OBJECT_HIDE:
      case EVENT_OBJECT_SHOW:
      case EVENT_OBJECT_FOCUS:
      case EVENT_SYSTEM_MENUPOPUPEND:
      case EVENT_SYSTEM_MENUPOPUPSTART:
        break;
      default:
        return;
    }
  } else if (node->IsIgnored()) {
    return;
  }

  HWND hwnd = GetParentHWND();
  if (!hwnd)
    return;

  // Pass the negation of this node's unique id in the |child_id|
  // argument to NotifyWinEvent; the AT client will then call get_accChild
  // on the HWND's accessibility object and pass it that same id, which
  // we can use to retrieve the IAccessible for this node.
  LONG child_id = -(ToBrowserAccessibilityWin(node)->GetCOM()->GetUniqueId());
  ::NotifyWinEvent(win_event_type, hwnd, OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::FireUiaAccessibilityEvent(
    LONG uia_event,
    BrowserAccessibility* node) {
  if (!::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return;
  if (!ShouldFireEventForNode(node))
    return;
  // Suppress events when |IGNORED_CHANGED| except for MenuClosed / MenuOpen
  // since a change in the ignored state may show / hide a popup by exposing
  // it to the tree or not.
  if (base::Contains(ignored_changed_nodes_, node)) {
    switch (uia_event) {
      case UIA_MenuClosedEventId:
      case UIA_MenuOpenedEventId:
        break;
      default:
        return;
    }
  } else if (node->IsIgnored()) {
    return;
  }

  ::UiaRaiseAutomationEvent(ToBrowserAccessibilityWin(node)->GetCOM(),
                            uia_event);
}

void BrowserAccessibilityManagerWin::FireUiaPropertyChangedEvent(
    LONG uia_property,
    BrowserAccessibility* node) {
  if (!::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return;
  if (!ShouldFireEventForNode(node))
    return;
  // Suppress events when |IGNORED_CHANGED| with the exception for firing
  // UIA_AriaPropertiesPropertyId-hidden event on non-text node marked as
  // ignored.
  if (node->IsIgnored() || base::Contains(ignored_changed_nodes_, node)) {
    if (uia_property != UIA_AriaPropertiesPropertyId || node->IsText())
      return;
  }

  // The old value is not used by the system
  VARIANT old_value = {};
  old_value.vt = VT_EMPTY;

  auto* provider = ToBrowserAccessibilityWin(node)->GetCOM();
  base::win::ScopedVariant new_value;
  if (SUCCEEDED(
          provider->GetPropertyValueImpl(uia_property, new_value.Receive()))) {
    ::UiaRaiseAutomationPropertyChangedEvent(provider, uia_property, old_value,
                                             new_value);
  }
}

void BrowserAccessibilityManagerWin::FireUiaStructureChangedEvent(
    StructureChangeType change_type,
    BrowserAccessibility* node) {
  if (!::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return;
  if (!ShouldFireEventForNode(node))
    return;
  // Suppress events when |IGNORED_CHANGED| except for related structure changes
  if (base::Contains(ignored_changed_nodes_, node)) {
    switch (change_type) {
      case StructureChangeType_ChildRemoved:
      case StructureChangeType_ChildAdded:
        break;
      default:
        return;
    }
  } else if (node->IsIgnored()) {
    return;
  }

  auto* provider = ToBrowserAccessibilityWin(node);
  auto* provider_com = provider ? provider->GetCOM() : nullptr;
  if (!provider || !provider_com)
    return;

  switch (change_type) {
    case StructureChangeType_ChildRemoved: {
      // 'ChildRemoved' fires on the parent and provides the runtime ID of
      // the removed child (which was passed as |node|).
      auto* parent = ToBrowserAccessibilityWin(node->PlatformGetParent());
      auto* parent_com = parent ? parent->GetCOM() : nullptr;
      if (parent && parent_com) {
        ui::AXPlatformNodeWin::RuntimeIdArray runtime_id;
        provider_com->GetRuntimeIdArray(runtime_id);
        UiaRaiseStructureChangedEvent(parent_com, change_type,
                                      runtime_id.data(), runtime_id.size());
      }
      break;
    }

    default: {
      // All other types are fired on |node|.  For 'ChildAdded' |node| is the
      // child that was added; for other types, it's the parent container.
      UiaRaiseStructureChangedEvent(provider_com, change_type, nullptr, 0);
    }
  }
}

void BrowserAccessibilityManagerWin::FireUiaTextContainerEvent(
    LONG uia_event,
    BrowserAccessibility* node) {
  // If the node supports text pattern, fire the event from itself, otherwise,
  // fire the event from the closest ancestor that supports text pattern.
  while (node) {
    if (ToBrowserAccessibilityWin(node)->GetCOM()->IsPatternProviderSupported(
            UIA_TextPatternId)) {
      FireUiaAccessibilityEvent(uia_event, node);
      return;
    }
    node = node->PlatformGetParent();
  }
}

bool BrowserAccessibilityManagerWin::CanFireEvents() const {
  return BrowserAccessibilityManager::CanFireEvents() &&
         GetDelegateFromRootManager() &&
         GetDelegateFromRootManager()->AccessibilityGetAcceleratedWidget();
}

gfx::Rect BrowserAccessibilityManagerWin::GetViewBoundsInScreenCoordinates()
    const {
  // We have to take the device scale factor into account on Windows.
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (delegate) {
    gfx::Rect bounds = delegate->AccessibilityGetViewBounds();

    // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
    // The bounds returned by the delegate are always in device-independent
    // pixels (DIPs), meaning physical pixels divided by device scale factor
    // (DSF). However, if UseZoomForDSF is enabled, then Blink does not apply
    // DSF when going from physical to screen pixels. In that case, we need to
    // multiply DSF back in to get to Blink's notion of "screen pixels."
    if (IsUseZoomForDSFEnabled() && device_scale_factor() > 0.0 &&
        device_scale_factor() != 1.0) {
      bounds = ScaleToEnclosingRect(bounds, device_scale_factor());
    }
    return bounds;
  }
  return gfx::Rect();
}

void BrowserAccessibilityManagerWin::OnSubtreeWillBeDeleted(ui::AXTree* tree,
                                                            ui::AXNode* node) {
  BrowserAccessibility* obj = GetFromAXNode(node);
  DCHECK(obj);
  if (obj) {
    FireWinAccessibilityEvent(EVENT_OBJECT_HIDE, obj);
    FireUiaStructureChangedEvent(StructureChangeType_ChildRemoved, obj);
  }
}

void BrowserAccessibilityManagerWin::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                         ui::AXNode* node) {
  if (node->data().role == ax::mojom::Role::kMenu) {
    BrowserAccessibility* obj = GetFromAXNode(node);
    DCHECK(obj);
    FireWinAccessibilityEvent(EVENT_SYSTEM_MENUPOPUPEND, obj);
    FireUiaAccessibilityEvent(UIA_MenuClosedEventId, obj);
  }
}

void BrowserAccessibilityManagerWin::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  // Do a sequence of Windows-specific updates on each node. Each one is
  // done in a single pass that must complete before the next step starts.
  // The nodes that need to be updated are all of the nodes that were changed,
  // plus some parents.
  std::set<ui::AXPlatformNode*> objs_to_update;
  CollectChangedNodesAndParentsForAtomicUpdate(tree, changes, &objs_to_update);

  // The first step moves win_attributes_ to old_win_attributes_ and then
  // recomputes all of win_attributes_ other than IAccessibleText.
  for (auto* node : objs_to_update) {
    static_cast<BrowserAccessibilityComWin*>(node)
        ->UpdateStep1ComputeWinAttributes();
  }

  // The next step updates the hypertext of each node, which is a
  // concatenation of all of its child text nodes, so it can't run until
  // the text of all of the nodes was computed in the previous step.
  for (auto* node : objs_to_update) {
    static_cast<BrowserAccessibilityComWin*>(node)
        ->UpdateStep2ComputeHypertext();
  }

  // The third step fires events on nodes based on what's changed - like
  // if the name, value, or description changed, or if the hypertext had
  // text inserted or removed. It's able to figure out exactly what changed
  // because we still have old_win_attributes_ populated.
  // This step has to run after the previous two steps complete because the
  // client may walk the tree when it receives any of these events.
  // At the end, it deletes old_win_attributes_ since they're not needed
  // anymore.
  for (auto* node : objs_to_update) {
    static_cast<BrowserAccessibilityComWin*>(node)->UpdateStep3FireEvents();
  }
}

// static
bool BrowserAccessibilityManagerWin::IsIA2NodeSelected(
    BrowserAccessibility* node) {
  return node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

// static
bool BrowserAccessibilityManagerWin::IsUIANodeSelected(
    BrowserAccessibility* node) {
  // https://www.w3.org/TR/core-aam-1.1/#mapping_state-property_table
  // SelectionItem.IsSelected is set according to the True or False value of
  // aria-checked for 'radio' and 'menuitemradio' roles.
  if (ui::IsRadio(node->GetRole()))
    return node->GetData().GetCheckedState() == ax::mojom::CheckedState::kTrue;

  return node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

void BrowserAccessibilityManagerWin::FireIA2SelectionEvents(
    BrowserAccessibility* container,
    BrowserAccessibility* only_selected_child,
    const SelectionEvents& changes) {
  if (only_selected_child) {
    // Fire 'ElementSelected' on the only selected child.
    FireWinAccessibilityEvent(EVENT_OBJECT_SELECTION, only_selected_child);
  } else {
    const bool container_is_multiselectable =
        container && container->HasState(ax::mojom::State::kMultiselectable);
    for (auto* item : changes.added) {
      if (container_is_multiselectable)
        FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONADD, item);
      else
        FireWinAccessibilityEvent(EVENT_OBJECT_SELECTION, item);
    }
    for (auto* item : changes.removed)
      FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONREMOVE, item);
  }
}

void BrowserAccessibilityManagerWin::FireUIASelectionEvents(
    BrowserAccessibility* container,
    BrowserAccessibility* only_selected_child,
    const SelectionEvents& changes) {
  if (only_selected_child) {
    // Fire 'ElementSelected' on the only selected child.
    FireUiaAccessibilityEvent(UIA_SelectionItem_ElementSelectedEventId,
                              only_selected_child);
    FireUiaPropertyChangedEvent(UIA_SelectionItemIsSelectedPropertyId,
                                only_selected_child);
    for (auto* item : changes.removed)
      FireUiaPropertyChangedEvent(UIA_SelectionItemIsSelectedPropertyId, item);
  } else {
    // Per UIA documentation, beyond the "invalidate limit" we're supposed to
    // fire a 'SelectionInvalidated' event.  The exact value isn't specified,
    // but System.Windows.Automation.Provider uses a value of 20.
    static const size_t kInvalidateLimit = 20;
    if ((changes.added.size() + changes.removed.size()) > kInvalidateLimit) {
      DCHECK_NE(container, nullptr);
      FireUiaAccessibilityEvent(UIA_Selection_InvalidatedEventId, container);
    } else {
      const bool container_is_multiselectable =
          container && container->HasState(ax::mojom::State::kMultiselectable);
      for (auto* item : changes.added) {
        if (container_is_multiselectable) {
          FireUiaAccessibilityEvent(
              UIA_SelectionItem_ElementAddedToSelectionEventId, item);
        } else {
          FireUiaAccessibilityEvent(UIA_SelectionItem_ElementSelectedEventId,
                                    item);
        }
        FireUiaPropertyChangedEvent(UIA_SelectionItemIsSelectedPropertyId,
                                    item);
      }
      for (auto* item : changes.removed) {
        FireUiaAccessibilityEvent(
            UIA_SelectionItem_ElementRemovedFromSelectionEventId, item);
        FireUiaPropertyChangedEvent(UIA_SelectionItemIsSelectedPropertyId,
                                    item);
      }
    }
  }
}

// static
void BrowserAccessibilityManagerWin::HandleSelectedStateChanged(
    SelectionEventsMap& selection_events_map,
    BrowserAccessibility* node,
    bool is_selected) {
  // If |node| belongs to a selection container, then map the events with the
  // selection container as the key because |FinalizeSelectionEvents| needs to
  // determine whether or not there is only one element selected in order to
  // optimize what platform events are sent.
  BrowserAccessibility* key = node;
  if (auto* selection_container = node->PlatformGetSelectionContainer())
    key = selection_container;

  if (is_selected)
    selection_events_map[key].added.push_back(node);
  else
    selection_events_map[key].removed.push_back(node);
}

// static
void BrowserAccessibilityManagerWin::FinalizeSelectionEvents(
    SelectionEventsMap& selection_events_map,
    IsSelectedPredicate is_selected_predicate,
    FirePlatformSelectionEventsCallback fire_platform_events_callback) {
  for (auto&& selected : selection_events_map) {
    BrowserAccessibility* key_node = selected.first;
    SelectionEvents& changes = selected.second;

    // Determine if |node| is a selection container with one selected child in
    // order to optimize what platform events are sent.
    BrowserAccessibility* container = nullptr;
    BrowserAccessibility* only_selected_child = nullptr;
    if (ui::IsContainerWithSelectableChildren(key_node->GetRole())) {
      container = key_node;
      for (auto it = container->InternalChildrenBegin();
           it != container->InternalChildrenEnd(); ++it) {
        auto* child = it.get();
        if (is_selected_predicate.Run(child)) {
          if (!only_selected_child) {
            only_selected_child = child;
            continue;
          }

          only_selected_child = nullptr;
          break;
        }
      }
    }

    fire_platform_events_callback.Run(container, only_selected_child, changes);
  }

  selection_events_map.clear();
}

void BrowserAccessibilityManagerWin::BeforeAccessibilityEvents() {
  BrowserAccessibilityManager::BeforeAccessibilityEvents();

  for (const auto& targeted_event : event_generator()) {
    if (targeted_event.event_params.event ==
        ui::AXEventGenerator::Event::IGNORED_CHANGED) {
      BrowserAccessibility* event_target = GetFromAXNode(targeted_event.node);
      if (!event_target)
        continue;

      const auto insert_pair = ignored_changed_nodes_.insert(event_target);

      // Expect that |IGNORED_CHANGED| only fires once for a given
      // node in a given event frame.
      DCHECK(insert_pair.second);
    }
  }
}

void BrowserAccessibilityManagerWin::FinalizeAccessibilityEvents() {
  BrowserAccessibilityManager::FinalizeAccessibilityEvents();

  // Finalize aria properties events.
  for (auto&& event_node : aria_properties_events_) {
    FireUiaPropertyChangedEvent(UIA_AriaPropertiesPropertyId, event_node);
  }
  aria_properties_events_.clear();

  // Finalize text selection events.
  for (auto&& sel_event_node : text_selection_changed_events_) {
    FireUiaTextContainerEvent(UIA_Text_TextSelectionChangedEventId,
                              sel_event_node);
  }
  text_selection_changed_events_.clear();

  // Finalize selection item events.
  FinalizeSelectionEvents(
      ia2_selection_events_, base::BindRepeating(&IsIA2NodeSelected),
      base::BindRepeating(
          &BrowserAccessibilityManagerWin::FireIA2SelectionEvents,
          base::Unretained(this)));
  FinalizeSelectionEvents(
      uia_selection_events_, base::BindRepeating(&IsUIANodeSelected),
      base::BindRepeating(
          &BrowserAccessibilityManagerWin::FireUIASelectionEvents,
          base::Unretained(this)));

  ignored_changed_nodes_.clear();
}

BrowserAccessibilityManagerWin::SelectionEvents::SelectionEvents() = default;
BrowserAccessibilityManagerWin::SelectionEvents::~SelectionEvents() = default;

}  // namespace content
