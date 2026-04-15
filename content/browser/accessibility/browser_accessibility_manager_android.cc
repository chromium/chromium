// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_android.h"

#include <optional>
#include <vector>

#include "base/android/android_info.h"
#include "base/check.h"
#include "base/i18n/char_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/web_contents_accessibility_android.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/one_shot_accessibility_tree_search.h"

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
  if (base::FeatureList::IsEnabled(
          features::kAccessibilityRequestScopedContentChangedEvents)) {
    SetAccessibilityEventsCallbackForTesting(
        base::BindRepeating(&BrowserAccessibilityManagerAndroid::
                                OnAccessibilityEventsProcessedForExperiment,
                            base::Unretained(this)));
    SetLocationChangeCallbackForTesting(
        base::BindRepeating(&BrowserAccessibilityManagerAndroid::
                                OnAccessibilityEventsProcessedForExperiment,
                            base::Unretained(this)));
  }

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

ui::BrowserAccessibility*
BrowserAccessibilityManagerAndroid::GetAccessibilityFocus() const {
  if (auto* wcax = GetWebContentsAXFromRootManager()) {
    return wcax->GetAccessibilityFocus();
  }
  return nullptr;
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
    NOTREACHED();
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
      NOTREACHED();
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
    ClearNodeInfoCacheForGivenId(android_last_focused_node->GetUniqueId());
  }

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(GetFromAXNode(node));
  wcax->HandleFocusChanged(
      android_node->GetUniqueId(),
      android_node->manager()->GetBrowserAccessibilityRoot() == android_node);
}

void BrowserAccessibilityManagerAndroid::FireLocationChanged(
    ui::BrowserAccessibility* node) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);
  bool set_subtree_changed = !base::FeatureList::IsEnabled(
      features::kAccessibilityRequestScopedContentChangedEvents);
  wcax->HandleContentChanged(android_node->GetUniqueId(), set_subtree_changed);
}

