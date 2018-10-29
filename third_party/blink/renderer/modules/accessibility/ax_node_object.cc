/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"

#include <math.h>

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/forms/radio_input_type.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_dlist_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_table_caption_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using namespace HTMLNames;

// In ARIA 1.1, default value of aria-level was changed to 2.
const int kDefaultHeadingLevel = 2;

AXNodeObject::AXNodeObject(Node* node, AXObjectCacheImpl& ax_object_cache)
    : AXObject(ax_object_cache),
      children_dirty_(false),
      native_role_(ax::mojom::Role::kUnknown),
      node_(node) {}

AXNodeObject* AXNodeObject::Create(Node* node,
                                   AXObjectCacheImpl& ax_object_cache) {
  return new AXNodeObject(node, ax_object_cache);
}

AXNodeObject::~AXNodeObject() {
  DCHECK(!node_);
}

void AXNodeObject::AlterSliderOrSpinButtonValue(bool increase) {
  if (!IsSlider() && !IsSpinButton())
    return;

  float value;
  if (!ValueForRange(&value))
    return;

  float step;
  StepValueForRange(&step);

  value += increase ? step : -step;

  OnNativeSetValueAction(String::Number(value));
  AXObjectCache().PostNotification(GetNode(), ax::mojom::Event::kValueChanged);
}

AXObject* AXNodeObject::ActiveDescendant() {
  Element* element = GetElement();
  if (!element)
    return nullptr;

  Element* descendant =
      GetAOMPropertyOrARIAAttribute(AOMRelationProperty::kActiveDescendant);
  if (!descendant)
    return nullptr;

  AXObject* ax_descendant = AXObjectCache().GetOrCreate(descendant);
  return ax_descendant;
}

bool AXNodeObject::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
#if DCHECK_IS_ON()
  // Double-check that an AXObject is never accessed before
  // it's been initialized.
  DCHECK(initialized_);
#endif

  // If this element is within a parent that cannot have children, it should not
  // be exposed.
  if (IsDescendantOfLeafNode()) {
    if (ignored_reasons)
      ignored_reasons->push_back(
          IgnoredReason(kAXAncestorIsLeafNode, LeafNodeAncestor()));
    return true;
  }

  // Ignore labels that are already referenced by a control.
  AXObject* control_object = CorrespondingControlForLabelElement();
  if (control_object && control_object->IsCheckboxOrRadio() &&
      control_object->NameFromLabelElement()) {
    if (ignored_reasons) {
      HTMLLabelElement* label = LabelElementContainer();
      if (label && label != GetNode()) {
        AXObject* label_ax_object = AXObjectCache().GetOrCreate(label);
        ignored_reasons->push_back(
            IgnoredReason(kAXLabelContainer, label_ax_object));
      }

      ignored_reasons->push_back(IgnoredReason(kAXLabelFor, control_object));
    }
    return true;
  }

  Element* element = GetNode()->IsElementNode() ? ToElement(GetNode())
                                                : GetNode()->parentElement();
  if (!GetLayoutObject() && (!element || !element->IsInCanvasSubtree()) &&
      !AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty::kHidden)) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXNotRendered));
    return true;
  }

  if (role_ == ax::mojom::Role::kUnknown) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return true;
  }
  return false;
}

static bool IsListElement(Node* node) {
  return IsHTMLUListElement(*node) || IsHTMLOListElement(*node) ||
         IsHTMLDListElement(*node);
}

static bool IsRequiredOwnedElement(AXObject* parent,
                                   ax::mojom::Role current_role,
                                   HTMLElement* current_element) {
  Node* parent_node = parent->GetNode();
  if (!parent_node || !parent_node->IsHTMLElement())
    return false;

  if (current_role == ax::mojom::Role::kListItem)
    return IsListElement(parent_node);
  if (current_role == ax::mojom::Role::kListMarker)
    return IsHTMLLIElement(*parent_node);
  if (current_role == ax::mojom::Role::kMenuItemCheckBox ||
      current_role == ax::mojom::Role::kMenuItem ||
      current_role == ax::mojom::Role::kMenuItemRadio)
    return IsHTMLMenuElement(*parent_node);

  if (!current_element)
    return false;
  if (IsHTMLTableCellElement(*current_element))
    return IsHTMLTableRowElement(*parent_node);
  if (IsHTMLTableRowElement(*current_element))
    return IsHTMLTableSectionElement(ToHTMLElement(*parent_node));

  // In case of ListboxRole and its child, ListBoxOptionRole, inheritance of
  // presentation role is handled in AXListBoxOption because ListBoxOption Role
  // doesn't have any child.
  // If it's just ignored because of presentation, we can't see any AX tree
  // related to ListBoxOption.
  return false;
}

const AXObject* AXNodeObject::InheritsPresentationalRoleFrom() const {
  // ARIA states if an item can get focus, it should not be presentational.
  if (CanSetFocusAttribute())
    return nullptr;

  if (IsPresentational())
    return this;

  // http://www.w3.org/TR/wai-aria/complete#presentation
  // ARIA spec says that the user agent MUST apply an inherited role of
  // presentation
  // to any owned elements that do not have an explicit role defined.
  if (AriaRoleAttribute() != ax::mojom::Role::kUnknown)
    return nullptr;

  AXObject* parent = ParentObject();
  if (!parent)
    return nullptr;

  HTMLElement* element = nullptr;
  if (GetNode() && GetNode()->IsHTMLElement())
    element = ToHTMLElement(GetNode());
  if (!parent->HasInheritedPresentationalRole())
    return nullptr;

  // ARIA spec says that when a parent object is presentational and this object
  // is a required owned element of that parent, then this object is also
  // presentational.
  if (IsRequiredOwnedElement(parent, RoleValue(), element))
    return parent;
  return nullptr;
}

// There should only be one banner/contentInfo per page. If header/footer are
// being used within an article, aside, nave, section, blockquote, details,
// fieldset, figure, td, or main, then it should not be exposed as whole
// page's banner/contentInfo.
static HashSet<QualifiedName>& GetLandmarkRolesNotAllowed() {
  DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, landmark_roles_not_allowed, ());
  if (landmark_roles_not_allowed.IsEmpty()) {
    landmark_roles_not_allowed.insert(articleTag);
    landmark_roles_not_allowed.insert(asideTag);
    landmark_roles_not_allowed.insert(navTag);
    landmark_roles_not_allowed.insert(sectionTag);
    landmark_roles_not_allowed.insert(blockquoteTag);
    landmark_roles_not_allowed.insert(detailsTag);
    landmark_roles_not_allowed.insert(fieldsetTag);
    landmark_roles_not_allowed.insert(figureTag);
    landmark_roles_not_allowed.insert(tdTag);
    landmark_roles_not_allowed.insert(mainTag);
  }
  return landmark_roles_not_allowed;
}

bool AXNodeObject::IsDescendantOfElementType(
    HashSet<QualifiedName>& tag_names) const {
  if (!GetNode())
    return false;

  for (Element* parent = GetNode()->parentElement(); parent;
       parent = parent->parentElement()) {
    if (tag_names.Contains(parent->TagQName()))
      return true;
  }
  return false;
}

// TODO(accessibility) Needs a new name as it does check ARIA, including
// checking the @role for an iframe, and @aria-haspopup/aria-pressed via
// ButtonType().
// TODO(accessibility) This value is cached in native_role_ so it needs to
// be recached if anything it depends on change, such as IsClickable(),
// DataList(), aria-pressed, the parent's tag, role on an iframe, etc.
ax::mojom::Role AXNodeObject::NativeRoleIgnoringAria() const {
  if (!GetNode())
    return ax::mojom::Role::kUnknown;

  // |HTMLAnchorElement| sets isLink only when it has hrefAttr.
  if (GetNode()->IsLink())
    return ax::mojom::Role::kLink;

  if (IsHTMLAnchorElement(*GetNode())) {
    // We assume that an anchor element is LinkRole if it has event listners
    // even though it doesn't have hrefAttr.
    if (IsClickable())
      return ax::mojom::Role::kLink;
    return ax::mojom::Role::kAnchor;
  }

  if (IsHTMLButtonElement(*GetNode()))
    return ButtonRoleType();

  if (IsHTMLDetailsElement(*GetNode()))
    return ax::mojom::Role::kDetails;

  if (IsHTMLSummaryElement(*GetNode())) {
    ContainerNode* parent = FlatTreeTraversal::Parent(*GetNode());
    if (parent && IsHTMLSlotElement(parent))
      parent = FlatTreeTraversal::Parent(*parent);
    if (parent && IsHTMLDetailsElement(parent))
      return ax::mojom::Role::kDisclosureTriangle;
    return ax::mojom::Role::kUnknown;
  }

  if (const auto* input = ToHTMLInputElementOrNull(*GetNode())) {
    const AtomicString& type = input->type();
    if (input->DataList())
      return ax::mojom::Role::kTextFieldWithComboBox;
    if (type == InputTypeNames::button) {
      if ((GetNode()->parentNode() &&
           IsHTMLMenuElement(GetNode()->parentNode())) ||
          (ParentObject() &&
           ParentObject()->RoleValue() == ax::mojom::Role::kMenu))
        return ax::mojom::Role::kMenuItem;
      return ButtonRoleType();
    }
    if (type == InputTypeNames::checkbox) {
      if ((GetNode()->parentNode() &&
           IsHTMLMenuElement(GetNode()->parentNode())) ||
          (ParentObject() &&
           ParentObject()->RoleValue() == ax::mojom::Role::kMenu))
        return ax::mojom::Role::kMenuItemCheckBox;
      return ax::mojom::Role::kCheckBox;
    }
    if (type == InputTypeNames::date)
      return ax::mojom::Role::kDate;
    if (type == InputTypeNames::datetime ||
        type == InputTypeNames::datetime_local ||
        type == InputTypeNames::month || type == InputTypeNames::week)
      return ax::mojom::Role::kDateTime;
    if (type == InputTypeNames::file)
      return ax::mojom::Role::kButton;
    if (type == InputTypeNames::radio) {
      if ((GetNode()->parentNode() &&
           IsHTMLMenuElement(GetNode()->parentNode())) ||
          (ParentObject() &&
           ParentObject()->RoleValue() == ax::mojom::Role::kMenu))
        return ax::mojom::Role::kMenuItemRadio;
      return ax::mojom::Role::kRadioButton;
    }
    if (type == InputTypeNames::number)
      return ax::mojom::Role::kSpinButton;
    if (input->IsTextButton())
      return ButtonRoleType();
    if (type == InputTypeNames::range)
      return ax::mojom::Role::kSlider;
    if (type == InputTypeNames::color)
      return ax::mojom::Role::kColorWell;
    if (type == InputTypeNames::time)
      return ax::mojom::Role::kInputTime;
    return ax::mojom::Role::kTextField;
  }

  if (auto* select_element = ToHTMLSelectElementOrNull(*GetNode())) {
    return select_element->IsMultiple() ? ax::mojom::Role::kListBox
                                        : ax::mojom::Role::kPopUpButton;
  }

  if (auto* option = ToHTMLOptionElementOrNull(*GetNode())) {
    HTMLSelectElement* select_element = option->OwnerSelectElement();
    return !select_element || select_element->IsMultiple()
               ? ax::mojom::Role::kListBoxOption
               : ax::mojom::Role::kMenuListOption;
  }

  if (IsHTMLTextAreaElement(*GetNode()))
    return ax::mojom::Role::kTextField;

  if (HeadingLevel())
    return ax::mojom::Role::kHeading;

  if (IsHTMLDivElement(*GetNode()))
    return ax::mojom::Role::kGenericContainer;

  if (IsHTMLMeterElement(*GetNode()))
    return ax::mojom::Role::kMeter;

  if (IsHTMLProgressElement(*GetNode()))
    return ax::mojom::Role::kProgressIndicator;

  if (IsHTMLOutputElement(*GetNode()))
    return ax::mojom::Role::kStatus;

  if (IsHTMLParagraphElement(*GetNode()))
    return ax::mojom::Role::kParagraph;

  if (IsHTMLLabelElement(*GetNode()))
    return ax::mojom::Role::kLabelText;

  if (IsHTMLLegendElement(*GetNode()))
    return ax::mojom::Role::kLegend;

  if (IsHTMLRubyElement(*GetNode()))
    return ax::mojom::Role::kRuby;

  if (IsHTMLDListElement(*GetNode()))
    return ax::mojom::Role::kDescriptionList;

  if (IsHTMLAudioElement(*GetNode()))
    return ax::mojom::Role::kAudio;
  if (IsHTMLVideoElement(*GetNode()))
    return ax::mojom::Role::kVideo;

  if (GetNode()->HasTagName(ddTag))
    return ax::mojom::Role::kDescriptionListDetail;

  if (GetNode()->HasTagName(dtTag))
    return ax::mojom::Role::kDescriptionListTerm;

  if (GetNode()->nodeName() == "math")
    return ax::mojom::Role::kMath;

  if (GetNode()->HasTagName(rpTag) || GetNode()->HasTagName(rtTag))
    return ax::mojom::Role::kAnnotation;

  if (IsHTMLFormElement(*GetNode()))
    return ax::mojom::Role::kForm;

  if (GetNode()->HasTagName(abbrTag))
    return ax::mojom::Role::kAbbr;

  if (GetNode()->HasTagName(articleTag))
    return ax::mojom::Role::kArticle;

  if (GetNode()->HasTagName(delTag))
    return ax::mojom::Role::kContentDeletion;

  if (GetNode()->HasTagName(insTag))
    return ax::mojom::Role::kContentInsertion;

  if (GetNode()->HasTagName(mainTag))
    return ax::mojom::Role::kMain;

  if (GetNode()->HasTagName(markTag))
    return ax::mojom::Role::kMark;

  if (GetNode()->HasTagName(navTag))
    return ax::mojom::Role::kNavigation;

  if (GetNode()->HasTagName(asideTag))
    return ax::mojom::Role::kComplementary;

  if (GetNode()->HasTagName(preTag))
    return ax::mojom::Role::kPre;

  if (GetNode()->HasTagName(sectionTag))
    return ax::mojom::Role::kRegion;

  // TODO(accessibility): http://crbug.com/873118
  if (GetNode()->HasTagName(addressTag))
    return ax::mojom::Role::kContentInfo;

  if (IsHTMLDialogElement(*GetNode()))
    return ax::mojom::Role::kDialog;

  // The HTML element should not be exposed as an element. That's what the
  // LayoutView element does.
  if (IsHTMLHtmlElement(*GetNode()))
    return ax::mojom::Role::kIgnored;

  // Treat <iframe> and <frame> the same.
  if (IsHTMLIFrameElement(*GetNode()) || IsHTMLFrameElement(*GetNode())) {
    const AtomicString& aria_role =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole);
    if (aria_role == "none" || aria_role == "presentation")
      return ax::mojom::Role::kIframePresentational;
    return ax::mojom::Role::kIframe;
  }

  // There should only be one banner/contentInfo per page. If header/footer are
  // being used within an article or section then it should not be exposed as
  // whole page's banner/contentInfo but as a generic container role.
  if (GetNode()->HasTagName(headerTag)) {
    if (IsDescendantOfElementType(GetLandmarkRolesNotAllowed()))
      return ax::mojom::Role::kGenericContainer;
    return ax::mojom::Role::kBanner;
  }

  if (GetNode()->HasTagName(footerTag)) {
    if (IsDescendantOfElementType(GetLandmarkRolesNotAllowed()))
      return ax::mojom::Role::kGenericContainer;
    return ax::mojom::Role::kFooter;
  }

  if (GetNode()->HasTagName(blockquoteTag))
    return ax::mojom::Role::kBlockquote;

  if (GetNode()->HasTagName(captionTag))
    return ax::mojom::Role::kCaption;

  if (GetNode()->HasTagName(figcaptionTag))
    return ax::mojom::Role::kFigcaption;

  if (GetNode()->HasTagName(figureTag))
    return ax::mojom::Role::kFigure;

  if (GetNode()->nodeName() == "TIME")
    return ax::mojom::Role::kTime;

  if (IsEmbeddedObject())
    return ax::mojom::Role::kEmbeddedObject;

  if (IsHTMLHRElement(*GetNode()))
    return ax::mojom::Role::kSplitter;

  if (IsFieldset())
    return ax::mojom::Role::kGroup;

  return ax::mojom::Role::kUnknown;
}

