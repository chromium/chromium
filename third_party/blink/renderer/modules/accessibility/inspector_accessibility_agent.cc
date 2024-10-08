// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"

#include <memory>

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_sheet.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/inspector_type_builder_helper.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"

namespace blink {

using protocol::Maybe;
using protocol::Accessibility::AXNode;
using protocol::Accessibility::AXNodeId;
using protocol::Accessibility::AXProperty;
using protocol::Accessibility::AXPropertyName;
using protocol::Accessibility::AXRelatedNode;
using protocol::Accessibility::AXValue;
using protocol::Accessibility::AXValueSource;
using protocol::Accessibility::AXValueType;
namespace AXPropertyNameEnum = protocol::Accessibility::AXPropertyNameEnum;

namespace {

static const AXID kIDForInspectedNodeWithNoAXNode = 0;
// Send node updates to the frontend not more often than once within the
// `kNodeSyncThrottlePeriod` time frame.
static const base::TimeDelta kNodeSyncThrottlePeriod = base::Milliseconds(250);
// Interval at which we check if there is a need to schedule visual updates.
static const base::TimeDelta kVisualUpdateCheckInterval =
    kNodeSyncThrottlePeriod + base::Milliseconds(10);

void AddHasPopupProperty(ax::mojom::blink::HasPopup has_popup,
                         protocol::Array<AXProperty>& properties) {
  switch (has_popup) {
    case ax::mojom::blink::HasPopup::kFalse:
      break;
    case ax::mojom::blink::HasPopup::kTrue:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("true", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::blink::HasPopup::kMenu:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("menu", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::blink::HasPopup::kListbox:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("listbox", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::blink::HasPopup::kTree:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("tree", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::blink::HasPopup::kGrid:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("grid", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::blink::HasPopup::kDialog:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("dialog", AXValueTypeEnum::Token)));
      break;
  }
}

void FillLiveRegionProperties(AXObject& ax_object,
                              const ui::AXNodeData& node_data,
                              protocol::Array<AXProperty>& properties) {
  if (!node_data.IsActiveLiveRegionRoot())
    return;

  const String& live =
      node_data
          .GetStringAttribute(
              ax::mojom::blink::StringAttribute::kContainerLiveStatus)
          .c_str();
  properties.emplace_back(CreateProperty(
      AXPropertyNameEnum::Live, CreateValue(live, AXValueTypeEnum::Token)));

  const bool atomic = node_data.GetBoolAttribute(
      ax::mojom::blink::BoolAttribute::kContainerLiveAtomic);
  properties.emplace_back(
      CreateProperty(AXPropertyNameEnum::Atomic, CreateBooleanValue(atomic)));

  const String& relevant =
      node_data
          .GetStringAttribute(
              ax::mojom::blink::StringAttribute::kContainerLiveRelevant)
          .c_str();
  properties.emplace_back(
      CreateProperty(AXPropertyNameEnum::Relevant,
                     CreateValue(relevant, AXValueTypeEnum::TokenList)));

  if (!ax_object.IsLiveRegionRoot()) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Root,
        CreateRelatedNodeListValue(*(ax_object.LiveRegionRoot()))));
  }
}

void FillGlobalStates(AXObject& ax_object,
                      const ui::AXNodeData& node_data,
                      protocol::Array<AXProperty>& properties) {
  if (node_data.GetRestriction() == ax::mojom::blink::Restriction::kDisabled) {
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Disabled, CreateBooleanValue(true)));
  }

  if (const AXObject* hidden_root = ax_object.AriaHiddenRoot()) {
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Hidden, CreateBooleanValue(true)));
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::HiddenRoot,
                       CreateRelatedNodeListValue(*hidden_root)));
  }

  ax::mojom::blink::InvalidState invalid_state = node_data.GetInvalidState();
  switch (invalid_state) {
    case ax::mojom::blink::InvalidState::kNone:
      break;
    case ax::mojom::blink::InvalidState::kFalse:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::Invalid,
                         CreateValue("false", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::blink::InvalidState::kTrue:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::Invalid,
                         CreateValue("true", AXValueTypeEnum::Token)));
      break;
    default:
      // TODO(aboxhall): expose invalid: <nothing> and source: aria-invalid as
      // invalid value
      properties.emplace_back(CreateProperty(
          AXPropertyNameEnum::Invalid,
          CreateValue(
              node_data
                  .GetStringAttribute(ax::mojom::blink::StringAttribute::
                                          kAriaInvalidValueDeprecated)
                  .c_str(),
              AXValueTypeEnum::String)));
      break;
  }

  if (node_data.HasState(ax::mojom::blink::State::kFocusable)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Focusable,
        CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
  }
  if (ax_object.IsFocused()) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Focused,
        CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
  }

  if (node_data.HasState(ax::mojom::blink::State::kEditable)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Editable,
        CreateValue(node_data.HasState(ax::mojom::blink::State::kRichlyEditable)
                        ? "richtext"
                        : "plaintext",
                    AXValueTypeEnum::Token)));
  }
  if (node_data.HasAction(ax::mojom::blink::Action::kSetValue)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Settable,
        CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
  }
}

bool RoleAllowsMultiselectable(ax::mojom::Role role) {
  return role == ax::mojom::Role::kGrid || role == ax::mojom::Role::kListBox ||
         role == ax::mojom::Role::kTabList ||
         role == ax::mojom::Role::kTreeGrid || role == ax::mojom::Role::kTree;
}

bool RoleAllowsReadonly(ax::mojom::Role role) {
  return role == ax::mojom::Role::kGrid || role == ax::mojom::Role::kGridCell ||
         role == ax::mojom::Role::kTextField ||
         role == ax::mojom::Role::kColumnHeader ||
         role == ax::mojom::Role::kRowHeader ||
         role == ax::mojom::Role::kTreeGrid;
}

bool RoleAllowsRequired(ax::mojom::Role role) {
  return role == ax::mojom::Role::kComboBoxGrouping ||
         role == ax::mojom::Role::kComboBoxMenuButton ||
         role == ax::mojom::Role::kGridCell ||
         role == ax::mojom::Role::kListBox ||
         role == ax::mojom::Role::kRadioGroup ||
         role == ax::mojom::Role::kSpinButton ||
         role == ax::mojom::Role::kTextField ||
         role == ax::mojom::Role::kTextFieldWithComboBox ||
         role == ax::mojom::Role::kTree ||
         role == ax::mojom::Role::kColumnHeader ||
         role == ax::mojom::Role::kRowHeader ||
         role == ax::mojom::Role::kTreeGrid;
}