void BrowserAccessibilityManagerAndroid::FireSourceEvent(
    ax::mojom::Event event_type,
    ui::BrowserAccessibility* node,
    int action_request_id) {
  ui::BrowserAccessibilityManager::FireSourceEvent(event_type, node,
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
    case ax::mojom::Event::kLoadComplete:
      wcax->HandleInitialLoadComplete(
          static_cast<BrowserAccessibilityAndroid*>(node)->GetUniqueId());
      break;
    case ax::mojom::Event::kScrolledToAnchor:
      wcax->HandleScrolledToAnchor(android_node->GetUniqueId());
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerAndroid::FireDocumentSelectionChangedEvent(
    WebContentsAccessibilityAndroid* wcax) {
  std::optional<SelectionRange> selection = GetSelectionRange();
  const bool extended_selection_enabled =
      base::FeatureList::IsEnabled(features::kAccessibilityExtendedSelection);
  const bool expose_children_enabled = base::FeatureList::IsEnabled(
      features::kAccessibilityExposeNonAtomicTextFieldChildren);

  if (extended_selection_enabled) {
    bool should_send_to_root = false;

    if (expose_children_enabled) {
      // Send the event to the root of the frame if selection should be
      // cleared, or multiple nodes are selected, or
      // a non-atomic text field. Atomic text fields will continue to receive
      // their event on them, the rest should go to the root web area.
      // Note that this is to support contenteditables, where the
      // contenteditable root itself is a non-atomic text field, and its
      // children may be editable.
      should_send_to_root =
          !selection.has_value() ||
          selection->focus_object != selection->anchor_object ||
          !selection->focus_object->IsAtomicTextField();
    } else {
      // Send the event to the root of the frame if selection should be
      // cleared, or multiple nodes are selected, or the node is not editable.
      should_send_to_root =
          !selection.has_value() ||
          selection->focus_object != selection->anchor_object ||
          !selection->focus_object->IsTextField();
    }

    if (should_send_to_root) {
      BrowserAccessibilityAndroid* android_root_object =
          static_cast<BrowserAccessibilityAndroid*>(
              GetFromAXNode(ax_tree()->root()));
      ClearNodeInfoCacheForGivenId(android_root_object->GetUniqueId());
      wcax->HandleTextSelectionChanged(android_root_object->GetUniqueId());
      return;
    }
  } else if (!selection.has_value()) {
    // If focus object does not exist and extended selection is not
    // enabled, there is nothing more to do since previous selection node is
    // not known here and can't be cleared.
    return;
  }

  // Send event to the focus node.
  CHECK(selection->focus_object);
  wcax->HandleTextSelectionChanged(selection->focus_object->GetUniqueId());
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

  // Always send AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED to notify
  // the Android system that the accessibility hierarchy rooted at this
  // node has changed.
  if (event_type != ui::AXEventGenerator::Event::SUBTREE_CREATED) {
    bool set_subtree_changed =
        !base::FeatureList::IsEnabled(
            features::kAccessibilityRequestScopedContentChangedEvents) ||
        event_type == ui::AXEventGenerator::Event::CHILDREN_CHANGED;
    wcax->HandleContentChanged(android_node->GetUniqueId(),
                               set_subtree_changed);
  }

  switch (event_type) {
    case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED: {
      wcax->HandleActiveDescendantChanged(android_node->GetUniqueId());
      break;
    }
    case ui::AXEventGenerator::Event::ALERT: {
      wcax->HandlePaneOpened(android_node->GetUniqueId());
      // ALERT events are only fired on the root node of the alert live region,
      // but verify that `node` is in fact an atomic live region root.
      if (base::FeatureList::IsEnabled(
              features::kAccessibilityAtomicLiveRegions) &&
          node->data().IsAtomicLiveRegionRoot()) {
        wcax->HandleAtomicLiveRegionChanged(android_node->GetUniqueId());
      }
      break;
    }
    case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      wcax->HandleCheckStateChanged(android_node->GetUniqueId());
      if (android_node->GetRole() == ax::mojom::Role::kToggleButton ||
          android_node->GetRole() == ax::mojom::Role::kSwitch ||
          android_node->GetRole() == ax::mojom::Role::kRadioButton) {
        wcax->HandleWindowContentChange(
            android_node->GetUniqueId(),
            ANDROID_ACCESSIBILITY_EVENT_CONTENT_CHANGE_TYPE_STATE_DESCRIPTION);
      }
      break;
    case ui::AXEventGenerator::Event::DEFAULT_ACTION_VERB_CHANGED:
      wcax->HandleDefaultActionVerbChanged(android_node->GetUniqueId());
      break;
    case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED: {
      wcax->HandleWindowContentChange(
          android_node->GetUniqueId(),
          ANDROID_ACCESSIBILITY_EVENT_CONTENT_CHANGE_TYPE_UNDEFINED);
      if (android_node->GetRole() == ax::mojom::Role::kDialog ||
          android_node->GetRole() == ax::mojom::Role::kAlertDialog) {
        wcax->HandleWindowContentChange(
            android_node->GetUniqueId(),
            ANDROID_ACCESSIBILITY_EVENT_CONTENT_CHANGE_TYPE_PANE_TITLE);
      }
      break;
    }
    case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED: {
      FireDocumentSelectionChangedEvent(wcax);
      break;
    }
    case ui::AXEventGenerator::Event::EXPANDED: {
      if (ui::IsComboBox(android_node->GetRole()) &&
          GetFocus()->IsDescendantOf(android_node)) {
        wcax->HandlePaneOpened(android_node->GetUniqueId());
      }
      wcax->HandleWindowContentChange(
            android_node->GetUniqueId(),
            ANDROID_ACCESSIBILITY_EVENT_CONTENT_CHANGE_TYPE_EXPANDED);
      break;
    }
    case ui::AXEventGenerator::Event::COLLAPSED: {
      wcax->HandleWindowContentChange(
            android_node->GetUniqueId(),
            ANDROID_ACCESSIBILITY_EVENT_CONTENT_CHANGE_TYPE_EXPANDED);
      break;
    }
    case ui::AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED: {
      wcax->HandleWindowContentChange(
          android_node->GetUniqueId(),
          ANDROID_ACCESSIBILITY_EVENT_CONTENT_CHANGE_TYPE_TEXT);
      break;
    }
    case ui::AXEventGenerator::Event::LIVE_REGION_CHANGED: {
      // When a change is made within a live region, this event is fired on the
      // root node of that live region. For atomic live regions, we should begin
      // at the root node and notify Android of every single node within the
      // subtree of this atomic live region root.
      if (base::FeatureList::IsEnabled(
              features::kAccessibilityAtomicLiveRegions) &&
          node->data().IsAtomicLiveRegionRoot()) {
        wcax->HandleAtomicLiveRegionChanged(android_node->GetUniqueId());
      }
      break;
    }
    case ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED: {
      //  This event is fired when an object appears in a live region.
      if (base::FeatureList::IsEnabled(
              features::kAccessibilityImproveLiveRegionAnnounce)) {
        bool is_atomic = node->data().IsAtomicLiveRegionRoot() ||
                         node->data().IsContainedInAtomicLiveRegion();
        // If kAccessibilityAtomicLiveRegions is enabled and our node is atomic,
        // it will have been handled by the LIVE_REGION_CHANGED case above.
        // Otherwise, fire a WINDOW_CONTENT_CHANGED event to inform the Android
        // Framework of the individual node change.
        if (!(is_atomic && base::FeatureList::IsEnabled(
                               features::kAccessibilityAtomicLiveRegions))) {
          wcax->HandleLiveRegionNodeChanged(android_node->GetUniqueId());
        }
      }
      // TODO(crbug.com/470048610): When the Finch experiment for
      // kAccessibilityAtomicLiveRegions is complete, we should convert these
      // two if-statements into an if-else statement. However, for the
      // experiment, we need both code paths to be preserved.
      if (!base::FeatureList::IsEnabled(
              features::kAccessibilityDeprecateTypeAnnounce)) {
        // If we don't support WINDOW_CONTENT_CHANGED events BUT have not yet
        // deprecated TYPE_ANNOUNCEMENT, we should fire a TYPE_ANNOUNCEMENT
        // event which contains the text of the changed node.
        std::u16string text = android_node->GetTextContentUTF16();
        wcax->AnnounceLiveRegionText(text);
      }
      // If kAccessibilityImproveLiveRegionAnnounce is disabled and
      // kAccessibilityDeprecateTypeAnnounce is enabled, we choose not to fire
      // an event here. However, this should not happen in practice as we should
      // not deprecate TYPE_ANNOUNCEMENT until we have landed its replacements.
      break;
    }
    case ui::AXEventGenerator::Event::MENU_POPUP_START: {
      wcax->HandleMenuOpened(android_node->GetUniqueId());
      break;
    }
    case ui::AXEventGenerator::Event::NAME_CHANGED: {
      // If this is a simple text element, also send an event to the framework.
      if (ui::IsText(android_node->GetRole()) ||
          android_node->IsAndroidTextView()) {
        wcax->HandleWindowContentChange(
            android_node->GetUniqueId(),
            ANDROID_ACCESSIBILITY_EVENT_CONTENT_CHANGE_TYPE_TEXT);
      }

      // If the name of a dialog changes, its pane title also changes.
      // Notify the Android framework about the pane title change.
      if (ui::IsDialog(android_node->GetRole())) {
        wcax->HandleWindowContentChange(
            android_node->GetUniqueId(),
            ANDROID_ACCESSIBILITY_EVENT_CONTENT_CHANGE_TYPE_PANE_TITLE);
      }
      break;
    }
    case ui::AXEventGenerator::Event::RANGE_VALUE_CHANGED:
      DCHECK(android_node->GetData().IsRangeValueSupported());
      if (android_node->IsSlider()) {
        wcax->HandleSliderChanged(android_node->GetUniqueId());
      } else if (base::FeatureList::IsEnabled(
                     features::kAccessibilityMeterEventsOnAndroid) &&
                 android_node->GetRole() == ax::mojom::Role::kMeter) {
        // TalkBack expects Meter value to be changed via state description.
        wcax->HandleWindowContentChange(
            android_node->GetUniqueId(),
            ANDROID_ACCESSIBILITY_EVENT_CONTENT_CHANGE_TYPE_STATE_DESCRIPTION);
      }
      break;
    case ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
    case ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
      wcax->HandleScrollPositionChanged(android_node->GetUniqueId());
      break;
    case ui::AXEventGenerator::Event::SORT_CHANGED:
      // TODO(crbug.com/465804174): Verify if removing aria-sort triggers this
      // event.
      wcax->HandleSortDirectionChanged(android_node->GetUniqueId());
      break;
    case ui::AXEventGenerator::Event::SUBTREE_CREATED: {
      // When a dialog is shown, we will send a SUBTREE_CREATED event.
      // When this happens, we want to generate a TYPE_WINDOW_STATE_CHANGED
      // event and populate the node's paneTitle with the dialog description.
      // Note that kAlertDialog is not included in this condition because the
      // pane opened event is already handled for kAlertDialog by the ALERT
      // event.
      if (android_node->GetRole() == ax::mojom::Role::kDialog) {
        wcax->HandlePaneOpened(android_node->GetUniqueId());
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
        int32_t text_change_types =
            ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGE_TYPE_UNDEFINED;
        if (::features::IsAccessibilityTextChangeTypesEnabled()) {
          if (android_node->GetBoolAttribute(
                  ax::mojom::BoolAttribute::kHasComposition)) {
            text_change_types |=
                ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGE_TYPE_IN_COMPOSITION;
          }
          if (android_node->GetBoolAttribute(
                  ax::mojom::BoolAttribute::kTextSuggestionSelectedByIME)) {
            text_change_types |=
                ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGE_TYPE_CONVERSION_SUGGESTION_SELECTED_BY_IME;
          }
          if (android_node->GetIntAttribute(
                  ax::mojom::IntAttribute::kCommittedTextLength) > 0) {
            text_change_types |=
                ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGE_TYPE_COMMITTED_BY_IME;
          }
        }
        wcax->HandleEditableTextChanged(android_node->GetUniqueId(),
                                        text_change_types);
      }
      break;

    // Currently unused events on this platform.
    case ui::AXEventGenerator::Event::NONE:
    case ui::AXEventGenerator::Event::ACCESS_KEY_CHANGED:
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
    case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
    case ui::AXEventGenerator::Event::DETAILS_CHANGED:
    case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
    case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
    case ui::AXEventGenerator::Event::EDITABLE_TEXT_CHANGED:
    case ui::AXEventGenerator::Event::ENABLED_CHANGED:
    case ui::AXEventGenerator::Event::FOCUS_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_FROM_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_TO_CHANGED:
    case ui::AXEventGenerator::Event::GRAMMAR_MARKER_CHANGED:
    case ui::AXEventGenerator::Event::HASPOPUP_CHANGED:
    case ui::AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
    case ui::AXEventGenerator::Event::HIGHLIGHT_MARKER_CHANGED:
    case ui::AXEventGenerator::Event::IGNORED_CHANGED:
    case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
    case ui::AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
    case ui::AXEventGenerator::Event::LABELED_BY_CHANGED:
    case ui::AXEventGenerator::Event::LANGUAGE_CHANGED:
    case ui::AXEventGenerator::Event::LAYOUT_INVALIDATED:
    case ui::AXEventGenerator::Event::LIVE_REGION_CREATED:
    case ui::AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_STATUS_CHANGED:
    case ui::AXEventGenerator::Event::MENU_POPUP_END:
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
    case ui::AXEventGenerator::Event::SPELLING_MARKER_CHANGED:
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
    ax::mojom::AriaNotificationPriority priority_property,
    ax::mojom::AriaNotificationInterrupt interrupt_property,
    const std::string& type) {
  DCHECK(node);

  auto* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }

  // TODO(aleventhal): If aria-notification becomes a web standard, a solution
  // that doesn't use a forced announcement must be implemented.
  if (!base::FeatureList::IsEnabled(
          features::kAccessibilityDeprecateTypeAnnounce)) {
    wcax->AnnounceLiveRegionText(base::UTF8ToUTF16(announcement));
  }
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
      NOTREACHED();
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
      NOTREACHED();
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
  if (!nodes_already_cleared_.emplace(unique_id).second) {
    return;
  }

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

std::unique_ptr<ui::BrowserAccessibility>
BrowserAccessibilityManagerAndroid::CreateBrowserAccessibility(
    ui::AXNode* node) {
  return ui::BrowserAccessibility::Create(this, node);
}

void BrowserAccessibilityManagerAndroid::OnAtomicUpdateStarting(
    ui::AXTree* tree,
    const absl::flat_hash_set<ui::AXNodeID>& deleting_nodes,
    const absl::flat_hash_set<ui::AXNodeID>& reparenting_nodes) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (wcax) {
    // This set needs to start fresh. This secondary cache is of requests to
    // java to clear that primary cache of Android objects. The idea being only
    // such such request is needed for each atomic update to the tree and node
    // data.
    nodes_already_cleared_.clear();

    // Update the maximum number of nodes in the cache after each atomic update.
    wcax->UpdateMaxNodesInCache();

    for (ui::AXNodeID id : deleting_nodes) {
      ui::BrowserAccessibility* wrapper = GetFromID(id);
      if (!wrapper) {
        continue;
      }

      BrowserAccessibilityAndroid* android_node =
          static_cast<BrowserAccessibilityAndroid*>(wrapper);

      ClearNodeInfoCacheForGivenId(android_node->GetUniqueId());

      // When a node will be deleted, clear its parent from the cache as well,
      // or the parent could erroneously report the cleared node as a child
      // later on.
      BrowserAccessibilityAndroid* parent_node =
          static_cast<BrowserAccessibilityAndroid*>(
              android_node->PlatformGetParent());
      if (parent_node != nullptr) {
        ClearNodeInfoCacheForGivenId(parent_node->GetUniqueId());
      }
    }
  }

  BrowserAccessibilityManager::OnAtomicUpdateStarting(tree, deleting_nodes,
                                                      reparenting_nodes);
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

  // Invalidate java-side cache for structural generated events. This
  // encompasses less nodes than `changes`, but includes unignored retargeted
  // event targets that isn't in `changes`. Eventually, we should prefer
  // invalidations of generated events to those in
  // BrowserAccessibility::OnDataChanged.
  for (const auto& targeted_event : event_generator()) {
    BrowserAccessibilityAndroid* wrapper =
        static_cast<BrowserAccessibilityAndroid*>(
            GetFromID(targeted_event.node_id));
    CHECK(wrapper);

    auto event_type = targeted_event.event_params->event;
    if (event_type == ui::AXEventGenerator::Event::CHILDREN_CHANGED ||
        event_type == ui::AXEventGenerator::Event::PARENT_CHANGED) {
      // Structural changes in the unignored/platform tree requires the leaf
      // cache be invalidated.
      BrowserAccessibilityAndroid::ResetLeafCache();
      ClearNodeInfoCacheForGivenId(wrapper->GetUniqueId());
    }
  }
}

WebContentsAccessibilityAndroid*
BrowserAccessibilityManagerAndroid::GetWebContentsAXFromRootManager() const {
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

std::optional<std::vector<std::string>>
BrowserAccessibilityManagerAndroid::GetMetadataForTree() const {
  return GetTreeData().metadata;
}

std::optional<BrowserAccessibilityManagerAndroid::SelectionRange>
BrowserAccessibilityManagerAndroid::GetSelectionRange() const {
  ui::AXSelection selection = ax_tree()->GetSelection();

  std::optional<std::pair<BrowserAccessibilityAndroid*, int>> anchor =
      ConvertChromeSelectionPositionToAndroid(
          selection.anchor_object_id, selection.anchor_offset,
          selection.anchor_affinity, selection.is_backward);
  if (!anchor.has_value()) {
    return std::nullopt;
  }

  std::optional<std::pair<BrowserAccessibilityAndroid*, int>> focus =
      ConvertChromeSelectionPositionToAndroid(
          selection.focus_object_id, selection.focus_offset,
          selection.focus_affinity, selection.is_backward);
  if (!focus.has_value()) {
    return std::nullopt;
  }

  SelectionRange selection_range;
  selection_range.anchor_object = anchor->first;
  selection_range.anchor_offset = anchor->second;
  selection_range.focus_object = focus->first;
  selection_range.focus_offset = focus->second;

  return selection_range;
}

std::optional<std::pair<BrowserAccessibilityAndroid*, int>>
BrowserAccessibilityManagerAndroid::ConvertChromeSelectionPositionToAndroid(
    ui::AXNodeID node_id,
    int offset,
    ax::mojom::TextAffinity affinity,
    bool is_backward) const {
  ui::AXNode* node = ax_tree()->GetFromId(node_id);
  if (!node) {
    return std::nullopt;
  }

  ui::AXNodePosition::AXPositionInstance position =
      ui::AXNodePosition::CreatePosition(*node, offset, affinity);
  position = position->AsUnignoredSelectionPosition(
      is_backward ? ui::AXPositionAdjustmentBehavior::kMoveForward
                  : ui::AXPositionAdjustmentBehavior::kMoveBackward);
  if (position->IsNullPosition()) {
    return std::nullopt;
  }

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(
          GetFromAXNode(position->GetAnchor()));
  if (!android_node) {
    return std::nullopt;
  }

  // If android node is a text selectable one, ensure position is a text
  // position and anchor exists in Android accessibility tree. This is done even
  // if the node is not a leaf since currently Selection API cannot send the
  // offset type to Android.
  // TODO(crbug.com/498376490): After Selection API with offset type is
  // released, do not force change the selection type and simplify the rest of
  // this function.
  if (android_node->IsTextSelectable()) {
    position = position->AsTextPosition();
    ui::BrowserAccessibility* platform_ancestor =
        android_node->PlatformGetLowestPlatformAncestor();
    CHECK(platform_ancestor);
    // Move Chrome position up to the lowest leaf in Android and perform the
    // right adjustments for offset and affinity.
    while (position->GetAnchor()->id() != platform_ancestor->GetId()) {
      position = position->CreateParentPosition();
    }
    CHECK(position->IsTextPosition());
    return std::make_pair(
        static_cast<BrowserAccessibilityAndroid*>(platform_ancestor),
        position->text_offset());
  }

  position = position->AsTreePosition();

  // Since the parent of the target node may be ignored, find the target node in
  // in Android accessibility tree, then find its parent in Android and compute
  // the offset based on that.
  // TODO(crbug.com/498376490): The conversion below is lossy and should be
  // improved by including affinity when Selection API supports it.
  ui::AXNode* target_node = nullptr;
  bool at_end_of_anchor = false;
  int anchor_child_count = position->GetAnchor()->GetChildCount();
  if (position->child_index() == ui::AXNodePosition::BEFORE_TEXT ||
      anchor_child_count == 0) {
    // A tree position with BEFORE_TEXT child index points to before the anchor
    // node, hence the node itself is considered as target.
    // A non BEFORE_TEXT child offset for a leaf node points to after the anchor
    // point. Hence again the target is set to the anchor node, but keeping a
    // note to select after it.
    // TODO(crbug.com/443078007): Add test for both cases. The position is
    // inside the container and not before or after the container, hence moving
    // it before or after the `target_node` is not right.
    target_node = position->GetAnchor();
    at_end_of_anchor =
        (position->child_index() != ui::AXNodePosition::BEFORE_TEXT);
  } else if (position->child_index() < anchor_child_count) {
    target_node =
        position->GetAnchor()->GetChildAtIndex(position->child_index());
  } else {
    target_node =
        position->GetAnchor()->GetChildAtIndex(position->child_index() - 1);
    at_end_of_anchor = true;
  }
  CHECK(target_node);

  offset = target_node->GetUnignoredIndexInParent();
  if (at_end_of_anchor) {
    offset++;
  }

  BrowserAccessibilityAndroid* parent_node =
      static_cast<BrowserAccessibilityAndroid*>(
          GetFromAXNode(target_node->GetUnignoredParent()));
  // TODO(crbug.com/498376490): Find a test case that triggers this behavior.
  if (!parent_node) {
    return std::nullopt;
  }

  return std::make_pair(parent_node, offset);
}

ui::BrowserAccessibility::AXPosition
BrowserAccessibilityManagerAndroid::ConvertAndroidSelectionPositionToChrome(
    BrowserAccessibilityAndroid* node,
    int32_t offset) {
  // TODO(crbug.com/498376490): Once Selection API supports sending offset type
  // to Android, create the position based on the received offset type as we
  // don't need to assume the offset type based on the node type.
  if (node->IsTextSelectable()) {
    return node->CreatePositionForSelectionAt(offset);
  }

  // When node 'c' is a child of node 'p' in the Android accessibility tree,
  // their equivalent nodes in Chrome accessibility tree may not have the same
  // relation and 'p' may not be an immediate parent ancestor. Therefore we need
  // to find the child that Android is pointing to, and then create a position
  // based on its direct parent in the Chrome tree.
  const size_t child_count = node->PlatformChildCount();

  // Since `node` is not text selectable, it is expected that `offset` would be
  // a child index to point to "before a certain child", or equal to the number
  // of children to point to "after the last child". Hence if there is no
  // children, or `offset` is out of this range, it is invalid and ignored.
  // TODO(crbug.com/498376490): Update the below conversion when the new API is
  // available and offset type is sent.
  if (child_count == 0 || offset < 0 ||
      static_cast<size_t>(offset) > child_count) {
    return ui::AXNodePosition::CreateNullPosition();
  }

  bool at_end_of_anchor =
      (static_cast<size_t>(offset) == node->PlatformChildCount());
  if (at_end_of_anchor) {
    offset--;
  }

  // Find the target node.
  // Note: When the mapping from Android results in an ignored node, we default
  // to a downstream adjustment (moving to the next unignored sibling). While an
  // upstream adjustment would also be valid, we lack the affinity information
  // from Android to make a more precise choice.
  // TODO(crbug.com/498376490): Use affinity in all next cases to avoid data
  // loss.
  ui::BrowserAccessibility* target = node->PlatformGetChild(offset);
  CHECK(target);

  if (at_end_of_anchor) {
    return ui::AXNodePosition::CreateTreePositionAtEndOfAnchor(*target->node())
        ->CreateParentPosition();
  }

  return ui::AXNodePosition::CreateTreePositionAtStartOfAnchor(*target->node())
      ->CreateParentPosition();
}

// TODO(crbug.com/485227837): Remove experiment's methods
void BrowserAccessibilityManagerAndroid::
    OnAccessibilityEventsProcessedForExperiment() {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax) {
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kAccessibilityRequestScopedContentChangedEvents)) {
    wcax->ValidateA11yCacheForExperiment();
  }
}

}  // namespace content