ax::mojom::Role AXNodeObject::DetermineAccessibilityRole() {
  if (!GetNode())
    return ax::mojom::Role::kUnknown;

  native_role_ = NativeRoleIgnoringAria();

  if ((aria_role_ = DetermineAriaRoleAttribute()) != ax::mojom::Role::kUnknown)
    return aria_role_;
  if (GetNode()->IsTextNode())
    return ax::mojom::Role::kStaticText;

  return native_role_ == ax::mojom::Role::kUnknown
             ? ax::mojom::Role::kGenericContainer
             : native_role_;
}

void AXNodeObject::AccessibilityChildrenFromAOMProperty(
    AOMRelationListProperty property,
    AXObject::AXObjectVector& children) const {
  HeapVector<Member<Element>> elements;
  if (!HasAOMPropertyOrARIAAttribute(property, elements))
    return;

  AXObjectCacheImpl& cache = AXObjectCache();
  for (const auto& element : elements) {
    if (AXObject* child = cache.GetOrCreate(element)) {
      // Only aria-labelledby and aria-describedby can target hidden elements.
      if (child->AccessibilityIsIgnored() &&
          property != AOMRelationListProperty::kLabeledBy &&
          property != AOMRelationListProperty::kDescribedBy) {
        continue;
      }
      children.push_back(child);
    }
  }
}

bool AXNodeObject::IsMultiline() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  const ax::mojom::Role role = RoleValue();
  const bool is_edit_box = role == ax::mojom::Role::kSearchBox ||
                           role == ax::mojom::Role::kTextField;
  if (!IsEditable() && !is_edit_box)
    return false;  // Doesn't support multiline.

  // Supports aria-multiline, so check for attribute.
  bool is_multiline = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kMultiline,
                                    is_multiline)) {
    return is_multiline;
  }

  // Default for <textarea> is true.
  if (IsHTMLTextAreaElement(*node))
    return true;

  // Default for other edit boxes is false, including for ARIA, says CORE-AAM.
  if (is_edit_box)
    return false;

  // If root of contenteditable area and no ARIA role of textbox/searchbox used,
  // default to multiline=true which is what the default behavior is.
  return HasContentEditableAttributeSet();
}

// This only returns true if this is the element that actually has the
// contentEditable attribute set, unlike node->hasEditableStyle() which will
// also return true if an ancestor is editable.
bool AXNodeObject::HasContentEditableAttributeSet() const {
  const AtomicString& content_editable_value =
      GetAttribute(contenteditableAttr);
  if (content_editable_value.IsNull())
    return false;
  // Both "true" (case-insensitive) and the empty string count as true.
  return content_editable_value.IsEmpty() ||
         EqualIgnoringASCIICase(content_editable_value, "true");
}

bool AXNodeObject::IsTextControl() const {
  if (!GetNode())
    return false;

  if (HasContentEditableAttributeSet())
    return true;

  switch (RoleValue()) {
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kSearchBox:
      return true;
    case ax::mojom::Role::kSpinButton:
      // When it's a native spin button, it behaves like a text box, i.e. users
      // can type in it and navigate around using cursors.
      if (const auto* input = ToHTMLInputElementOrNull(*GetNode())) {
        return input->IsTextField();
      }
      return false;
    default:
      return false;
  }
}

AXObject* AXNodeObject::MenuButtonForMenu() const {
  Element* menu_item = MenuItemElementForMenu();

  if (menu_item) {
    // ARIA just has generic menu items. AppKit needs to know if this is a top
    // level items like MenuBarButton or MenuBarItem
    AXObject* menu_item_ax = AXObjectCache().GetOrCreate(menu_item);
    if (menu_item_ax && menu_item_ax->IsMenuButton())
      return menu_item_ax;
  }
  return nullptr;
}

AXObject* AXNodeObject::MenuButtonForMenuIfExists() const {
  Element* menu_item = MenuItemElementForMenu();

  if (menu_item) {
    // ARIA just has generic menu items. AppKit needs to know if this is a top
    // level items like MenuBarButton or MenuBarItem
    AXObject* menu_item_ax = AXObjectCache().Get(menu_item);
    if (menu_item_ax && menu_item_ax->IsMenuButton())
      return menu_item_ax;
  }
  return nullptr;
}

static Element* SiblingWithAriaRole(String role, Node* node) {
  Node* parent = node->parentNode();
  if (!parent)
    return nullptr;

  for (Element* sibling = ElementTraversal::FirstChild(*parent); sibling;
       sibling = ElementTraversal::NextSibling(*sibling)) {
    const AtomicString& sibling_aria_role =
        AccessibleNode::GetPropertyOrARIAAttribute(sibling,
                                                   AOMStringProperty::kRole);
    if (EqualIgnoringASCIICase(sibling_aria_role, role))
      return sibling;
  }

  return nullptr;
}

Element* AXNodeObject::MenuItemElementForMenu() const {
  if (AriaRoleAttribute() != ax::mojom::Role::kMenu)
    return nullptr;

  return SiblingWithAriaRole("menuitem", GetNode());
}

Element* AXNodeObject::MouseButtonListener() const {
  Node* node = this->GetNode();
  if (!node)
    return nullptr;

  if (!node->IsElementNode())
    node = node->parentElement();

  if (!node)
    return nullptr;

  for (Element* element = ToElement(node); element;
       element = element->parentElement()) {
    if (element->HasEventListeners(EventTypeNames::click) ||
        element->HasEventListeners(EventTypeNames::mousedown) ||
        element->HasEventListeners(EventTypeNames::mouseup) ||
        element->HasEventListeners(EventTypeNames::DOMActivate))
      return element;
  }

  return nullptr;
}

void AXNodeObject::Init() {
#if DCHECK_IS_ON()
  DCHECK(!initialized_);
  initialized_ = true;
#endif
  AXObject::Init();
}

void AXNodeObject::Detach() {
  AXObject::Detach();
  node_ = nullptr;
}

bool AXNodeObject::IsAnchor() const {
  return !IsNativeImage() && IsLink();
}

bool AXNodeObject::IsControl() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  return ((node->IsElementNode() && ToElement(node)->IsFormControlElement()) ||
          AXObject::IsARIAControl(AriaRoleAttribute()));
}

bool AXNodeObject::IsControllingVideoElement() const {
  Node* node = this->GetNode();
  if (!node)
    return true;

  return IsHTMLVideoElement(
      MediaControlElementsHelper::ToParentMediaElement(node));
}

bool AXNodeObject::ComputeIsEditableRoot() const {
  Node* node = GetNode();
  if (!node)
    return false;
  if (IsNativeTextControl())
    return true;
  if (IsRootEditableElement(*node)) {
    // Editable roots created by the user agent are handled by
    // |IsNativeTextControl| above.
    ShadowRoot* root = node->ContainingShadowRoot();
    return !root || !root->IsUserAgent();
  }
  return false;
}

bool AXNodeObject::IsEmbeddedObject() const {
  return IsHTMLPlugInElement(GetNode());
}

bool AXNodeObject::IsFieldset() const {
  return IsHTMLFieldSetElement(GetNode());
}

bool AXNodeObject::IsHeading() const {
  return RoleValue() == ax::mojom::Role::kHeading;
}

bool AXNodeObject::IsHovered() const {
  if (Node* node = this->GetNode())
    return node->IsHovered();
  return false;
}

bool AXNodeObject::IsImage() const {
  return RoleValue() == ax::mojom::Role::kImage;
}

bool AXNodeObject::IsImageButton() const {
  return IsNativeImage() && IsButton();
}

bool AXNodeObject::IsInputImage() const {
  Node* node = this->GetNode();
  if (RoleValue() == ax::mojom::Role::kButton && IsHTMLInputElement(node))
    return ToHTMLInputElement(*node).type() == InputTypeNames::image;

  return false;
}

bool AXNodeObject::IsLink() const {
  return RoleValue() == ax::mojom::Role::kLink;
}

// It is not easily possible to find out if an element is the target of an
// in-page link.
// As a workaround, we check if the element is a sectioning element with an ID,
// or an anchor with a name.
bool AXNodeObject::IsInPageLinkTarget() const {
  if (!node_ || !node_->IsElementNode())
    return false;
  Element* element = ToElement(node_);
  // We exclude elements that are in the shadow DOM.
  if (element->ContainingShadowRoot())
    return false;

  if (auto* anchor = ToHTMLAnchorElementOrNull(element)) {
    return anchor->HasName() || anchor->HasID();
  }

  if (element->HasID() && (IsLandmarkRelated() || IsHTMLSpanElement(element) ||
                           IsHTMLDivElement(element))) {
    return true;
  }
  return false;
}

bool AXNodeObject::IsMenu() const {
  return RoleValue() == ax::mojom::Role::kMenu;
}

bool AXNodeObject::IsMenuButton() const {
  return RoleValue() == ax::mojom::Role::kMenuButton;
}

bool AXNodeObject::IsMeter() const {
  return RoleValue() == ax::mojom::Role::kMeter;
}