void FillWidgetProperties(AXObject& ax_object,
                          const ui::AXNodeData& node_data,
                          protocol::Array<AXProperty>& properties) {
  ax::mojom::blink::Role role = node_data.role;
  const String& autocomplete =
      node_data
          .GetStringAttribute(ax::mojom::blink::StringAttribute::kAutoComplete)
          .c_str();
  if (!autocomplete.empty())
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Autocomplete,
                       CreateValue(autocomplete, AXValueTypeEnum::Token)));

  AddHasPopupProperty(node_data.GetHasPopup(), properties);

  const int hierarchical_level = node_data.GetIntAttribute(
      ax::mojom::blink::IntAttribute::kHierarchicalLevel);
  if (hierarchical_level > 0) {
    properties.emplace_back(CreateProperty(AXPropertyNameEnum::Level,
                                           CreateValue(hierarchical_level)));
  }

  if (RoleAllowsMultiselectable(role)) {
    const bool multiselectable =
        node_data.HasState(ax::mojom::blink::State::kMultiselectable);
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Multiselectable,
                       CreateBooleanValue(multiselectable)));
  }

  if (node_data.HasState(ax::mojom::blink::State::kVertical)) {
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Orientation,
                       CreateValue("vertical", AXValueTypeEnum::Token)));
  } else if (node_data.HasState(ax::mojom::blink::State::kHorizontal)) {
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Orientation,
                       CreateValue("horizontal", AXValueTypeEnum::Token)));
  }

  if (role == ax::mojom::Role::kTextField) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Multiline,
        CreateBooleanValue(
            node_data.HasState(ax::mojom::blink::State::kMultiline))));
  }

  if (RoleAllowsReadonly(role)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Readonly,
        CreateBooleanValue(node_data.GetRestriction() ==
                           ax::mojom::blink::Restriction::kReadOnly)));
  }

  if (RoleAllowsRequired(role)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Required,
        CreateBooleanValue(
            node_data.HasState(ax::mojom::blink::State::kRequired))));
  }

  if (ax_object.IsRangeValueSupported()) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Valuemin,
        CreateValue(node_data.GetFloatAttribute(
            ax::mojom::blink::FloatAttribute::kMinValueForRange))));
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Valuemax,
        CreateValue(node_data.GetFloatAttribute(
            ax::mojom::blink::FloatAttribute::kMaxValueForRange))));
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Valuetext,
        CreateValue(
            node_data
                .GetStringAttribute(ax::mojom::blink::StringAttribute::kValue)
                .c_str())));
  }
}

void FillWidgetStates(AXObject& ax_object,
                      const ui::AXNodeData& node_data,
                      protocol::Array<AXProperty>& properties) {
  ax::mojom::blink::Role role = node_data.role;
  const char* checked_prop_val = nullptr;
  switch (node_data.GetCheckedState()) {
    case ax::mojom::CheckedState::kTrue:
      checked_prop_val = "true";
      break;
    case ax::mojom::CheckedState::kMixed:
      checked_prop_val = "mixed";
      break;
    case ax::mojom::CheckedState::kFalse:
      checked_prop_val = "false";
      break;
    case ax::mojom::CheckedState::kNone:
      break;
  }
  if (checked_prop_val) {
    auto* const checked_prop_name = role == ax::mojom::Role::kToggleButton
                                        ? AXPropertyNameEnum::Pressed
                                        : AXPropertyNameEnum::Checked;
    properties.emplace_back(CreateProperty(
        checked_prop_name,
        CreateValue(checked_prop_val, AXValueTypeEnum::Tristate)));
  }

  if (node_data.HasState(ax::mojom::blink::State::kCollapsed)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Expanded,
        CreateBooleanValue(false, AXValueTypeEnum::BooleanOrUndefined)));
  } else if (node_data.HasState(ax::mojom::blink::State::kExpanded)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Expanded,
        CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
  }

  if (node_data.HasBoolAttribute(ax::mojom::blink::BoolAttribute::kSelected)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Selected,
        CreateBooleanValue(node_data.GetBoolAttribute(
                               ax::mojom::blink::BoolAttribute::kSelected),
                           AXValueTypeEnum::BooleanOrUndefined)));
  }

  if (node_data.HasBoolAttribute(ax::mojom::blink::BoolAttribute::kModal)) {
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Modal,
                       CreateBooleanValue(node_data.GetBoolAttribute(
                           ax::mojom::blink::BoolAttribute::kModal))));
  }
}

// TODO(crbug/41469336): also show related elements from ElementInternals
void AccessibilityChildrenFromAttribute(const AXObject& ax_object,
                                        const QualifiedName& attribute,
                                        AXObject::AXObjectVector& children) {
  if (!ax_object.GetElement()) {
    return;
  }
  HeapVector<Member<Element>>* elements =
      ax_object.GetElement()->GetAttrAssociatedElements(
          attribute,
          /*resolve_reference_target=*/true);
  if (!elements) {
    return;
  }
  AXObjectCacheImpl& cache = ax_object.AXObjectCache();
  for (const auto& element : *elements) {
    if (AXObject* child = cache.Get(element)) {
      // Only aria-labelledby and aria-describedby can target hidden elements.
      if (!child) {
        continue;
      }
      if (child->IsIgnored() && attribute != html_names::kAriaLabelledbyAttr &&
          attribute != html_names::kAriaLabeledbyAttr &&
          attribute != html_names::kAriaDescribedbyAttr) {
        continue;
      }
      children.push_back(child);
    }
  }
}

void AriaDescribedbyElements(AXObject& ax_object,
                             AXObject::AXObjectVector& describedby) {
  AccessibilityChildrenFromAttribute(
      ax_object, html_names::kAriaDescribedbyAttr, describedby);
}

void AriaOwnsElements(AXObject& ax_object, AXObject::AXObjectVector& owns) {
  AccessibilityChildrenFromAttribute(ax_object, html_names::kAriaOwnsAttr,
                                     owns);
}

std::unique_ptr<AXProperty> CreateRelatedNodeListProperty(
    const String& key,
    AXRelatedObjectVector& nodes) {
  std::unique_ptr<AXValue> node_list_value =
      CreateRelatedNodeListValue(nodes, AXValueTypeEnum::NodeList);
  return CreateProperty(key, std::move(node_list_value));
}

