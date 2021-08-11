// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "ui/accessibility/ax_mode.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;
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

void AddHasPopupProperty(ax::mojom::HasPopup has_popup,
                         protocol::Array<AXProperty>& properties) {
  switch (has_popup) {
    case ax::mojom::HasPopup::kFalse:
      break;
    case ax::mojom::HasPopup::kTrue:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("true", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::HasPopup::kMenu:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("menu", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::HasPopup::kListbox:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("listbox", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::HasPopup::kTree:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("tree", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::HasPopup::kGrid:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("grid", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::HasPopup::kDialog:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::HasPopup,
                         CreateValue("dialog", AXValueTypeEnum::Token)));
      break;
  }
}

void FillLiveRegionProperties(AXObject& ax_object,
                              protocol::Array<AXProperty>& properties) {
  if (!ax_object.LiveRegionRoot())
    return;

  properties.emplace_back(
      CreateProperty(AXPropertyNameEnum::Live,
                     CreateValue(ax_object.ContainerLiveRegionStatus(),
                                 AXValueTypeEnum::Token)));
  properties.emplace_back(CreateProperty(
      AXPropertyNameEnum::Atomic,
      CreateBooleanValue(ax_object.ContainerLiveRegionAtomic())));
  properties.emplace_back(
      CreateProperty(AXPropertyNameEnum::Relevant,
                     CreateValue(ax_object.ContainerLiveRegionRelevant(),
                                 AXValueTypeEnum::TokenList)));

  if (!ax_object.IsLiveRegionRoot()) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Root,
        CreateRelatedNodeListValue(*(ax_object.LiveRegionRoot()))));
  }
}

void FillGlobalStates(AXObject& ax_object,
                      protocol::Array<AXProperty>& properties) {
  if (ax_object.Restriction() == kRestrictionDisabled) {
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

  ax::mojom::InvalidState invalid_state = ax_object.GetInvalidState();
  switch (invalid_state) {
    case ax::mojom::InvalidState::kNone:
      break;
    case ax::mojom::InvalidState::kFalse:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::Invalid,
                         CreateValue("false", AXValueTypeEnum::Token)));
      break;
    case ax::mojom::InvalidState::kTrue:
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::Invalid,
                         CreateValue("true", AXValueTypeEnum::Token)));
      break;
    default:
      // TODO(aboxhall): expose invalid: <nothing> and source: aria-invalid as
      // invalid value
      properties.emplace_back(CreateProperty(
          AXPropertyNameEnum::Invalid,
          CreateValue(ax_object.AriaInvalidValue(), AXValueTypeEnum::String)));
      break;
  }

  if (ax_object.CanSetFocusAttribute()) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Focusable,
        CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
  }
  if (ax_object.IsFocused()) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Focused,
        CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
  }
  if (ax_object.IsEditable()) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Editable,
        CreateValue(ax_object.IsRichlyEditable() ? "richtext" : "plaintext",
                    AXValueTypeEnum::Token)));
  }
  if (ax_object.CanSetValueAttribute()) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Settable,
        CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
  }
}

bool RoleAllowsModal(ax::mojom::Role role) {
  return role == ax::mojom::Role::kDialog ||
         role == ax::mojom::Role::kAlertDialog;
}

bool RoleAllowsMultiselectable(ax::mojom::Role role) {
  return role == ax::mojom::Role::kGrid || role == ax::mojom::Role::kListBox ||
         role == ax::mojom::Role::kTabList ||
         role == ax::mojom::Role::kTreeGrid || role == ax::mojom::Role::kTree;
}

bool RoleAllowsOrientation(ax::mojom::Role role) {
  return role == ax::mojom::Role::kScrollBar ||
         role == ax::mojom::Role::kSplitter || role == ax::mojom::Role::kSlider;
}

