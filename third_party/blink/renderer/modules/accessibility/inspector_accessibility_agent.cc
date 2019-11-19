// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"

#include <memory>
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
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

namespace blink {

using protocol::Accessibility::AXPropertyName;
using protocol::Accessibility::AXNode;
using protocol::Accessibility::AXNodeId;
using protocol::Accessibility::AXProperty;
using protocol::Accessibility::AXValueSource;
using protocol::Accessibility::AXValueType;
using protocol::Accessibility::AXRelatedNode;
using protocol::Accessibility::AXValue;
using protocol::Maybe;
using protocol::Response;
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

  AddHasPopupProperty(ax_object.HasPopup(), properties);

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

  if (ax_object.IsRange()) {
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

    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Valuetext,
                       CreateValue(ax_object.ValueDescription())));
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

class SparseAttributeAXPropertyAdapter
    : public GarbageCollected<SparseAttributeAXPropertyAdapter>,
      public AXSparseAttributeClient {
 public:
  SparseAttributeAXPropertyAdapter(AXObject& ax_object,
                                   protocol::Array<AXProperty>& properties)
      : ax_object_(&ax_object), properties_(properties) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(ax_object_); }

 private:
  Member<AXObject> ax_object_;
  protocol::Array<AXProperty>& properties_;

  void AddBoolAttribute(AXBoolAttribute attribute, bool value) override {
    switch (attribute) {
      case AXBoolAttribute::kAriaBusy:
        properties_.emplace_back(
            CreateProperty(AXPropertyNameEnum::Busy,
                           CreateValue(value, AXValueTypeEnum::Boolean)));
        break;
    }
  }

  void AddIntAttribute(AXIntAttribute attribute, int32_t value) override {}

  void AddUIntAttribute(AXUIntAttribute attribute, uint32_t value) override {}

  void AddStringAttribute(AXStringAttribute attribute,
                          const String& value) override {
    switch (attribute) {
      case AXStringAttribute::kAriaKeyShortcuts:
        properties_.emplace_back(
            CreateProperty(AXPropertyNameEnum::Keyshortcuts,
                           CreateValue(value, AXValueTypeEnum::String)));
        break;
      case AXStringAttribute::kAriaRoleDescription:
        properties_.emplace_back(
            CreateProperty(AXPropertyNameEnum::Roledescription,
                           CreateValue(value, AXValueTypeEnum::String)));
        break;
    }
  }

  void AddObjectAttribute(AXObjectAttribute attribute,
                          AXObject& object) override {
    switch (attribute) {
      case AXObjectAttribute::kAriaActiveDescendant:
        properties_.emplace_back(
            CreateProperty(AXPropertyNameEnum::Activedescendant,
                           CreateRelatedNodeListValue(object)));
        break;
      case AXObjectAttribute::kAriaDetails:
        properties_.emplace_back(CreateProperty(
            AXPropertyNameEnum::Details, CreateRelatedNodeListValue(object)));
        break;
      case AXObjectAttribute::kAriaErrorMessage:
        properties_.emplace_back(
            CreateProperty(AXPropertyNameEnum::Errormessage,
                           CreateRelatedNodeListValue(object)));
        break;
    }
  }

  void AddObjectVectorAttribute(
      AXObjectVectorAttribute attribute,
      HeapVector<Member<AXObject>>& objects) override {
    switch (attribute) {
      case AXObjectVectorAttribute::kAriaControls:
        properties_.emplace_back(CreateRelatedNodeListProperty(
            AXPropertyNameEnum::Controls, objects,
            html_names::kAriaControlsAttr, *ax_object_));
        break;
      case AXObjectVectorAttribute::kAriaFlowTo:
        properties_.emplace_back(CreateRelatedNodeListProperty(
            AXPropertyNameEnum::Flowto, objects, html_names::kAriaFlowtoAttr,
            *ax_object_));
        break;
    }
  }
};

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

std::unique_ptr<AXValue> CreateRoleNameValue(ax::mojom::Role role) {
  AtomicString role_name = AXObject::RoleName(role);
  std::unique_ptr<AXValue> role_name_value;
  if (!role_name.IsNull()) {
    role_name_value = CreateValue(role_name, AXValueTypeEnum::Role);
  } else {
    role_name_value = CreateValue(AXObject::InternalRoleName(role),
                                  AXValueTypeEnum::InternalRole);
  }
  return role_name_value;
}

}  // namespace

using EnabledAgentsMultimap =
    HeapHashMap<WeakMember<LocalFrame>,
                HeapHashSet<Member<InspectorAccessibilityAgent>>>;

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
  if (!response.isSuccess())
    return response;

  Document& document = dom_node->GetDocument();
  document.UpdateStyleAndLayout();
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());
  LocalFrame* local_frame = document.GetFrame();
  if (!local_frame)
    return Response::Error("Frame is detached.");
  AXContext ax_context(document);
  AXObjectCacheImpl& cache = ToAXObjectCacheImpl(ax_context.GetAXObjectCache());

  AXObject* inspected_ax_object = cache.GetOrCreate(dom_node);
  *nodes = std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();
  if (!inspected_ax_object || inspected_ax_object->AccessibilityIsIgnored()) {
    (*nodes)->emplace_back(BuildObjectForIgnoredNode(
        dom_node, inspected_ax_object, fetch_relatives.fromMaybe(true), *nodes,
        cache));
    return Response::OK();
  } else {
    (*nodes)->emplace_back(
        BuildProtocolAXObject(*inspected_ax_object, inspected_ax_object,
                              fetch_relatives.fromMaybe(true), *nodes, cache));
  }

  if (!inspected_ax_object)
    return Response::OK();

  AXObject* parent = inspected_ax_object->ParentObjectUnignored();
  if (!parent)
    return Response::OK();

  if (fetch_relatives.fromMaybe(true))
    AddAncestors(*parent, inspected_ax_object, *nodes, cache);

  return Response::OK();
}