std::unique_ptr<AXProperty> CreateRelatedNodeListProperty(
    const String& key,
    AXObject::AXObjectVector& nodes,
    const QualifiedName& attr,
    AXObject& ax_object) {
  std::unique_ptr<AXValue> node_list_value = CreateRelatedNodeListValue(nodes);
  const AtomicString& attr_value = ax_object.AriaAttribute(attr);
  node_list_value->setValue(protocol::StringValue::create(attr_value));
  return CreateProperty(key, std::move(node_list_value));
}

void FillRelationships(AXObject& ax_object,
                       protocol::Array<AXProperty>& properties) {
  AXObject::AXObjectVector results;
  AriaDescribedbyElements(ax_object, results);
  if (!results.empty()) {
    properties.emplace_back(CreateRelatedNodeListProperty(
        AXPropertyNameEnum::Describedby, results,
        html_names::kAriaDescribedbyAttr, ax_object));
  }
  results.clear();

  AriaOwnsElements(ax_object, results);
  if (!results.empty()) {
    properties.emplace_back(
        CreateRelatedNodeListProperty(AXPropertyNameEnum::Owns, results,
                                      html_names::kAriaOwnsAttr, ax_object));
  }
  results.clear();
}

void GetObjectsFromAXIDs(const AXObjectCacheImpl& cache,
                         const std::vector<int32_t>& ax_ids,
                         AXObject::AXObjectVector* ax_objects) {
  for (const auto& ax_id : ax_ids) {
    AXObject* ax_object = cache.ObjectFromAXID(ax_id);
    if (!ax_object)
      continue;
    ax_objects->push_back(ax_object);
  }
}

void FillSparseAttributes(AXObject& ax_object,
                          const ui::AXNodeData& node_data,
                          protocol::Array<AXProperty>& properties) {
  if (node_data.HasBoolAttribute(ax::mojom::blink::BoolAttribute::kBusy)) {
    const auto is_busy =
        node_data.GetBoolAttribute(ax::mojom::blink::BoolAttribute::kBusy);
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Busy,
                       CreateValue(is_busy, AXValueTypeEnum::Boolean)));
  }

  if (node_data.HasStringAttribute(ax::mojom::blink::StringAttribute::kUrl)) {
    const auto url =
        node_data.GetStringAttribute(ax::mojom::blink::StringAttribute::kUrl);
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Url,
        CreateValue(WTF::String(url.c_str()), AXValueTypeEnum::String)));
  }

  if (node_data.HasStringAttribute(
          ax::mojom::blink::StringAttribute::kKeyShortcuts)) {
    const auto key_shortcuts = node_data.GetStringAttribute(
        ax::mojom::blink::StringAttribute::kKeyShortcuts);
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Keyshortcuts,
                       CreateValue(WTF::String(key_shortcuts.c_str()),
                                   AXValueTypeEnum::String)));
  }

  if (node_data.HasStringAttribute(
          ax::mojom::blink::StringAttribute::kRoleDescription)) {
    const auto role_description = node_data.GetStringAttribute(
        ax::mojom::blink::StringAttribute::kRoleDescription);
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Roledescription,
                       CreateValue(WTF::String(role_description.c_str()),
                                   AXValueTypeEnum::String)));
  }

  if (node_data.HasIntAttribute(
          ax::mojom::blink::IntAttribute::kActivedescendantId)) {
    AXObject* target =
        ax_object.AXObjectCache().ObjectFromAXID(node_data.GetIntAttribute(
            ax::mojom::blink::IntAttribute::kActivedescendantId));
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Activedescendant,
                       CreateRelatedNodeListValue(*target)));
  }

  if (node_data.HasIntListAttribute(
          ax::mojom::blink::IntListAttribute::kErrormessageIds)) {
    const auto ax_ids = node_data.GetIntListAttribute(
        ax::mojom::blink::IntListAttribute::kErrormessageIds);
    AXObject::AXObjectVector ax_objects;
    GetObjectsFromAXIDs(ax_object.AXObjectCache(), ax_ids, &ax_objects);
    properties.emplace_back(CreateRelatedNodeListProperty(
        AXPropertyNameEnum::Errormessage, ax_objects,
        html_names::kAriaErrormessageAttr, ax_object));
  }

  if (node_data.HasIntListAttribute(
          ax::mojom::blink::IntListAttribute::kControlsIds)) {
    const auto ax_ids = node_data.GetIntListAttribute(
        ax::mojom::blink::IntListAttribute::kControlsIds);
    AXObject::AXObjectVector ax_objects;
    GetObjectsFromAXIDs(ax_object.AXObjectCache(), ax_ids, &ax_objects);
    properties.emplace_back(CreateRelatedNodeListProperty(
        AXPropertyNameEnum::Controls, ax_objects, html_names::kAriaControlsAttr,
        ax_object));
  }

  if (node_data.HasIntListAttribute(
          ax::mojom::blink::IntListAttribute::kDetailsIds)) {
    const auto ax_ids = node_data.GetIntListAttribute(
        ax::mojom::blink::IntListAttribute::kDetailsIds);
    AXObject::AXObjectVector ax_objects;
    GetObjectsFromAXIDs(ax_object.AXObjectCache(), ax_ids, &ax_objects);
    properties.emplace_back(
        CreateRelatedNodeListProperty(AXPropertyNameEnum::Details, ax_objects,
                                      html_names::kAriaDetailsAttr, ax_object));
  }

  if (node_data.HasIntListAttribute(
          ax::mojom::blink::IntListAttribute::kFlowtoIds)) {
    const auto ax_ids = node_data.GetIntListAttribute(
        ax::mojom::blink::IntListAttribute::kFlowtoIds);
    AXObject::AXObjectVector ax_objects;
    GetObjectsFromAXIDs(ax_object.AXObjectCache(), ax_ids, &ax_objects);
    properties.emplace_back(
        CreateRelatedNodeListProperty(AXPropertyNameEnum::Flowto, ax_objects,
                                      html_names::kAriaFlowtoAttr, ax_object));
  }
  return;
}

// Creates a role name value that is easy to read by developers. This function
// reduces the granularity of the role and uses ARIA role strings when possible.
std::unique_ptr<AXValue> CreateRoleNameValue(ax::mojom::Role role) {
  bool is_internal = false;
  const String& role_name = AXObject::RoleName(role, &is_internal);
  const auto& value_type =
      is_internal ? AXValueTypeEnum::InternalRole : AXValueTypeEnum::Role;
  return CreateValue(role_name, value_type);
}