bool RoleAllowsReadonly(ax::mojom::Role role) {
  return role == ax::mojom::Role::kGrid || role == ax::mojom::Role::kCell ||
         role == ax::mojom::Role::kTextField ||
         role == ax::mojom::Role::kColumnHeader ||
         role == ax::mojom::Role::kRowHeader ||
         role == ax::mojom::Role::kTreeGrid;
}

bool RoleAllowsRequired(ax::mojom::Role role) {
  return role == ax::mojom::Role::kComboBoxGrouping ||
         role == ax::mojom::Role::kComboBoxMenuButton ||
         role == ax::mojom::Role::kCell || role == ax::mojom::Role::kListBox ||
         role == ax::mojom::Role::kRadioGroup ||
         role == ax::mojom::Role::kSpinButton ||
         role == ax::mojom::Role::kTextField ||
         role == ax::mojom::Role::kTextFieldWithComboBox ||
         role == ax::mojom::Role::kTree ||
         role == ax::mojom::Role::kColumnHeader ||
         role == ax::mojom::Role::kRowHeader ||
         role == ax::mojom::Role::kTreeGrid;
}

bool RoleAllowsSort(ax::mojom::Role role) {
  return role == ax::mojom::Role::kColumnHeader ||
         role == ax::mojom::Role::kRowHeader;
}

void FillWidgetProperties(AXObject& ax_object,
                          protocol::Array<AXProperty>& properties) {
  ax::mojom::Role role = ax_object.RoleValue();
  String autocomplete = ax_object.AutoComplete();
  if (!autocomplete.IsEmpty())
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Autocomplete,
                       CreateValue(autocomplete, AXValueTypeEnum::Token)));

  AddHasPopupProperty(ax_object.HasPopup(), properties);

  int heading_level = ax_object.HeadingLevel();
  if (heading_level > 0) {
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Level, CreateValue(heading_level)));
  }
  int hierarchical_level = ax_object.HierarchicalLevel();
  if (hierarchical_level > 0 ||
      ax_object.HasAttribute(html_names::kAriaLevelAttr)) {
    properties.emplace_back(CreateProperty(AXPropertyNameEnum::Level,
                                           CreateValue(hierarchical_level)));
  }

  if (RoleAllowsMultiselectable(role)) {
    bool multiselectable = ax_object.IsMultiSelectable();
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Multiselectable,
                       CreateBooleanValue(multiselectable)));
  }

  if (RoleAllowsOrientation(role)) {
    AccessibilityOrientation orientation = ax_object.Orientation();
    switch (orientation) {
      case kAccessibilityOrientationVertical:
        properties.emplace_back(
            CreateProperty(AXPropertyNameEnum::Orientation,
                           CreateValue("vertical", AXValueTypeEnum::Token)));
        break;
      case kAccessibilityOrientationHorizontal:
        properties.emplace_back(
            CreateProperty(AXPropertyNameEnum::Orientation,
                           CreateValue("horizontal", AXValueTypeEnum::Token)));
        break;
      case kAccessibilityOrientationUndefined:
        break;
    }
  }

  if (role == ax::mojom::Role::kTextField) {
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Multiline,
                       CreateBooleanValue(ax_object.IsMultiline())));
  }

  if (RoleAllowsReadonly(role)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Readonly,
        CreateBooleanValue(ax_object.Restriction() == kRestrictionReadOnly)));
  }

  if (RoleAllowsRequired(role)) {
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Required,
                       CreateBooleanValue(ax_object.IsRequired())));
  }

  if (RoleAllowsSort(role)) {
    // TODO(aboxhall): sort
  }

  if (ax_object.IsRangeValueSupported()) {
    float min_value;
    if (ax_object.MinValueForRange(&min_value)) {
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::Valuemin, CreateValue(min_value)));
    }

    float max_value;
    if (ax_object.MaxValueForRange(&max_value)) {
      properties.emplace_back(
          CreateProperty(AXPropertyNameEnum::Valuemax, CreateValue(max_value)));
    }

    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Valuetext,
        CreateValue(
            ax_object
                .GetAOMPropertyOrARIAAttribute(AOMStringProperty::kValueText)
                .GetString())));
  }
}