bool AXNodeObject::IsMultiSelectable() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kTreeGrid:
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kTabList: {
      bool multiselectable = false;
      if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kMultiselectable,
                                        multiselectable)) {
        return multiselectable;
      }
      break;
    }
    default:
      break;
  }

  return IsHTMLSelectElement(GetNode()) &&
         ToHTMLSelectElement(*GetNode()).IsMultiple();
}

bool AXNodeObject::IsNativeCheckboxOrRadio() const {
  if (const auto* input = ToHTMLInputElementOrNull(GetNode())) {
    return input->type() == InputTypeNames::checkbox ||
           input->type() == InputTypeNames::radio;
  }
  return false;
}

bool AXNodeObject::IsNativeImage() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  if (IsHTMLImageElement(*node))
    return true;

  if (IsHTMLPlugInElement(*node))
    return true;

  if (const auto* input = ToHTMLInputElementOrNull(*node))
    return input->type() == InputTypeNames::image;

  return false;
}

bool AXNodeObject::IsNativeTextControl() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  if (IsHTMLTextAreaElement(*node))
    return true;

  if (const auto* input = ToHTMLInputElementOrNull(*node))
    return input->IsTextField();

  return false;
}

bool AXNodeObject::IsNonNativeTextControl() const {
  if (IsNativeTextControl())
    return false;

  if (HasContentEditableAttributeSet())
    return true;

  if (IsARIATextControl())
    return true;

  return false;
}

bool AXNodeObject::IsPasswordField() const {
  Node* node = this->GetNode();
  if (!IsHTMLInputElement(node))
    return false;

  ax::mojom::Role aria_role = AriaRoleAttribute();
  if (aria_role != ax::mojom::Role::kTextField &&
      aria_role != ax::mojom::Role::kUnknown)
    return false;

  return ToHTMLInputElement(node)->type() == InputTypeNames::password;
}

bool AXNodeObject::IsProgressIndicator() const {
  return RoleValue() == ax::mojom::Role::kProgressIndicator;
}

bool AXNodeObject::IsRichlyEditable() const {
  return HasContentEditableAttributeSet();
}

bool AXNodeObject::IsSlider() const {
  return RoleValue() == ax::mojom::Role::kSlider;
}

bool AXNodeObject::IsSpinButton() const {
  return RoleValue() == ax::mojom::Role::kSpinButton;
}

bool AXNodeObject::IsNativeSlider() const {
  if (const auto* input = ToHTMLInputElementOrNull(GetNode()))
    return input->type() == InputTypeNames::range;
  return false;
}

bool AXNodeObject::IsNativeSpinButton() const {
  if (const auto* input = ToHTMLInputElementOrNull(GetNode()))
    return input->type() == InputTypeNames::number;
  return false;
}

bool AXNodeObject::IsMoveableSplitter() const {
  return RoleValue() == ax::mojom::Role::kSplitter && CanSetFocusAttribute();
}

bool AXNodeObject::IsClickable() const {
  Node* node = GetNode();
  if (!node)
    return false;
  if (node->IsElementNode() && ToElement(node)->IsDisabledFormControl()) {
    return false;
  }

  // Note: we can't call |node->WillRespondToMouseClickEvents()| because that
  // triggers a style recalc and can delete this.
  if (node->HasEventListeners(EventTypeNames::mouseup) ||
      node->HasEventListeners(EventTypeNames::mousedown) ||
      node->HasEventListeners(EventTypeNames::click) ||
      node->HasEventListeners(EventTypeNames::DOMActivate)) {
    return true;
  }

  return AXObject::IsClickable();
}

AXRestriction AXNodeObject::Restriction() const {
  Element* elem = GetElement();
  if (!elem)
    return kNone;

  // An <optgroup> is not exposed directly in the AX tree.
  if (IsHTMLOptGroupElement(elem))
    return kNone;

  // According to ARIA, all elements of the base markup can be disabled.
  // According to CORE-AAM, any focusable descendant of aria-disabled
  // ancestor is also disabled.
  bool is_disabled;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kDisabled,
                                    is_disabled)) {
    // Has aria-disabled, overrides native markup determining disabled.
    if (is_disabled)
      return kDisabled;
  } else if (elem->IsDisabledFormControl() ||
             (CanSetFocusAttribute() && IsDescendantOfDisabledNode())) {
    // No aria-disabled, but other markup says it's disabled.
    return kDisabled;
  }

  // Check aria-readonly if supported by current role.
  bool is_read_only;
  if (SupportsARIAReadOnly() &&
      HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kReadOnly,
                                    is_read_only)) {
    // ARIA overrides other readonly state markup.
    return is_read_only ? kReadOnly : kNone;
  }

  // Only editable fields can be marked @readonly (unlike @aria-readonly).
  if (IsHTMLTextAreaElement(*elem) && ToHTMLTextAreaElement(*elem).IsReadOnly())
    return kReadOnly;
  if (const auto* input = ToHTMLInputElementOrNull(*elem)) {
    if (input->IsTextField() && input->IsReadOnly())
      return kReadOnly;
  }

  // If a grid cell does not have it's own ARIA input restriction,
  // fall back on parent grid's readonly state.
  // See ARIA specification regarding grid/treegrid and readonly.
  if (IsTableCellLikeRole()) {
    AXObject* row = ParentObjectUnignored();
    if (row->IsTableRowLikeRole()) {
      AXObject* table = row->ParentObjectUnignored();
      if (table->IsTableLikeRole() &&
          (table->RoleValue() == ax::mojom::Role::kGrid ||
           table->RoleValue() == ax::mojom::Role::kTreeGrid)) {
        if (table->Restriction() == kReadOnly)
          return kReadOnly;
      }
    }
  }

  // This is a node that is not readonly and not disabled.
  return kNone;
}

AccessibilityExpanded AXNodeObject::IsExpanded() const {
  if (!SupportsARIAExpanded())
    return kExpandedUndefined;

  if (GetNode() && IsHTMLSummaryElement(*GetNode())) {
    if (GetNode()->parentNode() &&
        IsHTMLDetailsElement(GetNode()->parentNode()))
      return ToElement(GetNode()->parentNode())->hasAttribute(openAttr)
                 ? kExpandedExpanded
                 : kExpandedCollapsed;
  }

  bool expanded = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kExpanded, expanded)) {
    return expanded ? kExpandedExpanded : kExpandedCollapsed;
  }

  return kExpandedUndefined;
}

bool AXNodeObject::IsModal() const {
  if (RoleValue() != ax::mojom::Role::kDialog &&
      RoleValue() != ax::mojom::Role::kAlertDialog)
    return false;

  bool modal = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kModal, modal))
    return modal;

  if (GetNode() && IsHTMLDialogElement(*GetNode()))
    return ToElement(GetNode())->IsInTopLayer();

  return false;
}

bool AXNodeObject::IsRequired() const {
  Node* n = this->GetNode();
  if (n && (n->IsElementNode() && ToElement(n)->IsFormControlElement()) &&
      HasAttribute(requiredAttr))
    return ToHTMLFormControlElement(n)->IsRequired();

  if (AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kRequired))
    return true;

  return false;
}

bool AXNodeObject::CanvasHasFallbackContent() const {
  if (IsDetached())
    return false;
  Node* node = this->GetNode();
  return IsHTMLCanvasElement(node) && node->hasChildren();
}

int AXNodeObject::HeadingLevel() const {
  // headings can be in block flow and non-block flow
  Node* node = this->GetNode();
  if (!node)
    return 0;

  if (RoleValue() == ax::mojom::Role::kHeading) {
    uint32_t level;
    if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kLevel, level)) {
      if (level >= 1 && level <= 9)
        return level;
    }
  }

  if (!node->IsHTMLElement())
    return 0;

  HTMLElement& element = ToHTMLElement(*node);
  if (element.HasTagName(h1Tag))
    return 1;

  if (element.HasTagName(h2Tag))
    return 2;

  if (element.HasTagName(h3Tag))
    return 3;

  if (element.HasTagName(h4Tag))
    return 4;

  if (element.HasTagName(h5Tag))
    return 5;

  if (element.HasTagName(h6Tag))
    return 6;

  if (RoleValue() == ax::mojom::Role::kHeading)
    return kDefaultHeadingLevel;

  return 0;
}

unsigned AXNodeObject::HierarchicalLevel() const {
  Element* element = GetElement();
  if (!element)
    return 0;

  uint32_t level;
  if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kLevel, level)) {
    if (level >= 1 && level <= 9)
      return level;
  }

  // Only tree item will calculate its level through the DOM currently.
  if (RoleValue() != ax::mojom::Role::kTreeItem)
    return 0;

  // Hierarchy leveling starts at 1, to match the aria-level spec.
  // We measure tree hierarchy by the number of groups that the item is within.
  level = 1;
  for (AXObject* parent = ParentObject(); parent;
       parent = parent->ParentObject()) {
    ax::mojom::Role parent_role = parent->RoleValue();
    if (parent_role == ax::mojom::Role::kGroup)
      level++;
    else if (parent_role == ax::mojom::Role::kTree)
      break;
  }

  return level;
}

// TODO: rename this just AutoComplete, it's not only ARIA.
String AXNodeObject::AriaAutoComplete() const {
  if (IsNativeTextControl() || IsARIATextControl()) {
    const AtomicString& aria_auto_complete =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kAutocomplete)
            .DeprecatedLower();
    // Illegal values must be passed through, according to CORE-AAM.
    if (!aria_auto_complete.IsNull())
      return aria_auto_complete == "none" ? String() : aria_auto_complete;
  }

  if (GetNode() && IsHTMLInputElement(*GetNode())) {
    HTMLInputElement& input = ToHTMLInputElement(*GetNode());
    if (input.DataList())
      return "list";
  }

  return String();
}

namespace {

bool MarkerTypeIsUsedForAccessibility(DocumentMarker::MarkerType type) {
  return DocumentMarker::MarkerTypes(
             DocumentMarker::kSpelling | DocumentMarker::kGrammar |
             DocumentMarker::kTextMatch | DocumentMarker::kActiveSuggestion |
             DocumentMarker::kSuggestion)
      .Contains(type);
}

}  // namespace

void AXNodeObject::Markers(Vector<DocumentMarker::MarkerType>& marker_types,
                           Vector<AXRange>& marker_ranges) const {
  if (!GetNode() || !GetDocument() || !GetDocument()->View())
    return;

  if (!GetNode()->IsTextNode())
    return;

  DocumentMarkerController& marker_controller = GetDocument()->Markers();
  DocumentMarkerVector markers =
      marker_controller.MarkersFor(ToText(*GetNode()));
  for (DocumentMarker* marker : markers) {
    if (!MarkerTypeIsUsedForAccessibility(marker->GetType()))
      continue;

    const Position start_position(*GetNode(), marker->StartOffset());
    const Position end_position(*GetNode(), marker->EndOffset());
    if (!start_position.IsValidFor(*GetDocument()) ||
        !end_position.IsValidFor(*GetDocument())) {
      continue;
    }

    marker_types.push_back(marker->GetType());
    marker_ranges.emplace_back(
        AXPosition::FromPosition(start_position, TextAffinity::kDownstream,
                                 AXPositionAdjustmentBehavior::kMoveLeft),
        AXPosition::FromPosition(end_position, TextAffinity::kDownstream,
                                 AXPositionAdjustmentBehavior::kMoveRight));
  }
}

AXObject* AXNodeObject::InPageLinkTarget() const {
  if (!node_ || !IsHTMLAnchorElement(node_) || !GetDocument())
    return AXObject::InPageLinkTarget();

  HTMLAnchorElement* anchor = ToHTMLAnchorElement(node_);
  DCHECK(anchor);
  KURL link_url = anchor->Href();
  if (!link_url.IsValid())
    return AXObject::InPageLinkTarget();
  String fragment = link_url.FragmentIdentifier();
  if (fragment.IsEmpty())
    return AXObject::InPageLinkTarget();

  KURL document_url = GetDocument()->Url();
  if (!document_url.IsValid() ||
      !EqualIgnoringFragmentIdentifier(document_url, link_url)) {
    return AXObject::InPageLinkTarget();
  }

  TreeScope& tree_scope = anchor->GetTreeScope();
  Element* target = tree_scope.FindAnchor(fragment);
  if (!target)
    return AXObject::InPageLinkTarget();
  // If the target is not in the accessibility tree, get the first unignored
  // sibling.
  return AXObjectCache().FirstAccessibleObjectFromNode(target);
}

