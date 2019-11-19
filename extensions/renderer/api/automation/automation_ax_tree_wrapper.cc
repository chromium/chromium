// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/api/automation/automation_internal_custom_bindings.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"

namespace extensions {

namespace {

// Convert from ax::mojom::Event to api::automation::EventType.
api::automation::EventType ToAutomationEvent(ax::mojom::Event event_type) {
  switch (event_type) {
    case ax::mojom::Event::kNone:
      return api::automation::EVENT_TYPE_NONE;
    case ax::mojom::Event::kActiveDescendantChanged:
      return api::automation::EVENT_TYPE_ACTIVEDESCENDANTCHANGED;
    case ax::mojom::Event::kAlert:
      return api::automation::EVENT_TYPE_ALERT;
    case ax::mojom::Event::kAriaAttributeChanged:
      return api::automation::EVENT_TYPE_ARIAATTRIBUTECHANGED;
    case ax::mojom::Event::kAutocorrectionOccured:
      return api::automation::EVENT_TYPE_AUTOCORRECTIONOCCURED;
    case ax::mojom::Event::kBlur:
      return api::automation::EVENT_TYPE_BLUR;
    case ax::mojom::Event::kCheckedStateChanged:
      return api::automation::EVENT_TYPE_CHECKEDSTATECHANGED;
    case ax::mojom::Event::kChildrenChanged:
      return api::automation::EVENT_TYPE_CHILDRENCHANGED;
    case ax::mojom::Event::kClicked:
      return api::automation::EVENT_TYPE_CLICKED;
    case ax::mojom::Event::kControlsChanged:
      return api::automation::EVENT_TYPE_CONTROLSCHANGED;
    case ax::mojom::Event::kDocumentSelectionChanged:
      return api::automation::EVENT_TYPE_DOCUMENTSELECTIONCHANGED;
    case ax::mojom::Event::kDocumentTitleChanged:
      return api::automation::EVENT_TYPE_DOCUMENTTITLECHANGED;
    case ax::mojom::Event::kEndOfTest:
      return api::automation::EVENT_TYPE_ENDOFTEST;
    case ax::mojom::Event::kExpandedChanged:
      return api::automation::EVENT_TYPE_EXPANDEDCHANGED;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
      return api::automation::EVENT_TYPE_NONE;
    case ax::mojom::Event::kHide:
      return api::automation::EVENT_TYPE_HIDE;
    case ax::mojom::Event::kHitTestResult:
      return api::automation::EVENT_TYPE_HITTESTRESULT;
    case ax::mojom::Event::kHover:
      return api::automation::EVENT_TYPE_HOVER;
    case ax::mojom::Event::kImageFrameUpdated:
      return api::automation::EVENT_TYPE_IMAGEFRAMEUPDATED;
    case ax::mojom::Event::kInvalidStatusChanged:
      return api::automation::EVENT_TYPE_INVALIDSTATUSCHANGED;
    case ax::mojom::Event::kLayoutComplete:
      return api::automation::EVENT_TYPE_LAYOUTCOMPLETE;
    case ax::mojom::Event::kLiveRegionCreated:
      return api::automation::EVENT_TYPE_LIVEREGIONCREATED;
    case ax::mojom::Event::kLiveRegionChanged:
      return api::automation::EVENT_TYPE_LIVEREGIONCHANGED;
    case ax::mojom::Event::kLoadComplete:
      return api::automation::EVENT_TYPE_LOADCOMPLETE;
    case ax::mojom::Event::kLoadStart:
      return api::automation::EVENT_TYPE_LOADSTART;
    case ax::mojom::Event::kLocationChanged:
      return api::automation::EVENT_TYPE_LOCATIONCHANGED;
    case ax::mojom::Event::kMediaStartedPlaying:
      return api::automation::EVENT_TYPE_MEDIASTARTEDPLAYING;
    case ax::mojom::Event::kMediaStoppedPlaying:
      return api::automation::EVENT_TYPE_MEDIASTOPPEDPLAYING;
    case ax::mojom::Event::kMenuEnd:
      return api::automation::EVENT_TYPE_MENUEND;
    case ax::mojom::Event::kMenuListItemSelected:
      return api::automation::EVENT_TYPE_MENULISTITEMSELECTED;
    case ax::mojom::Event::kMenuListValueChanged:
      return api::automation::EVENT_TYPE_MENULISTVALUECHANGED;
    case ax::mojom::Event::kMenuPopupEnd:
      return api::automation::EVENT_TYPE_MENUPOPUPEND;
    case ax::mojom::Event::kMenuPopupHide:
      return api::automation::EVENT_TYPE_MENUPOPUPHIDE;
    case ax::mojom::Event::kMenuPopupStart:
      return api::automation::EVENT_TYPE_MENUPOPUPSTART;
    case ax::mojom::Event::kMenuStart:
      return api::automation::EVENT_TYPE_MENUSTART;
    case ax::mojom::Event::kMouseCanceled:
      return api::automation::EVENT_TYPE_MOUSECANCELED;
    case ax::mojom::Event::kMouseDragged:
      return api::automation::EVENT_TYPE_MOUSEDRAGGED;
    case ax::mojom::Event::kMouseMoved:
      return api::automation::EVENT_TYPE_MOUSEMOVED;
    case ax::mojom::Event::kMousePressed:
      return api::automation::EVENT_TYPE_MOUSEPRESSED;
    case ax::mojom::Event::kMouseReleased:
      return api::automation::EVENT_TYPE_MOUSERELEASED;
    case ax::mojom::Event::kRowCollapsed:
      return api::automation::EVENT_TYPE_ROWCOLLAPSED;
    case ax::mojom::Event::kRowCountChanged:
      return api::automation::EVENT_TYPE_ROWCOUNTCHANGED;
    case ax::mojom::Event::kRowExpanded:
      return api::automation::EVENT_TYPE_ROWEXPANDED;
    case ax::mojom::Event::kScrollPositionChanged:
      return api::automation::EVENT_TYPE_SCROLLPOSITIONCHANGED;
    case ax::mojom::Event::kScrolledToAnchor:
      return api::automation::EVENT_TYPE_SCROLLEDTOANCHOR;
    case ax::mojom::Event::kSelectedChildrenChanged:
      return api::automation::EVENT_TYPE_SELECTEDCHILDRENCHANGED;
    case ax::mojom::Event::kSelection:
      return api::automation::EVENT_TYPE_SELECTION;
    case ax::mojom::Event::kSelectionAdd:
      return api::automation::EVENT_TYPE_SELECTIONADD;
    case ax::mojom::Event::kSelectionRemove:
      return api::automation::EVENT_TYPE_SELECTIONREMOVE;
    case ax::mojom::Event::kShow:
      return api::automation::EVENT_TYPE_SHOW;
    case ax::mojom::Event::kStateChanged:
      return api::automation::EVENT_TYPE_NONE;
    case ax::mojom::Event::kTextChanged:
      return api::automation::EVENT_TYPE_TEXTCHANGED;
    case ax::mojom::Event::kTextSelectionChanged:
      return api::automation::EVENT_TYPE_TEXTSELECTIONCHANGED;
    case ax::mojom::Event::kTooltipClosed:
    case ax::mojom::Event::kTooltipOpened:
      return api::automation::EVENT_TYPE_NONE;
    case ax::mojom::Event::kWindowActivated:
      return api::automation::EVENT_TYPE_WINDOWACTIVATED;
    case ax::mojom::Event::kWindowDeactivated:
      return api::automation::EVENT_TYPE_WINDOWDEACTIVATED;
    case ax::mojom::Event::kWindowVisibilityChanged:
      return api::automation::EVENT_TYPE_WINDOWVISIBILITYCHANGED;
    case ax::mojom::Event::kTreeChanged:
      return api::automation::EVENT_TYPE_TREECHANGED;
    case ax::mojom::Event::kValueChanged:
      return api::automation::EVENT_TYPE_VALUECHANGED;
  }

  NOTREACHED();
  return api::automation::EVENT_TYPE_NONE;
}

// Convert from ui::AXEventGenerator::Event to api::automation::EventType.
api::automation::EventType ToAutomationEvent(
    ui::AXEventGenerator::Event event_type) {
  switch (event_type) {
    case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      return api::automation::EVENT_TYPE_ACTIVEDESCENDANTCHANGED;
    case ui::AXEventGenerator::Event::ALERT:
      return api::automation::EVENT_TYPE_ALERT;
    case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      return api::automation::EVENT_TYPE_CHECKEDSTATECHANGED;
    case ui::AXEventGenerator::Event::CHILDREN_CHANGED:
      return api::automation::EVENT_TYPE_CHILDRENCHANGED;
    case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED:
      return api::automation::EVENT_TYPE_DOCUMENTSELECTIONCHANGED;
    case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
      return api::automation::EVENT_TYPE_DOCUMENTTITLECHANGED;
    case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      return api::automation::EVENT_TYPE_INVALIDSTATUSCHANGED;
    case ui::AXEventGenerator::Event::LIVE_REGION_CHANGED:
      return api::automation::EVENT_TYPE_LIVEREGIONCHANGED;
    case ui::AXEventGenerator::Event::LIVE_REGION_CREATED:
      return api::automation::EVENT_TYPE_LIVEREGIONCREATED;
    case ui::AXEventGenerator::Event::LOAD_COMPLETE:
      return api::automation::EVENT_TYPE_LOADCOMPLETE;
    case ui::AXEventGenerator::Event::LOAD_START:
      return api::automation::EVENT_TYPE_LOADSTART;
    case ui::AXEventGenerator::Event::MENU_ITEM_SELECTED:
      return api::automation::EVENT_TYPE_MENULISTITEMSELECTED;
    case ui::AXEventGenerator::Event::RELATED_NODE_CHANGED:
      return api::automation::EVENT_TYPE_ARIAATTRIBUTECHANGED;
    case ui::AXEventGenerator::Event::ROW_COUNT_CHANGED:
      return api::automation::EVENT_TYPE_ROWCOUNTCHANGED;
    case ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
    case ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
      return api::automation::EVENT_TYPE_SCROLLPOSITIONCHANGED;
    case ui::AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      return api::automation::EVENT_TYPE_SELECTEDCHILDRENCHANGED;
    case ui::AXEventGenerator::Event::VALUE_CHANGED:
      return api::automation::EVENT_TYPE_VALUECHANGED;

    // Map these into generic attribute changes (not necessarily aria related,
    // but mapping for backward compat).
    case ui::AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
    case ui::AXEventGenerator::Event::COLLAPSED:
    case ui::AXEventGenerator::Event::EXPANDED:
    case ui::AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
    case ui::AXEventGenerator::Event::NAME_CHANGED:
    case ui::AXEventGenerator::Event::ROLE_CHANGED:
    case ui::AXEventGenerator::Event::SELECTED_CHANGED:
    case ui::AXEventGenerator::Event::STATE_CHANGED:
      return api::automation::EVENT_TYPE_ARIAATTRIBUTECHANGED;

    case ui::AXEventGenerator::Event::ACCESS_KEY_CHANGED:
    case ui::AXEventGenerator::Event::ATOMIC_CHANGED:
    case ui::AXEventGenerator::Event::BUSY_CHANGED:
    case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
    case ui::AXEventGenerator::Event::CLASS_NAME_CHANGED:
    case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
    case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
    case ui::AXEventGenerator::Event::DROPEFFECT_CHANGED:
    case ui::AXEventGenerator::Event::ENABLED_CHANGED:
    case ui::AXEventGenerator::Event::FOCUS_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_FROM_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_TO_CHANGED:
    case ui::AXEventGenerator::Event::GRABBED_CHANGED:
    case ui::AXEventGenerator::Event::HASPOPUP_CHANGED:
    case ui::AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
    case ui::AXEventGenerator::Event::IGNORED_CHANGED:
    case ui::AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
    case ui::AXEventGenerator::Event::LABELED_BY_CHANGED:
    case ui::AXEventGenerator::Event::LANGUAGE_CHANGED:
    case ui::AXEventGenerator::Event::LAYOUT_INVALIDATED:
    case ui::AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_STATUS_CHANGED:
    case ui::AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
    case ui::AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
    case ui::AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::PLACEHOLDER_CHANGED:
    case ui::AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
    case ui::AXEventGenerator::Event::READONLY_CHANGED:
    case ui::AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
    case ui::AXEventGenerator::Event::SET_SIZE_CHANGED:
    case ui::AXEventGenerator::Event::SORT_CHANGED:
    case ui::AXEventGenerator::Event::SUBTREE_CREATED:
    case ui::AXEventGenerator::Event::VALUE_MAX_CHANGED:
    case ui::AXEventGenerator::Event::VALUE_MIN_CHANGED:
    case ui::AXEventGenerator::Event::VALUE_STEP_CHANGED:
      return api::automation::EVENT_TYPE_NONE;
  }

  NOTREACHED();
  return api::automation::EVENT_TYPE_NONE;
}

}  // namespace

AutomationAXTreeWrapper::AutomationAXTreeWrapper(
    ui::AXTreeID tree_id,
    AutomationInternalCustomBindings* owner)
    : tree_id_(tree_id), owner_(owner), event_generator_(&tree_) {
  tree_.AddObserver(this);
}

AutomationAXTreeWrapper::~AutomationAXTreeWrapper() {
  // Stop observing so we don't get a callback for every node being deleted.
  event_generator_.SetTree(nullptr);
  tree_.RemoveObserver(this);
}

// static
AutomationAXTreeWrapper* AutomationAXTreeWrapper::GetParentOfTreeId(
    ui::AXTreeID tree_id) {
  std::map<ui::AXTreeID, AutomationAXTreeWrapper*>& child_tree_id_reverse_map =
      GetChildTreeIDReverseMap();
  const auto& iter = child_tree_id_reverse_map.find(tree_id);
  if (iter != child_tree_id_reverse_map.end())
    return iter->second;

  return nullptr;
}

bool AutomationAXTreeWrapper::OnAccessibilityEvents(
    const ExtensionMsg_AccessibilityEventBundleParams& event_bundle,
    bool is_active_profile) {
  std::map<ui::AXTreeID, AutomationAXTreeWrapper*>& child_tree_id_reverse_map =
      GetChildTreeIDReverseMap();
  const auto& child_tree_ids = tree_.GetAllChildTreeIds();

  // Invalidate any reverse child tree id mappings. Note that it is possible
  // there are no entries in this map for a given child tree to |this|, if this
  // is the first event from |this| tree or if |this| was destroyed and (and
  // then reset).
  base::EraseIf(child_tree_id_reverse_map, [child_tree_ids](auto& pair) {
    return child_tree_ids.count(pair.first);
  });

  // Unserialize all incoming data.
  for (const auto& update : event_bundle.updates) {
    deleted_node_ids_.clear();
    did_send_tree_change_during_unserialization_ = false;

    if (!tree_.Unserialize(update)) {
      event_generator_.ClearEvents();
      return false;
    }

    if (is_active_profile) {
      owner_->SendNodesRemovedEvent(&tree_, deleted_node_ids_);

      if (update.nodes.size() && did_send_tree_change_during_unserialization_) {
        ui::AXNode* target = tree_.GetFromId(update.nodes[0].id);
        if (target) {
          owner_->SendTreeChangeEvent(
              api::automation::TREE_CHANGE_TYPE_SUBTREEUPDATEEND, &tree_,
              target);
        }
      }
    }
  }

  // Refresh child tree id  mappings.
  for (const ui::AXTreeID& tree_id : tree_.GetAllChildTreeIds()) {
    DCHECK(!base::Contains(child_tree_id_reverse_map, tree_id));
    child_tree_id_reverse_map.insert(std::make_pair(tree_id, this));
  }

  // Exit early if this isn't the active profile.
  if (!is_active_profile) {
    event_generator_.ClearEvents();
    return true;
  }

  // Perform language detection first thing if we see a load complete event.
  // We have to run *before* we send the load complete event to javascript
  // otherwise code which runs immediately on load complete will not be able
  // to see the results of language detection.
  //
  // Currently language detection only runs once for initial load complete, any
  // content loaded after this will not have language detection performed for
  // it.
  for (const auto& targeted_event : event_generator_) {
    if (targeted_event.event_params.event ==
        ui::AXEventGenerator::Event::LOAD_COMPLETE) {
      tree_.language_detection_manager->DetectLanguageForSubtree(tree_.root());
      tree_.language_detection_manager->LabelLanguageForSubtree(tree_.root());
      break;
    }
  }

  // Send all blur and focus events first.
  owner_->MaybeSendFocusAndBlur(this, event_bundle);

  // Send auto-generated AXEventGenerator events.
  for (const auto& targeted_event : event_generator_) {
    api::automation::EventType event_type =
        ToAutomationEvent(targeted_event.event_params.event);

    if (IsEventTypeHandledByAXEventGenerator(event_type)) {
      ui::AXEvent generated_event;
      generated_event.id = targeted_event.node->id();
      generated_event.event_from = targeted_event.event_params.event_from;
      owner_->SendAutomationEvent(event_bundle.tree_id,
                                  event_bundle.mouse_location, generated_event,
                                  event_type);
    }
  }
  event_generator_.ClearEvents();

  for (const auto& event : event_bundle.events) {
    if (event.event_type == ax::mojom::Event::kFocus ||
        event.event_type == ax::mojom::Event::kBlur)
      continue;

    api::automation::EventType automation_event_type =
        ToAutomationEvent(event.event_type);

    // Send some events directly from the event message, if they're not
    // handled by AXEventGenerator yet.
    if (!IsEventTypeHandledByAXEventGenerator(automation_event_type)) {
      owner_->SendAutomationEvent(event_bundle.tree_id,
                                  event_bundle.mouse_location, event,
                                  automation_event_type);
    }
  }

  return true;
}

bool AutomationAXTreeWrapper::IsDesktopTree() const {
  return tree_.root() ? tree_.root()->data().role == ax::mojom::Role::kDesktop
                      : false;
}

bool AutomationAXTreeWrapper::IsInFocusChain(int32_t node_id) {
  if (tree()->data().focus_id != node_id)
    return false;

  if (IsDesktopTree())
    return true;

  AutomationAXTreeWrapper* child_of_ancestor = this;
  AutomationAXTreeWrapper* ancestor = nullptr;
  while ((ancestor =
              GetParentOfTreeId(child_of_ancestor->tree()->data().tree_id))) {
    int32_t focus_id = ancestor->tree()->data().focus_id;
    ui::AXNode* focus = ancestor->tree()->GetFromId(focus_id);
    if (!focus)
      return false;

    const ui::AXTreeID& child_tree_id =
        child_of_ancestor->tree()->data().tree_id;

    // Either the focused node points to the child tree, or the ancestor tree
    // points to the child tree via the focused tree id. Exit early if both are
    // not true.
    if (ui::AXTreeID::FromString(focus->GetStringAttribute(
            ax::mojom::StringAttribute::kChildTreeId)) != child_tree_id &&
        ancestor->tree()->data().focused_tree_id != child_tree_id)
      return false;

    if (ancestor->IsDesktopTree())
      return true;

    child_of_ancestor = ancestor;
  }

  // The only way we end up here is if the tree is detached from any desktop.
  // This can occur in tabs-only mode.
  return true;
}

ui::AXTree::Selection AutomationAXTreeWrapper::GetUnignoredSelection() {
  // As there is no Tree Manager, this is necessary for AXPositions to work.
  ui::AXNodePosition::SetTree(tree());
  ui::AXTree::Selection unignored_selection = tree()->GetUnignoredSelection();
  ui::AXNodePosition::SetTree(nullptr);
  return unignored_selection;
}

ui::AXNode* AutomationAXTreeWrapper::GetUnignoredNodeFromId(int32_t id) {
  ui::AXNode* node = tree_.GetFromId(id);
  return (node && !node->IsIgnored()) ? node : nullptr;
}

// static
std::map<ui::AXTreeID, AutomationAXTreeWrapper*>&
AutomationAXTreeWrapper::GetChildTreeIDReverseMap() {
  static base::NoDestructor<std::map<ui::AXTreeID, AutomationAXTreeWrapper*>>
      child_tree_id_reverse_map;
  return *child_tree_id_reverse_map;
}

void AutomationAXTreeWrapper::OnNodeDataChanged(
    ui::AXTree* tree,
    const ui::AXNodeData& old_node_data,
    const ui::AXNodeData& new_node_data) {
  if (old_node_data.GetStringAttribute(ax::mojom::StringAttribute::kName) !=
      new_node_data.GetStringAttribute(ax::mojom::StringAttribute::kName))
    text_changed_node_ids_.push_back(new_node_data.id);
}

void AutomationAXTreeWrapper::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                  ui::AXNode* node) {
  did_send_tree_change_during_unserialization_ |= owner_->SendTreeChangeEvent(
      api::automation::TREE_CHANGE_TYPE_NODEREMOVED, tree, node);
  deleted_node_ids_.push_back(node->id());
}