void FillWidgetStates(AXObject& ax_object,
                      protocol::Array<AXProperty>& properties) {
  ax::mojom::Role role = ax_object.RoleValue();
  const char* checked_prop_val = nullptr;
  switch (ax_object.CheckedState()) {
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

  AccessibilityExpanded expanded = ax_object.IsExpanded();
  switch (expanded) {
    case kExpandedUndefined:
      break;
    case kExpandedCollapsed:
      properties.emplace_back(CreateProperty(
          AXPropertyNameEnum::Expanded,
          CreateBooleanValue(false, AXValueTypeEnum::BooleanOrUndefined)));
      break;
    case kExpandedExpanded:
      properties.emplace_back(CreateProperty(
          AXPropertyNameEnum::Expanded,
          CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
      break;
  }

  AccessibilitySelectedState selected = ax_object.IsSelected();
  switch (selected) {
    case kSelectedStateUndefined:
      break;
    case kSelectedStateFalse:
      properties.emplace_back(CreateProperty(
          AXPropertyNameEnum::Selected,
          CreateBooleanValue(false, AXValueTypeEnum::BooleanOrUndefined)));
      break;
    case kSelectedStateTrue:
      properties.emplace_back(CreateProperty(
          AXPropertyNameEnum::Selected,
          CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
      break;
  }

  if (RoleAllowsModal(role)) {
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Modal, CreateBooleanValue(ax_object.IsModal())));
  }
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
  const AtomicString& attr_value = ax_object.GetAttribute(attr);
  node_list_value->setValue(protocol::StringValue::create(attr_value));
  return CreateProperty(key, std::move(node_list_value));
}

void FillRelationships(AXObject& ax_object,
                       protocol::Array<AXProperty>& properties) {
  AXObject::AXObjectVector results;
  ax_object.AriaDescribedbyElements(results);
  if (!results.IsEmpty()) {
    properties.emplace_back(CreateRelatedNodeListProperty(
        AXPropertyNameEnum::Describedby, results,
        html_names::kAriaDescribedbyAttr, ax_object));
  }
  results.clear();

  ax_object.AriaOwnsElements(results);
  if (!results.IsEmpty()) {
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
                          protocol::Array<AXProperty>& properties) {
  ui::AXNodeData node_data;
  ax_object.Serialize(&node_data, ui::kAXModeComplete);

  if (node_data.HasBoolAttribute(ax::mojom::blink::BoolAttribute::kBusy)) {
    const auto is_busy =
        node_data.GetBoolAttribute(ax::mojom::blink::BoolAttribute::kBusy);
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Busy,
                       CreateValue(is_busy, AXValueTypeEnum::Boolean)));
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

  if (node_data.HasIntAttribute(
          ax::mojom::blink::IntAttribute::kErrormessageId)) {
    AXObject* target =
        ax_object.AXObjectCache().ObjectFromAXID(node_data.GetIntAttribute(
            ax::mojom::blink::IntAttribute::kErrormessageId));
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Errormessage, CreateRelatedNodeListValue(*target)));
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

std::unique_ptr<AXValue> CreateRoleNameValue(ax::mojom::Role role) {
  bool is_internal = false;
  const String& role_name = AXObject::RoleName(role, &is_internal);
  const auto& value_type =
      is_internal ? AXValueTypeEnum::InternalRole : AXValueTypeEnum::Role;
  return CreateValue(role_name, value_type);
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

Response InspectorAccessibilityAgent::getPartialAXTree(
    Maybe<int> dom_node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    Maybe<bool> fetch_relatives,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  Node* dom_node = nullptr;
  Response response =
      dom_agent_->AssertNode(dom_node_id, backend_node_id, object_id, dom_node);
  if (!response.IsSuccess())
    return response;

  Document& document = dom_node->GetDocument();
  document.UpdateStyleAndLayout(DocumentUpdateReason::kInspector);
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());
  LocalFrame* local_frame = document.GetFrame();
  if (!local_frame)
    return Response::ServerError("Frame is detached.");

  RetainAXContextForDocument(&document);

  AXContext ax_context(document, ui::kAXModeComplete);
  auto& cache = To<AXObjectCacheImpl>(ax_context.GetAXObjectCache());

  AXObject* inspected_ax_object = cache.GetOrCreate(dom_node);
  *nodes = std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();
  if (!inspected_ax_object || inspected_ax_object->AccessibilityIsIgnored()) {
    (*nodes)->emplace_back(BuildObjectForIgnoredNode(
        dom_node, inspected_ax_object, fetch_relatives.fromMaybe(true), *nodes,
        cache));
    return Response::Success();
  } else {
    (*nodes)->emplace_back(
        BuildProtocolAXObject(*inspected_ax_object, inspected_ax_object,
                              fetch_relatives.fromMaybe(true), *nodes, cache));
  }

  if (!inspected_ax_object)
    return Response::Success();

  AXObject* parent = inspected_ax_object->ParentObjectUnignored();
  if (!parent)
    return Response::Success();

  if (fetch_relatives.fromMaybe(true))
    AddAncestors(*parent, inspected_ax_object, *nodes, cache);

  return Response::Success();
}

void InspectorAccessibilityAgent::AddAncestors(
    AXObject& first_ancestor,
    AXObject* inspected_ax_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject* ancestor = &first_ancestor;
  while (ancestor) {
    std::unique_ptr<AXNode> parent_node_object = BuildProtocolAXObject(
        *ancestor, inspected_ax_object, true, nodes, cache);
    nodes->emplace_back(std::move(parent_node_object));
    ancestor = ancestor->ParentObjectUnignored();
  }
}

std::unique_ptr<AXNode> InspectorAccessibilityAgent::BuildObjectForIgnoredNode(
    Node* dom_node,
    AXObject* ax_object,
    bool fetch_relatives,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject::IgnoredReasons ignored_reasons;
  AXID ax_id = kIDForInspectedNodeWithNoAXNode;
  if (ax_object && ax_object->IsAXLayoutObject())
    ax_id = ax_object->AXObjectID();
  std::unique_ptr<AXNode> ignored_node_object =
      AXNode::create()
          .setNodeId(String::Number(ax_id))
          .setIgnored(true)
          .build();
  ax::mojom::blink::Role role = ax::mojom::blink::Role::kNone;
  ignored_node_object->setRole(CreateRoleNameValue(role));

  if (ax_object && ax_object->IsAXLayoutObject()) {
    ax_object->ComputeAccessibilityIsIgnored(&ignored_reasons);

    AXObject* parent_object = ax_object->ParentObjectUnignored();
    if (parent_object && fetch_relatives)
      AddAncestors(*parent_object, ax_object, nodes, cache);
  } else if (dom_node && !dom_node->GetLayoutObject()) {
    if (fetch_relatives) {
      PopulateDOMNodeAncestors(*dom_node, *(ignored_node_object.get()), nodes,
                               cache);
    }
    ignored_reasons.emplace_back(IgnoredReason(kAXNotRendered));
  }

  if (dom_node) {
    ignored_node_object->setBackendDOMNodeId(
        IdentifiersFactory::IntIdForNode(dom_node));
  }

  auto ignored_reason_properties =
      std::make_unique<protocol::Array<AXProperty>>();
  for (IgnoredReason& reason : ignored_reasons)
    ignored_reason_properties->emplace_back(CreateProperty(reason));
  ignored_node_object->setIgnoredReasons(std::move(ignored_reason_properties));

  return ignored_node_object;
}

void InspectorAccessibilityAgent::PopulateDOMNodeAncestors(
    Node& inspected_dom_node,
    AXNode& node_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  // Walk up parents until an AXObject can be found.
  auto* shadow_root = DynamicTo<ShadowRoot>(inspected_dom_node);
  Node* parent_node = shadow_root
                          ? &shadow_root->host()
                          : FlatTreeTraversal::Parent(inspected_dom_node);
  AXObject* parent_ax_object = cache.GetOrCreate(parent_node);
  while (parent_node && !parent_ax_object) {
    shadow_root = DynamicTo<ShadowRoot>(parent_node);
    parent_node = shadow_root ? &shadow_root->host()
                              : FlatTreeTraversal::Parent(*parent_node);
    parent_ax_object = cache.GetOrCreate(parent_node);
  }

  if (!parent_ax_object)
    return;

  if (parent_ax_object->AccessibilityIsIgnored())
    parent_ax_object = parent_ax_object->ParentObjectUnignored();
  if (!parent_ax_object)
    return;

  std::unique_ptr<AXNode> parent_node_object =
      BuildProtocolAXObject(*parent_ax_object, nullptr, true, nodes, cache);
  auto child_ids = std::make_unique<protocol::Array<AXNodeId>>();
  auto* existing_child_ids = parent_node_object->getChildIds(nullptr);

  // put the Ignored node first regardless of DOM structure
  child_ids->insert(child_ids->begin(),
                    String::Number(kIDForInspectedNodeWithNoAXNode));
  if (existing_child_ids) {
    for (auto id : *existing_child_ids)
      child_ids->push_back(id);
  }

  parent_node_object->setChildIds(std::move(child_ids));
  nodes->emplace_back(std::move(parent_node_object));

  parent_ax_object = parent_ax_object->ParentObjectUnignored();
  if (parent_ax_object) {
    // Populate ancestors.
    AddAncestors(*parent_ax_object, nullptr, nodes, cache);
  }
}

std::unique_ptr<AXNode> InspectorAccessibilityAgent::BuildProtocolAXObject(
    AXObject& ax_object,
    AXObject* inspected_ax_object,
    bool fetch_relatives,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  ax::mojom::Role role = ax_object.RoleValue();
  std::unique_ptr<AXNode> node_object =
      AXNode::create()
          .setNodeId(String::Number(ax_object.AXObjectID()))
          .setIgnored(false)
          .build();
  node_object->setRole(CreateRoleNameValue(role));

  auto properties = std::make_unique<protocol::Array<AXProperty>>();
  FillLiveRegionProperties(ax_object, *(properties.get()));
  FillGlobalStates(ax_object, *(properties.get()));
  FillWidgetProperties(ax_object, *(properties.get()));
  FillWidgetStates(ax_object, *(properties.get()));
  FillRelationships(ax_object, *(properties.get()));
  FillSparseAttributes(ax_object, *(properties.get()));

  AXObject::NameSources name_sources;
  String computed_name = ax_object.GetName(&name_sources);
  if (!name_sources.IsEmpty()) {
    std::unique_ptr<AXValue> name =
        CreateValue(computed_name, AXValueTypeEnum::ComputedString);
    if (!name_sources.IsEmpty()) {
      auto name_source_properties =
          std::make_unique<protocol::Array<AXValueSource>>();
      for (NameSource& name_source : name_sources) {
        name_source_properties->emplace_back(CreateValueSource(name_source));
        if (name_source.text.IsNull() || name_source.superseded)
          continue;
        if (!name_source.related_objects.IsEmpty()) {
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

  FillCoreProperties(ax_object, inspected_ax_object, fetch_relatives,
                     *(node_object.get()), nodes, cache);
  return node_object;
}

LocalFrame* InspectorAccessibilityAgent::FrameFromIdOrRoot(
    const protocol::Maybe<String>& frame_id) {
  if (frame_id.isJust()) {
    return IdentifiersFactory::FrameById(inspected_frames_,
                                         frame_id.fromJust());
  }
  return inspected_frames_->Root();
}

Response InspectorAccessibilityAgent::getFullAXTree(
    protocol::Maybe<int> max_depth,
    Maybe<String> frame_id,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  LocalFrame* frame = FrameFromIdOrRoot(frame_id);
  if (!frame) {
    return Response::InvalidParams(
        "Frame with the given frameId is not found.");
  }

  Document* document = frame->GetDocument();
  if (!document)
    return Response::InternalError();
  if (document->View()->NeedsLayout() || document->NeedsLayoutTreeUpdate())
    document->UpdateStyleAndLayout(DocumentUpdateReason::kInspector);

  *nodes = WalkAXNodesToDepth(document, max_depth.fromMaybe(-1));

  return Response::Success();
}

std::unique_ptr<protocol::Array<AXNode>>
InspectorAccessibilityAgent::WalkAXNodesToDepth(Document* document,
                                                int max_depth) {
  std::unique_ptr<protocol::Array<AXNode>> nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  RetainAXContextForDocument(document);
  AXContext ax_context(*document, ui::kAXModeComplete);
  auto& cache = To<AXObjectCacheImpl>(ax_context.GetAXObjectCache());

  Deque<std::pair<AXID, int>> id_depths;
  id_depths.emplace_back(cache.Root()->AXObjectID(), 0);

  while (!id_depths.empty()) {
    std::pair<AXID, int> id_depth = id_depths.front();
    id_depths.pop_front();
    AXObject* ax_object = cache.ObjectFromAXID(id_depth.first);
    std::unique_ptr<AXNode> node =
        BuildProtocolAXObject(*ax_object, nullptr, false, nodes, cache);

    auto child_ids = std::make_unique<protocol::Array<AXNodeId>>();
    const AXObject::AXObjectVector& children = ax_object->UnignoredChildren();

    for (auto& child_ax_object : children) {
      child_ids->emplace_back(String::Number(child_ax_object->AXObjectID()));

      int depth = id_depth.second;
      if (max_depth == -1 || depth < max_depth)
        id_depths.emplace_back(child_ax_object->AXObjectID(), depth + 1);
    }
    node->setChildIds(std::move(child_ids));
    nodes->emplace_back(std::move(node));
  }

  return nodes;
}

protocol::Response InspectorAccessibilityAgent::getChildAXNodes(
    const String& in_id,
    Maybe<String> frame_id,
    std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*
        out_nodes) {
  if (!enabled_.Get())
    return Response::ServerError("Accessibility has not been enabled.");

  LocalFrame* frame = FrameFromIdOrRoot(frame_id);
  if (!frame) {
    return Response::InvalidParams(
        "Frame with the given frameId is not found.");
  }

  Document* document = frame->GetDocument();
  if (!document)
    return Response::InternalError();

  if (document->View()->NeedsLayout() || document->NeedsLayoutTreeUpdate())
    document->UpdateStyleAndLayout(DocumentUpdateReason::kInspector);

  RetainAXContextForDocument(document);
  AXContext ax_context(*document, ui::kAXModeComplete);

  auto& cache = To<AXObjectCacheImpl>(ax_context.GetAXObjectCache());

  AXID ax_id = in_id.ToUInt();
  AXObject* ax_object = cache.ObjectFromAXID(ax_id);

  if (!ax_object)
    return Response::InvalidParams("Invalid ID");

  *out_nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  const AXObject::AXObjectVector& children = ax_object->UnignoredChildren();
  for (auto& child_ax_object : children) {
    std::unique_ptr<AXNode> child_node = BuildProtocolAXObject(
        *child_ax_object, nullptr, false, *out_nodes, cache);
    auto grandchild_ids = std::make_unique<protocol::Array<AXNodeId>>();
    const AXObject::AXObjectVector& grandchildren =
        child_ax_object->UnignoredChildren();
    for (AXObject* grandchild : grandchildren)
      grandchild_ids->emplace_back(String::Number(grandchild->AXObjectID()));
    child_node->setChildIds(std::move(grandchild_ids));
    (*out_nodes)->emplace_back(std::move(child_node));
  }

  return Response::Success();
}

void InspectorAccessibilityAgent::FillCoreProperties(
    AXObject& ax_object,
    AXObject* inspected_ax_object,
    bool fetch_relatives,
    AXNode& node_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  ax::mojom::NameFrom name_from;
  AXObject::AXObjectVector name_objects;
  ax_object.GetName(name_from, &name_objects);

  ax::mojom::DescriptionFrom description_from;
  AXObject::AXObjectVector description_objects;
  String description =
      ax_object.Description(name_from, description_from, &description_objects);
  if (!description.IsEmpty()) {
    node_object.setDescription(
        CreateValue(description, AXValueTypeEnum::ComputedString));
  }
  // Value.
  if (ax_object.IsRangeValueSupported()) {
    float value;
    if (ax_object.ValueForRange(&value))
      node_object.setValue(CreateValue(value));
  } else {
    String value = ax_object.SlowGetValueForControlIncludingContentEditable();
    if (!value.IsEmpty())
      node_object.setValue(CreateValue(value));
  }

  if (fetch_relatives)
    PopulateRelatives(ax_object, inspected_ax_object, node_object, nodes,
                      cache);

  Node* node = ax_object.GetNode();
  if (node)
    node_object.setBackendDOMNodeId(IdentifiersFactory::IntIdForNode(node));
}

void InspectorAccessibilityAgent::PopulateRelatives(
    AXObject& ax_object,
    AXObject* inspected_ax_object,
    AXNode& node_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject* parent_object = ax_object.ParentObject();
  if (parent_object && parent_object != inspected_ax_object) {
    // Use unignored parent unless parent is inspected ignored object.
    parent_object = ax_object.ParentObjectUnignored();
  }

  auto child_ids = std::make_unique<protocol::Array<AXNodeId>>();

  if (!ax_object.AccessibilityIsIgnored())
    AddChildren(ax_object, inspected_ax_object, child_ids, nodes, cache);

  node_object.setChildIds(std::move(child_ids));
}

void InspectorAccessibilityAgent::AddChildren(
    AXObject& ax_object,
    AXObject* inspected_ax_object,
    std::unique_ptr<protocol::Array<AXNodeId>>& child_ids,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  if (inspected_ax_object && inspected_ax_object->AccessibilityIsIgnored() &&
      &ax_object == inspected_ax_object->ParentObjectUnignored()) {
    child_ids->emplace_back(String::Number(inspected_ax_object->AXObjectID()));
    return;
  }

  const AXObject::AXObjectVector& children = ax_object.UnignoredChildren();
  for (unsigned i = 0; i < children.size(); i++) {
    AXObject& child_ax_object = *children[i].Get();
    child_ids->emplace_back(String::Number(child_ax_object.AXObjectID()));

    if (&child_ax_object == inspected_ax_object)
      continue;

    if (&ax_object != inspected_ax_object) {
      if (!inspected_ax_object)
        continue;

      if (ax_object.ParentObject() != inspected_ax_object ||
          ax_object.GetNode()) {
        continue;
      }
    }

    // Only add children of inspected node (or un-inspectable children of
    // inspected node) to returned nodes.
    std::unique_ptr<AXNode> child_node = BuildProtocolAXObject(
        child_ax_object, inspected_ax_object, true, nodes, cache);
    nodes->emplace_back(std::move(child_node));
  }
}

namespace {

void setNameAndRole(const AXObject& ax_object, std::unique_ptr<AXNode>& node) {
  ax::mojom::blink::Role role = ax_object.RoleValue();
  node->setRole(CreateRoleNameValue(role));
  AXObject::NameSources name_sources;
  String computed_name = ax_object.GetName(&name_sources);
  std::unique_ptr<AXValue> name =
      CreateValue(computed_name, AXValueTypeEnum::ComputedString);
  node->setName(std::move(name));
}

}  // namespace

Response InspectorAccessibilityAgent::queryAXTree(
    Maybe<int> dom_node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    Maybe<String> accessible_name,
    Maybe<String> role,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  Node* root_dom_node = nullptr;
  Response response = dom_agent_->AssertNode(dom_node_id, backend_node_id,
                                             object_id, root_dom_node);
  if (!response.IsSuccess())
    return response;

  // Shadow roots are missing from a11y tree.
  // We start searching the host element instead as a11y tree does not
  // care about shadow roots.
  if (root_dom_node->IsShadowRoot()) {
    root_dom_node = root_dom_node->OwnerShadowHost();
  }
  if (!root_dom_node)
    return Response::InvalidParams("Root DOM node could not be found");
  Document& document = root_dom_node->GetDocument();

  document.UpdateStyleAndLayout(DocumentUpdateReason::kInspector);
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());

  RetainAXContextForDocument(&document);
  AXContext ax_context(document, ui::kAXModeComplete);

  *nodes = std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();
  auto& cache = To<AXObjectCacheImpl>(ax_context.GetAXObjectCache());
  AXObject* root_ax_node = cache.GetOrCreate(root_dom_node);

  const String sought_name = accessible_name.fromMaybe("");

  HeapVector<Member<AXObject>> reachable;
  if (root_ax_node)
    reachable.push_back(root_ax_node);

  while (!reachable.IsEmpty()) {
    AXObject* ax_object = reachable.back();
    reachable.pop_back();
    const AXObject::AXObjectVector& children =
        ax_object->ChildrenIncludingIgnored();
    reachable.AppendRange(children.rbegin(), children.rend());

    // if querying by name: skip if name of current object does not match.
    if (accessible_name.isJust() && sought_name != ax_object->ComputedName())
      continue;

    // if querying by role: skip if role of current object does not match.
    if (role.isJust() &&
        role.fromJust() != AXObject::RoleName(ax_object->RoleValue())) {
      continue;
    }

    // both name and role are OK, so we can add current object to the result.
    if (ax_object->AccessibilityIsIgnored()) {
      Node* dom_node = ax_object->GetNode();
      std::unique_ptr<AXNode> protocol_node =
          BuildObjectForIgnoredNode(dom_node, ax_object, false, *nodes, cache);
      setNameAndRole(*ax_object, protocol_node);
      (*nodes)->push_back(std::move(protocol_node));
    } else {
      (*nodes)->push_back(
          BuildProtocolAXObject(*ax_object, nullptr, false, *nodes, cache));
    }
  }

  return Response::Success();
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
}

protocol::Response InspectorAccessibilityAgent::enable() {
  if (!enabled_.Get())
    EnableAndReset();
  return Response::Success();
}

protocol::Response InspectorAccessibilityAgent::disable() {
  if (!enabled_.Get())
    return Response::Success();
  enabled_.Set(false);
  document_to_context_map_.clear();
  LocalFrame* frame = inspected_frames_->Root();
  DCHECK(EnabledAgents().Contains(frame));
  auto it = EnabledAgents().find(frame);
  it->value->erase(this);
  if (it->value->IsEmpty())
    EnabledAgents().erase(frame);
  return Response::Success();
}

void InspectorAccessibilityAgent::Restore() {
  if (enabled_.Get())
    EnableAndReset();
}

void InspectorAccessibilityAgent::ProvideTo(LocalFrame* frame) {
  if (!EnabledAgents().Contains(frame))
    return;
  for (InspectorAccessibilityAgent* agent : *EnabledAgents().find(frame)->value)
    agent->RetainAXContextForDocument(frame->GetDocument());
}

void InspectorAccessibilityAgent::RetainAXContextForDocument(
    Document* document) {
  if (!enabled_.Get()) {
    return;
  }
  if (!document_to_context_map_.Contains(document)) {
    document_to_context_map_.insert(
        document, std::make_unique<AXContext>(*document, ui::kAXModeComplete));
  }
}

void InspectorAccessibilityAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  visitor->Trace(dom_agent_);
  visitor->Trace(document_to_context_map_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