void InspectorAccessibilityAgent::AddAncestors(
    AXObject& first_ancestor,
    AXObject* inspected_ax_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject* ancestor = &first_ancestor;
  AXObject* child = inspected_ax_object;
  while (ancestor) {
    std::unique_ptr<AXNode> parent_node_object = BuildProtocolAXObject(
        *ancestor, inspected_ax_object, true, nodes, cache);
    auto child_ids = std::make_unique<protocol::Array<AXNodeId>>();
    if (child)
      child_ids->emplace_back(String::Number(child->AXObjectID()));
    else
      child_ids->emplace_back(String::Number(kIDForInspectedNodeWithNoAXNode));
    parent_node_object->setChildIds(std::move(child_ids));
    nodes->emplace_back(std::move(parent_node_object));
    child = ancestor;
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
  ax::mojom::Role role = ax::mojom::Role::kIgnored;
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

  // Populate parent and ancestors.
  AddAncestors(*parent_ax_object, nullptr, nodes, cache);
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

  SparseAttributeAXPropertyAdapter adapter(ax_object, *properties);
  ax_object.GetSparseAXAttributes(adapter);

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

Response InspectorAccessibilityAgent::getFullAXTree(
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  Document* document = inspected_frames_->Root()->GetDocument();
  if (!document)
    return Response::Error("No document.");
  if (document->View()->NeedsLayout() || document->NeedsLayoutTreeUpdate())
    document->UpdateStyleAndLayout();
  *nodes = std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();
  AXContext ax_context(*document);
  AXObjectCacheImpl& cache = ToAXObjectCacheImpl(ax_context.GetAXObjectCache());
  Deque<AXID> ids;
  ids.emplace_back(cache.Root()->AXObjectID());
  while (!ids.empty()) {
    AXID ax_id = ids.front();
    ids.pop_front();
    AXObject* ax_object = cache.ObjectFromAXID(ax_id);
    std::unique_ptr<AXNode> node =
        BuildProtocolAXObject(*ax_object, nullptr, false, *nodes, cache);

    auto child_ids = std::make_unique<protocol::Array<AXNodeId>>();
    const AXObject::AXObjectVector& children = ax_object->Children();
    for (unsigned i = 0; i < children.size(); i++) {
      AXObject& child_ax_object = *children[i].Get();
      child_ids->emplace_back(String::Number(child_ax_object.AXObjectID()));
      ids.emplace_back(child_ax_object.AXObjectID());
    }
    node->setChildIds(std::move(child_ids));
    (*nodes)->emplace_back(std::move(node));
  }
  return Response::OK();
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
  if (ax_object.SupportsRangeValue()) {
    float value;
    if (ax_object.ValueForRange(&value))
      node_object.setValue(CreateValue(value));
  } else {
    String string_value = ax_object.StringValue();
    if (!string_value.IsEmpty())
      node_object.setValue(CreateValue(string_value));
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

  const AXObject::AXObjectVector& children = ax_object.Children();
  for (unsigned i = 0; i < children.size(); i++) {
    AXObject& child_ax_object = *children[i].Get();
    child_ids->emplace_back(String::Number(child_ax_object.AXObjectID()));
    if (&child_ax_object == inspected_ax_object)
      continue;
    if (&ax_object != inspected_ax_object) {
      if (!inspected_ax_object)
        continue;
      if (&ax_object != inspected_ax_object->ParentObjectUnignored() &&
          ax_object.GetNode())
        continue;
    }

    // Only add children of inspected node (or un-inspectable children of
    // inspected node) to returned nodes.
    std::unique_ptr<AXNode> child_node = BuildProtocolAXObject(
        child_ax_object, inspected_ax_object, true, nodes, cache);
    nodes->emplace_back(std::move(child_node));
  }
}

void InspectorAccessibilityAgent::EnableAndReset() {
  enabled_.Set(true);
  LocalFrame* frame = inspected_frames_->Root();
  if (!EnabledAgents().Contains(frame)) {
    EnabledAgents().Set(frame,
                        HeapHashSet<Member<InspectorAccessibilityAgent>>());
  }
  EnabledAgents().find(frame)->value.insert(this);
  CreateAXContext();
}

protocol::Response InspectorAccessibilityAgent::enable() {
  if (!enabled_.Get())
    EnableAndReset();
  return Response::OK();
}

protocol::Response InspectorAccessibilityAgent::disable() {
  if (!enabled_.Get())
    return Response::OK();
  enabled_.Set(false);
  context_ = nullptr;
  LocalFrame* frame = inspected_frames_->Root();
  DCHECK(EnabledAgents().Contains(frame));
  auto it = EnabledAgents().find(frame);
  it->value.erase(this);
  if (it->value.IsEmpty())
    EnabledAgents().erase(frame);
  return Response::OK();
}

void InspectorAccessibilityAgent::Restore() {
  if (enabled_.Get())
    EnableAndReset();
}

void InspectorAccessibilityAgent::ProvideTo(LocalFrame* frame) {
  if (!EnabledAgents().Contains(frame))
    return;
  for (InspectorAccessibilityAgent* agent : EnabledAgents().find(frame)->value)
    agent->CreateAXContext();
}

void InspectorAccessibilityAgent::CreateAXContext() {
  Document* document = inspected_frames_->Root()->GetDocument();
  if (document)
    context_ = std::make_unique<AXContext>(*document);
}

void InspectorAccessibilityAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(dom_agent_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
