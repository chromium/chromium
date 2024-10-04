// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_android.h"

#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/i18n/char_iterator.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/web_contents_accessibility_android.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"

namespace content {

// static
ui::BrowserAccessibilityManager* BrowserAccessibilityManagerAndroid::Create(
    const ui::AXTreeUpdate& initial_tree,
    ui::AXNodeIdDelegate& node_id_delegate,
    ui::AXPlatformTreeManagerDelegate* delegate) {
  if (!delegate) {
    return new BrowserAccessibilityManagerAndroid(initial_tree, nullptr,
                                                  node_id_delegate, nullptr);
  }

  WebContentsAccessibilityAndroid* wcax = nullptr;
  if (delegate->AccessibilityIsRootFrame()) {
    wcax = static_cast<WebContentsAccessibilityAndroid*>(
        delegate->AccessibilityGetWebContentsAccessibility());
  }
  return new BrowserAccessibilityManagerAndroid(
      initial_tree, wcax ? wcax->GetWeakPtr() : nullptr, node_id_delegate,
      delegate);
}

// static
ui::BrowserAccessibilityManager* BrowserAccessibilityManagerAndroid::Create(
    ui::AXNodeIdDelegate& node_id_delegate,
    ui::AXPlatformTreeManagerDelegate* delegate) {
  return BrowserAccessibilityManagerAndroid::Create(
      BrowserAccessibilityManagerAndroid::GetEmptyDocument(), node_id_delegate,
      delegate);
}

BrowserAccessibilityManagerAndroid::BrowserAccessibilityManagerAndroid(
    const ui::AXTreeUpdate& initial_tree,
    base::WeakPtr<WebContentsAccessibilityAndroid> web_contents_accessibility,
    ui::AXNodeIdDelegate& node_id_delegate,
    ui::AXPlatformTreeManagerDelegate* delegate)
    : ui::BrowserAccessibilityManager(node_id_delegate, delegate),
      web_contents_accessibility_(std::move(web_contents_accessibility)),
      prune_tree_for_screen_reader_(true) {
  // The Java layer handles the root scroll offset.
  use_root_scroll_offsets_when_computing_bounds_ = false;

  Initialize(initial_tree);
}

BrowserAccessibilityManagerAndroid::~BrowserAccessibilityManagerAndroid() =
    default;

// static
ui::AXTreeUpdate BrowserAccessibilityManagerAndroid::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = ui::kInitialEmptyDocumentRootNodeID;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  empty_document.SetRestriction(ax::mojom::Restriction::kReadOnly);
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerAndroid::ResetWebContentsAccessibility() {
  web_contents_accessibility_.reset();
}

bool BrowserAccessibilityManagerAndroid::ShouldAllowImageDescriptions() {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  return (wcax && wcax->should_allow_image_descriptions()) ||
         allow_image_descriptions_for_testing_;
}

ui::BrowserAccessibility* BrowserAccessibilityManagerAndroid::GetFocus() const {
  // On Android, don't follow active descendant when focus is in a textfield,
  // otherwise editable comboboxes such as the search field on google.com do
  // not work with Talkback. See crbug.com/761501.
  // TODO(accessibility) How does Talkback then read the active item?
  // This fix came in crrev.com/c/647339 but said that a more comprehensive fix
  // was landing in in crrev.com/c/642056, so is this override still necessary?
  ui::AXNodeID focus_id = GetTreeData().focus_id;
  ui::BrowserAccessibility* focus = GetFromID(focus_id);
  if (focus && focus->IsAtomicTextField()) {
    return focus;
  }

  return ui::BrowserAccessibilityManager::GetFocus();
}

ui::AXNode* BrowserAccessibilityManagerAndroid::RetargetForEvents(
    ui::AXNode* node,
    RetargetEventType type) const {
  // TODO(crbug.com/40856596): Node should not be null. But this seems to be
  // happening in the wild for reasons not yet determined. Because the only
  // consequence of node being null is that we'll fail to fire an event on a
  // non-existent object, the style guide's suggestion of using a CHECK
  // temporarily seems a bit strong. Nonetheless we should get to the bottom of
  // this. So we are temporarily using NOTREACHED in the hopes that ClusterFuzz
  // will lead to a reliably-reproducible test case.
  if (!node) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  // Sometimes we get events on nodes in our internal accessibility tree
  // that aren't exposed on Android. Get |updated| to point to the lowest
  // ancestor that is exposed.
  DUMP_WILL_BE_CHECK(node);
  ui::BrowserAccessibility* wrapper = GetFromAXNode(node);
  DUMP_WILL_BE_CHECK(wrapper);
  ui::BrowserAccessibility* updated =
      wrapper->PlatformGetLowestPlatformAncestor();
  DCHECK(updated);

  switch (type) {
    case RetargetEventType::RetargetEventTypeGenerated: {
      // If the closest platform object is a password field, the event we're
      // getting is doing something in the shadow dom, for example replacing a
      // character with a dot after a short pause. On Android we don't want to
      // fire an event for those changes, but we do want to make sure our
      // internal state is correct, so we call OnDataChanged() and then return.
      if (updated->IsPasswordField() && wrapper != updated) {
        updated->OnDataChanged();
        return nullptr;
      }
      break;
    }
    case RetargetEventType::RetargetEventTypeBlinkGeneral:
      break;
    case RetargetEventType::RetargetEventTypeBlinkHover: {
      // If this node is uninteresting and just a wrapper around a sole
      // interesting descendant, prefer that descendant instead.
      const BrowserAccessibilityAndroid* android_node =
          static_cast<BrowserAccessibilityAndroid*>(updated);
      const BrowserAccessibilityAndroid* sole_interesting_node =
          android_node->GetSoleInterestingNodeFromSubtree();
      if (sole_interesting_node) {
        android_node = sole_interesting_node;
      }

      // Finally, if this node is still uninteresting, try to walk up to
      // find an interesting parent.
      while (android_node && !android_node->IsInterestingOnAndroid()) {
        android_node = static_cast<BrowserAccessibilityAndroid*>(
            android_node->PlatformGetParent());
      }
      updated = const_cast<BrowserAccessibilityAndroid*>(android_node);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return updated ? updated->node() : nullptr;
}

void BrowserAccessibilityManagerAndroid::FireFocusEvent(ui::AXNode* node) {
  ui::AXTreeManager::FireFocusEvent(node);
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  // When focusing a node on Android, we want to ensure that we clear the
  // Java-side cache for the previously focused node as well.
  if (ui::BrowserAccessibility* last_focused_node =
          GetFromAXNode(GetLastFocusedNode())) {
    BrowserAccessibilityAndroid* android_last_focused_node =
        static_cast<BrowserAccessibilityAndroid*>(last_focused_node);
    wcax->ClearNodeInfoCacheForGivenId(
        android_last_focused_node->GetUniqueId());
  }

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(GetFromAXNode(node));
  wcax->HandleFocusChanged(android_node->GetUniqueId());
}

void BrowserAccessibilityManagerAndroid::FireLocationChanged(
    ui::BrowserAccessibility* node) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);
  wcax->HandleContentChanged(android_node->GetUniqueId());
}

void BrowserAccessibilityManagerAndroid::FireBlinkEvent(
    ax::mojom::Event event_type,
    ui::BrowserAccessibility* node,
    int action_request_id) {
  ui::BrowserAccessibilityManager::FireBlinkEvent(event_type, node,
                                                  action_request_id);
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  switch (event_type) {
    case ax::mojom::Event::kClicked:
      wcax->HandleClicked(android_node->GetUniqueId());
      break;
    case ax::mojom::Event::kEndOfTest:
      wcax->HandleEndOfTestSignal();
      break;
    case ax::mojom::Event::kHover:
      HandleHoverEvent(node);
      break;
    case ax::mojom::Event::kScrolledToAnchor:
      wcax->HandleScrolledToAnchor(android_node->GetUniqueId());
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerAndroid::FireGeneratedEvent(
    ui::AXEventGenerator::Event event_type,
    const ui::AXNode* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  ui::BrowserAccessibility* wrapper = GetFromAXNode(node);
  DCHECK(wrapper);
  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(wrapper);

  if (event_type == ui::AXEventGenerator::Event::CHILDREN_CHANGED) {
    BrowserAccessibilityAndroid::ResetLeafCache();
  }

  // Always send AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED to notify
  // the Android system that the accessibility hierarchy rooted at this
  // node has changed.
  if (event_type != ui::AXEventGenerator::Event::SUBTREE_CREATED) {
    wcax->HandleContentChanged(android_node->GetUniqueId());
  }

  switch (event_type) {
    case ui::AXEventGenerator::Event::ALERT: {
      // When an alertdialog is shown, we will announce the hint, which
      // (should) contain the description set by the author. If it is
      // empty, then we will try GetTextContentUTF16() as a fallback.
      std::u16string text = android_node->GetHint();
      if (text.empty()) {
        text = android_node->GetTextContentUTF16();
      }

      wcax->AnnounceLiveRegionText(text);
      wcax->HandleDialogModalOpened(android_node->GetUniqueId());
      break;
    }
    case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      wcax->HandleCheckStateChanged(android_node->GetUniqueId());
      if (android_node->GetRole() == ax::mojom::Role::kToggleButton ||
          android_node->GetRole() == ax::mojom::Role::kSwitch ||
          android_node->GetRole() == ax::mojom::Role::kRadioButton) {
        wcax->HandleStateDescriptionChanged(android_node->GetUniqueId());
      }
      break;
    case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED: {
      ui::AXNodeID focus_id =
          ax_tree()->GetUnignoredSelection().focus_object_id;
      ui::BrowserAccessibility* focus_object = GetFromID(focus_id);
      if (focus_object) {
        BrowserAccessibilityAndroid* android_focus_object =
            static_cast<BrowserAccessibilityAndroid*>(focus_object);
        wcax->HandleTextSelectionChanged(android_focus_object->GetUniqueId());
      }
      break;
    }
    case ui::AXEventGenerator::Event::EXPANDED: {
      if (ui::IsComboBox(android_node->GetRole()) &&
          GetFocus()->IsDescendantOf(android_node)) {
        wcax->AnnounceLiveRegionText(android_node->GetComboboxExpandedText());
      }
      break;
    }
    case ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED: {
      // This event is fired when an object appears in a live region.
      // Speak its text.
      std::u16string text = android_node->GetTextContentUTF16();
      wcax->AnnounceLiveRegionText(text);
      break;
    }
    case ui::AXEventGenerator::Event::NAME_CHANGED: {
      // Clear node from cache whenever the name changes to ensure fresh data.
      wcax->ClearNodeInfoCacheForGivenId(android_node->GetUniqueId());

      // If this is a simple text element, also send an event to the framework.
      if (ui::IsText(android_node->GetRole()) ||
          android_node->IsAndroidTextView()) {
        wcax->HandleTextContentChanged(android_node->GetUniqueId());
      }
      break;
    }
    case ui::AXEventGenerator::Event::RANGE_VALUE_CHANGED:
      DCHECK(android_node->GetData().IsRangeValueSupported());
      if (android_node->IsSlider()) {
        wcax->HandleSliderChanged(android_node->GetUniqueId());
      }
      break;
    case ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
    case ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
      wcax->HandleScrollPositionChanged(android_node->GetUniqueId());
      break;
    case ui::AXEventGenerator::Event::SUBTREE_CREATED: {
      // When a dialog is shown, we will send a SUBTREE_CREATED event.
      // When this happens, we want to generate a TYPE_WINDOW_STATE_CHANGED
      // event and populate the node's paneTitle with the dialog description.
      if (android_node->GetRole() == ax::mojom::Role::kDialog) {
        wcax->HandleDialogModalOpened(android_node->GetUniqueId());
      }
      break;
    }
    case ui::AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED:
      // Sometimes `RetargetForEvents` will walk up to the lowest platform leaf
      // and fire the same event on that node. However, in some rare cases the
      // leaf node might not be a text field. For example, in the unusual case
      // when the text field is inside a button, the leaf node is the button not
      // the text field.
      if (android_node->IsTextField() && GetFocus() == wrapper) {
        wcax->HandleEditableTextChanged(android_node->GetUniqueId());
      }
      break;

    // Currently unused events on this platform.
    case ui::AXEventGenerator::Event::NONE:
    case ui::AXEventGenerator::Event::ACCESS_KEY_CHANGED:
    case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
    case ui::AXEventGenerator::Event::ARIA_CURRENT_CHANGED:
    case ui::AXEventGenerator::Event::ARIA_NOTIFICATIONS_POSTED:
    case ui::AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::ATOMIC_CHANGED:
    case ui::AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
    case ui::AXEventGenerator::Event::AUTOFILL_AVAILABILITY_CHANGED:
    case ui::AXEventGenerator::Event::BUSY_CHANGED:
    case ui::AXEventGenerator::Event::CARET_BOUNDS_CHANGED:
    case ui::AXEventGenerator::Event::CHECKED_STATE_DESCRIPTION_CHANGED:
    case ui::AXEventGenerator::Event::CHILDREN_CHANGED:
    case ui::AXEventGenerator::Event::COLLAPSED:
    case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
    case ui::AXEventGenerator::Event::DETAILS_CHANGED:
    case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
    case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
    case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
    case ui::AXEventGenerator::Event::EDITABLE_TEXT_CHANGED:
    case ui::AXEventGenerator::Event::ENABLED_CHANGED:
    case ui::AXEventGenerator::Event::FOCUS_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_FROM_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_TO_CHANGED:
    case ui::AXEventGenerator::Event::HASPOPUP_CHANGED:
    case ui::AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
    case ui::AXEventGenerator::Event::IGNORED_CHANGED:
    case ui::AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
    case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
    case ui::AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
    case ui::AXEventGenerator::Event::LABELED_BY_CHANGED:
    case ui::AXEventGenerator::Event::LANGUAGE_CHANGED:
    case ui::AXEventGenerator::Event::LAYOUT_INVALIDATED:
    case ui::AXEventGenerator::Event::LIVE_REGION_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_REGION_CREATED:
    case ui::AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_STATUS_CHANGED:
    case ui::AXEventGenerator::Event::MENU_POPUP_END:
    case ui::AXEventGenerator::Event::MENU_POPUP_START:
    case ui::AXEventGenerator::Event::MENU_ITEM_SELECTED:
    case ui::AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
    case ui::AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
    case ui::AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::ORIENTATION_CHANGED:
    case ui::AXEventGenerator::Event::PARENT_CHANGED:
    case ui::AXEventGenerator::Event::PLACEHOLDER_CHANGED:
    case ui::AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
    case ui::AXEventGenerator::Event::RANGE_VALUE_MAX_CHANGED:
    case ui::AXEventGenerator::Event::RANGE_VALUE_MIN_CHANGED:
    case ui::AXEventGenerator::Event::RANGE_VALUE_STEP_CHANGED:
    case ui::AXEventGenerator::Event::READONLY_CHANGED:
    case ui::AXEventGenerator::Event::RELATED_NODE_CHANGED:
    case ui::AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
    case ui::AXEventGenerator::Event::ROLE_CHANGED:
    case ui::AXEventGenerator::Event::ROW_COUNT_CHANGED:
    case ui::AXEventGenerator::Event::SELECTED_CHANGED:
    case ui::AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
    case ui::AXEventGenerator::Event::SELECTED_VALUE_CHANGED:
    case ui::AXEventGenerator::Event::SET_SIZE_CHANGED:
    case ui::AXEventGenerator::Event::SORT_CHANGED:
    case ui::AXEventGenerator::Event::STATE_CHANGED:
    case ui::AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED:
    case ui::AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
      break;
  }
}

void BrowserAccessibilityManagerAndroid::FireAriaNotificationEvent(
    ui::BrowserAccessibility* node,
    const std::string& announcement,
    const std::string& notification_id,
    ax::mojom::AriaNotificationInterrupt interrupt_property,
    ax::mojom::AriaNotificationPriority priority_property) {
  DCHECK(node);

  auto* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  wcax->AnnounceLiveRegionText(base::UTF8ToUTF16(announcement));
}

void BrowserAccessibilityManagerAndroid::SendLocationChangeEvents(
    const std::vector<ui::AXLocationChange>& changes) {
  // Android is not very efficient at handling notifications, and location
  // changes in particular are frequent and not time-critical. If a lot of
  // nodes changed location, just send a single notification after a short
  // delay (to batch them), rather than lots of individual notifications.
  if (changes.size() > 3) {
    auto* wcax = GetWebContentsAXFromRootManager();
    if (!wcax) {
      return;
    }
    wcax->SendDelayedWindowContentChangedEvent();
    return;
  }
  BrowserAccessibilityManager::SendLocationChangeEvents(changes);
}

bool BrowserAccessibilityManagerAndroid::NextAtGranularity(
    int32_t granularity,
    int32_t cursor_index,
    BrowserAccessibilityAndroid* node,
    int32_t* start_index,
    int32_t* end_index) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER: {
      std::u16string text = node->GetTextContentUTF16();
      if (cursor_index >= static_cast<int32_t>(text.length())) {
        return false;
      }
      base::i18n::UTF16CharIterator iter(text);
      while (!iter.end() &&
             static_cast<int32_t>(iter.array_pos()) <= cursor_index) {
        iter.Advance();
      }
      *end_index = iter.array_pos();
      iter.Rewind();
      *start_index = iter.array_pos();
      break;
    }
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE: {
      std::vector<int32_t> starts;
      std::vector<int32_t> ends;
      node->GetGranularityBoundaries(granularity, &starts, &ends, 0);
      if (starts.size() == 0) {
        return false;
      }

      size_t index = 0;
      while (index < starts.size() - 1 && starts[index] < cursor_index) {
        index++;
      }

      if (starts[index] < cursor_index) {
        return false;
      }

      *start_index = starts[index];
      *end_index = ends[index];
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return true;
}

bool BrowserAccessibilityManagerAndroid::PreviousAtGranularity(
    int32_t granularity,
    int32_t cursor_index,
    BrowserAccessibilityAndroid* node,
    int32_t* start_index,
    int32_t* end_index) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER: {
      if (cursor_index <= 0) {
        return false;
      }
      std::u16string text = node->GetTextContentUTF16();
      base::i18n::UTF16CharIterator iter(text);
      int previous_index = 0;
      while (!iter.end() &&
             static_cast<int32_t>(iter.array_pos()) < cursor_index) {
        previous_index = iter.array_pos();
        iter.Advance();
      }
      *start_index = previous_index;
      *end_index = iter.array_pos();
      break;
    }
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE: {
      std::vector<int32_t> starts;
      std::vector<int32_t> ends;
      node->GetGranularityBoundaries(granularity, &starts, &ends, 0);
      if (starts.size() == 0) {
        return false;
      }

      size_t index = starts.size() - 1;
      while (index > 0 && starts[index] >= cursor_index) {
        index--;
      }

      if (starts[index] >= cursor_index) {
        return false;
      }

      *start_index = starts[index];
      *end_index = ends[index];
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return true;
}

void BrowserAccessibilityManagerAndroid::ClearNodeInfoCacheForGivenId(
    int32_t unique_id) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  // We do not need to clear a node more than once per atomic update.
  if (base::Contains(nodes_already_cleared_, unique_id)) {
    return;
  }

  nodes_already_cleared_.emplace(unique_id);
  wcax->ClearNodeInfoCacheForGivenId(unique_id);
}

bool BrowserAccessibilityManagerAndroid::OnHoverEvent(
    const ui::MotionEventAndroid& event) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  return wcax ? wcax->OnHoverEvent(event) : false;
}

void BrowserAccessibilityManagerAndroid::HandleHoverEvent(
    ui::BrowserAccessibility* node) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  if (android_node) {
    wcax->HandleHover(android_node->GetUniqueId());
  }
}