AccessibilityOrientation AXNodeObject::Orientation() const {
  const AtomicString& aria_orientation =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kOrientation);
  AccessibilityOrientation orientation = kAccessibilityOrientationUndefined;
  if (EqualIgnoringASCIICase(aria_orientation, "horizontal"))
    orientation = kAccessibilityOrientationHorizontal;
  else if (EqualIgnoringASCIICase(aria_orientation, "vertical"))
    orientation = kAccessibilityOrientationVertical;

  switch (RoleValue()) {
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kTree:
      if (orientation == kAccessibilityOrientationUndefined)
        orientation = kAccessibilityOrientationVertical;

      return orientation;
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kTabList:
    case ax::mojom::Role::kToolbar:
      if (orientation == kAccessibilityOrientationUndefined)
        orientation = kAccessibilityOrientationHorizontal;

      return orientation;
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kTreeGrid:
      return orientation;
    default:
      return AXObject::Orientation();
  }
}

AXObject::AXObjectVector AXNodeObject::RadioButtonsInGroup() const {
  AXObjectVector radio_buttons;
  if (!node_ || RoleValue() != ax::mojom::Role::kRadioButton)
    return radio_buttons;

  if (auto* radio_button = ToHTMLInputElementOrNull(node_)) {
    HeapVector<Member<HTMLInputElement>> html_radio_buttons =
        FindAllRadioButtonsWithSameName(radio_button);
    for (HTMLInputElement* radio_button : html_radio_buttons) {
      AXObject* ax_radio_button = AXObjectCache().GetOrCreate(radio_button);
      if (ax_radio_button)
        radio_buttons.push_back(ax_radio_button);
    }
    return radio_buttons;
  }

  // If the immediate parent is a radio group, return all its children that are
  // radio buttons.
  AXObject* parent = ParentObject();
  if (parent && parent->RoleValue() == ax::mojom::Role::kRadioGroup) {
    for (AXObject* child : parent->Children()) {
      DCHECK(child);
      if (child->RoleValue() == ax::mojom::Role::kRadioButton &&
          !child->AccessibilityIsIgnored()) {
        radio_buttons.push_back(child);
      }
    }
  }

  return radio_buttons;
}

// static
HeapVector<Member<HTMLInputElement>>
AXNodeObject::FindAllRadioButtonsWithSameName(HTMLInputElement* radio_button) {
  HeapVector<Member<HTMLInputElement>> all_radio_buttons;
  if (!radio_button || radio_button->type() != InputTypeNames::radio)
    return all_radio_buttons;

  constexpr bool kTraverseForward = true;
  constexpr bool kTraverseBackward = false;
  HTMLInputElement* first_radio_button = radio_button;
  do {
    radio_button = RadioInputType::NextRadioButtonInGroup(first_radio_button,
                                                          kTraverseBackward);
    if (radio_button)
      first_radio_button = radio_button;
  } while (radio_button);

  HTMLInputElement* next_radio_button = first_radio_button;
  do {
    all_radio_buttons.push_back(next_radio_button);
    next_radio_button = RadioInputType::NextRadioButtonInGroup(
        next_radio_button, kTraverseForward);
  } while (next_radio_button);
  return all_radio_buttons;
}

String AXNodeObject::GetText() const {
  if (!IsTextControl())
    return String();

  Node* node = this->GetNode();
  if (!node)
    return String();

  if (IsNativeTextControl() &&
      (IsHTMLTextAreaElement(*node) || IsHTMLInputElement(*node))) {
    return ToTextControl(*node).value();
  }

  if (!node->IsElementNode())
    return String();

  return ToElement(node)->innerText();
}

RGBA32 AXNodeObject::ColorValue() const {
  if (!IsHTMLInputElement(GetNode()) || !IsColorWell())
    return AXObject::ColorValue();

  HTMLInputElement* input = ToHTMLInputElement(GetNode());
  const AtomicString& type = input->getAttribute(typeAttr);
  if (!EqualIgnoringASCIICase(type, "color"))
    return AXObject::ColorValue();

  // HTMLInputElement::value always returns a string parseable by Color.
  Color color;
  bool success = color.SetFromString(input->value());
  DCHECK(success);
  return color.Rgb();
}

ax::mojom::AriaCurrentState AXNodeObject::GetAriaCurrentState() const {
  const AtomicString& attribute_value =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kCurrent);
  if (attribute_value.IsNull())
    return ax::mojom::AriaCurrentState::kNone;
  if (attribute_value.IsEmpty() ||
      EqualIgnoringASCIICase(attribute_value, "false"))
    return ax::mojom::AriaCurrentState::kFalse;
  if (EqualIgnoringASCIICase(attribute_value, "true"))
    return ax::mojom::AriaCurrentState::kTrue;
  if (EqualIgnoringASCIICase(attribute_value, "page"))
    return ax::mojom::AriaCurrentState::kPage;
  if (EqualIgnoringASCIICase(attribute_value, "step"))
    return ax::mojom::AriaCurrentState::kStep;
  if (EqualIgnoringASCIICase(attribute_value, "location"))
    return ax::mojom::AriaCurrentState::kLocation;
  if (EqualIgnoringASCIICase(attribute_value, "date"))
    return ax::mojom::AriaCurrentState::kDate;
  if (EqualIgnoringASCIICase(attribute_value, "time"))
    return ax::mojom::AriaCurrentState::kTime;
  // An unknown value should return true.
  if (!attribute_value.IsEmpty())
    return ax::mojom::AriaCurrentState::kTrue;

  return AXObject::GetAriaCurrentState();
}

ax::mojom::InvalidState AXNodeObject::GetInvalidState() const {
  const AtomicString& attribute_value =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kInvalid);
  if (EqualIgnoringASCIICase(attribute_value, "false"))
    return ax::mojom::InvalidState::kFalse;
  if (EqualIgnoringASCIICase(attribute_value, "true"))
    return ax::mojom::InvalidState::kTrue;
  if (EqualIgnoringASCIICase(attribute_value, "spelling"))
    return ax::mojom::InvalidState::kSpelling;
  if (EqualIgnoringASCIICase(attribute_value, "grammar"))
    return ax::mojom::InvalidState::kGrammar;
  // A yet unknown value.
  if (!attribute_value.IsEmpty())
    return ax::mojom::InvalidState::kOther;

  if (GetNode() && GetNode()->IsElementNode() &&
      ToElement(GetNode())->IsFormControlElement()) {
    HTMLFormControlElement* element = ToHTMLFormControlElement(GetNode());
    HeapVector<Member<HTMLFormControlElement>> invalid_controls;
    bool is_invalid = !element->checkValidity(&invalid_controls,
                                              kCheckValidityDispatchNoEvent);
    return is_invalid ? ax::mojom::InvalidState::kTrue
                      : ax::mojom::InvalidState::kFalse;
  }

  return AXObject::GetInvalidState();
}

int AXNodeObject::PosInSet() const {
  if (SupportsARIASetSizeAndPosInSet()) {
    uint32_t pos_in_set;
    if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kPosInSet, pos_in_set))
      return pos_in_set;

    return AutoPosInSet();
  }

  return 0;
}

int AXNodeObject::SetSize() const {
  if (SupportsARIASetSizeAndPosInSet()) {
    int32_t set_size;
    if (HasAOMPropertyOrARIAAttribute(AOMIntProperty::kSetSize, set_size))
      return set_size;

    return AutoSetSize();
  }

  return 0;
}

int AXNodeObject::AutoPosInSet() const {
  AXObject* parent = ParentObjectUnignored();

  // Do not continue if the children will need updating soon, because
  // the calculation requires all the siblings to remain stable.
  if (!parent || parent->NeedsToUpdateChildren())
    return 0;

  int pos_in_set = 1;
  const AXObject::AXObjectVector siblings = parent->Children();

  ax::mojom::Role role = RoleValue();
  int level = HierarchicalLevel();
  int index_in_parent = IndexInParent();

  for (int index = index_in_parent - 1; index >= 0; index--) {
    const AXObject* sibling = siblings[index];
    ax::mojom::Role sibling_role = sibling->RoleValue();
    if (sibling_role == ax::mojom::Role::kSplitter ||
        sibling_role == ax::mojom::Role::kGroup)
      break;  // Set stops at a separator or an optgroup.
    if (sibling_role != role || sibling->AccessibilityIsIgnored())
      continue;

    int sibling_level = sibling->HierarchicalLevel();
    if (sibling_level < level)
      break;

    if (sibling_level > level)
      continue;  // Skip subset

    ++pos_in_set;
  }

  return pos_in_set;
}

int AXNodeObject::AutoSetSize() const {
  AXObject* parent = ParentObjectUnignored();

  // Do not continue if the children will need updating soon, because
  // the calculation requires all the siblings to remain stable.
  if (!parent || parent->NeedsToUpdateChildren())
    return 0;

  int set_size = AutoPosInSet();
  auto siblings = parent->Children();

  ax::mojom::Role role = RoleValue();
  int level = HierarchicalLevel();
  int index_in_parent = IndexInParent();
  int sibling_count = siblings.size();

  for (int index = index_in_parent + 1; index < sibling_count; index++) {
    const auto sibling = siblings[index];
    ax::mojom::Role sibling_role = sibling->RoleValue();
    if (sibling_role == ax::mojom::Role::kSplitter ||
        sibling_role == ax::mojom::Role::kGroup)
      break;  // Set stops at a separator or an optgroup.
    if (sibling_role != role || sibling->AccessibilityIsIgnored())
      continue;

    int sibling_level = sibling->HierarchicalLevel();
    if (sibling_level < level)
      break;

    if (sibling_level > level)
      continue;  // Skip subset

    ++set_size;
  }

  return set_size;
}

String AXNodeObject::AriaInvalidValue() const {
  if (GetInvalidState() == ax::mojom::InvalidState::kOther)
    return GetAOMPropertyOrARIAAttribute(AOMStringProperty::kInvalid);

  return String();
}

String AXNodeObject::ValueDescription() const {
  if (!SupportsRangeValue())
    return String();

  return GetAOMPropertyOrARIAAttribute(AOMStringProperty::kValueText)
      .GetString();
}

bool AXNodeObject::ValueForRange(float* out_value) const {
  float value_now;
  if (HasAOMPropertyOrARIAAttribute(AOMFloatProperty::kValueNow, value_now)) {
    *out_value = value_now;
    return true;
  }

  if (IsNativeSlider() || IsNativeSpinButton()) {
    *out_value = ToHTMLInputElement(*GetNode()).valueAsNumber();
    return std::isfinite(*out_value);
  }

  if (auto* meter = ToHTMLMeterElementOrNull(GetNode())) {
    *out_value = meter->value();
    return true;
  }

  // In ARIA 1.1, default values for aria-valuenow were changed as below.
  // - scrollbar, slider : half way between aria-valuemin and aria-valuemax
  // - separator : 50
  // - spinbutton : 0
  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider: {
      float min_value, max_value;
      if (MinValueForRange(&min_value) && MaxValueForRange(&max_value)) {
        *out_value = (min_value + max_value) / 2.0f;
        return true;
      }
      FALLTHROUGH;
    }
    case ax::mojom::Role::kSplitter: {
      *out_value = 50.0f;
      return true;
    }
    case ax::mojom::Role::kSpinButton: {
      *out_value = 0.0f;
      return true;
    }
    default:
      break;
  }

  return false;
}

bool AXNodeObject::MaxValueForRange(float* out_value) const {
  float value_max;
  if (HasAOMPropertyOrARIAAttribute(AOMFloatProperty::kValueMax, value_max)) {
    *out_value = value_max;
    return true;
  }

  if (IsNativeSlider() || IsNativeSpinButton()) {
    *out_value = static_cast<float>(ToHTMLInputElement(*GetNode()).Maximum());
    return std::isfinite(*out_value);
  }

  if (auto* meter = ToHTMLMeterElementOrNull(GetNode())) {
    *out_value = meter->max();
    return true;
  }

  // In ARIA 1.1, default value of scrollbar, separator and slider
  // for aria-valuemax were changed to 100.
  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSlider: {
      *out_value = 100.0f;
      return true;
    }
    default:
      break;
  }

  return false;
}

bool AXNodeObject::MinValueForRange(float* out_value) const {
  float value_min;
  if (HasAOMPropertyOrARIAAttribute(AOMFloatProperty::kValueMin, value_min)) {
    *out_value = value_min;
    return true;
  }

  if (IsNativeSlider() || IsNativeSpinButton()) {
    *out_value = static_cast<float>(ToHTMLInputElement(*GetNode()).Minimum());
    return std::isfinite(*out_value);
  }

  if (auto* meter = ToHTMLMeterElementOrNull(GetNode())) {
    *out_value = meter->min();
    return true;
  }

  // In ARIA 1.1, default value of scrollbar, separator and slider
  // for aria-valuemin were changed to 0.
  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSlider: {
      *out_value = 0.0f;
      return true;
    }
    default:
      break;
  }

  return false;
}