// Creates an integer role value that is fixed over releases, is not lossy, and
// is more suitable for machine learning models or automation.
std::unique_ptr<AXValue> CreateInternalRoleValue(ax::mojom::Role role) {
  return CreateValue(static_cast<int>(role), AXValueTypeEnum::InternalRole);
}

}  // namespace

using EnabledAgentsMultimap =
    HeapHashMap<WeakMember<LocalFrame>,
                Member<HeapHashSet<Member<InspectorAccessibilityAgent>>>>;

EnabledAgentsMultimap& EnabledAgents() {
  DEFINE_STATIC_LOCAL(Persistent<EnabledAgentsMultimap>, enabled_agents,
                      (MakeGarbageCollected<EnabledAgentsMultimap>()));
  return *enabled_agents;
}

InspectorAccessibilityAgent::InspectorAccessibilityAgent(
    InspectedFrames* inspected_frames,
    InspectorDOMAgent* dom_agent)
    : inspected_frames_(inspected_frames),
      dom_agent_(dom_agent),
      enabled_(&agent_state_, /*default_value=*/false) {}

protocol::Response InspectorAccessibilityAgent::getPartialAXTree(
    Maybe<int> dom_node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    Maybe<bool> fetch_relatives,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  Node* dom_node = nullptr;
  protocol::Response response =
      dom_agent_->AssertNode(dom_node_id, backend_node_id, object_id, dom_node);
  if (!response.IsSuccess())
    return response;

  Document& document = dom_node->GetDocument();
  LocalFrame* local_frame = document.GetFrame();
  if (!local_frame)
    return protocol::Response::ServerError("Frame is detached.");

  auto& cache = AttachToAXObjectCache(&document);
  cache.UpdateAXForAllDocuments();
  ScopedFreezeAXCache freeze(cache);

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());

  AXObject* inspected_ax_object = cache.Get(dom_node);
  *nodes = std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();
  if (inspected_ax_object) {
    (*nodes)->emplace_back(
        BuildProtocolAXNodeForAXObject(*inspected_ax_object));
  } else {
    (*nodes)->emplace_back(BuildProtocolAXNodeForDOMNodeWithNoAXNode(
        IdentifiersFactory::IntIdForNode(dom_node)));
  }

  if (!fetch_relatives.value_or(true)) {
    return protocol::Response::Success();
  }

  if (inspected_ax_object && !inspected_ax_object->IsIgnored())
    AddChildren(*inspected_ax_object, true, *nodes, cache);

  AXObject* parent_ax_object;
  if (inspected_ax_object) {
    parent_ax_object = inspected_ax_object->ParentObjectIncludedInTree();
  } else {
    // Walk up parents until an AXObject can be found.
    auto* shadow_root = DynamicTo<ShadowRoot>(dom_node);
    Node* parent_node = shadow_root ? &shadow_root->host()
                                    : FlatTreeTraversal::Parent(*dom_node);
    parent_ax_object = cache.Get(parent_node);
    while (parent_node && !parent_ax_object) {
      shadow_root = DynamicTo<ShadowRoot>(parent_node);
      parent_node = shadow_root ? &shadow_root->host()
                                : FlatTreeTraversal::Parent(*parent_node);
      parent_ax_object = cache.Get(parent_node);
    }
  }
  if (!parent_ax_object)
    return protocol::Response::Success();
  AddAncestors(*parent_ax_object, inspected_ax_object, *nodes, cache);

  return protocol::Response::Success();
}

void InspectorAccessibilityAgent::AddAncestors(
    AXObject& first_ancestor,
    AXObject* inspected_ax_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  std::unique_ptr<AXNode> first_parent_node_object =
      BuildProtocolAXNodeForAXObject(first_ancestor);
  // Since the inspected node is ignored it is missing from the first ancestors
  // childIds. We therefore add it to maintain the tree structure:
  if (!inspected_ax_object || inspected_ax_object->IsIgnored()) {
    auto child_ids = std::make_unique<protocol::Array<AXNodeId>>();
    auto* existing_child_ids = first_parent_node_object->getChildIds(nullptr);

    // put the ignored node first regardless of DOM structure.
    child_ids->insert(
        child_ids->begin(),
        String::Number(inspected_ax_object ? inspected_ax_object->AXObjectID()
                                           : kIDForInspectedNodeWithNoAXNode));
    if (existing_child_ids) {
      for (auto id : *existing_child_ids)
        child_ids->push_back(id);
    }
    first_parent_node_object->setChildIds(std::move(child_ids));
  }
  nodes->emplace_back(std::move(first_parent_node_object));
  AXObject* ancestor = first_ancestor.ParentObjectIncludedInTree();
  while (ancestor) {
    std::unique_ptr<AXNode> parent_node_object =
        BuildProtocolAXNodeForAXObject(*ancestor);
    nodes->emplace_back(std::move(parent_node_object));
    ancestor = ancestor->ParentObjectIncludedInTree();
  }
}

std::unique_ptr<AXNode>
InspectorAccessibilityAgent::BuildProtocolAXNodeForDOMNodeWithNoAXNode(
    int backend_node_id) const {
  AXID ax_id = kIDForInspectedNodeWithNoAXNode;
  std::unique_ptr<AXNode> ignored_node_object =
      AXNode::create()
          .setNodeId(String::Number(ax_id))
          .setIgnored(true)
          .build();
  ax::mojom::blink::Role role = ax::mojom::blink::Role::kNone;
  ignored_node_object->setRole(CreateRoleNameValue(role));
  ignored_node_object->setChromeRole(CreateInternalRoleValue(role));
  auto ignored_reason_properties =
      std::make_unique<protocol::Array<AXProperty>>();
  ignored_reason_properties->emplace_back(
      CreateProperty(IgnoredReason(kAXNotRendered)));
  ignored_node_object->setIgnoredReasons(std::move(ignored_reason_properties));
  ignored_node_object->setBackendDOMNodeId(backend_node_id);
  return ignored_node_object;
}

