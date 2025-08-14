// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/inspector_type_builder_helper.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/protocol/accessibility.h"
#include "third_party/blink/renderer/modules/accessibility/ax_enums.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object-inl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

using protocol::Accessibility::AXProperty;
using protocol::Accessibility::AXRelatedNode;
using protocol::Accessibility::AXValue;
using protocol::Accessibility::AXValueSource;
namespace AXPropertyNameEnum = protocol::Accessibility::AXPropertyNameEnum;
namespace AXValueTypeEnum = protocol::Accessibility::AXValueTypeEnum;

namespace {

std::unique_ptr<AXProperty> CreateProperty(const String& name,
                                           std::unique_ptr<AXValue> value) {
  return AXProperty::create().setName(name).setValue(std::move(value)).build();
}

std::unique_ptr<AXValue> CreateValue(
    const String& value,
    const String& type = AXValueTypeEnum::String) {
  return AXValue::create()
      .setType(type)
      .setValue(protocol::ValueConversions<String>::toValue(value))
      .build();
}

std::unique_ptr<AXValue> CreateValue(
    int value,
    const String& type = AXValueTypeEnum::Integer) {
  return AXValue::create()
      .setType(type)
      .setValue(protocol::ValueConversions<int>::toValue(value))
      .build();
}

std::unique_ptr<AXValue> CreateValue(
    float value,
    const String& type = AXValueTypeEnum::Number) {
  return AXValue::create()
      .setType(type)
      .setValue(protocol::ValueConversions<double>::toValue(value))
      .build();
}

std::unique_ptr<AXValue> CreateBooleanValue(
    bool value,
    const String& type = AXValueTypeEnum::Boolean) {
  return AXValue::create()
      .setType(type)
      .setValue(protocol::ValueConversions<bool>::toValue(value))
      .build();
}

std::unique_ptr<AXRelatedNode> RelatedNodeForAXObject(const AXObject& ax_object,
                                                      String* name = nullptr) {
  Node* node = ax_object.GetNode();
  if (!node)
    return nullptr;
  int backend_node_id = IdentifiersFactory::IntIdForNode(node);
  if (!backend_node_id)
    return nullptr;
  std::unique_ptr<AXRelatedNode> related_node =
      AXRelatedNode::create().setBackendDOMNodeId(backend_node_id).build();
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return related_node;

  String idref = element->GetIdAttribute();
  if (!idref.empty())
    related_node->setIdref(idref);

  if (name)
    related_node->setText(*name);
  return related_node;
}

std::unique_ptr<AXValue> CreateRelatedNodeListValue(
    const AXObject& ax_object,
    String* name = nullptr,
    const String& value_type = AXValueTypeEnum::Idref) {
  auto related_nodes = std::make_unique<protocol::Array<AXRelatedNode>>();
  std::unique_ptr<AXRelatedNode> related_node =
      RelatedNodeForAXObject(ax_object, name);
  if (related_node)
    related_nodes->emplace_back(std::move(related_node));
  return AXValue::create()
      .setType(value_type)
      .setRelatedNodes(std::move(related_nodes))
      .build();
}

std::unique_ptr<AXProperty> CreateProperty(IgnoredReason reason) {
  if (reason.related_object) {
    return CreateProperty(
        IgnoredReasonName(reason.reason),
        CreateRelatedNodeListValue(*(reason.related_object), nullptr,
                                   AXValueTypeEnum::Idref));
  }
  return CreateProperty(IgnoredReasonName(reason.reason),
                        CreateBooleanValue(true));
}

std::unique_ptr<AXValue> CreateRelatedNodeListValue(
    AXRelatedObjectVector& related_objects,
    const String& value_type) {
  auto frontend_related_nodes =
      std::make_unique<protocol::Array<AXRelatedNode>>();
  for (unsigned i = 0; i < related_objects.size(); i++) {
    std::unique_ptr<AXRelatedNode> frontend_related_node =
        RelatedNodeForAXObject(*(related_objects[i]->object),
                               &(related_objects[i]->text));
    if (frontend_related_node)
      frontend_related_nodes->emplace_back(std::move(frontend_related_node));
  }
  return AXValue::create()
      .setType(value_type)
      .setRelatedNodes(std::move(frontend_related_nodes))
      .build();
}

std::unique_ptr<AXValue> CreateRelatedNodeListValue(
    AXObject::AXObjectVector& ax_objects,
    const String& value_type = AXValueTypeEnum::IdrefList) {
  auto related_nodes = std::make_unique<protocol::Array<AXRelatedNode>>();
  for (unsigned i = 0; i < ax_objects.size(); i++) {
    std::unique_ptr<AXRelatedNode> related_node =
        RelatedNodeForAXObject(*(ax_objects[i].Get()));
    if (related_node)
      related_nodes->emplace_back(std::move(related_node));
  }
  return AXValue::create()
      .setType(value_type)
      .setRelatedNodes(std::move(related_nodes))
      .build();
}

String ValueSourceType(ax::mojom::blink::NameFrom name_from) {
  namespace SourceType = protocol::Accessibility::AXValueSourceTypeEnum;

  switch (name_from) {
    case ax::mojom::blink::NameFrom::kAttribute:
    case ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty:
    case ax::mojom::blink::NameFrom::kTitle:
    case ax::mojom::blink::NameFrom::kValue:
      return SourceType::Attribute;
    case ax::mojom::blink::NameFrom::kContents:
      return SourceType::Contents;
    case ax::mojom::blink::NameFrom::kPlaceholder:
      return SourceType::Placeholder;
    case ax::mojom::blink::NameFrom::kCaption:
    case ax::mojom::blink::NameFrom::kRelatedElement:
      return SourceType::RelatedElement;
    default:
      return SourceType::Implicit;  // TODO(aboxhall): what to do here?
  }
}

String NativeSourceType(AXTextSource native_source) {
  namespace SourceType = protocol::Accessibility::AXValueNativeSourceTypeEnum;

  switch (native_source) {
    case kAXTextFromNativeSVGDescElement:
      return SourceType::Description;
    case kAXTextFromNativeHTMLLabel:
      return SourceType::Label;
    case kAXTextFromNativeHTMLLabelFor:
      return SourceType::Labelfor;
    case kAXTextFromNativeHTMLLabelWrapped:
      return SourceType::Labelwrapped;
    case kAXTextFromNativeHTMLRubyAnnotation:
      return SourceType::Rubyannotation;
    case kAXTextFromNativeHTMLTableCaption:
      return SourceType::Tablecaption;
    case kAXTextFromNativeHTMLLegend:
      return SourceType::Legend;
    case kAXTextFromNativeTitleElement:
      return SourceType::Title;
    default:
      return SourceType::Other;
  }
}

std::unique_ptr<AXValueSource> CreateValueSource(NameSource& name_source) {
  String type = ValueSourceType(name_source.type);
  std::unique_ptr<AXValueSource> value_source =
      AXValueSource::create().setType(type).build();
  if (!name_source.related_objects.empty()) {
    if ((*name_source.attribute) == html_names::kAriaLabelledbyAttr ||
        (*name_source.attribute) == html_names::kAriaLabeledbyAttr) {
      std::unique_ptr<AXValue> attribute_value = CreateRelatedNodeListValue(
          name_source.related_objects, AXValueTypeEnum::IdrefList);
      if (!name_source.attribute_value.IsNull())
        attribute_value->setValue(protocol::StringValue::create(
            name_source.attribute_value.GetString()));
      value_source->setAttributeValue(std::move(attribute_value));
    } else if ((*name_source.attribute) == QualifiedName::Null()) {
      value_source->setNativeSourceValue(CreateRelatedNodeListValue(
          name_source.related_objects, AXValueTypeEnum::NodeList));
    }
  } else if (!name_source.attribute_value.IsNull()) {
    value_source->setAttributeValue(CreateValue(name_source.attribute_value));
  }
  if (!name_source.text.IsNull())
    value_source->setValue(
        CreateValue(name_source.text, AXValueTypeEnum::ComputedString));
  if ((*name_source.attribute) != QualifiedName::Null()) {
    value_source->setAttribute(name_source.attribute->LocalName().GetString());
  }
  if (name_source.superseded)
    value_source->setSuperseded(true);
  if (name_source.invalid)
    value_source->setInvalid(true);
  if (name_source.native_source != kAXTextFromNativeSourceUninitialized)
    value_source->setNativeSource(NativeSourceType(name_source.native_source));
  return value_source;
}

// Creates a role name value that is easy to read by developers. This function
// reduces the granularity of the role and uses ARIA role strings when possible.
std::unique_ptr<AXValue> CreateRoleNameValue(ax::mojom::blink::Role role) {
  bool is_internal = false;
  const String& role_name = AXObject::RoleName(role, &is_internal);
  const auto& value_type =
      is_internal ? AXValueTypeEnum::InternalRole : AXValueTypeEnum::Role;
  return CreateValue(role_name, value_type);
}

// Creates an integer role value that is fixed over releases, is not lossy, and
// is more suitable for machine learning models or automation.
std::unique_ptr<AXValue> CreateInternalRoleValue(ax::mojom::blink::Role role) {
  return CreateValue(static_cast<int>(role), AXValueTypeEnum::InternalRole);
}

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
  if (!node_data.IsActiveLiveRegionRoot()) {
    return;
  }

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

bool RoleAllowsMultiselectable(ax::mojom::blink::Role role) {
  return role == ax::mojom::blink::Role::kGrid ||
         role == ax::mojom::blink::Role::kListBox ||
         role == ax::mojom::blink::Role::kTabList ||
         role == ax::mojom::blink::Role::kTreeGrid ||
         role == ax::mojom::blink::Role::kTree;
}

bool RoleAllowsReadonly(ax::mojom::blink::Role role) {
  return role == ax::mojom::blink::Role::kGrid ||
         role == ax::mojom::blink::Role::kGridCell ||
         role == ax::mojom::blink::Role::kTextField ||
         role == ax::mojom::blink::Role::kColumnHeader ||
         role == ax::mojom::blink::Role::kRowHeader ||
         role == ax::mojom::blink::Role::kTreeGrid;
}

bool RoleAllowsRequired(ax::mojom::blink::Role role) {
  return role == ax::mojom::blink::Role::kComboBoxGrouping ||
         role == ax::mojom::blink::Role::kComboBoxMenuButton ||
         role == ax::mojom::blink::Role::kGridCell ||
         role == ax::mojom::blink::Role::kListBox ||
         role == ax::mojom::blink::Role::kRadioGroup ||
         role == ax::mojom::blink::Role::kSpinButton ||
         role == ax::mojom::blink::Role::kTextField ||
         role == ax::mojom::blink::Role::kTextFieldWithComboBox ||
         role == ax::mojom::blink::Role::kTree ||
         role == ax::mojom::blink::Role::kColumnHeader ||
         role == ax::mojom::blink::Role::kRowHeader ||
         role == ax::mojom::blink::Role::kTreeGrid;
}

void FillWidgetProperties(AXObject& ax_object,
                          const ui::AXNodeData& node_data,
                          protocol::Array<AXProperty>& properties) {
  ax::mojom::blink::Role role = node_data.role;
  const String& autocomplete =
      node_data
          .GetStringAttribute(ax::mojom::blink::StringAttribute::kAutoComplete)
          .c_str();
  if (!autocomplete.empty()) {
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Autocomplete,
                       CreateValue(autocomplete, AXValueTypeEnum::Token)));
  }

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

  if (role == ax::mojom::blink::Role::kTextField) {
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
    case ax::mojom::blink::CheckedState::kTrue:
      checked_prop_val = "true";
      break;
    case ax::mojom::blink::CheckedState::kMixed:
      checked_prop_val = "mixed";
      break;
    case ax::mojom::blink::CheckedState::kFalse:
      checked_prop_val = "false";
      break;
    case ax::mojom::blink::CheckedState::kNone:
      break;
  }
  if (checked_prop_val) {
    auto* const checked_prop_name =
        role == ax::mojom::blink::Role::kToggleButton
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
  GCedHeapVector<Member<Element>>* elements =
      ax_object.GetElement()->GetAttrAssociatedElements(attribute);
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
    if (!ax_object) {
      continue;
    }
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
        CreateValue(String(url.c_str()), AXValueTypeEnum::String)));
  }

  if (node_data.HasStringAttribute(
          ax::mojom::blink::StringAttribute::kKeyShortcuts)) {
    const auto key_shortcuts = node_data.GetStringAttribute(
        ax::mojom::blink::StringAttribute::kKeyShortcuts);
    properties.emplace_back(CreateProperty(
        AXPropertyNameEnum::Keyshortcuts,
        CreateValue(String(key_shortcuts.c_str()), AXValueTypeEnum::String)));
  }

  if (node_data.HasStringAttribute(
          ax::mojom::blink::StringAttribute::kRoleDescription)) {
    const auto role_description = node_data.GetStringAttribute(
        ax::mojom::blink::StringAttribute::kRoleDescription);
    properties.emplace_back(
        CreateProperty(AXPropertyNameEnum::Roledescription,
                       CreateValue(String(role_description.c_str()),
                                   AXValueTypeEnum::String)));
  }

  if (node_data.HasIntListAttribute(
          ax::mojom::blink::IntListAttribute::kActionsIds)) {
    const auto ax_ids = node_data.GetIntListAttribute(
        ax::mojom::blink::IntListAttribute::kActionsIds);
    AXObject::AXObjectVector ax_objects;
    GetObjectsFromAXIDs(ax_object.AXObjectCache(), ax_ids, &ax_objects);
    properties.emplace_back(
        CreateRelatedNodeListProperty(AXPropertyNameEnum::Actions, ax_objects,
                                      html_names::kAriaActionsAttr, ax_object));
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

void FillCoreProperties(AXObject& ax_object, AXNode* node_object) {
  ax::mojom::blink::NameFrom name_from;
  AXObject::AXObjectVector name_objects;
  ax_object.GetName(name_from, &name_objects, /*name_sources=*/nullptr);

  ax::mojom::blink::DescriptionFrom description_from;
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
    if (ax_object.ValueForRange(&value)) {
      node_object->setValue(CreateValue(value));
    }
  } else {
    String value = ax_object.SlowGetValueForControlIncludingContentEditable();
    if (!value.empty()) {
      node_object->setValue(CreateValue(value));
    }
  }
}

}  // namespace