bool AXNodeObject::StepValueForRange(float* out_value) const {
  if (IsNativeSlider() || IsNativeSpinButton()) {
    Decimal step =
        ToHTMLInputElement(*GetNode()).CreateStepRange(kRejectAny).Step();
    *out_value = step.ToString().ToFloat();
    return std::isfinite(*out_value);
  }

  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSlider: {
      *out_value = 0.0f;
      return true;
    }
    default:
      break;
  }

  return false;
}

String AXNodeObject::StringValue() const {
  Node* node = this->GetNode();
  if (!node)
    return String();

  if (auto* select_element = ToHTMLSelectElementOrNull(*node)) {
    int selected_index = select_element->selectedIndex();
    const HeapVector<Member<HTMLElement>>& list_items =
        select_element->GetListItems();
    if (selected_index >= 0 &&
        static_cast<wtf_size_t>(selected_index) < list_items.size()) {
      const AtomicString& overridden_description =
          list_items[selected_index]->FastGetAttribute(aria_labelAttr);
      if (!overridden_description.IsNull())
        return overridden_description;
    }
    if (!select_element->IsMultiple())
      return select_element->value();
    return String();
  }

  if (IsNativeTextControl())
    return GetText();

  // Handle other HTML input elements that aren't text controls, like date and
  // time controls, by returning their value converted to text, with the
  // exception of checkboxes and radio buttons (which would return "on"), and
  // buttons which will return their name.
  // https://html.spec.whatwg.org/multipage/forms.html#dom-input-value
  if (const auto* input = ToHTMLInputElementOrNull(node)) {
    if (input->type() != InputTypeNames::button &&
        input->type() != InputTypeNames::checkbox &&
        input->type() != InputTypeNames::image &&
        input->type() != InputTypeNames::radio &&
        input->type() != InputTypeNames::reset &&
        input->type() != InputTypeNames::submit) {
      return input->value();
    }
  }

  return String();
}

ax::mojom::Role AXNodeObject::AriaRoleAttribute() const {
  return aria_role_;
}

// Returns the nearest block-level LayoutBlockFlow ancestor
static LayoutBlockFlow* NonInlineBlockFlow(LayoutObject* object) {
  LayoutObject* current = object;
  while (current) {
    if (current->IsLayoutBlockFlow()) {
      LayoutBlockFlow* block_flow = ToLayoutBlockFlow(current);
      if (!block_flow->IsAtomicInlineLevel())
        return block_flow;
    }
    current = current->Parent();
  }

  NOTREACHED();
  return nullptr;
}

// Returns true if |r1| and |r2| are both non-null, both inline, and are
// contained within the same non-inline LayoutBlockFlow.
static bool IsInSameNonInlineBlockFlow(LayoutObject* r1, LayoutObject* r2) {
  if (!r1 || !r2)
    return false;
  if (!r1->IsInline() || !r2->IsInline())
    return false;
  LayoutBlockFlow* b1 = NonInlineBlockFlow(r1);
  LayoutBlockFlow* b2 = NonInlineBlockFlow(r2);
  return b1 && b2 && b1 == b2;
}

//
// New AX name calculation.
//

String AXNodeObject::GetName(ax::mojom::NameFrom& name_from,
                             AXObjectVector* name_objects) const {
  String name = AXObject::GetName(name_from, name_objects);
  if (RoleValue() == ax::mojom::Role::kSpinButton && DatetimeAncestor()) {
    // Fields inside a datetime control need to merge the field name with
    // the name of the <input> element.
    name_objects->clear();
    String input_name = DatetimeAncestor()->GetName(name_from, name_objects);
    if (!input_name.IsEmpty())
      return name + " " + input_name;
  }

  return name;
}

String AXNodeObject::TextAlternative(bool recursive,
                                     bool in_aria_labelled_by_traversal,
                                     AXObjectSet& visited,
                                     ax::mojom::NameFrom& name_from,
                                     AXRelatedObjectVector* related_objects,
                                     NameSources* name_sources) const {
  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (name_sources)
    DCHECK(related_objects);

  bool found_text_alternative = false;

  if (!GetNode() && !GetLayoutObject())
    return String();

  String text_alternative = AriaTextAlternative(
      recursive, in_aria_labelled_by_traversal, visited, name_from,
      related_objects, name_sources, &found_text_alternative);
  if (found_text_alternative && !name_sources)
    return text_alternative;

  // Step 2E from: http://www.w3.org/TR/accname-aam-1.1 -- value from control
  if (recursive && !in_aria_labelled_by_traversal && CanSetValueAttribute()) {
    // No need to set any name source info in a recursive call.
    if (IsTextControl())
      return GetText();

    if (IsRange()) {
      const AtomicString& aria_valuetext =
          GetAOMPropertyOrARIAAttribute(AOMStringProperty::kValueText);
      if (!aria_valuetext.IsNull())
        return aria_valuetext.GetString();
      float value;
      if (ValueForRange(&value))
        return String::Number(value);
    }

    return StringValue();
  }

  // Step 2E from: http://www.w3.org/TR/accname-aam-1.1
  // "If the embedded control has role combobox or listbox, return the text
  // alternative of the chosen option."
  if (NameFromSelectedOption(recursive)) {
    StringBuilder accumulated_text;
    AXObjectVector selected_options;
    SelectedOptions(selected_options);
    for (const auto& child : selected_options) {
      if (accumulated_text.length())
        accumulated_text.Append(" ");
      accumulated_text.Append(child->ComputedName());
    }
    return accumulated_text.ToString();
  }

  // Step 2D from: http://www.w3.org/TR/accname-aam-1.1
  text_alternative =
      NativeTextAlternative(visited, name_from, related_objects, name_sources,
                            &found_text_alternative);
  const bool has_text_alternative =
      !text_alternative.IsEmpty() ||
      name_from == ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
  if (has_text_alternative && !name_sources)
    return text_alternative;

  // Step 2F / 2G from: http://www.w3.org/TR/accname-aam-1.1
  if (in_aria_labelled_by_traversal || NameFromContents(recursive)) {
    Node* node = GetNode();
    if (!IsHTMLSelectElement(node)) {  // Avoid option descendant text
      name_from = ax::mojom::NameFrom::kContents;
      if (name_sources) {
        name_sources->push_back(NameSource(found_text_alternative));
        name_sources->back().type = name_from;
      }

      if (node && node->IsTextNode())
        text_alternative = ToText(node)->wholeText();
      else if (IsHTMLBRElement(node))
        text_alternative = String("\n");
      else
        text_alternative = TextFromDescendants(visited, false);

      if (!text_alternative.IsEmpty()) {
        if (name_sources) {
          found_text_alternative = true;
          name_sources->back().text = text_alternative;
        } else {
          return text_alternative;
        }
      }
    }
  }

  // Step 2H from: http://www.w3.org/TR/accname-aam-1.1
  name_from = ax::mojom::NameFrom::kTitle;
  if (name_sources) {
    name_sources->push_back(NameSource(found_text_alternative, titleAttr));
    name_sources->back().type = name_from;
  }
  const AtomicString& title = GetAttribute(titleAttr);
  if (!title.IsEmpty()) {
    text_alternative = title;
    name_from = ax::mojom::NameFrom::kTitle;
    if (name_sources) {
      found_text_alternative = true;
      name_sources->back().text = text_alternative;
    } else {
      return text_alternative;
    }
  }

  name_from = ax::mojom::NameFrom::kUninitialized;

  if (name_sources && found_text_alternative) {
    for (NameSource& name_source : *name_sources) {
      if (!name_source.text.IsNull() && !name_source.superseded) {
        name_from = name_source.type;
        if (!name_source.related_objects.IsEmpty())
          *related_objects = name_source.related_objects;
        return name_source.text;
      }
    }
  }

  return String();
}

static bool ShouldInsertSpaceBetweenObjectsIfNeeded(
    AXObject* previous,
    AXObject* next,
    ax::mojom::NameFrom last_used_name_from,
    ax::mojom::NameFrom name_from) {
  // If we're going between two layoutObjects that are in separate
  // LayoutBoxes, add whitespace if it wasn't there already. Intuitively if
  // you have <span>Hello</span><span>World</span>, those are part of the same
  // LayoutBox so we should return "HelloWorld", but given
  // <div>Hello</div><div>World</div> the strings are in separate boxes so we
  // should return "Hello World".
  if (!IsInSameNonInlineBlockFlow(next->GetLayoutObject(),
                                  previous->GetLayoutObject()))
    return true;

  // Even if it is in the same inline block flow, if we are using a text
  // alternative such as an ARIA label or HTML title, we should separate
  // the strings. Doing so is consistent with what is stated in the AccName
  // spec and with what is done in other user agents.
  switch (last_used_name_from) {
    case ax::mojom::NameFrom::kNone:
    case ax::mojom::NameFrom::kUninitialized:
    case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
    case ax::mojom::NameFrom::kContents:
      break;
    case ax::mojom::NameFrom::kAttribute:
    case ax::mojom::NameFrom::kCaption:
    case ax::mojom::NameFrom::kPlaceholder:
    case ax::mojom::NameFrom::kRelatedElement:
    case ax::mojom::NameFrom::kTitle:
    case ax::mojom::NameFrom::kValue:
      return true;
  }
  switch (name_from) {
    case ax::mojom::NameFrom::kNone:
    case ax::mojom::NameFrom::kUninitialized:
    case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
    case ax::mojom::NameFrom::kContents:
      break;
    case ax::mojom::NameFrom::kAttribute:
    case ax::mojom::NameFrom::kCaption:
    case ax::mojom::NameFrom::kPlaceholder:
    case ax::mojom::NameFrom::kRelatedElement:
    case ax::mojom::NameFrom::kTitle:
    case ax::mojom::NameFrom::kValue:
      return true;
  }

  // According to the AccName spec, we need to separate controls from text nodes
  // using a space.
  return previous->IsControl() || next->IsControl();
}

String AXNodeObject::TextFromDescendants(AXObjectSet& visited,
                                         bool recursive) const {
  if (!CanHaveChildren() && recursive)
    return String();

  StringBuilder accumulated_text;
  AXObject* previous = nullptr;
  ax::mojom::NameFrom last_used_name_from = ax::mojom::NameFrom::kUninitialized;

  AXObjectVector children;

  HeapVector<Member<AXObject>> owned_children;
  ComputeAriaOwnsChildren(owned_children);
  for (AXObject* obj = RawFirstChild(); obj; obj = obj->RawNextSibling()) {
    if (!AXObjectCache().IsAriaOwned(obj))
      children.push_back(obj);
  }
  for (const auto& owned_child : owned_children)
    children.push_back(owned_child);

  for (AXObject* child : children) {
    // If a child is a continuation, we should ignore attributes like
    // hidden and presentational. See LAYOUT TREE WALKING ALGORITHM in
    // ax_layout_object.cc for more information on continuations.
    bool is_continuation = child->GetLayoutObject() &&
                           child->GetLayoutObject()->IsElementContinuation();

    // Don't recurse into children that are explicitly marked as aria-hidden.
    // Note that we don't call isInertOrAriaHidden because that would return
    // true if any ancestor is hidden, but we need to be able to compute the
    // accessible name of object inside hidden subtrees (for example, if
    // aria-labelledby points to an object that's hidden).
    if (!is_continuation &&
        child->AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kHidden))
      continue;

    ax::mojom::NameFrom child_name_from = ax::mojom::NameFrom::kUninitialized;
    String result;
    if (!is_continuation && child->IsPresentational()) {
      result = child->TextFromDescendants(visited, true);
    } else {
      result =
          RecursiveTextAlternative(*child, false, visited, child_name_from);
    }

    if (!result.IsEmpty() && previous && accumulated_text.length() &&
        !IsHTMLSpace(accumulated_text[accumulated_text.length() - 1]) &&
        !IsHTMLSpace(result[0])) {
      if (ShouldInsertSpaceBetweenObjectsIfNeeded(
              previous, child, last_used_name_from, child_name_from)) {
        accumulated_text.Append(' ');
      }
    }

    accumulated_text.Append(result);

    // We keep track of all non-hidden children, even those whose content is
    // not included, because all rendered children impact whether or not a
    // space should be inserted between objects. Example: A label which has
    // a single, nameless input surrounded by CSS-generated content should
    // have a space separating the before and after content.
    previous = child;

    // We only keep track of the source of children whose content is included.
    // Example: Three spans, the first with an aria-label, the second with no
    // content, and the third whose name comes from content. There should be a
    // space between the first and third because of the aria-label in the first.
    if (!result.IsEmpty())
      last_used_name_from = child_name_from;
  }

  return accumulated_text.ToString();
}