std::unique_ptr<AXNode>
InspectorAccessibilityAgent::BuildProtocolAXNodeForAXObject(
    AXObject& ax_object,
    bool force_name_and_role) const {
  std::unique_ptr<protocol::Accessibility::AXNode> protocol_node;
  if (ax_object.IsIgnored()) {
    protocol_node =
        BuildProtocolAXNodeForIgnoredAXObject(ax_object, force_name_and_role);
  } else {
    protocol_node = BuildProtocolAXNodeForUnignoredAXObject(ax_object);
  }
  const AXObject::AXObjectVector& children =
      ax_object.ChildrenIncludingIgnored();
  auto child_ids = std::make_unique<protocol::Array<AXNodeId>>();
  for (AXObject* child : children)
    child_ids->emplace_back(String::Number(child->AXObjectID()));
  protocol_node->setChildIds(std::move(child_ids));

  Node* node = ax_object.GetNode();
  if (node)
    protocol_node->setBackendDOMNodeId(IdentifiersFactory::IntIdForNode(node));

  const AXObject* parent = ax_object.ParentObjectIncludedInTree();
  if (parent) {
    protocol_node->setParentId(String::Number(parent->AXObjectID()));
  } else {
    DCHECK(ax_object.GetDocument() && ax_object.GetDocument()->GetFrame());
    auto& frame_token =
        ax_object.GetDocument()->GetFrame()->GetDevToolsFrameToken();
    protocol_node->setFrameId(IdentifiersFactory::IdFromToken(frame_token));
  }
  return protocol_node;
}

std::unique_ptr<AXNode>
InspectorAccessibilityAgent::BuildProtocolAXNodeForIgnoredAXObject(
    AXObject& ax_object,
    bool force_name_and_role) const {
  std::unique_ptr<AXNode> ignored_node_object =
      AXNode::create()
          .setNodeId(String::Number(ax_object.AXObjectID()))
          .setIgnored(true)
          .build();
  ax::mojom::blink::Role role = ax::mojom::blink::Role::kNone;
  ignored_node_object->setRole(CreateRoleNameValue(role));
  ignored_node_object->setChromeRole(CreateInternalRoleValue(role));

  if (force_name_and_role) {
    // Compute accessible name and sources and attach to protocol node:
    AXObject::NameSources name_sources;
    String computed_name = ax_object.GetName(&name_sources);
    std::unique_ptr<AXValue> name =
        CreateValue(computed_name, AXValueTypeEnum::ComputedString);
    ignored_node_object->setName(std::move(name));
    ignored_node_object->setRole(CreateRoleNameValue(ax_object.RoleValue()));
    ignored_node_object->setChromeRole(
        CreateInternalRoleValue(ax_object.RoleValue()));
  }

  // Compute and attach reason for node to be ignored:
  AXObject::IgnoredReasons ignored_reasons;
  ax_object.ComputeIsIgnored(&ignored_reasons);
  auto ignored_reason_properties =
      std::make_unique<protocol::Array<AXProperty>>();
  for (IgnoredReason& reason : ignored_reasons)
    ignored_reason_properties->emplace_back(CreateProperty(reason));
  ignored_node_object->setIgnoredReasons(std::move(ignored_reason_properties));

  return ignored_node_object;
}

std::unique_ptr<AXNode>
InspectorAccessibilityAgent::BuildProtocolAXNodeForUnignoredAXObject(
    AXObject& ax_object) const {
  std::unique_ptr<AXNode> node_object =
      AXNode::create()
          .setNodeId(String::Number(ax_object.AXObjectID()))
          .setIgnored(false)
          .build();
  auto properties = std::make_unique<protocol::Array<AXProperty>>();
  ui::AXNodeData node_data;
  ax_object.Serialize(&node_data, ui::kAXModeComplete);
  node_object->setRole(CreateRoleNameValue(node_data.role));
  node_object->setChromeRole(CreateInternalRoleValue(node_data.role));
  FillLiveRegionProperties(ax_object, node_data, *(properties.get()));
  FillGlobalStates(ax_object, node_data, *(properties.get()));
  FillWidgetProperties(ax_object, node_data, *(properties.get()));
  FillWidgetStates(ax_object, node_data, *(properties.get()));
  FillRelationships(ax_object, *(properties.get()));
  FillSparseAttributes(ax_object, node_data, *(properties.get()));

  AXObject::NameSources name_sources;
  String computed_name = ax_object.GetName(&name_sources);
  if (!name_sources.empty()) {
    std::unique_ptr<AXValue> name =
        CreateValue(computed_name, AXValueTypeEnum::ComputedString);
    if (!name_sources.empty()) {
      auto name_source_properties =
          std::make_unique<protocol::Array<AXValueSource>>();
      for (NameSource& name_source : name_sources) {
        name_source_properties->emplace_back(CreateValueSource(name_source));
        if (name_source.text.IsNull() || name_source.superseded)
          continue;
        if (!name_source.related_objects.empty()) {
          properties->emplace_back(CreateRelatedNodeListProperty(
              AXPropertyNameEnum::Labelledby, name_source.related_objects));
        }
      }
      name->setSources(std::move(name_source_properties));
    }
    node_object->setProperties(std::move(properties));
    node_object->setName(std::move(name));
  } else {
    node_object->setProperties(std::move(properties));
  }

  FillCoreProperties(ax_object, node_object.get());
  return node_object;
}

LocalFrame* InspectorAccessibilityAgent::FrameFromIdOrRoot(
    const protocol::Maybe<String>& frame_id) {
  if (frame_id.has_value()) {
    return IdentifiersFactory::FrameById(inspected_frames_.Get(),
                                         frame_id.value());
  }
  return inspected_frames_->Root();
}

protocol::Response InspectorAccessibilityAgent::getFullAXTree(
    protocol::Maybe<int> depth,
    Maybe<String> frame_id,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  LocalFrame* frame = FrameFromIdOrRoot(frame_id);
  if (!frame) {
    return protocol::Response::InvalidParams(
        "Frame with the given frameId is not found.");
  }

  Document* document = frame->GetDocument();
  if (!document)
    return protocol::Response::InternalError();
  if (document->View()->NeedsLayout() || document->NeedsLayoutTreeUpdate())
    document->UpdateStyleAndLayout(DocumentUpdateReason::kInspector);

  *nodes = WalkAXNodesToDepth(document, depth.value_or(-1));

  return protocol::Response::Success();
}