void BrowserAccessibilityManagerAndroid::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                             ui::AXNode* node) {
  // https://crbug.com/361196029 looks like a nullptr deref. It's unexpected
  // that ui::AXTree would pass a null node to an observer, and that the
  // manager would not have a BrowserAccessibility wrapper for it.
  DUMP_WILL_BE_CHECK(node);
  ui::BrowserAccessibility* wrapper = GetFromAXNode(node);
  DUMP_WILL_BE_CHECK(wrapper);

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(wrapper);

  ClearNodeInfoCacheForGivenId(android_node->GetUniqueId());

  // When a node will be deleted, clear its parent from the cache as well, or
  // the parent could erroneously report the cleared node as a child later on.
  BrowserAccessibilityAndroid* parent_node =
      static_cast<BrowserAccessibilityAndroid*>(
          android_node->PlatformGetParent());
  if (parent_node != nullptr) {
    ClearNodeInfoCacheForGivenId(parent_node->GetUniqueId());
  }

  BrowserAccessibilityManager::OnNodeWillBeDeleted(tree, node);
}

std::unique_ptr<ui::BrowserAccessibility>
BrowserAccessibilityManagerAndroid::CreateBrowserAccessibility(
    ui::AXNode* node) {
  return ui::BrowserAccessibility::Create(this, node);
}