void AutomationAXTreeWrapper::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  DCHECK_EQ(&tree_, tree);
  for (const auto change : changes) {
    ui::AXNode* node = change.node;
    switch (change.type) {
      case NODE_CREATED:
        did_send_tree_change_during_unserialization_ |=
            owner_->SendTreeChangeEvent(
                api::automation::TREE_CHANGE_TYPE_NODECREATED, tree, node);
        break;
      case SUBTREE_CREATED:
        did_send_tree_change_during_unserialization_ |=
            owner_->SendTreeChangeEvent(
                api::automation::TREE_CHANGE_TYPE_SUBTREECREATED, tree, node);
        break;
      case NODE_CHANGED:
        did_send_tree_change_during_unserialization_ |=
            owner_->SendTreeChangeEvent(
                api::automation::TREE_CHANGE_TYPE_NODECHANGED, tree, node);
        break;
      // Unhandled.
      case NODE_REPARENTED:
      case SUBTREE_REPARENTED:
        break;
    }
  }

  for (int id : text_changed_node_ids_) {
    did_send_tree_change_during_unserialization_ |= owner_->SendTreeChangeEvent(
        api::automation::TREE_CHANGE_TYPE_TEXTCHANGED, tree,
        tree->GetFromId(id));
  }
  text_changed_node_ids_.clear();
}

