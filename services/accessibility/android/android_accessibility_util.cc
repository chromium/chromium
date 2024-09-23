// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/android_accessibility_util.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ax::android {

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXEventIntProperty = mojom::AccessibilityEventIntProperty;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;

std::optional<ax::mojom::Event> ToAXEvent(
    mojom::AccessibilityEventType android_event_type,
    AccessibilityInfoDataWrapper* source_node,
    AccessibilityInfoDataWrapper* focused_node) {
  switch (android_event_type) {
    case mojom::AccessibilityEventType::VIEW_FOCUSED:
    case mojom::AccessibilityEventType::VIEW_ACCESSIBILITY_FOCUSED:
      return ax::mojom::Event::kFocus;
    case mojom::AccessibilityEventType::VIEW_ACCESSIBILITY_FOCUS_CLEARED:
      return ax::mojom::Event::kBlur;
    case mojom::AccessibilityEventType::VIEW_CLICKED:
    case mojom::AccessibilityEventType::VIEW_LONG_CLICKED:
      return ax::mojom::Event::kClicked;
    case mojom::AccessibilityEventType::VIEW_TEXT_CHANGED:
      return std::nullopt;
    case mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED:
      return ax::mojom::Event::kTextSelectionChanged;
    case mojom::AccessibilityEventType::WINDOW_STATE_CHANGED: {
      if (focused_node) {
        return ax::mojom::Event::kFocus;
      } else {
        return std::nullopt;
      }
    }
    case mojom::AccessibilityEventType::WINDOW_CONTENT_CHANGED:
      int live_region_type_int;
      if (source_node && source_node->GetNode() &&
          GetProperty(source_node->GetNode()->int_properties,
                      AXIntProperty::LIVE_REGION, &live_region_type_int)) {
        mojom::AccessibilityLiveRegionType live_region_type =
            static_cast<mojom::AccessibilityLiveRegionType>(
                live_region_type_int);
        if (live_region_type != mojom::AccessibilityLiveRegionType::NONE) {
          // Dispatch a kLiveRegionChanged event to ensure that all liveregions
          // (inc. snackbar) will get announced. It is currently difficult to
          // determine when liveregions need to be announced, in particular
          // differentiaiting between when they first appear (vs text changed).
          // This case is made evident with snackbar handling, which needs to be
          // announced when it appears.
          // TODO(b/187465133): Revisit this liveregion handling logic, once
          // the talkback spec has been clarified. There is a proposal to write
          // an API to expose attributes similar to aria-relevant, which will
          // eventually allow liveregions to be handled similar to how it gets
          // handled on the web.
          return ax::mojom::Event::kLiveRegionChanged;
        }
      }
      return std::nullopt;
    case mojom::AccessibilityEventType::VIEW_HOVER_ENTER:
      return ax::mojom::Event::kHover;
    case mojom::AccessibilityEventType::ANNOUNCEMENT: {
      // NOTE: Announcement event is handled in
      // ArcAccessibilityHelperBridge::OnAccessibilityEvent.
      NOTREACHED_IN_MIGRATION();
      break;
    }
    case mojom::AccessibilityEventType::VIEW_SCROLLED:
      return ax::mojom::Event::kScrollPositionChanged;
    case mojom::AccessibilityEventType::VIEW_SELECTED: {
      // VIEW_SELECTED event is not selection event in Chrome.
      // See the comment on AXTreeSourceAndroid::UpdateAndroidFocusedId.
      if (source_node && source_node->IsNode() &&
          source_node->GetNode()->range_info) {
        return std::nullopt;
      } else {
        return ax::mojom::Event::kFocus;
      }
    }
    case mojom::AccessibilityEventType::INVALID_ENUM_VALUE: {
      NOTREACHED_IN_MIGRATION();
      break;
    }
    case mojom::AccessibilityEventType::NOTIFICATION_STATE_CHANGED:
    case mojom::AccessibilityEventType::VIEW_HOVER_EXIT:
    case mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_START:
    case mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_END:
    case mojom::AccessibilityEventType::
        VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
    case mojom::AccessibilityEventType::GESTURE_DETECTION_START:
    case mojom::AccessibilityEventType::GESTURE_DETECTION_END:
    case mojom::AccessibilityEventType::TOUCH_INTERACTION_START:
    case mojom::AccessibilityEventType::TOUCH_INTERACTION_END:
    case mojom::AccessibilityEventType::WINDOWS_CHANGED:
    case mojom::AccessibilityEventType::VIEW_CONTEXT_CLICKED:
    case mojom::AccessibilityEventType::ASSIST_READING_CONTEXT:
      return std::nullopt;
  }
  return std::nullopt;
}