std::unique_ptr<protocol::Array<AXNode>>
InspectorAccessibilityAgent::WalkAXNodesToDepth(Document* document,
                                                int max_depth) {
  std::unique_ptr<protocol::Array<AXNode>> nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  auto& cache = AttachToAXObjectCache(document);
  cache.UpdateAXForAllDocuments();
  ScopedFreezeAXCache freeze(cache);

  Deque<std::pair<AXID, int>> id_depths;
  id_depths.emplace_back(cache.Root()->AXObjectID(), 1);
  nodes->emplace_back(BuildProtocolAXNodeForAXObject(*cache.Root()));

  while (!id_depths.empty()) {
    std::pair<AXID, int> id_depth = id_depths.front();
    id_depths.pop_front();
    AXObject* ax_object = cache.ObjectFromAXID(id_depth.first);
    if (!ax_object)
      continue;
    AddChildren(*ax_object, true, nodes, cache);

    const AXObject::AXObjectVector& children = ax_object->UnignoredChildren();

    for (auto& child_ax_object : children) {
      int depth = id_depth.second;
      if (max_depth == -1 || depth < max_depth)
        id_depths.emplace_back(child_ax_object->AXObjectID(), depth + 1);
    }
  }

  return nodes;
}

protocol::Response InspectorAccessibilityAgent::getRootAXNode(
    Maybe<String> frame_id,
    std::unique_ptr<AXNode>* node) {
  LocalFrame* frame = FrameFromIdOrRoot(frame_id);
  if (!frame) {
    return protocol::Response::InvalidParams(
        "Frame with the given frameId is not found.");
  }
  if (!enabled_.Get()) {
    return protocol::Response::ServerError(
        "Accessibility has not been enabled.");
  }

  Document* document = frame->GetDocument();
  if (!document)
    return protocol::Response::InternalError();

  auto& cache = AttachToAXObjectCache(document);
  cache.UpdateAXForAllDocuments();
  auto& root = *cache.Root();

  ScopedFreezeAXCache freeze(cache);

  *node = BuildProtocolAXNodeForAXObject(root);
  nodes_requested_.insert(root.AXObjectID());

  return protocol::Response::Success();
}

protocol::Response InspectorAccessibilityAgent::getAXNodeAndAncestors(
    Maybe<int> dom_node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*
        out_nodes) {
  if (!enabled_.Get()) {
    return protocol::Response::ServerError(
        "Accessibility has not been enabled.");
  }

  Node* dom_node = nullptr;
  protocol::Response response =
      dom_agent_->AssertNode(dom_node_id, backend_node_id, object_id, dom_node);
  if (!response.IsSuccess())
    return response;

  Document& document = dom_node->GetDocument();
  LocalFrame* local_frame = document.GetFrame();
  if (!local_frame)
    return protocol::Response::ServerError("Frame is detached.");

  auto& cache = AttachToAXObjectCache(&document);
  cache.UpdateAXForAllDocuments();
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());

  AXObject* ax_object = cache.Get(dom_node);

  ScopedFreezeAXCache freeze(cache);

  *out_nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  if (!ax_object) {
    (*out_nodes)
        ->emplace_back(BuildProtocolAXNodeForDOMNodeWithNoAXNode(
            IdentifiersFactory::IntIdForNode(dom_node)));
    return protocol::Response::Success();
  }

  do {
    nodes_requested_.insert(ax_object->AXObjectID());
    std::unique_ptr<AXNode> ancestor =
        BuildProtocolAXNodeForAXObject(*ax_object);
    (*out_nodes)->emplace_back(std::move(ancestor));
    ax_object = ax_object->ParentObjectIncludedInTree();
  } while (ax_object);

  return protocol::Response::Success();
}

protocol::Response InspectorAccessibilityAgent::getChildAXNodes(
    const String& in_id,
    Maybe<String> frame_id,
    std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*
        out_nodes) {
  if (!enabled_.Get()) {
    return protocol::Response::ServerError(
        "Accessibility has not been enabled.");
  }

  LocalFrame* frame = FrameFromIdOrRoot(frame_id);
  if (!frame) {
    return protocol::Response::InvalidParams(
        "Frame with the given frameId is not found.");
  }

  Document* document = frame->GetDocument();
  if (!document)
    return protocol::Response::InternalError();

  auto& cache = AttachToAXObjectCache(document);
  cache.UpdateAXForAllDocuments();

  ScopedFreezeAXCache freeze(cache);

  AXID ax_id = in_id.ToInt();
  AXObject* ax_object = cache.ObjectFromAXID(ax_id);

  if (!ax_object || ax_object->IsDetached())
    return protocol::Response::InvalidParams("Invalid ID");

  *out_nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  AddChildren(*ax_object, /* follow_ignored */ true, *out_nodes, cache);

  for (const auto& child : **out_nodes)
    nodes_requested_.insert(child->getNodeId().ToInt());

  return protocol::Response::Success();
}

void InspectorAccessibilityAgent::FillCoreProperties(
    AXObject& ax_object,
    AXNode* node_object) const {
  ax::mojom::NameFrom name_from;
  AXObject::AXObjectVector name_objects;
  ax_object.GetName(name_from, &name_objects);

  ax::mojom::DescriptionFrom description_from;
  AXObject::AXObjectVector description_objects;
  String description =
      ax_object.Description(name_from, description_from, &description_objects);
  if (!description.empty()) {
    node_object->setDescription(
        CreateValue(description, AXValueTypeEnum::ComputedString));
  }
  // Value.
  if (ax_object.IsRangeValueSupported()) {
    float value;
    if (ax_object.ValueForRange(&value))
      node_object->setValue(CreateValue(value));
  } else {
    String value = ax_object.SlowGetValueForControlIncludingContentEditable();
    if (!value.empty())
      node_object->setValue(CreateValue(value));
  }
}

void InspectorAccessibilityAgent::AddChildren(
    AXObject& ax_object,
    bool follow_ignored,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  HeapVector<Member<AXObject>> reachable;
  reachable.AppendRange(ax_object.ChildrenIncludingIgnored().rbegin(),
                        ax_object.ChildrenIncludingIgnored().rend());

  while (!reachable.empty()) {
    AXObject* descendant = reachable.back();
    reachable.pop_back();
    if (descendant->IsDetached())
      continue;

    // If the node is ignored or has no corresponding DOM node, we include
    // another layer of children.
    if (follow_ignored &&
        (descendant->IsIgnoredButIncludedInTree() ||
         !descendant->GetNode())) {
      reachable.AppendRange(descendant->ChildrenIncludingIgnored().rbegin(),
                            descendant->ChildrenIncludingIgnored().rend());
    }
    auto child_node = BuildProtocolAXNodeForAXObject(*descendant);
    nodes->emplace_back(std::move(child_node));
  }
}