bool AutomationAXTreeWrapper::IsEventTypeHandledByAXEventGenerator(
    api::automation::EventType event_type) const {
  switch (event_type) {
    // Generated by AXEventGenerator.
    case api::automation::EVENT_TYPE_ACTIVEDESCENDANTCHANGED:
    case api::automation::EVENT_TYPE_ARIAATTRIBUTECHANGED:
    case api::automation::EVENT_TYPE_CHECKEDSTATECHANGED:
    case api::automation::EVENT_TYPE_CHILDRENCHANGED:
    case api::automation::EVENT_TYPE_DOCUMENTSELECTIONCHANGED:
    case api::automation::EVENT_TYPE_DOCUMENTTITLECHANGED:
    case api::automation::EVENT_TYPE_EXPANDEDCHANGED:
    case api::automation::EVENT_TYPE_INVALIDSTATUSCHANGED:
    case api::automation::EVENT_TYPE_LIVEREGIONCHANGED:
    case api::automation::EVENT_TYPE_LIVEREGIONCREATED:
    case api::automation::EVENT_TYPE_LOADCOMPLETE:
    case api::automation::EVENT_TYPE_LOADSTART:
    case api::automation::EVENT_TYPE_ROWCOLLAPSED:
    case api::automation::EVENT_TYPE_ROWCOUNTCHANGED:
    case api::automation::EVENT_TYPE_ROWEXPANDED:
    case api::automation::EVENT_TYPE_SCROLLPOSITIONCHANGED:
    case api::automation::EVENT_TYPE_SELECTEDCHILDRENCHANGED:
      return true;

    // Not generated by AXEventGenerator and possible candidates
    // for removal from the automation API entirely.
    case api::automation::EVENT_TYPE_HIDE:
    case api::automation::EVENT_TYPE_LAYOUTCOMPLETE:
    case api::automation::EVENT_TYPE_MENULISTVALUECHANGED:
    case api::automation::EVENT_TYPE_MENUPOPUPEND:
    case api::automation::EVENT_TYPE_MENUPOPUPHIDE:
    case api::automation::EVENT_TYPE_MENUPOPUPSTART:
    case api::automation::EVENT_TYPE_SELECTIONADD:
    case api::automation::EVENT_TYPE_SELECTIONREMOVE:
    case api::automation::EVENT_TYPE_SHOW:
    case api::automation::EVENT_TYPE_STATECHANGED:
    case api::automation::EVENT_TYPE_TREECHANGED:
      return false;

    // These events will never be generated by AXEventGenerator.
    // These are all events that can't be inferred from a tree change.
    case api::automation::EVENT_TYPE_NONE:
    case api::automation::EVENT_TYPE_AUTOCORRECTIONOCCURED:
    case api::automation::EVENT_TYPE_CLICKED:
    case api::automation::EVENT_TYPE_ENDOFTEST:
    case api::automation::EVENT_TYPE_FOCUSCONTEXT:
    case api::automation::EVENT_TYPE_HITTESTRESULT:
    case api::automation::EVENT_TYPE_HOVER:
    case api::automation::EVENT_TYPE_MEDIASTARTEDPLAYING:
    case api::automation::EVENT_TYPE_MEDIASTOPPEDPLAYING:
    case api::automation::EVENT_TYPE_MOUSECANCELED:
    case api::automation::EVENT_TYPE_MOUSEDRAGGED:
    case api::automation::EVENT_TYPE_MOUSEMOVED:
    case api::automation::EVENT_TYPE_MOUSEPRESSED:
    case api::automation::EVENT_TYPE_MOUSERELEASED:
    case api::automation::EVENT_TYPE_SCROLLEDTOANCHOR:
    case api::automation::EVENT_TYPE_TOOLTIPCLOSED:
    case api::automation::EVENT_TYPE_TOOLTIPOPENED:
    case api::automation::EVENT_TYPE_WINDOWACTIVATED:
    case api::automation::EVENT_TYPE_WINDOWDEACTIVATED:
    case api::automation::EVENT_TYPE_WINDOWVISIBILITYCHANGED:
      return false;

    // These events might need to be migrated to AXEventGenerator.
    case api::automation::EVENT_TYPE_ALERT:
    case api::automation::EVENT_TYPE_BLUR:
    case api::automation::EVENT_TYPE_CONTROLSCHANGED:
    case api::automation::EVENT_TYPE_FOCUS:
    case api::automation::EVENT_TYPE_IMAGEFRAMEUPDATED:
    case api::automation::EVENT_TYPE_LOCATIONCHANGED:
    case api::automation::EVENT_TYPE_MENUEND:
    case api::automation::EVENT_TYPE_MENULISTITEMSELECTED:
    case api::automation::EVENT_TYPE_MENUSTART:
    case api::automation::EVENT_TYPE_SELECTION:
    case api::automation::EVENT_TYPE_TEXTCHANGED:
    case api::automation::EVENT_TYPE_TEXTSELECTIONCHANGED:
    case api::automation::EVENT_TYPE_VALUECHANGED:
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace extensions