std::optional<mojom::AccessibilityActionType> ConvertToAndroidAction(
    ax::mojom::Action action) {
  switch (action) {
    case ax::mojom::Action::kDoDefault:
      return ax::android::mojom::AccessibilityActionType::CLICK;
    case ax::mojom::Action::kFocus:
      return ax::android::mojom::AccessibilityActionType::FOCUS;
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
      return ax::android::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kScrollToMakeVisible:
      return ax::android::mojom::AccessibilityActionType::SHOW_ON_SCREEN;
    case ax::mojom::Action::kScrollBackward:
      return ax::android::mojom::AccessibilityActionType::SCROLL_BACKWARD;
    case ax::mojom::Action::kScrollForward:
      return ax::android::mojom::AccessibilityActionType::SCROLL_FORWARD;
    case ax::mojom::Action::kScrollUp:
      return ax::android::mojom::AccessibilityActionType::SCROLL_UP;
    case ax::mojom::Action::kScrollDown:
      return ax::android::mojom::AccessibilityActionType::SCROLL_DOWN;
    case ax::mojom::Action::kScrollLeft:
      return ax::android::mojom::AccessibilityActionType::SCROLL_LEFT;
    case ax::mojom::Action::kScrollRight:
      return ax::android::mojom::AccessibilityActionType::SCROLL_RIGHT;
    case ax::mojom::Action::kScrollToPositionAtRowColumn:
      return ax::android::mojom::AccessibilityActionType::SCROLL_TO_POSITION;
    case ax::mojom::Action::kCustomAction:
      return ax::android::mojom::AccessibilityActionType::CUSTOM_ACTION;
    case ax::mojom::Action::kSetAccessibilityFocus:
      return ax::android::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kClearAccessibilityFocus:
      return ax::android::mojom::AccessibilityActionType::
          CLEAR_ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kGetTextLocation:
      return ax::android::mojom::AccessibilityActionType::GET_TEXT_LOCATION;
    case ax::mojom::Action::kShowTooltip:
      return ax::android::mojom::AccessibilityActionType::SHOW_TOOLTIP;
    case ax::mojom::Action::kHideTooltip:
      return ax::android::mojom::AccessibilityActionType::HIDE_TOOLTIP;
    case ax::mojom::Action::kCollapse:
      return ax::android::mojom::AccessibilityActionType::COLLAPSE;
    case ax::mojom::Action::kExpand:
      return ax::android::mojom::AccessibilityActionType::EXPAND;
    case ax::mojom::Action::kLongClick:
      return ax::android::mojom::AccessibilityActionType::LONG_CLICK;
    default:
      return std::nullopt;
  }
}