std::unique_ptr<AXNode> BuildProtocolAXNodeForDOMNodeWithNoAXNode(
    int backend_node_id) {
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

std::unique_ptr<AXNode> BuildProtocolAXNodeForAXObject(
    AXObject& ax_object,
    bool force_name_and_role) {
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
  for (AXObject* child : children) {
    child_ids->emplace_back(String::Number(child->AXObjectID()));
  }
  protocol_node->setChildIds(std::move(child_ids));

  Node* node = ax_object.GetNode();
  if (node) {
    protocol_node->setBackendDOMNodeId(IdentifiersFactory::IntIdForNode(node));
  }

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

std::unique_ptr<AXNode> BuildProtocolAXNodeForIgnoredAXObject(
    AXObject& ax_object,
    bool force_name_and_role) {
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
    ax::mojom::blink::NameFrom name_from;
    AXObject::NameSources name_sources;
    String computed_name =
        ax_object.GetName(name_from, /*name_objects=*/nullptr, &name_sources);
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
  for (IgnoredReason& reason : ignored_reasons) {
    ignored_reason_properties->emplace_back(CreateProperty(reason));
  }
  ignored_node_object->setIgnoredReasons(std::move(ignored_reason_properties));

  return ignored_node_object;
}

std::unique_ptr<AXNode> BuildProtocolAXNodeForUnignoredAXObject(
    AXObject& ax_object) {
  std::unique_ptr<AXNode> node_object =
      AXNode::create()
          .setNodeId(String::Number(ax_object.AXObjectID()))
          .setIgnored(false)
          .build();
  auto properties = std::make_unique<protocol::Array<AXProperty>>();
  ui::AXNodeData node_data;
  ax_object.Serialize(&node_data, ui::kAXModeInspector);
  node_object->setRole(CreateRoleNameValue(node_data.role));
  node_object->setChromeRole(CreateInternalRoleValue(node_data.role));
  FillLiveRegionProperties(ax_object, node_data, *(properties.get()));
  FillGlobalStates(ax_object, node_data, *(properties.get()));
  FillWidgetProperties(ax_object, node_data, *(properties.get()));
  FillWidgetStates(ax_object, node_data, *(properties.get()));
  FillRelationships(ax_object, *(properties.get()));
  FillSparseAttributes(ax_object, node_data, *(properties.get()));

  ax::mojom::blink::NameFrom name_from;
  AXObject::NameSources name_sources;
  String computed_name =
      ax_object.GetName(name_from, /*name_objects=*/nullptr, &name_sources);
  std::unique_ptr<AXValue> name =
      CreateValue(computed_name, AXValueTypeEnum::ComputedString);
  if (!name_sources.empty()) {
    auto name_source_properties =
        std::make_unique<protocol::Array<AXValueSource>>();
    for (NameSource& name_source : name_sources) {
      name_source_properties->emplace_back(CreateValueSource(name_source));
      if (name_source.text.IsNull() || name_source.superseded) {
        continue;
      }
      if (!name_source.related_objects.empty()) {
        properties->emplace_back(CreateRelatedNodeListProperty(
            AXPropertyNameEnum::Labelledby, name_source.related_objects));
      }
    }
    name->setSources(std::move(name_source_properties));
  }
  node_object->setProperties(std::move(properties));
  node_object->setName(std::move(name));

  FillCoreProperties(ax_object, node_object.get());
  return node_object;
}

}  // namespace blink