bool AXNodeObject::NameFromLabelElement() const {
  // This unfortunately duplicates a bit of logic from textAlternative and
  // nativeTextAlternative, but it's necessary because nameFromLabelElement
  // needs to be called from computeAccessibilityIsIgnored, which isn't allowed
  // to call axObjectCache->getOrCreate.

  if (!GetNode() && !GetLayoutObject())
    return false;

  // Step 2A from: http://www.w3.org/TR/accname-aam-1.1
  if (IsHiddenForTextAlternativeCalculation())
    return false;

  // Step 2B from: http://www.w3.org/TR/accname-aam-1.1
  HeapVector<Member<Element>> elements;
  Vector<String> ids;
  AriaLabelledbyElementVector(elements, ids);
  if (ids.size() > 0)
    return false;

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1
  const AtomicString& aria_label =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kLabel);
  if (!aria_label.IsEmpty())
    return false;

  // Based on
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
  // 5.1/5.5 Text inputs, Other labelable Elements
  HTMLElement* html_element = nullptr;
  if (GetNode()->IsHTMLElement())
    html_element = ToHTMLElement(GetNode());
  if (html_element && IsLabelableElement(html_element)) {
    if (ToLabelableElement(html_element)->labels() &&
        ToLabelableElement(html_element)->labels()->length() > 0)
      return true;
  }

  return false;
}

void AXNodeObject::GetRelativeBounds(AXObject** out_container,
                                     FloatRect& out_bounds_in_container,
                                     SkMatrix44& out_container_transform,
                                     bool* clips_children) const {
  if (LayoutObjectForRelativeBounds()) {
    AXObject::GetRelativeBounds(out_container, out_bounds_in_container,
                                out_container_transform, clips_children);
    return;
  }

  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  // First check if it has explicit bounds, for example if this element is tied
  // to a canvas path. When explicit coordinates are provided, the ID of the
  // explicit container element that the coordinates are relative to must be
  // provided too.
  if (!explicit_element_rect_.IsEmpty()) {
    *out_container = AXObjectCache().ObjectFromAXID(explicit_container_id_);
    if (*out_container) {
      out_bounds_in_container = FloatRect(explicit_element_rect_);
      return;
    }
  }

  // If it's in a canvas but doesn't have an explicit rect, get the bounding
  // rect of its children.
  if (GetNode()->parentElement()->IsInCanvasSubtree()) {
    Vector<FloatRect> rects;
    for (Node& child : NodeTraversal::ChildrenOf(*GetNode())) {
      if (child.IsHTMLElement()) {
        if (AXObject* obj = AXObjectCache().Get(&child)) {
          AXObject* container;
          FloatRect bounds;
          obj->GetRelativeBounds(&container, bounds, out_container_transform,
                                 clips_children);
          if (container) {
            *out_container = container;
            rects.push_back(bounds);
          }
        }
      }
    }

    if (*out_container) {
      out_bounds_in_container = UnionRect(rects);
      return;
    }
  }

  // If this object doesn't have an explicit element rect or computable from its
  // children, for now, let's return the position of the ancestor that does have
  // a position, and make it the width of that parent, and about the height of a
  // line of text, so that it's clear the object is a child of the parent.
  for (AXObject* position_provider = ParentObject(); position_provider;
       position_provider = position_provider->ParentObject()) {
    if (position_provider->IsAXLayoutObject()) {
      position_provider->GetRelativeBounds(
          out_container, out_bounds_in_container, out_container_transform,
          clips_children);
      if (*out_container)
        out_bounds_in_container.SetSize(
            FloatSize(out_bounds_in_container.Width(),
                      std::min(10.0f, out_bounds_in_container.Height())));
      break;
    }
  }
}

static Node* GetParentNodeForComputeParent(Node* node) {
  if (!node)
    return nullptr;

  Node* parent_node = nullptr;

  // Skip over <optgroup> and consider the <select> the immediate parent of an
  // <option>.
  if (auto* option = ToHTMLOptionElementOrNull(node))
    parent_node = option->OwnerSelectElement();

  if (!parent_node)
    parent_node = node->parentNode();

  return parent_node;
}

AXObject* AXNodeObject::ComputeParent() const {
  DCHECK(!IsDetached());
  if (Node* parent_node = GetParentNodeForComputeParent(GetNode()))
    return AXObjectCache().GetOrCreate(parent_node);

  return nullptr;
}

AXObject* AXNodeObject::ComputeParentIfExists() const {
  if (Node* parent_node = GetParentNodeForComputeParent(GetNode()))
    return AXObjectCache().Get(parent_node);

  return nullptr;
}

AXObject* AXNodeObject::RawFirstChild() const {
  if (!GetNode())
    return nullptr;

  Node* first_child = GetNode()->firstChild();

  if (!first_child)
    return nullptr;

  return AXObjectCache().GetOrCreate(first_child);
}

AXObject* AXNodeObject::RawNextSibling() const {
  if (!GetNode())
    return nullptr;

  Node* next_sibling = GetNode()->nextSibling();
  if (!next_sibling)
    return nullptr;

  return AXObjectCache().GetOrCreate(next_sibling);
}

void AXNodeObject::AddChildren() {
  if (IsDetached())
    return;

  // If the need to add more children in addition to existing children arises,
  // childrenChanged should have been called, leaving the object with no
  // children.
  DCHECK(!have_children_);
  have_children_ = true;

  // The only time we add children from the DOM tree to a node with a
  // layoutObject is when it's a canvas.
  if (GetLayoutObject() && !IsHTMLCanvasElement(*node_))
    return;

  AXObjectVector owned_children;
  ComputeAriaOwnsChildren(owned_children);

  for (Node& child : NodeTraversal::ChildrenOf(*node_)) {
    AXObject* child_obj = AXObjectCache().GetOrCreate(&child);
    if (child_obj && !AXObjectCache().IsAriaOwned(child_obj))
      AddChild(child_obj);
  }

  for (const auto& owned_child : owned_children)
    AddChild(owned_child);

  for (const auto& child : children_)
    child->SetParent(this);

  AddAccessibleNodeChildren();
}

void AXNodeObject::AddChild(AXObject* child) {
  InsertChild(child, children_.size());
}

void AXNodeObject::InsertChild(AXObject* child, unsigned index) {
  if (!child)
    return;

  // If the parent is asking for this child's children, then either it's the
  // first time (and clearing is a no-op), or its visibility has changed. In the
  // latter case, this child may have a stale child cached.  This can prevent
  // aria-hidden changes from working correctly. Hence, whenever a parent is
  // getting children, ensure data is not stale.
  child->ClearChildren();

  if (child->AccessibilityIsIgnored()) {
    const auto& children = child->Children();
    wtf_size_t length = children.size();
    for (wtf_size_t i = 0; i < length; ++i)
      children_.insert(index + i, children[i]);
  } else {
    DCHECK_EQ(child->ParentObject(), this);
    children_.insert(index, child);
  }
}

void AXNodeObject::ClearChildren() {
  AXObject::ClearChildren();
  children_dirty_ = false;
}

bool AXNodeObject::CanHaveChildren() const {
  // If this is an AXLayoutObject, then it's okay if this object
  // doesn't have a node - there are some layoutObjects that don't have
  // associated nodes, like scroll areas and css-generated text.
  if (!GetNode() && !IsAXLayoutObject())
    return false;

  if (GetNode() && IsHTMLMapElement(GetNode()))
    return false;  // Does not have a role, so check here

  // Placeholder gets exposed as an attribute on the input accessibility node,
  // so there's no need to add its text children.
  if (GetElement() && GetElement()->ShadowPseudoId() ==
                          AtomicString("-webkit-input-placeholder")) {
    return false;
  }

  switch (native_role_) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kScrollBar:
    // case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    // case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kToggleButton:
      return false;
    case ax::mojom::Role::kPopUpButton:
      return true;
    case ax::mojom::Role::kStaticText:
      return AXObjectCache().InlineTextBoxAccessibilityEnabled();
    default:
      break;
  }

  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kImage:
      return false;
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMath:  // role="math" is flat, unlike <math>
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kToggleButton: {
      // These roles have ChildrenPresentational: true in the ARIA spec.
      // We used to remove/prune all descendants of them, but that removed
      // useful content if the author didn't follow the spec perfectly, for
      // example if they wanted a complex radio button with a textfield child.
      // We are now only pruning these if there is a single text child,
      // otherwise the subtree is exposed. The ChildrenPresentational rule
      // is thus useful for authoring/verification tools but does not break
      // complex widget implementations.
      Element* element = GetElement();
      return element && !element->HasOneTextChild();
    }
    default:
      break;
  }
  return true;
}

Element* AXNodeObject::ActionElement() const {
  Node* node = this->GetNode();
  if (!node)
    return nullptr;

  if (node->IsElementNode() && IsClickable())
    return ToElement(node);

  Element* anchor = AnchorElement();
  Element* click_element = MouseButtonListener();
  if (!anchor || (click_element && click_element->IsDescendantOf(anchor)))
    return click_element;
  return anchor;
}

Element* AXNodeObject::AnchorElement() const {
  Node* node = this->GetNode();
  if (!node)
    return nullptr;

  AXObjectCacheImpl& cache = AXObjectCache();

  // search up the DOM tree for an anchor element
  // NOTE: this assumes that any non-image with an anchor is an
  // HTMLAnchorElement
  for (; node; node = node->parentNode()) {
    if (IsHTMLAnchorElement(*node) ||
        (node->GetLayoutObject() &&
         cache.GetOrCreate(node->GetLayoutObject())->IsAnchor()))
      return ToElement(node);
  }

  return nullptr;
}

Document* AXNodeObject::GetDocument() const {
  if (!GetNode())
    return nullptr;
  return &GetNode()->GetDocument();
}

void AXNodeObject::SetNode(Node* node) {
  node_ = node;
}

AXObject* AXNodeObject::CorrespondingControlForLabelElement() const {
  HTMLLabelElement* label_element = LabelElementContainer();
  if (!label_element)
    return nullptr;

  HTMLElement* corresponding_control = label_element->control();
  if (!corresponding_control)
    return nullptr;

  // Make sure the corresponding control isn't a descendant of this label
  // that's in the middle of being destroyed.
  if (corresponding_control->GetLayoutObject() &&
      !corresponding_control->GetLayoutObject()->Parent())
    return nullptr;

  return AXObjectCache().GetOrCreate(corresponding_control);
}

HTMLLabelElement* AXNodeObject::LabelElementContainer() const {
  if (!GetNode())
    return nullptr;

  // the control element should not be considered part of the label
  if (IsControl())
    return nullptr;

  // the link element should not be considered part of the label
  if (IsLink())
    return nullptr;

  // find if this has a ancestor that is a label
  return Traversal<HTMLLabelElement>::FirstAncestorOrSelf(*GetNode());
}

bool AXNodeObject::OnNativeFocusAction() {
  if (!CanSetFocusAttribute())
    return false;

  Document* document = GetDocument();
  if (IsWebArea()) {
    document->ClearFocusedElement();
    return true;
  }

  Element* element = GetElement();
  if (!element) {
    document->ClearFocusedElement();
    return true;
  }

  // If this node is already the currently focused node, then calling
  // focus() won't do anything.  That is a problem when focus is removed
  // from the webpage to chrome, and then returns.  In these cases, we need
  // to do what keyboard and mouse focus do, which is reset focus first.
  if (document->FocusedElement() == element)
    document->ClearFocusedElement();

  // If the object is not natively focusable but can be focused using an ARIA
  // active descendant, perform a native click instead. This will enable Web
  // apps that set accessibility focus using an active descendant to capture and
  // act on the click event. Otherwise, there is no other way to inform the app
  // that an AT has requested the focus to be changed, except if the app is
  // using AOM. To be extra safe, exclude objects that are clickable themselves.
  // This won't prevent anyone from having a click handler on the object's
  // container.
  if (!IsClickable() && element->FastHasAttribute(idAttr) &&
      CanBeActiveDescendant()) {
    return OnNativeClickAction();
  }

  element->focus();
  return true;
}

bool AXNodeObject::OnNativeIncrementAction() {
  LocalFrame* frame = GetDocument() ? GetDocument()->GetFrame() : nullptr;
  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(frame, UserGestureToken::kNewGesture);
  AlterSliderOrSpinButtonValue(true);
  return true;
}

bool AXNodeObject::OnNativeDecrementAction() {
  LocalFrame* frame = GetDocument() ? GetDocument()->GetFrame() : nullptr;
  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(frame, UserGestureToken::kNewGesture);
  AlterSliderOrSpinButtonValue(false);
  return true;
}