ax::mojom::Action ConvertToChromeAction(
    const mojom::AccessibilityActionType action) {
  switch (action) {
    case ax::android::mojom::AccessibilityActionType::CLICK:
      return ax::mojom::Action::kDoDefault;
    case ax::android::mojom::AccessibilityActionType::FOCUS:
      return ax::mojom::Action::kFocus;
    case ax::android::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS:
      // TODO(hirokisato): there are multiple actions converted to
      // ACCESSIBILITY_FOCUS. Consider if this is appropriate.
      return ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint;
    case ax::android::mojom::AccessibilityActionType::SHOW_ON_SCREEN:
      return ax::mojom::Action::kScrollToMakeVisible;
    case ax::android::mojom::AccessibilityActionType::SCROLL_BACKWARD:
      return ax::mojom::Action::kScrollBackward;
    case ax::android::mojom::AccessibilityActionType::SCROLL_FORWARD:
      return ax::mojom::Action::kScrollForward;
    case ax::android::mojom::AccessibilityActionType::SCROLL_UP:
      return ax::mojom::Action::kScrollUp;
    case ax::android::mojom::AccessibilityActionType::SCROLL_DOWN:
      return ax::mojom::Action::kScrollDown;
    case ax::android::mojom::AccessibilityActionType::SCROLL_LEFT:
      return ax::mojom::Action::kScrollLeft;
    case ax::android::mojom::AccessibilityActionType::SCROLL_RIGHT:
      return ax::mojom::Action::kScrollRight;
    case ax::android::mojom::AccessibilityActionType::CUSTOM_ACTION:
      return ax::mojom::Action::kCustomAction;
    case ax::android::mojom::AccessibilityActionType::CLEAR_ACCESSIBILITY_FOCUS:
      return ax::mojom::Action::kClearAccessibilityFocus;
    case ax::android::mojom::AccessibilityActionType::GET_TEXT_LOCATION:
      return ax::mojom::Action::kGetTextLocation;
    case ax::android::mojom::AccessibilityActionType::SHOW_TOOLTIP:
      return ax::mojom::Action::kShowTooltip;
    case ax::android::mojom::AccessibilityActionType::HIDE_TOOLTIP:
      return ax::mojom::Action::kHideTooltip;
    case ax::android::mojom::AccessibilityActionType::COLLAPSE:
      return ax::mojom::Action::kCollapse;
    case ax::android::mojom::AccessibilityActionType::EXPAND:
      return ax::mojom::Action::kExpand;
    case ax::android::mojom::AccessibilityActionType::LONG_CLICK:
      return ax::mojom::Action::kLongClick;
    case ax::android::mojom::AccessibilityActionType::SCROLL_TO_POSITION:
      return ax::mojom::Action::kScrollToPositionAtRowColumn;
    // Below are actions not mapped in ConvertToAndroidAction().
    case ax::android::mojom::AccessibilityActionType::CLEAR_FOCUS:
    case ax::android::mojom::AccessibilityActionType::SELECT:
    case ax::android::mojom::AccessibilityActionType::CLEAR_SELECTION:
    case ax::android::mojom::AccessibilityActionType::
        NEXT_AT_MOVEMENT_GRANULARITY:
    case ax::android::mojom::AccessibilityActionType::
        PREVIOUS_AT_MOVEMENT_GRANULARITY:
    case ax::android::mojom::AccessibilityActionType::NEXT_HTML_ELEMENT:
    case ax::android::mojom::AccessibilityActionType::PREVIOUS_HTML_ELEMENT:
    case ax::android::mojom::AccessibilityActionType::COPY:
    case ax::android::mojom::AccessibilityActionType::PASTE:
    case ax::android::mojom::AccessibilityActionType::CUT:
    case ax::android::mojom::AccessibilityActionType::SET_SELECTION:
    case ax::android::mojom::AccessibilityActionType::DISMISS:
    case ax::android::mojom::AccessibilityActionType::SET_TEXT:
    case ax::android::mojom::AccessibilityActionType::CONTEXT_CLICK:
    case ax::android::mojom::AccessibilityActionType::SET_PROGRESS:
      return ax::mojom::Action::kNone;
    case mojom::AccessibilityActionType::INVALID_ENUM_VALUE:
      NOTREACHED_IN_MIGRATION();
      return ax::mojom::Action::kNone;
  }
}

AccessibilityInfoDataWrapper* GetSelectedNodeInfoFromAdapterViewEvent(
    const mojom::AccessibilityEventData& event_data,
    AccessibilityInfoDataWrapper* source_node) {
  if (!source_node || !source_node->IsNode()) {
    return nullptr;
  }

  AXNodeInfoData* node_info = source_node->GetNode();
  if (!node_info) {
    return nullptr;
  }

  AccessibilityInfoDataWrapper* selected_node = source_node;
  if (!node_info->collection_item_info) {
    // The event source is not an item of AdapterView. If the event source is
    // AdapterView, select the child. Otherwise, this is an unrelated event.
    int item_count, from_index, current_item_index;
    if (!GetProperty(event_data.int_properties, AXEventIntProperty::ITEM_COUNT,
                     &item_count) ||
        !GetProperty(event_data.int_properties, AXEventIntProperty::FROM_INDEX,
                     &from_index) ||
        !GetProperty(event_data.int_properties,
                     AXEventIntProperty::CURRENT_ITEM_INDEX,
                     &current_item_index)) {
      return nullptr;
    }

    int index = current_item_index - from_index;
    if (index < 0) {
      return nullptr;
    }

    std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>
        children;
    source_node->GetChildren(&children);
    if (index >= static_cast<int>(children.size())) {
      return nullptr;
    }

    selected_node = children[index];
  }

  // Sometimes a collection item is wrapped by a non-focusable node.
  // Find a node with focusable property.
  while (selected_node && !GetBooleanProperty(selected_node->GetNode(),
                                              AXBooleanProperty::FOCUSABLE)) {
    std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>
        children;
    selected_node->GetChildren(&children);
    if (children.size() != 1) {
      break;
    }
    selected_node = children[0];
  }
  return selected_node;
}

std::string ToLiveStatusString(mojom::AccessibilityLiveRegionType type) {
  switch (type) {
    case mojom::AccessibilityLiveRegionType::NONE:
      return "off";
    case mojom::AccessibilityLiveRegionType::POLITE:
      return "polite";
    case mojom::AccessibilityLiveRegionType::ASSERTIVE:
      return "assertive";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return std::string();  // Placeholder.
}

}  // namespace ax::android