void InspectorAccessibilityAgent::queryAXTree(
    Maybe<int> dom_node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    Maybe<String> accessible_name,
    Maybe<String> role,
    std::unique_ptr<QueryAXTreeCallback> callback) {
  Node* root_dom_node = nullptr;
  protocol::Response response = dom_agent_->AssertNode(
      dom_node_id, backend_node_id, object_id, root_dom_node);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }

  // Shadow roots are missing from a11y tree.
  // We start searching the host element instead as a11y tree does not
  // care about shadow roots.
  if (root_dom_node->IsShadowRoot()) {
    root_dom_node = root_dom_node->OwnerShadowHost();
  }
  if (!root_dom_node) {
    callback->sendFailure(
        protocol::Response::InvalidParams("Root DOM node could not be found"));
    return;
  }

  Document& document = root_dom_node->GetDocument();
  auto& cache = AttachToAXObjectCache(&document);
  cache.UpdateAXForAllDocuments();

  AXQuery query = {std::move(dom_node_id), std::move(backend_node_id),
                   std::move(object_id),   std::move(accessible_name),
                   std::move(role),        std::move(callback)};
  auto it = queries_.find(&document);
  if (it != queries_.end()) {
    it->value.push_back(std::move(query));
  } else {
    Vector<AXQuery> vector;
    vector.emplace_back(std::move(query));
    queries_.insert(&document, std::move(vector));
  }
  // ScheduleAXUpdate() ensures the lifecycle doesn't get stalled,
  // and therefore ensures we get the AXReadyCallback callback as soon as a11y
  // is clean again.
  cache.ScheduleAXUpdate();
}

void InspectorAccessibilityAgent::CompleteQuery(AXQuery& query) {
  Node* root_dom_node = nullptr;

  protocol::Response response = dom_agent_->AssertNode(
      query.dom_node_id, query.backend_node_id, query.object_id, root_dom_node);
  if (!response.IsSuccess()) {
    query.callback->sendFailure(response);
    return;
  }

  // Shadow roots are missing from a11y tree.
  // We start searching the host element instead as a11y tree does not
  // care about shadow roots.
  if (root_dom_node->IsShadowRoot())
    root_dom_node = root_dom_node->OwnerShadowHost();
  if (!root_dom_node) {
    query.callback->sendFailure(
        protocol::Response::InvalidParams("Root DOM node could not be found"));
    return;
  }
  Document& document = root_dom_node->GetDocument();

  document.UpdateStyleAndLayout(DocumentUpdateReason::kInspector);
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());
  auto& cache = AttachToAXObjectCache(&document);
  ScopedFreezeAXCache freeze(cache);

  std::unique_ptr<protocol::Array<AXNode>> nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();
  AXObject* root_ax_node = cache.Get(root_dom_node);

  HeapVector<Member<AXObject>> reachable;
  if (root_ax_node)
    reachable.push_back(root_ax_node);

  while (!reachable.empty()) {
    AXObject* ax_object = reachable.back();
    if (ax_object->IsDetached() ||
        !ax_object->IsIncludedInTree()) {
      reachable.pop_back();
      continue;
    }
    ui::AXNodeData node_data;
    ax_object->Serialize(&node_data, ui::kAXModeComplete);
    reachable.pop_back();
    const AXObject::AXObjectVector& children =
        ax_object->ChildrenIncludingIgnored();
    reachable.AppendRange(children.rbegin(), children.rend());

    const bool ignored = ax_object->IsIgnored();
    // if querying by name: skip if name of current object does not match.
    // For now, we need to handle names of ignored nodes separately, since they
    // do not get a name assigned when serializing to AXNodeData.
    if (ignored && query.accessible_name.has_value() &&
        query.accessible_name.value() != ax_object->ComputedName()) {
      continue;
    }
    if (!ignored && query.accessible_name.has_value() &&
        query.accessible_name.value().Utf8() !=
            node_data.GetStringAttribute(
                ax::mojom::blink::StringAttribute::kName)) {
      continue;
    }

    // if querying by role: skip if role of current object does not match.
    if (query.role.has_value() &&
        query.role.value() != AXObject::RoleName(node_data.role)) {
      continue;
    }

    // both name and role are OK, so we can add current object to the result.
    nodes->push_back(BuildProtocolAXNodeForAXObject(
        *ax_object, /* force_name_and_role */ true));
  }

  query.callback->sendSuccess(std::move(nodes));
}

void InspectorAccessibilityAgent::AXReadyCallback(Document& document) {
  ProcessPendingQueries(document);
  ProcessPendingDirtyNodes(document);
  if (load_complete_needs_processing_.Contains(&document) &&
      document.IsLoadCompleted()) {
    load_complete_needs_processing_.erase(&document);
    AXObjectCache* cache = document.ExistingAXObjectCache();
    CHECK(cache);
    AXObject* root = cache->Root();
    CHECK(root);
    dirty_nodes_.clear();
    nodes_requested_.clear();
    nodes_requested_.insert(root->AXObjectID());
    ScopedFreezeAXCache freeze(*cache);
    GetFrontend()->loadComplete(BuildProtocolAXNodeForAXObject(*root));
  }
}

void InspectorAccessibilityAgent::ProcessPendingQueries(Document& document) {
  auto it = queries_.find(&document);
  if (it == queries_.end())
    return;
  for (auto& query : it->value)
    CompleteQuery(query);
  queries_.erase(&document);
}

void InspectorAccessibilityAgent::ProcessPendingDirtyNodes(Document& document) {
  auto now = base::Time::Now();

  if (!last_sync_times_.Contains(&document))
    last_sync_times_.insert(&document, now);
  else if (now - last_sync_times_.at(&document) < kNodeSyncThrottlePeriod)
    return;
  else
    last_sync_times_.at(&document) = now;

  if (!dirty_nodes_.Contains(&document))
    return;
  // Sometimes, computing properties for an object while serializing will
  // mark other objects dirty. This makes us re-enter this function.
  // To make this benign, we use a copy of dirty_nodes_ when iterating.
  Member<HeapHashSet<WeakMember<AXObject>>> dirty_nodes =
      dirty_nodes_.Take(&document);
  auto nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  CHECK(document.ExistingAXObjectCache());
  ScopedFreezeAXCache freeze(*document.ExistingAXObjectCache());
  for (AXObject* changed_node : *dirty_nodes) {
    if (!changed_node->IsDetached())
      nodes->push_back(BuildProtocolAXNodeForAXObject(*changed_node));
  }
  GetFrontend()->nodesUpdated(std::move(nodes));
}