bool AXNodeObject::OnNativeSetSequentialFocusNavigationStartingPointAction() {
  if (!GetNode())
    return false;

  Document* document = GetDocument();
  document->ClearFocusedElement();
  document->SetSequentialFocusNavigationStartingPoint(GetNode());
  return true;
}

void AXNodeObject::ChildrenChanged() {
  if (!GetNode() && !GetLayoutObject())
    return;

  // Call SetNeedsToUpdateChildren on this node, and if this node is
  // ignored, call it on each existing parent until reaching an unignored node,
  // because unignored nodes recursively include all children of ignored
  // nodes. This method is called during layout, so we need to be careful to
  // only explore existing objects.
  AXObject* node_to_update = this;
  while (node_to_update) {
    node_to_update->SetNeedsToUpdateChildren();
    if (!node_to_update->LastKnownIsIgnoredValue())
      break;
    node_to_update = node_to_update->ParentObjectIfExists();
  }

  // If this node's children are not part of the accessibility tree then
  // skip notification and walking up the ancestors.
  // Cases where this happens:
  // - an ancestor has only presentational children, or
  // - this or an ancestor is a leaf node
  // Uses |cached_is_descendant_of_leaf_node_| to avoid updating cached
  // attributes for eachc change via | UpdateCachedAttributeValuesIfNeeded()|.
  if (!CanHaveChildren() || cached_is_descendant_of_leaf_node_)
    return;

  // Calling CanHaveChildren(), above, can occasionally detach |this|.
  if (IsDetached())
    return;

  AXObjectCache().PostNotification(this, ax::mojom::Event::kChildrenChanged);

  // Go up the accessibility parent chain, but only if the element already
  // exists. This method is called during layout, minimal work should be done.
  // If AX elements are created now, they could interrogate the layout tree
  // while it's in a funky state.  At the same time, process ARIA live region
  // changes.
  for (AXObject* parent = this; parent;
       parent = parent->ParentObjectIfExists()) {
    // These notifications always need to be sent because screenreaders are
    // reliant on them to perform.  In other words, they need to be sent even
    // when the screen reader has not accessed this live region since the last
    // update.

    // If this element supports ARIA live regions, then notify the AT of
    // changes.
    if (parent->IsLiveRegion()) {
      AXObjectCache().PostNotification(parent,
                                       ax::mojom::Event::kLiveRegionChanged);
    }

    // If this element is an ARIA text box or content editable, post a "value
    // changed" notification on it so that it behaves just like a native input
    // element or textarea.
    if (IsNonNativeTextControl())
      AXObjectCache().PostNotification(parent, ax::mojom::Event::kValueChanged);
  }
}

void AXNodeObject::UpdateChildrenIfNecessary() {
  if (NeedsToUpdateChildren())
    ClearChildren();

  AXObject::UpdateChildrenIfNecessary();
}

void AXNodeObject::SelectedOptions(AXObjectVector& options) const {
  if (IsHTMLSelectElement(GetNode())) {
    HTMLSelectElement* select = ToHTMLSelectElement(GetNode());
    for (auto* const option : *select->selectedOptions()) {
      options.push_back(AXObjectCache().GetOrCreate(option));
    }
    return;
  }

  // If the combobox or listbox is a descendant of a label element for another
  // widget, it may be ignored and Children() won't return all its children.
  // As a result, we need to use RawFirstChild and RawNextSibling to iterate
  // over the children in search of the selected option(s).

  if (RoleValue() == ax::mojom::Role::kComboBoxGrouping ||
      RoleValue() == ax::mojom::Role::kComboBoxMenuButton) {
    for (AXObject* obj = RawFirstChild(); obj; obj = obj->RawNextSibling()) {
      if (obj->RoleValue() == ax::mojom::Role::kListBox) {
        obj->SelectedOptions(options);
        return;
      }
    }
  }

  for (AXObject* obj = RawFirstChild(); obj; obj = obj->RawNextSibling()) {
    if (obj->IsSelected() == kSelectedStateTrue)
      options.push_back(obj);
  }
}

void AXNodeObject::SelectionChanged() {
  // Post the selected text changed event on the first ancestor that's
  // focused (to handle form controls, ARIA text boxes and contentEditable),
  // or the web area if the selection is just in the document somewhere.
  if (IsFocused() || IsWebArea()) {
    AXObjectCache().PostNotification(this,
                                     ax::mojom::Event::kTextSelectionChanged);
    if (GetDocument()) {
      AXObject* document_object = AXObjectCache().GetOrCreate(GetDocument());
      AXObjectCache().PostNotification(
          document_object, ax::mojom::Event::kDocumentSelectionChanged);
    }
  } else {
    AXObject::SelectionChanged();  // Calls selectionChanged on parent.
  }
}

void AXNodeObject::TextChanged() {
  // If this element supports ARIA live regions, or is part of a region with an
  // ARIA editable role, then notify the AT of changes.
  AXObjectCacheImpl& cache = AXObjectCache();
  for (Node* parent_node = GetNode(); parent_node;
       parent_node = parent_node->parentNode()) {
    AXObject* parent = cache.Get(parent_node);
    if (!parent)
      continue;

    if (parent->IsLiveRegion())
      cache.PostNotification(parent_node, ax::mojom::Event::kLiveRegionChanged);

    // If this element is an ARIA text box or content editable, post a "value
    // changed" notification on it so that it behaves just like a native input
    // element or textarea.
    if (parent->IsNonNativeTextControl())
      cache.PostNotification(parent_node, ax::mojom::Event::kValueChanged);
  }
}

void AXNodeObject::ComputeAriaOwnsChildren(
    HeapVector<Member<AXObject>>& owned_children) const {
  Vector<String> id_vector;
  // Case 1: owned children not allowed
  if (!CanHaveChildren() || IsNativeTextControl() ||
      HasContentEditableAttributeSet()) {
    if (GetNode())
      AXObjectCache().UpdateAriaOwns(this, id_vector, owned_children);
    return;
  }

  // Case 2: AOM owns property
  HeapVector<Member<Element>> elements;
  if (HasAOMProperty(AOMRelationListProperty::kOwns, elements)) {
    AXObjectCache().UpdateAriaOwns(this, id_vector, owned_children);

    for (const auto& element : elements) {
      AXObject* ax_element = ax_object_cache_->GetOrCreate(&*element);
      if (ax_element && !ax_element->AccessibilityIsIgnored())
        owned_children.push_back(ax_element);
    }

    return;
  }

  if (!HasAttribute(aria_ownsAttr))
    return;

  // Case 3: aria-owns attribute
  TokenVectorFromAttribute(id_vector, aria_ownsAttr);
  AXObjectCache().UpdateAriaOwns(this, id_vector, owned_children);
}