void BrowserAccessibilityManagerAndroid::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  // Reset content changed events counter every time we finish an atomic update.
  wcax->ResetContentChangedEventsCounter();

  // Clear unordered_set of nodes cleared from the cache after atomic update.
  nodes_already_cleared_.clear();

  // When the root changes, send the new root id and a navigate signal to Java.
  if (root_changed) {
    auto* root_manager = static_cast<BrowserAccessibilityManagerAndroid*>(
        GetManagerForRootFrame());
    DCHECK(root_manager);

    auto* root = static_cast<BrowserAccessibilityAndroid*>(
        root_manager->GetBrowserAccessibilityRoot());
    DCHECK(root);

    wcax->HandleNavigate(root->GetUniqueId());
  }

  // Update the maximum number of nodes in the cache after each atomic update.
  wcax->UpdateMaxNodesInCache();
}

WebContentsAccessibilityAndroid*
BrowserAccessibilityManagerAndroid::GetWebContentsAXFromRootManager() {
  ui::BrowserAccessibility* parent_node =
      GetParentNodeFromParentTreeAsBrowserAccessibility();
  if (!parent_node) {
    return web_contents_accessibility_.get();
  }

  auto* parent_manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(parent_node->manager());
  return parent_manager->GetWebContentsAXFromRootManager();
}

std::u16string
BrowserAccessibilityManagerAndroid::GenerateAccessibilityNodeInfoString(
    int32_t unique_id) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return {};
  }

  return wcax->GenerateAccessibilityNodeInfoString(unique_id);
}

std::vector<std::string>
BrowserAccessibilityManagerAndroid::GetMetadataForTree() const {
  return GetTreeData().metadata;
}

}  // namespace content