void InspectorAccessibilityAgent::ScheduleAXUpdateIfNeeded(TimerBase*,
                                                           Document* document) {
  DCHECK(document);

  if (!dirty_nodes_.Contains(document))
    return;

  // Scheduling an AX update for the cache will schedule it for both the main
  // document, and the popup document (if present).
  if (auto* cache = document->ExistingAXObjectCache()) {
    cache->ScheduleAXUpdate();
  }
}

void InspectorAccessibilityAgent::ScheduleAXChangeNotification(
    Document* document) {
  DCHECK(document);
  if (!timers_.Contains(document)) {
    timers_.insert(document,
                   MakeGarbageCollected<DisallowNewWrapper<DocumentTimer>>(
                       document, this,
                       &InspectorAccessibilityAgent::ScheduleAXUpdateIfNeeded));
  }
  DisallowNewWrapper<DocumentTimer>* timer = timers_.at(document);
  if (!timer->Value().IsActive())
    timer->Value().StartOneShot(kVisualUpdateCheckInterval, FROM_HERE);
}

void InspectorAccessibilityAgent::AXEventFired(AXObject* ax_object,
                                               ax::mojom::blink::Event event) {
  if (!enabled_.Get())
    return;
  DCHECK(ax_object->IsIncludedInTree());

  switch (event) {
    case ax::mojom::blink::Event::kLoadComplete: {
      // Will be handled in AXReadyCallback().
      load_complete_needs_processing_.insert(ax_object->GetDocument());
    } break;
    case ax::mojom::blink::Event::kLocationChanged:
      // Since we do not serialize location data we can ignore changes to this.
      break;
    default:
      MarkAXObjectDirty(ax_object);
      ScheduleAXChangeNotification(ax_object->GetDocument());
      break;
  }
}

bool InspectorAccessibilityAgent::MarkAXObjectDirty(AXObject* ax_object) {
  if (!nodes_requested_.Contains(ax_object->AXObjectID()))
    return false;
  Document* document = ax_object->GetDocument();
  auto inserted = dirty_nodes_.insert(document, nullptr);
  if (inserted.is_new_entry) {
    inserted.stored_value->value =
        MakeGarbageCollected<HeapHashSet<WeakMember<AXObject>>>();
  }
  return inserted.stored_value->value->insert(ax_object).is_new_entry;
}

void InspectorAccessibilityAgent::AXObjectModified(AXObject* ax_object,
                                                   bool subtree) {
  if (!enabled_.Get())
    return;
  DCHECK(ax_object->IsIncludedInTree());
  if (subtree) {
    HeapVector<Member<AXObject>> reachable;
    reachable.push_back(ax_object);
    while (!reachable.empty()) {
      AXObject* descendant = reachable.back();
      reachable.pop_back();
      DCHECK(descendant->IsIncludedInTree());
      if (!MarkAXObjectDirty(descendant))
        continue;
      const AXObject::AXObjectVector& children =
          descendant->ChildrenIncludingIgnored();
      reachable.AppendRange(children.rbegin(), children.rend());
    }
  } else {
    MarkAXObjectDirty(ax_object);
  }
  ScheduleAXChangeNotification(ax_object->GetDocument());
}

void InspectorAccessibilityAgent::EnableAndReset() {
  enabled_.Set(true);
  LocalFrame* frame = inspected_frames_->Root();
  if (!EnabledAgents().Contains(frame)) {
    EnabledAgents().Set(
        frame, MakeGarbageCollected<
                   HeapHashSet<Member<InspectorAccessibilityAgent>>>());
  }
  EnabledAgents().find(frame)->value->insert(this);
  for (auto& context : document_to_context_map_.Values()) {
    auto& cache = To<AXObjectCacheImpl>(context->GetAXObjectCache());
    cache.AddInspectorAgent(this);
  }
}

protocol::Response InspectorAccessibilityAgent::enable() {
  if (!enabled_.Get())
    EnableAndReset();
  return protocol::Response::Success();
}

protocol::Response InspectorAccessibilityAgent::disable() {
  if (!enabled_.Get())
    return protocol::Response::Success();
  enabled_.Set(false);
  for (auto& document : document_to_context_map_.Keys()) {
    DCHECK(document);
    // We do not rely on AXContext::GetAXObjectCache here, since it might
    // dereference nullptrs and requires several preconditions to be checked.
    // Instead, we remove the agent from any document that has an existing
    // AXObjectCache.
    AXObjectCache* existing_cache = document->ExistingAXObjectCache();
    if (!existing_cache) {
      continue;
    }
    auto& cache = To<AXObjectCacheImpl>(*existing_cache);
    cache.RemoveInspectorAgent(this);
  }
  document_to_context_map_.clear();
  nodes_requested_.clear();
  dirty_nodes_.clear();
  LocalFrame* frame = inspected_frames_->Root();
  DCHECK(EnabledAgents().Contains(frame));
  auto it = EnabledAgents().find(frame);
  it->value->erase(this);
  if (it->value->empty())
    EnabledAgents().erase(frame);
  return protocol::Response::Success();
}

void InspectorAccessibilityAgent::Restore() {
  if (enabled_.Get())
    EnableAndReset();
}

void InspectorAccessibilityAgent::ProvideTo(LocalFrame* frame) {
  if (!EnabledAgents().Contains(frame))
    return;
  for (InspectorAccessibilityAgent* agent :
       *EnabledAgents().find(frame)->value) {
    agent->AttachToAXObjectCache(frame->GetDocument());
  }
}

AXObjectCacheImpl& InspectorAccessibilityAgent::AttachToAXObjectCache(
    Document* document) {
  DCHECK(document);
  DCHECK(document->IsActive());
  if (!document_to_context_map_.Contains(document)) {
    auto context = std::make_unique<AXContext>(*document, ui::kAXModeComplete);
    document_to_context_map_.insert(document, std::move(context));
  }
  AXObjectCacheImpl* cache =
      To<AXObjectCacheImpl>(document->ExistingAXObjectCache());
  cache->AddInspectorAgent(this);
  return *cache;
}

void InspectorAccessibilityAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  visitor->Trace(dom_agent_);
  visitor->Trace(document_to_context_map_);
  visitor->Trace(dirty_nodes_);
  visitor->Trace(timers_);
  visitor->Trace(queries_);
  visitor->Trace(last_sync_times_);
  visitor->Trace(load_complete_needs_processing_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