// Based on
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
String AXNodeObject::NativeTextAlternative(
    AXObjectSet& visited,
    ax::mojom::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources,
    bool* found_text_alternative) const {
  if (!GetNode())
    return String();

  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (name_sources)
    DCHECK(related_objects);

  String text_alternative;
  AXRelatedObjectVector local_related_objects;

  const HTMLInputElement* input_element = ToHTMLInputElementOrNull(GetNode());

  // 5.1/5.5 Text inputs, Other labelable Elements
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  HTMLElement* html_element = nullptr;
  if (GetNode()->IsHTMLElement())
    html_element = ToHTMLElement(GetNode());

  if (html_element && html_element->IsLabelable()) {
    name_from = ax::mojom::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLLabel;
    }

    LabelsNodeList* labels = ToLabelableElement(html_element)->labels();
    if (labels && labels->length() > 0) {
      HeapVector<Member<Element>> label_elements;
      for (unsigned label_index = 0; label_index < labels->length();
           ++label_index) {
        Element* label = labels->item(label_index);
        if (name_sources) {
          if (!label->getAttribute(forAttr).IsEmpty() &&
              label->getAttribute(forAttr) == html_element->GetIdAttribute()) {
            name_sources->back().native_source = kAXTextFromNativeHTMLLabelFor;
          } else {
            name_sources->back().native_source =
                kAXTextFromNativeHTMLLabelWrapped;
          }
        }
        label_elements.push_back(label);
      }

      text_alternative =
          TextFromElements(false, visited, label_elements, related_objects);
      if (!text_alternative.IsNull()) {
        *found_text_alternative = true;
        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
        } else {
          return text_alternative.StripWhiteSpace();
        }
      } else if (name_sources) {
        name_sources->back().invalid = true;
      }
    }
  }

  // 5.2 input type="button", input type="submit" and input type="reset"
  if (input_element && input_element->IsTextButton()) {
    // value attribue
    name_from = ax::mojom::NameFrom::kValue;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, valueAttr));
      name_sources->back().type = name_from;
    }
    String value = input_element->value();
    if (!value.IsNull()) {
      text_alternative = value;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    // Get default value if object is not laid out.
    // If object is laid out, it will have a layout object for the label.
    if (!GetLayoutObject()) {
      String default_label = input_element->ValueOrDefaultLabel();
      if (value.IsNull() && !default_label.IsNull()) {
        // default label
        name_from = ax::mojom::NameFrom::kContents;
        if (name_sources) {
          name_sources->push_back(NameSource(*found_text_alternative));
          name_sources->back().type = name_from;
        }
        text_alternative = default_label;
        if (name_sources) {
          NameSource& source = name_sources->back();
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }
    return text_alternative;
  }

  // 5.3 input type="image"
  if (input_element &&
      input_element->getAttribute(typeAttr) == InputTypeNames::image) {
    // alt attr
    const AtomicString& alt = input_element->getAttribute(altAttr);
    const bool is_empty = alt.IsEmpty() && !alt.IsNull();
    name_from = is_empty ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                         : ax::mojom::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, altAttr));
      name_sources->back().type = name_from;
    }
    if (!alt.IsNull()) {
      text_alternative = alt;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.attribute_value = alt;
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    // value attr
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, valueAttr));
      name_sources->back().type = name_from;
    }
    name_from = ax::mojom::NameFrom::kAttribute;
    String value = input_element->value();
    if (!value.IsNull()) {
      text_alternative = value;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    // title attr
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, titleAttr));
      name_sources->back().type = name_from;
    }
    name_from = ax::mojom::NameFrom::kTitle;
    const AtomicString& title = input_element->getAttribute(titleAttr);
    if (!title.IsNull()) {
      text_alternative = title;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.attribute_value = title;
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    // localised default value ("Submit")
    name_from = ax::mojom::NameFrom::kValue;
    text_alternative = input_element->GetLocale().QueryString(
        WebLocalizedString::kSubmitButtonDefaultLabel);
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, typeAttr));
      NameSource& source = name_sources->back();
      source.attribute_value = input_element->getAttribute(typeAttr);
      source.type = name_from;
      source.text = text_alternative;
      *found_text_alternative = true;
    } else {
      return text_alternative;
    }
    return text_alternative;
  }

  // 5.1 Text inputs - step 3 (placeholder attribute)
  if (html_element && html_element->IsTextControl()) {
    name_from = ax::mojom::NameFrom::kPlaceholder;
    if (name_sources) {
      name_sources->push_back(
          NameSource(*found_text_alternative, placeholderAttr));
      NameSource& source = name_sources->back();
      source.type = name_from;
    }
    const String placeholder = PlaceholderFromNativeAttribute();
    if (!placeholder.IsEmpty()) {
      text_alternative = placeholder;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        source.attribute_value =
            html_element->FastGetAttribute(placeholderAttr);
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }
  }

  // Also check for aria-placeholder.
  if (IsTextControl()) {
    name_from = ax::mojom::NameFrom::kPlaceholder;
    if (name_sources) {
      name_sources->push_back(
          NameSource(*found_text_alternative, aria_placeholderAttr));
      NameSource& source = name_sources->back();
      source.type = name_from;
    }
    const AtomicString& aria_placeholder =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kPlaceholder);
    if (!aria_placeholder.IsEmpty()) {
      text_alternative = aria_placeholder;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        source.attribute_value = aria_placeholder;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    return text_alternative;
  }

  // 5.7 figure and figcaption Elements
  if (GetNode()->HasTagName(figureTag)) {
    // figcaption
    name_from = ax::mojom::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLFigcaption;
    }
    Element* figcaption = nullptr;
    for (Element& element : ElementTraversal::DescendantsOf(*(GetNode()))) {
      if (element.HasTagName(figcaptionTag)) {
        figcaption = &element;
        break;
      }
    }
    if (figcaption) {
      AXObject* figcaption_ax_object = AXObjectCache().GetOrCreate(figcaption);
      if (figcaption_ax_object) {
        text_alternative =
            RecursiveTextAlternative(*figcaption_ax_object, false, visited);

        if (related_objects) {
          local_related_objects.push_back(new NameSourceRelatedObject(
              figcaption_ax_object, text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }

        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }
    return text_alternative;
  }

  // 5.8 img or area Element
  if (IsHTMLImageElement(GetNode()) || IsHTMLAreaElement(GetNode()) ||
      (GetLayoutObject() && GetLayoutObject()->IsSVGImage())) {
    // alt
    const AtomicString& alt = GetAttribute(altAttr);
    const bool is_empty = alt.IsEmpty() && !alt.IsNull();
    name_from = is_empty ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                         : ax::mojom::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, altAttr));
      name_sources->back().type = name_from;
    }
    if (!alt.IsNull()) {
      text_alternative = alt;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.attribute_value = alt;
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }
    return text_alternative;
  }

  // 5.9 table Element
  if (auto* table_element = ToHTMLTableElementOrNull(GetNode())) {
    // caption
    name_from = ax::mojom::NameFrom::kCaption;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLTableCaption;
    }
    HTMLTableCaptionElement* caption = table_element->caption();
    if (caption) {
      AXObject* caption_ax_object = AXObjectCache().GetOrCreate(caption);
      if (caption_ax_object) {
        text_alternative =
            RecursiveTextAlternative(*caption_ax_object, false, visited);
        if (related_objects) {
          local_related_objects.push_back(
              new NameSourceRelatedObject(caption_ax_object, text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }

        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }

    // summary
    name_from = ax::mojom::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, summaryAttr));
      name_sources->back().type = name_from;
    }
    const AtomicString& summary = GetAttribute(summaryAttr);
    if (!summary.IsNull()) {
      text_alternative = summary;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.attribute_value = summary;
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    return text_alternative;
  }

  // Per SVG AAM 1.0's modifications to 2D of this algorithm.
  if (GetNode()->IsSVGElement()) {
    name_from = ax::mojom::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLTitleElement;
    }
    DCHECK(GetNode()->IsContainerNode());
    Element* title = ElementTraversal::FirstChild(
        ToContainerNode(*(GetNode())), HasTagName(svg_names::kTitleTag));

    if (title) {
      AXObject* title_ax_object = AXObjectCache().GetOrCreate(title);
      if (title_ax_object && !visited.Contains(title_ax_object)) {
        text_alternative =
            RecursiveTextAlternative(*title_ax_object, false, visited);
        if (related_objects) {
          local_related_objects.push_back(
              new NameSourceRelatedObject(title_ax_object, text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }
      }
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        source.related_objects = *related_objects;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }
  }

  // Fieldset / legend.
  if (IsHTMLFieldSetElement(GetNode())) {
    name_from = ax::mojom::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLLegend;
    }
    HTMLElement* legend = ToHTMLFieldSetElement(GetNode())->Legend();
    if (legend) {
      AXObject* legend_ax_object = AXObjectCache().GetOrCreate(legend);
      // Avoid an infinite loop
      if (legend_ax_object && !visited.Contains(legend_ax_object)) {
        text_alternative =
            RecursiveTextAlternative(*legend_ax_object, false, visited);

        if (related_objects) {
          local_related_objects.push_back(
              new NameSourceRelatedObject(legend_ax_object, text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }

        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }
  }

  // Document.
  if (IsWebArea()) {
    Document* document = this->GetDocument();
    if (document) {
      name_from = ax::mojom::NameFrom::kAttribute;
      if (name_sources) {
        name_sources->push_back(
            NameSource(found_text_alternative, aria_labelAttr));
        name_sources->back().type = name_from;
      }
      if (Element* document_element = document->documentElement()) {
        const AtomicString& aria_label =
            AccessibleNode::GetPropertyOrARIAAttribute(
                document_element, AOMStringProperty::kLabel);
        if (!aria_label.IsEmpty()) {
          text_alternative = aria_label;

          if (name_sources) {
            NameSource& source = name_sources->back();
            source.text = text_alternative;
            source.attribute_value = aria_label;
            *found_text_alternative = true;
          } else {
            return text_alternative;
          }
        }
      }

      name_from = ax::mojom::NameFrom::kRelatedElement;
      if (name_sources) {
        name_sources->push_back(NameSource(*found_text_alternative));
        name_sources->back().type = name_from;
        name_sources->back().native_source = kAXTextFromNativeHTMLTitleElement;
      }

      text_alternative = document->title();

      Element* title_element = document->TitleElement();
      AXObject* title_ax_object = AXObjectCache().GetOrCreate(title_element);
      if (title_ax_object) {
        if (related_objects) {
          local_related_objects.push_back(
              new NameSourceRelatedObject(title_ax_object, text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }

        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }
  }

  return text_alternative;
}

String AXNodeObject::Description(ax::mojom::NameFrom name_from,
                                 ax::mojom::DescriptionFrom& description_from,
                                 AXObjectVector* description_objects) const {
  AXRelatedObjectVector related_objects;
  String result =
      Description(name_from, description_from, nullptr, &related_objects);
  if (description_objects) {
    description_objects->clear();
    for (NameSourceRelatedObject* related_object : related_objects)
      description_objects->push_back(related_object->object);
  }

  result = CollapseWhitespace(result);

  if (RoleValue() == ax::mojom::Role::kSpinButton && DatetimeAncestor()) {
    // Fields inside a datetime control need to merge the field description
    // with the description of the <input> element.
    const AXObject* datetime_ancestor = DatetimeAncestor();
    ax::mojom::NameFrom name_from;
    datetime_ancestor->GetName(name_from, nullptr);
    description_objects->clear();
    String ancestor_description = DatetimeAncestor()->Description(
        name_from, description_from, description_objects);
    if (!result.IsEmpty() && !ancestor_description.IsEmpty())
      return result + " " + ancestor_description;
    if (!ancestor_description.IsEmpty())
      return ancestor_description;
  }

  return result;
}

// Based on
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
String AXNodeObject::Description(ax::mojom::NameFrom name_from,
                                 ax::mojom::DescriptionFrom& description_from,
                                 DescriptionSources* description_sources,
                                 AXRelatedObjectVector* related_objects) const {
  // If descriptionSources is non-null, relatedObjects is used in filling it in,
  // so it must be non-null as well.
  if (description_sources)
    DCHECK(related_objects);

  if (!GetNode())
    return String();

  String description;
  bool found_description = false;

  description_from = ax::mojom::DescriptionFrom::kRelatedElement;
  if (description_sources) {
    description_sources->push_back(
        DescriptionSource(found_description, aria_describedbyAttr));
    description_sources->back().type = description_from;
  }

  // aria-describedby overrides any other accessible description, from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  // AOM version.
  HeapVector<Member<Element>> elements;
  if (HasAOMProperty(AOMRelationListProperty::kDescribedBy, elements)) {
    AXObjectSet visited;
    description = TextFromElements(true, visited, elements, related_objects);
    if (!description.IsNull()) {
      if (description_sources) {
        DescriptionSource& source = description_sources->back();
        source.type = description_from;
        source.related_objects = *related_objects;
        source.text = description;
        found_description = true;
      } else {
        return description;
      }
    } else if (description_sources) {
      description_sources->back().invalid = true;
    }
  }

  // aria-describedby overrides any other accessible description, from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  const AtomicString& aria_describedby = GetAttribute(aria_describedbyAttr);
  if (!aria_describedby.IsNull()) {
    if (description_sources)
      description_sources->back().attribute_value = aria_describedby;

    Vector<String> ids;
    description = TextFromAriaDescribedby(related_objects, ids);
    AXObjectCache().UpdateReverseRelations(this, ids);

    if (!description.IsNull()) {
      if (description_sources) {
        DescriptionSource& source = description_sources->back();
        source.type = description_from;
        source.related_objects = *related_objects;
        source.text = description;
        found_description = true;
      } else {
        return description;
      }
    } else if (description_sources) {
      description_sources->back().invalid = true;
    }
  }

  const HTMLInputElement* input_element = ToHTMLInputElementOrNull(GetNode());

  // value, 5.2.2 from: http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (name_from != ax::mojom::NameFrom::kValue && input_element &&
      input_element->IsTextButton()) {
    description_from = ax::mojom::DescriptionFrom::kAttribute;
    if (description_sources) {
      description_sources->push_back(
          DescriptionSource(found_description, valueAttr));
      description_sources->back().type = description_from;
    }
    String value = input_element->value();
    if (!value.IsNull()) {
      description = value;
      if (description_sources) {
        DescriptionSource& source = description_sources->back();
        source.text = description;
        found_description = true;
      } else {
        return description;
      }
    }
  }

  // table caption, 5.9.2 from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (name_from != ax::mojom::NameFrom::kCaption &&
      IsHTMLTableElement(GetNode())) {
    HTMLTableElement* table_element = ToHTMLTableElement(GetNode());

    description_from = ax::mojom::DescriptionFrom::kRelatedElement;
    if (description_sources) {
      description_sources->push_back(DescriptionSource(found_description));
      description_sources->back().type = description_from;
      description_sources->back().native_source =
          kAXTextFromNativeHTMLTableCaption;
    }
    HTMLTableCaptionElement* caption = table_element->caption();
    if (caption) {
      AXObject* caption_ax_object = AXObjectCache().GetOrCreate(caption);
      if (caption_ax_object) {
        AXObjectSet visited;
        description =
            RecursiveTextAlternative(*caption_ax_object, false, visited);
        if (related_objects)
          related_objects->push_back(
              new NameSourceRelatedObject(caption_ax_object, description));

        if (description_sources) {
          DescriptionSource& source = description_sources->back();
          source.related_objects = *related_objects;
          source.text = description;
          found_description = true;
        } else {
          return description;
        }
      }
    }
  }

  // summary, 5.6.2 from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (name_from != ax::mojom::NameFrom::kContents &&
      IsHTMLSummaryElement(GetNode())) {
    description_from = ax::mojom::DescriptionFrom::kContents;
    if (description_sources) {
      description_sources->push_back(DescriptionSource(found_description));
      description_sources->back().type = description_from;
    }

    AXObjectSet visited;
    description = TextFromDescendants(visited, false);

    if (!description.IsEmpty()) {
      if (description_sources) {
        found_description = true;
        description_sources->back().text = description;
      } else {
        return description;
      }
    }
  }

  // title attribute, from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (name_from != ax::mojom::NameFrom::kTitle) {
    description_from = ax::mojom::DescriptionFrom::kAttribute;
    if (description_sources) {
      description_sources->push_back(
          DescriptionSource(found_description, titleAttr));
      description_sources->back().type = description_from;
    }
    const AtomicString& title = GetAttribute(titleAttr);
    if (!title.IsEmpty()) {
      description = title;
      if (description_sources) {
        found_description = true;
        description_sources->back().text = description;
      } else {
        return description;
      }
    }
  }

  // aria-help.
  // FIXME: this is not part of the official standard, but it's needed because
  // the built-in date/time controls use it.
  description_from = ax::mojom::DescriptionFrom::kAttribute;
  if (description_sources) {
    description_sources->push_back(
        DescriptionSource(found_description, aria_helpAttr));
    description_sources->back().type = description_from;
  }
  const AtomicString& help = GetAttribute(aria_helpAttr);
  if (!help.IsEmpty()) {
    description = help;
    if (description_sources) {
      found_description = true;
      description_sources->back().text = description;
    } else {
      return description;
    }
  }

  description_from = ax::mojom::DescriptionFrom::kUninitialized;

  if (found_description) {
    for (DescriptionSource& description_source : *description_sources) {
      if (!description_source.text.IsNull() && !description_source.superseded) {
        description_from = description_source.type;
        if (!description_source.related_objects.IsEmpty())
          *related_objects = description_source.related_objects;
        return description_source.text;
      }
    }
  }

  return String();
}

String AXNodeObject::Placeholder(ax::mojom::NameFrom name_from) const {
  if (name_from == ax::mojom::NameFrom::kPlaceholder)
    return String();

  Node* node = GetNode();
  if (!node || !node->IsHTMLElement())
    return String();

  String native_placeholder = PlaceholderFromNativeAttribute();
  if (!native_placeholder.IsEmpty())
    return native_placeholder;

  const AtomicString& aria_placeholder =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kPlaceholder);
  if (!aria_placeholder.IsEmpty())
    return aria_placeholder;

  return String();
}

String AXNodeObject::PlaceholderFromNativeAttribute() const {
  Node* node = GetNode();
  if (!node || !blink::IsTextControl(*node))
    return String();
  return ToTextControl(node)->StrippedPlaceholder();
}

void AXNodeObject::Trace(blink::Visitor* visitor) {
  visitor->Trace(node_);
  AXObject::Trace(visitor);
}

}  // namespace blink
