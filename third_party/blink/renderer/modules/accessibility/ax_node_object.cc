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
#include <memory>

#include <algorithm>

#include "base/optional.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
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
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_table_caption_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace {

blink::HTMLMapElement* GetMapForImage(blink::LayoutObject* layout_object) {
  blink::LayoutImage* layout_image =
      blink::DynamicTo<blink::LayoutImage>(layout_object);
  if (!layout_image)
    return nullptr;

  return layout_image->ImageMap();
}

bool IsNeutralWithinTable(blink::AXObject* obj) {
  if (!obj)
    return false;
  ax::mojom::blink::Role role = obj->RoleValue();
  return role == ax::mojom::blink::Role::kGroup ||
         role == ax::mojom::blink::Role::kGenericContainer ||
         role == ax::mojom::blink::Role::kRowGroup;
}

// Within a table, provide the accessible, semantic parent of |node|,
// by traversing the DOM tree, ignoring elements that are neutral in a table.
// Return the AXObject for the ancestor.
blink::AXObject* GetDOMTableAXAncestor(blink::Node* node,
                                       blink::AXObjectCacheImpl& cache) {
  // Used by code to determine roles of elements inside of an HTML table,
  // Use DOM to get parent since parent_ is not initialized yet when role is
  // being computed, and because HTML table structure should not take into
  // account aria-owns.
  if (!node)
    return nullptr;

  while (true) {
    node = blink::LayoutTreeBuilderTraversal::Parent(*node);
    if (!node)
      return nullptr;

    blink::AXObject* ax_object = cache.GetOrCreate(node);
    if (ax_object && !IsNeutralWithinTable(ax_object))
      return ax_object;
  }

  return nullptr;
}

enum class AXAction {
  kActionIncrement = 0,
  kActionDecrement,
};

blink::KeyboardEvent* CreateKeyboardEvent(
    blink::LocalDOMWindow* local_dom_window,
    blink::WebInputEvent::Type type,
    AXAction action) {
  blink::WebKeyboardEvent key(type,
                              blink::WebInputEvent::Modifiers::kNoModifiers,
                              base::TimeTicks::Now());

  // TODO(crbug.com/1099069): Fire different arrow events depending on
  // orientation and dir (RTL/LTR)
  switch (action) {
    case AXAction::kActionIncrement:
      key.dom_key = ui::DomKey::ARROW_UP;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_UP);
      key.native_key_code = key.windows_key_code = blink::VKEY_UP;
      break;
    case AXAction::kActionDecrement:
      key.dom_key = ui::DomKey::ARROW_DOWN;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_DOWN);
      key.native_key_code = key.windows_key_code = blink::VKEY_DOWN;
      break;
  }

  return blink::KeyboardEvent::Create(key, local_dom_window, true);
}

unsigned TextStyleFlag(ax::mojom::blink::TextStyle text_style_enum) {
  return static_cast<unsigned>(1 << static_cast<int>(text_style_enum));
}

ax::mojom::blink::TextDecorationStyle
TextDecorationStyleToAXTextDecorationStyle(
    const blink::ETextDecorationStyle text_decoration_style) {
  switch (text_decoration_style) {
    case blink::ETextDecorationStyle::kDashed:
      return ax::mojom::blink::TextDecorationStyle::kDashed;
    case blink::ETextDecorationStyle::kSolid:
      return ax::mojom::blink::TextDecorationStyle::kSolid;
    case blink::ETextDecorationStyle::kDotted:
      return ax::mojom::blink::TextDecorationStyle::kDotted;
    case blink::ETextDecorationStyle::kDouble:
      return ax::mojom::blink::TextDecorationStyle::kDouble;
    case blink::ETextDecorationStyle::kWavy:
      return ax::mojom::blink::TextDecorationStyle::kWavy;
  }

  NOTREACHED();
  return ax::mojom::blink::TextDecorationStyle::kNone;
}

}  // namespace

namespace blink {

using html_names::kAltAttr;
using html_names::kTitleAttr;
using html_names::kTypeAttr;
using html_names::kValueAttr;

// In ARIA 1.1, default value of aria-level was changed to 2.
const int kDefaultHeadingLevel = 2;

AXNodeObject::AXNodeObject(Node* node, AXObjectCacheImpl& ax_object_cache)
    : AXObject(ax_object_cache),
      native_role_(ax::mojom::blink::Role::kUnknown),
      node_(node) {}

AXNodeObject::~AXNodeObject() {
  DCHECK(!node_);
}

void AXNodeObject::AlterSliderOrSpinButtonValue(bool increase) {
  if (!GetNode())
    return;
  if (!IsSlider() && !IsSpinButton())
    return;

  float value;
  if (!ValueForRange(&value))
    return;

  // If no step was provided on the element, use a default value.
  float step;
  if (!StepValueForRange(&step)) {
    if (IsNativeSlider() || IsNativeSpinButton()) {
      step = StepRange().Step().ToString().ToFloat();
    } else {
      return;
    }
  }

  value += increase ? step : -step;

  // If this is a native element, set the value directly.
  if (native_role_ == ax::mojom::blink::Role::kSlider ||
      native_role_ == ax::mojom::blink::Role::kSpinButton) {
    OnNativeSetValueAction(String::Number(value));
    // Dispatching an event could result in changes to the document, like
    // this AXObject becoming detached.
    if (IsDetached())
      return;

    AXObjectCache().HandleValueChanged(GetNode());
    return;
  }

  // TODO(crbug.com/1099069): add a separate flag for keyboard event synthesis
  if (!RuntimeEnabledFeatures::AccessibilityObjectModelEnabled())
    return;

  // Otherwise, fire a keyboard event instead.
  AXAction action =
      increase ? AXAction::kActionIncrement : AXAction::kActionDecrement;
  LocalDOMWindow* local_dom_window = GetDocument()->domWindow();

  KeyboardEvent* keydown = CreateKeyboardEvent(
      local_dom_window, WebInputEvent::Type::kRawKeyDown, action);
  GetNode()->DispatchEvent(*keydown);

  // TODO(crbug.com/1099069): add a brief pause between keydown and keyup?
  // TODO(crbug.com/1099069): fire a "char" event depending on platform?

  // The keydown handler may have caused the node to be removed.
  if (!GetNode())
    return;

  KeyboardEvent* keyup = CreateKeyboardEvent(
      local_dom_window, WebInputEvent::Type::kKeyUp, action);
  GetNode()->DispatchEvent(*keyup);
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
  return ax_descendant && ax_descendant->IsVisible() ? ax_descendant : nullptr;
}

AXObjectInclusion AXNodeObject::ShouldIncludeBasedOnSemantics(
    IgnoredReasons* ignored_reasons) const {
  // If this element is within a parent that cannot have children, it should not
  // be exposed.
  if (IsDescendantOfLeafNode()) {
    if (ignored_reasons)
      ignored_reasons->push_back(
          IgnoredReason(kAXAncestorIsLeafNode, LeafNodeAncestor()));
    return kIgnoreObject;
  }

  if (HasInheritedPresentationalRole()) {
    if (ignored_reasons) {
      const AXObject* inherits_from = InheritsPresentationalRoleFrom();
      if (inherits_from == this) {
        ignored_reasons->push_back(IgnoredReason(kAXPresentational));
      } else {
        ignored_reasons->push_back(
            IgnoredReason(kAXInheritsPresentation, inherits_from));
      }
    }
    return kIgnoreObject;
  }

  // Objects inside a portal should be ignored. Portals don't directly expose
  // their contents as the contents are not focusable (portals do not currently
  // support input events). Portals do use their contents to compute a default
  // accessible name.
  if (GetDocument() && GetDocument()->GetPage() &&
      GetDocument()->GetPage()->InsidePortal()) {
    return kIgnoreObject;
  }

  if (IsTableLikeRole() || IsTableRowLikeRole() || IsTableCellLikeRole())
    return kIncludeObject;

  // Ignore labels that are already referenced by a control but are not set to
  // be focusable.
  AXObject* control_ax_object = CorrespondingControlAXObjectForLabelElement();
  if (control_ax_object && control_ax_object->IsCheckboxOrRadio() &&
      control_ax_object->NameFromLabelElement() &&
      AccessibleNode::GetPropertyOrARIAAttribute(
          LabelElementContainer(), AOMStringProperty::kRole) == g_null_atom) {
    AXObject* label_ax_object = CorrespondingLabelAXObject();
    // If the label is set to be focusable, we should expose it.
    if (label_ax_object && label_ax_object->CanSetFocusAttribute())
      return kIncludeObject;

    if (ignored_reasons) {
      if (label_ax_object && label_ax_object != this)
        ignored_reasons->push_back(
            IgnoredReason(kAXLabelContainer, label_ax_object));

      ignored_reasons->push_back(IgnoredReason(kAXLabelFor, control_ax_object));
    }
    return kIgnoreObject;
  }

  if (GetNode() && !IsA<HTMLBodyElement>(GetNode()) && CanSetFocusAttribute())
    return kIncludeObject;

  if (IsLink() || IsInPageLinkTarget())
    return kIncludeObject;

  // A click handler might be placed on an otherwise ignored non-empty block
  // element, e.g. a div. We shouldn't ignore such elements because if an AT
  // sees the |ax::mojom::blink::DefaultActionVerb::kClickAncestor|, it will
  // look for the clickable ancestor and it expects to find one.
  if (IsClickable())
    return kIncludeObject;

  if (IsHeading() || IsLandmarkRelated())
    return kIncludeObject;

  // Header and footer tags may also be exposed as landmark roles but not
  // always.
  if (GetNode() && (GetNode()->HasTagName(html_names::kHeaderTag) ||
                    GetNode()->HasTagName(html_names::kFooterTag)))
    return kIncludeObject;

  // All controls are accessible.
  if (IsControl())
    return kIncludeObject;

  // Anything with an explicit ARIA role should be included.
  if (AriaRoleAttribute() != ax::mojom::blink::Role::kUnknown)
    return kIncludeObject;

  // Anything with CSS alt should be included.
  // Note: this is duplicated from AXLayoutObject because CSS alt text may apply
  // to both Elements and pseudo-elements.
  base::Optional<String> alt_text = GetCSSAltText(GetNode());
  if (alt_text && !alt_text->IsEmpty())
    return kIncludeObject;

  // Don't ignore labels, because they serve as TitleUIElements.
  Node* node = GetNode();
  if (IsA<HTMLLabelElement>(node))
    return kIncludeObject;

  // Don't ignored legends, because JAWS uses them to determine redundant text.
  if (IsA<HTMLLegendElement>(node))
    return kIncludeObject;

  // Anything that is content editable should not be ignored.
  // However, one cannot just call node->hasEditableStyle() since that will ask
  // if its parents are also editable. Only the top level content editable
  // region should be exposed.
  if (HasContentEditableAttributeSet())
    return kIncludeObject;

  static const HashSet<ax::mojom::blink::Role> always_included_computed_roles =
      {
          ax::mojom::blink::Role::kAbbr,
          ax::mojom::blink::Role::kBlockquote,
          ax::mojom::blink::Role::kContentDeletion,
          ax::mojom::blink::Role::kContentInsertion,
          ax::mojom::blink::Role::kDetails,
          ax::mojom::blink::Role::kDescriptionList,
          ax::mojom::blink::Role::kDescriptionListDetail,
          ax::mojom::blink::Role::kDescriptionListTerm,
          ax::mojom::blink::Role::kDialog,
          ax::mojom::blink::Role::kFigcaption,
          ax::mojom::blink::Role::kFigure,
          ax::mojom::blink::Role::kList,
          ax::mojom::blink::Role::kListItem,
          ax::mojom::blink::Role::kMark,
          ax::mojom::blink::Role::kMath,
          ax::mojom::blink::Role::kMeter,
          ax::mojom::blink::Role::kPluginObject,
          ax::mojom::blink::Role::kProgressIndicator,
          ax::mojom::blink::Role::kRuby,
          ax::mojom::blink::Role::kSplitter,
          ax::mojom::blink::Role::kTime,
      };

  if (always_included_computed_roles.find(RoleValue()) !=
      always_included_computed_roles.end())
    return kIncludeObject;

  // Avoid double speech. The ruby text describes pronunciation of the ruby
  // base, and generally produces redundant screen reader output. Expose it only
  // as a description on the <ruby> element so that screen reader users can
  // toggle it on/off as with other descriptions/annotations.
  if (RoleValue() == ax::mojom::blink::Role::kRubyAnnotation ||
      (RoleValue() == ax::mojom::blink::Role::kStaticText && ParentObject() &&
       ParentObject()->RoleValue() ==
           ax::mojom::blink::Role::kRubyAnnotation)) {
    return kIgnoreObject;
  }

  // If this element has aria attributes on it, it should not be ignored.
  if (HasGlobalARIAAttribute())
    return kIncludeObject;

  bool has_non_empty_alt_attribute = !GetAttribute(kAltAttr).IsEmpty();
  if (IsImage()) {
    if (has_non_empty_alt_attribute || GetAttribute(kAltAttr).IsNull())
      return kIncludeObject;
    else if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXEmptyAlt));
    return kIgnoreObject;
  }
  // Using the title or accessibility description (so we
  // check if there's some kind of accessible name for the element)
  // to decide an element's visibility is not as definitive as
  // previous checks, so this should remain as one of the last.
  //
  // These checks are simplified in the interest of execution speed;
  // for example, any element having an alt attribute will make it
  // not ignored, rather than just images.
  if (HasAriaAttribute() || !GetAttribute(kTitleAttr).IsEmpty() ||
      has_non_empty_alt_attribute)
    return kIncludeObject;

  // <span> tags are inline tags and not meant to convey information if they
  // have no other ARIA information on them. If we don't ignore them, they may
  // emit signals expected to come from their parent.
  if (node && IsA<HTMLSpanElement>(node)) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return kIgnoreObject;
  }

  return kDefaultBehavior;
}

base::Optional<String> AXNodeObject::GetCSSAltText(const Node* node) {
  if (!node || !node->GetComputedStyle() ||
      node->GetComputedStyle()->ContentBehavesAsNormal()) {
    return base::nullopt;
  }

  const ComputedStyle* style = node->GetComputedStyle();
  if (node->IsPseudoElement()) {
    for (const ContentData* content_data = style->GetContentData();
         content_data; content_data = content_data->Next()) {
      if (content_data->IsAltText())
        return To<AltTextContentData>(content_data)->GetText();
    }
    return base::nullopt;
  }

  // If the content property is used on a non-pseudo element, match the
  // behaviour of LayoutObject::CreateObject and only honour the style if
  // there is exactly one piece of content, which is an image.
  const ContentData* content_data = style->GetContentData();
  if (content_data && content_data->IsImage() && content_data->Next() &&
      content_data->Next()->IsAltText()) {
    return To<AltTextContentData>(content_data->Next())->GetText();
  }

  return base::nullopt;
}

bool AXNodeObject::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
#if DCHECK_IS_ON()
  // Double-check that an AXObject is never accessed before
  // it's been initialized.
  DCHECK(initialized_);
#endif

  // If we don't have a node, then ignore the node object.
  // TODO(vmpstr/aleventhal): Investigate how this can happen.
  if (!GetNode()) {
    NOTREACHED();
    return true;
  }

  // All nodes must have an unignored parent within their tree under
  // the root node of the web area, so force that node to always be unignored.
  if (IsWebArea())
    return false;

  DCHECK_NE(role_, ax::mojom::blink::Role::kUnknown);
  // Use AXLayoutObject::ComputeAccessibilityIsIgnored().
  DCHECK(!GetLayoutObject());

  if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*GetNode())) {
    if (DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
            *GetNode(), DisplayLockActivationReason::kAccessibility)) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXNotRendered));
      return true;
    }
    return ShouldIncludeBasedOnSemantics(ignored_reasons) == kIgnoreObject;
  }

  auto* element = DynamicTo<Element>(GetNode());
  if (!element)
    element = GetNode()->parentElement();

  if (!element)
    return true;

  if (element->IsInCanvasSubtree())
    return ShouldIncludeBasedOnSemantics(ignored_reasons) == kIgnoreObject;

  if (AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty::kHidden))
    return false;

  if (element->HasDisplayContentsStyle()) {
    if (ShouldIncludeBasedOnSemantics(ignored_reasons) == kIncludeObject)
      return false;
  }

  if (ignored_reasons)
    ignored_reasons->push_back(IgnoredReason(kAXNotRendered));
  return true;
}

static bool IsListElement(Node* node) {
  return IsA<HTMLUListElement>(*node) || IsA<HTMLOListElement>(*node) ||
         IsA<HTMLDListElement>(*node);
}

static bool IsRequiredOwnedElement(AXObject* parent,
                                   ax::mojom::blink::Role current_role,
                                   HTMLElement* current_element) {
  Node* parent_node = parent->GetNode();
  auto* parent_html_element = DynamicTo<HTMLElement>(parent_node);
  if (!parent_html_element)
    return false;

  if (current_role == ax::mojom::blink::Role::kListItem)
    return IsListElement(parent_node);
  if (current_role == ax::mojom::blink::Role::kListMarker)
    return IsA<HTMLLIElement>(*parent_node);

  if (!current_element)
    return false;
  if (IsA<HTMLTableCellElement>(*current_element))
    return IsA<HTMLTableRowElement>(*parent_node);
  if (IsA<HTMLTableRowElement>(*current_element))
    return IsA<HTMLTableSectionElement>(parent_html_element);

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
  // presentation to any owned elements that do not have an explicit role.
  if (AriaRoleAttribute() != ax::mojom::blink::Role::kUnknown)
    return nullptr;

  AXObject* parent = ParentObject();
  if (!parent)
    return nullptr;

  auto* element = DynamicTo<HTMLElement>(GetNode());
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
    landmark_roles_not_allowed.insert(html_names::kArticleTag);
    landmark_roles_not_allowed.insert(html_names::kAsideTag);
    landmark_roles_not_allowed.insert(html_names::kNavTag);
    landmark_roles_not_allowed.insert(html_names::kSectionTag);
    landmark_roles_not_allowed.insert(html_names::kBlockquoteTag);
    landmark_roles_not_allowed.insert(html_names::kDetailsTag);
    landmark_roles_not_allowed.insert(html_names::kFieldsetTag);
    landmark_roles_not_allowed.insert(html_names::kFigureTag);
    landmark_roles_not_allowed.insert(html_names::kTdTag);
    landmark_roles_not_allowed.insert(html_names::kMainTag);
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

static bool IsNonEmptyNonHeaderCell(const Node* cell) {
  return cell && cell->hasChildren() && cell->HasTagName(html_names::kTdTag);
}

static bool IsHeaderCell(const Node* cell) {
  return cell && cell->HasTagName(html_names::kThTag);
}

static ax::mojom::blink::Role DecideRoleFromSiblings(Element* cell) {
  // If this header is only cell in its row, it is a column header.
  // It is also a column header if it has a header on either side of it.
  // If instead it has a non-empty td element next to it, it is a row header.

  const Node* next_cell = LayoutTreeBuilderTraversal::NextSibling(*cell);
  const Node* previous_cell =
      LayoutTreeBuilderTraversal::PreviousSibling(*cell);
  if (!next_cell && !previous_cell)
    return ax::mojom::blink::Role::kColumnHeader;
  if (IsHeaderCell(next_cell) && IsHeaderCell(previous_cell))
    return ax::mojom::blink::Role::kColumnHeader;
  if (IsNonEmptyNonHeaderCell(next_cell) ||
      IsNonEmptyNonHeaderCell(previous_cell))
    return ax::mojom::blink::Role::kRowHeader;

  const auto* row = To<Element>(cell->parentNode());
  if (!row || !row->HasTagName(html_names::kTrTag))
    return ax::mojom::blink::Role::kColumnHeader;

  // If this row's first or last cell is a non-empty td, this is a row header.
  // Do the same check for the second and second-to-last cells because tables
  // often have an empty cell at the intersection of the row and column headers.
  const Element* first_cell = ElementTraversal::FirstChild(*row);
  DCHECK(first_cell);

  const Element* last_cell = ElementTraversal::LastChild(*row);
  DCHECK(last_cell);

  if (IsNonEmptyNonHeaderCell(first_cell) || IsNonEmptyNonHeaderCell(last_cell))
    return ax::mojom::blink::Role::kRowHeader;

  if (IsNonEmptyNonHeaderCell(ElementTraversal::NextSibling(*first_cell)) ||
      IsNonEmptyNonHeaderCell(ElementTraversal::PreviousSibling(*last_cell)))
    return ax::mojom::blink::Role::kRowHeader;

  // We have no evidence that this is not a column header.
  return ax::mojom::blink::Role::kColumnHeader;
}

ax::mojom::blink::Role AXNodeObject::DetermineTableSectionRole() const {
  if (!GetElement())
    return ax::mojom::blink::Role::kUnknown;

  AXObject* parent = GetDOMTableAXAncestor(GetNode(), AXObjectCache());
  if (!parent || !parent->IsTableLikeRole())
    return ax::mojom::blink::Role::kGenericContainer;

  if (parent->RoleValue() == ax::mojom::blink::Role::kLayoutTable)
    return ax::mojom::blink::Role::kGenericContainer;

  return ax::mojom::blink::Role::kRowGroup;
}

ax::mojom::blink::Role AXNodeObject::DetermineTableRowRole() const {
  AXObject* parent = GetDOMTableAXAncestor(GetNode(), AXObjectCache());

  if (!parent || !parent->IsTableLikeRole())
    return ax::mojom::blink::Role::kGenericContainer;

  if (parent->RoleValue() == ax::mojom::blink::Role::kLayoutTable)
    return ax::mojom::blink::Role::kLayoutTableRow;

  return ax::mojom::blink::Role::kRow;
}

ax::mojom::blink::Role AXNodeObject::DetermineTableCellRole() const {
  AXObject* parent = GetDOMTableAXAncestor(GetNode(), AXObjectCache());
  if (!parent || !parent->IsTableRowLikeRole())
    return ax::mojom::blink::Role::kGenericContainer;

  // Ensure table container.
  AXObject* grandparent =
      GetDOMTableAXAncestor(parent->GetNode(), AXObjectCache());
  if (!grandparent || !grandparent->IsTableLikeRole())
    return ax::mojom::blink::Role::kGenericContainer;

  if (parent->RoleValue() == ax::mojom::blink::Role::kLayoutTableRow)
    return ax::mojom::blink::Role::kLayoutTableCell;

  if (!GetElement() || !GetNode()->HasTagName(html_names::kThTag))
    return ax::mojom::blink::Role::kCell;

  const AtomicString& scope = GetAttribute(html_names::kScopeAttr);
  if (EqualIgnoringASCIICase(scope, "row") ||
      EqualIgnoringASCIICase(scope, "rowgroup"))
    return ax::mojom::blink::Role::kRowHeader;
  if (EqualIgnoringASCIICase(scope, "col") ||
      EqualIgnoringASCIICase(scope, "colgroup"))
    return ax::mojom::blink::Role::kColumnHeader;

  return DecideRoleFromSiblings(GetElement());
}

// TODO(accessibility) Needs a new name as it does check ARIA, including
// checking the @role for an iframe, and @aria-haspopup/aria-pressed via
// ButtonType().
// TODO(accessibility) This value is cached in native_role_ so it needs to
// be recached if anything it depends on change, such as IsClickable(),
// DataList(), aria-pressed, the parent's tag, role on an iframe, etc.
ax::mojom::blink::Role AXNodeObject::NativeRoleIgnoringAria() const {
  if (!GetNode())
    return RoleFromLayoutObject(ax::mojom::blink::Role::kUnknown);

  if (IsA<HTMLImageElement>(GetNode()))
    return ax::mojom::blink::Role::kImage;

  if (GetNode()->IsLink()) {  // <a href> or <svg:a xlink:href>
    // |HTMLAnchorElement| sets isLink only when it has kHrefAttr.
    return ax::mojom::blink::Role::kLink;
  }

  if (IsA<HTMLPortalElement>(*GetNode())) {
    return ax::mojom::blink::Role::kPortal;
  }

  if (IsA<HTMLAnchorElement>(*GetNode())) {
    // We assume that an anchor element is LinkRole if it has event listeners
    // even though it doesn't have kHrefAttr.
    if (IsClickable())
      return ax::mojom::blink::Role::kLink;
    return ax::mojom::blink::Role::kAnchor;
  }

  if (IsA<HTMLButtonElement>(*GetNode()))
    return ButtonRoleType();

  if (IsA<HTMLDetailsElement>(*GetNode()))
    return ax::mojom::blink::Role::kDetails;

  if (IsA<HTMLSummaryElement>(*GetNode())) {
    ContainerNode* parent = LayoutTreeBuilderTraversal::Parent(*GetNode());
    if (IsA<HTMLSlotElement>(parent))
      parent = LayoutTreeBuilderTraversal::Parent(*parent);
    if (parent && IsA<HTMLDetailsElement>(parent))
      return ax::mojom::blink::Role::kDisclosureTriangle;
    return RoleFromLayoutObject(ax::mojom::blink::Role::kUnknown);
  }

  // Chrome exposes both table markup and table CSS as a tables, letting
  // the screen reader determine what to do for CSS tables.
  if (IsA<HTMLTableElement>(*GetNode())) {
    if (IsDataTable())
      return ax::mojom::blink::Role::kTable;
    else
      return ax::mojom::blink::Role::kLayoutTable;
  }
  if (IsA<HTMLTableRowElement>(*GetNode()))
    return DetermineTableRowRole();
  if (IsA<HTMLTableCellElement>(*GetNode()))
    return DetermineTableCellRole();
  if (IsA<HTMLTableSectionElement>(*GetNode()))
    return DetermineTableSectionRole();

  if (const auto* input = DynamicTo<HTMLInputElement>(*GetNode())) {
    const AtomicString& type = input->type();
    if (input->DataList() && type != input_type_names::kColor)
      return ax::mojom::blink::Role::kTextFieldWithComboBox;
    if (type == input_type_names::kButton)
      return ButtonRoleType();
    if (type == input_type_names::kCheckbox)
      return ax::mojom::blink::Role::kCheckBox;
    if (type == input_type_names::kDate)
      return ax::mojom::blink::Role::kDate;
    if (type == input_type_names::kDatetime ||
        type == input_type_names::kDatetimeLocal ||
        type == input_type_names::kMonth || type == input_type_names::kWeek)
      return ax::mojom::blink::Role::kDateTime;
    if (type == input_type_names::kFile)
      return ax::mojom::blink::Role::kButton;
    if (type == input_type_names::kRadio)
      return ax::mojom::blink::Role::kRadioButton;
    if (type == input_type_names::kNumber)
      return ax::mojom::blink::Role::kSpinButton;
    if (input->IsTextButton())
      return ButtonRoleType();
    if (type == input_type_names::kRange)
      return ax::mojom::blink::Role::kSlider;
    if (type == input_type_names::kSearch)
      return ax::mojom::blink::Role::kSearchBox;
    if (type == input_type_names::kColor)
      return ax::mojom::blink::Role::kColorWell;
    if (type == input_type_names::kTime)
      return ax::mojom::blink::Role::kInputTime;
    if (type == input_type_names::kButton || type == input_type_names::kImage ||
        type == input_type_names::kReset || type == input_type_names::kSubmit) {
      return ax::mojom::blink::Role::kButton;
    }
    return ax::mojom::blink::Role::kTextField;
  }

  if (auto* select_element = DynamicTo<HTMLSelectElement>(*GetNode())) {
    if (select_element->IsMultiple())
      return ax::mojom::blink::Role::kListBox;
    else
      return ax::mojom::blink::Role::kPopUpButton;
  }

  if (auto* option = DynamicTo<HTMLOptionElement>(*GetNode())) {
    HTMLSelectElement* select_element = option->OwnerSelectElement();
    if (!select_element || select_element->IsMultiple())
      return ax::mojom::blink::Role::kListBoxOption;
    else
      return ax::mojom::blink::Role::kMenuListOption;
  }

  if (IsA<HTMLTextAreaElement>(*GetNode()))
    return ax::mojom::blink::Role::kTextField;

  if (HeadingLevel())
    return ax::mojom::blink::Role::kHeading;

  if (IsA<HTMLDivElement>(*GetNode()))
    return RoleFromLayoutObject(ax::mojom::blink::Role::kGenericContainer);

  if (IsA<HTMLMenuElement>(*GetNode()) || IsA<HTMLUListElement>(*GetNode()) ||
      IsA<HTMLOListElement>(*GetNode())) {
    // <menu> is a deprecated feature of HTML 5, but is included for semantic
    // compatibility with HTML3, and may contain list items. Exposing it as an
    // unordered list works better than the current HTML-AAM recommendaton of
    // exposing as a role=menu, because if it's just used semantically, it won't
    // be interactive. If used as a widget, the author must provide role=menu.
    return ax::mojom::blink::Role::kList;
  }

  if (IsA<HTMLMeterElement>(*GetNode()))
    return ax::mojom::blink::Role::kMeter;

  if (IsA<HTMLProgressElement>(*GetNode()))
    return ax::mojom::blink::Role::kProgressIndicator;

  if (IsA<HTMLOutputElement>(*GetNode()))
    return ax::mojom::blink::Role::kStatus;

  if (IsA<HTMLParagraphElement>(*GetNode()))
    return ax::mojom::blink::Role::kParagraph;

  if (IsA<HTMLLabelElement>(*GetNode()))
    return ax::mojom::blink::Role::kLabelText;

  if (IsA<HTMLLegendElement>(*GetNode()))
    return ax::mojom::blink::Role::kLegend;

  if (IsA<HTMLRubyElement>(*GetNode()))
    return ax::mojom::blink::Role::kRuby;

  if (IsA<HTMLDListElement>(*GetNode()))
    return ax::mojom::blink::Role::kDescriptionList;

  if (IsA<HTMLAudioElement>(*GetNode()))
    return ax::mojom::blink::Role::kAudio;
  if (IsA<HTMLVideoElement>(*GetNode()))
    return ax::mojom::blink::Role::kVideo;

  if (GetNode()->HasTagName(html_names::kDdTag))
    return ax::mojom::blink::Role::kDescriptionListDetail;

  if (GetNode()->HasTagName(html_names::kDtTag))
    return ax::mojom::blink::Role::kDescriptionListTerm;

  if (GetNode()->nodeName() == mathml_names::kMathTag.LocalName())
    return ax::mojom::blink::Role::kMath;

  if (GetNode()->HasTagName(html_names::kRpTag) ||
      GetNode()->HasTagName(html_names::kRtTag))
    return ax::mojom::blink::Role::kRubyAnnotation;

  if (IsA<HTMLFormElement>(*GetNode()))
    return ax::mojom::blink::Role::kForm;

  if (GetNode()->HasTagName(html_names::kAbbrTag))
    return ax::mojom::blink::Role::kAbbr;

  if (GetNode()->HasTagName(html_names::kArticleTag))
    return ax::mojom::blink::Role::kArticle;

  if (GetNode()->HasTagName(html_names::kCodeTag))
    return ax::mojom::blink::Role::kCode;

  if (GetNode()->HasTagName(html_names::kEmTag))
    return ax::mojom::blink::Role::kEmphasis;

  if (GetNode()->HasTagName(html_names::kStrongTag))
    return ax::mojom::blink::Role::kStrong;

  if (GetNode()->HasTagName(html_names::kDelTag))
    return ax::mojom::blink::Role::kContentDeletion;

  if (GetNode()->HasTagName(html_names::kInsTag))
    return ax::mojom::blink::Role::kContentInsertion;

  if (GetNode()->HasTagName(html_names::kMainTag))
    return ax::mojom::blink::Role::kMain;

  if (GetNode()->HasTagName(html_names::kMarkTag))
    return ax::mojom::blink::Role::kMark;

  if (GetNode()->HasTagName(html_names::kNavTag))
    return ax::mojom::blink::Role::kNavigation;

  if (GetNode()->HasTagName(html_names::kAsideTag))
    return ax::mojom::blink::Role::kComplementary;

  if (GetNode()->HasTagName(html_names::kPreTag))
    return ax::mojom::blink::Role::kPre;

  if (GetNode()->HasTagName(html_names::kSectionTag))
    return ax::mojom::blink::Role::kSection;

  if (GetNode()->HasTagName(html_names::kAddressTag))
    return RoleFromLayoutObject(ax::mojom::blink::Role::kGenericContainer);

  if (IsA<HTMLDialogElement>(*GetNode()))
    return ax::mojom::blink::Role::kDialog;

  // The HTML element.
  if (IsA<HTMLHtmlElement>(GetNode()))
    return RoleFromLayoutObject(ax::mojom::blink::Role::kGenericContainer);

  // Treat <iframe> and <frame> the same.
  if (IsA<HTMLIFrameElement>(*GetNode()) || IsA<HTMLFrameElement>(*GetNode())) {
    const AtomicString& aria_role =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole);
    if (aria_role == "none" || aria_role == "presentation")
      return ax::mojom::blink::Role::kIframePresentational;
    return ax::mojom::blink::Role::kIframe;
  }

  // There should only be one banner/contentInfo per page. If header/footer are
  // being used within an article or section then it should not be exposed as
  // whole page's banner/contentInfo but as a generic container role.
  if (GetNode()->HasTagName(html_names::kHeaderTag)) {
    if (IsDescendantOfElementType(GetLandmarkRolesNotAllowed()))
      return ax::mojom::blink::Role::kHeaderAsNonLandmark;
    return ax::mojom::blink::Role::kHeader;
  }

  if (GetNode()->HasTagName(html_names::kFooterTag)) {
    if (IsDescendantOfElementType(GetLandmarkRolesNotAllowed()))
      return ax::mojom::blink::Role::kFooterAsNonLandmark;
    return ax::mojom::blink::Role::kFooter;
  }

  if (GetNode()->HasTagName(html_names::kBlockquoteTag))
    return ax::mojom::blink::Role::kBlockquote;

  if (GetNode()->HasTagName(html_names::kCaptionTag))
    return ax::mojom::blink::Role::kCaption;

  if (GetNode()->HasTagName(html_names::kFigcaptionTag))
    return ax::mojom::blink::Role::kFigcaption;

  if (GetNode()->HasTagName(html_names::kFigureTag))
    return ax::mojom::blink::Role::kFigure;

  if (GetNode()->HasTagName(html_names::kTimeTag))
    return ax::mojom::blink::Role::kTime;

  if (IsA<HTMLPlugInElement>(GetNode())) {
    if (IsA<HTMLEmbedElement>(GetNode()))
      return ax::mojom::blink::Role::kEmbeddedObject;
    return ax::mojom::blink::Role::kPluginObject;
  }

  if (IsA<HTMLHRElement>(*GetNode()))
    return ax::mojom::blink::Role::kSplitter;

  if (IsFieldset())
    return ax::mojom::blink::Role::kGroup;

  return RoleFromLayoutObject(ax::mojom::blink::Role::kUnknown);
}

ax::mojom::blink::Role AXNodeObject::DetermineAccessibilityRole() {
  if (!GetNode()) {
    NOTREACHED();
    return ax::mojom::blink::Role::kUnknown;
  }

  native_role_ = NativeRoleIgnoringAria();

  if ((aria_role_ = DetermineAriaRoleAttribute()) !=
      ax::mojom::blink::Role::kUnknown)
    return aria_role_;
  if (GetNode()->IsTextNode())
    return ax::mojom::blink::Role::kStaticText;

  return native_role_ == ax::mojom::blink::Role::kUnknown
             ? ax::mojom::blink::Role::kGenericContainer
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
      if (!child)
        continue;
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

  const ax::mojom::blink::Role role = RoleValue();
  const bool is_edit_box = role == ax::mojom::blink::Role::kSearchBox ||
                           role == ax::mojom::blink::Role::kTextField;
  if (!IsEditable() && !is_edit_box)
    return false;  // Doesn't support multiline.

  // Supports aria-multiline, so check for attribute.
  bool is_multiline = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kMultiline,
                                    is_multiline)) {
    return is_multiline;
  }

  // Default for <textarea> is true.
  if (IsA<HTMLTextAreaElement>(*node))
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
      GetAttribute(html_names::kContenteditableAttr);
  if (content_editable_value.IsNull())
    return false;
  // Both "true" (case-insensitive) and the empty string count as true.
  return content_editable_value.IsEmpty() ||
         EqualIgnoringASCIICase(content_editable_value, "true");
}

bool AXNodeObject::IsTextControl() const {
  if (!GetNode())
    return false;

  if (IsNativeTextControl() || HasContentEditableAttributeSet() ||
      IsARIATextControl()) {
    return true;
  }

  return false;
}

static Element* SiblingWithAriaRole(String role, Node* node) {
  Node* parent = LayoutTreeBuilderTraversal::Parent(*node);
  if (!parent)
    return nullptr;

  for (Node* sibling = LayoutTreeBuilderTraversal::FirstChild(*parent); sibling;
       sibling = LayoutTreeBuilderTraversal::NextSibling(*sibling)) {
    auto* element = DynamicTo<Element>(sibling);
    if (!element)
      continue;
    const AtomicString& sibling_aria_role =
        AccessibleNode::GetPropertyOrARIAAttribute(element,
                                                   AOMStringProperty::kRole);
    if (EqualIgnoringASCIICase(sibling_aria_role, role))
      return element;
  }

  return nullptr;
}

Element* AXNodeObject::MenuItemElementForMenu() const {
  if (AriaRoleAttribute() != ax::mojom::blink::Role::kMenu)
    return nullptr;

  return SiblingWithAriaRole("menuitem", GetNode());
}

Element* AXNodeObject::MouseButtonListener() const {
  Node* node = this->GetNode();
  if (!node)
    return nullptr;

  auto* element = DynamicTo<Element>(node);
  if (!element)
    node = node->parentElement();

  if (!node)
    return nullptr;

  for (element = To<Element>(node); element;
       element = element->parentElement()) {
    if (element->HasAnyEventListeners(event_util::MouseButtonEventTypes()))
      return element;
  }

  return nullptr;
}

void AXNodeObject::Init(AXObject* parent_if_known) {
#if DCHECK_IS_ON()
  DCHECK(!initialized_);
  initialized_ = true;
#endif
  AXObject::Init(parent_if_known);

  DCHECK(node_ ||
         (GetLayoutObject() &&
          AXObjectCacheImpl::IsPseudoElementDescendant(*GetLayoutObject())))
      << "Nodeless AXNodeObject can only exist inside a pseudo element: "
      << GetLayoutObject();
}

void AXNodeObject::Detach() {
#if DCHECK_IS_ON()
  DCHECK(!is_adding_children_) << "Cannot Detach |this| during AddChildren()";
#endif
  AXObject::Detach();
  node_ = nullptr;
}

bool AXNodeObject::IsAXNodeObject() const {
  return true;
}

bool AXNodeObject::IsControl() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  auto* element = DynamicTo<Element>(node);
  return ((element && element->IsFormControlElement()) ||
          AXObject::IsARIAControl(AriaRoleAttribute()));
}

bool AXNodeObject::IsControllingVideoElement() const {
  Node* node = this->GetNode();
  if (!node)
    return true;

  return IsA<HTMLVideoElement>(
      MediaControlElementsHelper::ToParentMediaElement(node));
}

bool AXNodeObject::IsAutofillAvailable() const {
  // Autofill state is stored in AXObjectCache.
  WebAXAutofillState state = AXObjectCache().GetAutofillState(AXObjectID());
  return state == WebAXAutofillState::kAutofillAvailable;
}

bool AXNodeObject::IsDefault() const {
  if (IsDetached())
    return false;

  // Checks for any kind of disabled, including aria-disabled.
  if (Restriction() == kRestrictionDisabled ||
      RoleValue() != ax::mojom::blink::Role::kButton) {
    return false;
  }

  // Will only match :default pseudo class if it's the first default button in
  // a form.
  return GetElement()->MatchesDefaultPseudoClass();
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

bool AXNodeObject::IsFieldset() const {
  return IsA<HTMLFieldSetElement>(GetNode());
}

bool AXNodeObject::IsHovered() const {
  if (Node* node = this->GetNode())
    return node->IsHovered();
  return false;
}

bool AXNodeObject::IsImageButton() const {
  return IsNativeImage() && IsButton();
}

bool AXNodeObject::IsInputImage() const {
  auto* html_input_element = DynamicTo<HTMLInputElement>(this->GetNode());
  if (html_input_element && RoleValue() == ax::mojom::blink::Role::kButton)
    return html_input_element->type() == input_type_names::kImage;

  return false;
}

// It is not easily possible to find out if an element is the target of an
// in-page link.
// As a workaround, we check if the element is a sectioning element with an ID,
// or an anchor with a name.
bool AXNodeObject::IsInPageLinkTarget() const {
  auto* element = DynamicTo<Element>(node_.Get());
  if (!element)
    return false;
  // We exclude elements that are in the shadow DOM.
  if (element->ContainingShadowRoot())
    return false;

  if (auto* anchor = DynamicTo<HTMLAnchorElement>(element)) {
    return anchor->HasName() || anchor->HasID();
  }

  if (element->HasID() &&
      (IsLandmarkRelated() || IsA<HTMLSpanElement>(element) ||
       IsA<HTMLDivElement>(element))) {
    return true;
  }
  return false;
}

bool AXNodeObject::IsLoaded() const {
  if (!GetDocument())
    return false;
  return !GetDocument()->Parser();
}

bool AXNodeObject::IsMultiSelectable() const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kGrid:
    case ax::mojom::blink::Role::kTreeGrid:
    case ax::mojom::blink::Role::kTree:
    case ax::mojom::blink::Role::kListBox:
    case ax::mojom::blink::Role::kTabList: {
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

  auto* html_select_element = DynamicTo<HTMLSelectElement>(GetNode());
  return html_select_element && html_select_element->IsMultiple();
}

bool AXNodeObject::IsNativeImage() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  if (IsA<HTMLImageElement>(*node) || IsA<HTMLPlugInElement>(*node))
    return true;

  if (const auto* input = DynamicTo<HTMLInputElement>(*node))
    return input->type() == input_type_names::kImage;

  return false;
}

bool AXNodeObject::IsNativeTextControl() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  if (IsA<HTMLTextAreaElement>(*node))
    return true;

  if (const auto* input = DynamicTo<HTMLInputElement>(*node))
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

bool AXNodeObject::IsOffScreen() const {
  if (IsDetached())
    return false;
  DCHECK(GetNode());
  return DisplayLockUtilities::NearestLockedExclusiveAncestor(*GetNode());
}

bool AXNodeObject::IsPasswordField() const {
  auto* html_input_element = DynamicTo<HTMLInputElement>(this->GetNode());
  if (!html_input_element)
    return false;

  ax::mojom::blink::Role aria_role = AriaRoleAttribute();
  if (aria_role != ax::mojom::blink::Role::kTextField &&
      aria_role != ax::mojom::blink::Role::kUnknown)
    return false;

  return html_input_element->type() == input_type_names::kPassword;
}

bool AXNodeObject::IsProgressIndicator() const {
  return RoleValue() == ax::mojom::blink::Role::kProgressIndicator;
}

bool AXNodeObject::IsRichlyEditable() const {
  // This check is necessary to support the richlyEditable and editable states
  // in canvas fallback, for contenteditable elements.
  // TODO(accessiblity) Support on descendants of the fallback element that
  // has contenteditable set.
  return HasContentEditableAttributeSet();
}

bool AXNodeObject::IsEditable() const {
  if (IsNativeTextControl())
    return true;

  // Support editable states in canvas fallback content.
  return AXNodeObject::IsRichlyEditable();
}

bool AXNodeObject::IsSlider() const {
  return RoleValue() == ax::mojom::blink::Role::kSlider;
}

bool AXNodeObject::IsSpinButton() const {
  return RoleValue() == ax::mojom::blink::Role::kSpinButton;
}

bool AXNodeObject::IsNativeSlider() const {
  if (const auto* input = DynamicTo<HTMLInputElement>(GetNode()))
    return input->type() == input_type_names::kRange;
  return false;
}

bool AXNodeObject::IsNativeSpinButton() const {
  if (const auto* input = DynamicTo<HTMLInputElement>(GetNode()))
    return input->type() == input_type_names::kNumber;
  return false;
}

bool AXNodeObject::IsClickable() const {
  Node* node = GetNode();
  if (!node)
    return false;
  auto* element = DynamicTo<Element>(node);
  if (element && element->IsDisabledFormControl()) {
    return false;
  }

  // Note: we can't call |node->WillRespondToMouseClickEvents()| because that
  // triggers a style recalc and can delete this.
  if (node->HasAnyEventListeners(event_util::MouseButtonEventTypes()))
    return true;

  return IsTextControl() || AXObject::IsClickable();
}

bool AXNodeObject::IsFocused() const {
  if (!GetDocument())
    return false;

  // A web area is represented by the Document node in the DOM tree, which isn't
  // focusable.  Check instead if the frame's selection controller is focused.
  if (IsWebArea() &&
      GetDocument()->GetFrame()->Selection().FrameIsFocusedAndActive()) {
    return true;
  }

  Element* focused_element = GetDocument()->FocusedElement();
  return focused_element && focused_element == GetElement();
}

// aria-grabbed is deprecated in WAI-ARIA 1.1.
AccessibilityGrabbedState AXNodeObject::IsGrabbed() const {
  if (!SupportsARIADragging())
    return kGrabbedStateUndefined;

  const AtomicString& grabbed = GetAttribute(html_names::kAriaGrabbedAttr);
  return EqualIgnoringASCIICase(grabbed, "true") ? kGrabbedStateTrue
                                                 : kGrabbedStateFalse;
}

AccessibilitySelectedState AXNodeObject::IsSelected() const {
  if (!GetNode() || !GetLayoutObject() || !IsSubWidget())
    return kSelectedStateUndefined;

  // The aria-selected attribute overrides automatic behaviors.
  bool is_selected;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kSelected, is_selected))
    return is_selected ? kSelectedStateTrue : kSelectedStateFalse;

  // The selection should only follow the focus when the aria-selected attribute
  // is marked as required or implied for this element in the ARIA specs.
  // If this object can't follow the focus, then we can't say that it's selected
  // nor that it's not.
  if (!SelectionShouldFollowFocus())
    return kSelectedStateUndefined;

  // Selection follows focus, but ONLY in single selection containers, and only
  // if aria-selected was not present to override.
  return IsSelectedFromFocus() ? kSelectedStateTrue : kSelectedStateFalse;
}

// In single selection containers, selection follows focus unless aria_selected
// is set to false. This is only valid for a subset of elements.
bool AXNodeObject::IsSelectedFromFocus() const {
  if (!SelectionShouldFollowFocus())
    return false;

  // A tab item can also be selected if it is associated to a focused tabpanel
  // via the aria-labelledby attribute.
  if (IsTabItem() && IsTabItemSelected())
    return kSelectedStateTrue;

  // If not a single selection container, selection does not follow focus.
  AXObject* container = ContainerWidget();
  if (!container || container->IsMultiSelectable())
    return false;

  // If this object is not accessibility focused, then it is not selected from
  // focus.
  AXObject* focused_object = AXObjectCache().FocusedObject();
  if (focused_object != this &&
      (!focused_object || focused_object->ActiveDescendant() != this))
    return false;

  // In single selection container and accessibility focused => true if
  // aria-selected wasn't used as an override.
  bool is_selected;
  return !HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kSelected,
                                        is_selected);
}

// Returns true if the node's aria-selected attribute should be set to true
// when the node is focused. This is true for only a subset of roles.
bool AXNodeObject::SelectionShouldFollowFocus() const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kMenuListOption:
    case ax::mojom::blink::Role::kTab:
      return true;
    default:
      break;
  }
  return false;
}

bool AXNodeObject::IsTabItemSelected() const {
  if (!IsTabItem() || !GetLayoutObject())
    return false;

  Node* node = GetNode();
  if (!node || !node->IsElementNode())
    return false;

  // The ARIA spec says a tab item can also be selected if it is aria-labeled by
  // a tabpanel that has keyboard focus inside of it, or if a tabpanel in its
  // aria-controls list has KB focus inside of it.
  AXObject* focused_element = AXObjectCache().FocusedObject();
  if (!focused_element)
    return false;

  HeapVector<Member<Element>> elements;
  if (!HasAOMPropertyOrARIAAttribute(AOMRelationListProperty::kControls,
                                     elements))
    return false;

  for (const auto& element : elements) {
    AXObject* tab_panel = AXObjectCache().GetOrCreate(element);

    // A tab item should only control tab panels.
    if (!tab_panel ||
        tab_panel->RoleValue() != ax::mojom::blink::Role::kTabPanel) {
      continue;
    }

    AXObject* check_focus_element = focused_element;
    // Check if the focused element is a descendant of the element controlled by
    // the tab item.
    while (check_focus_element) {
      if (tab_panel == check_focus_element)
        return true;
      check_focus_element = check_focus_element->ParentObject();
    }
  }

  return false;
}

AXRestriction AXNodeObject::Restriction() const {
  Element* elem = GetElement();
  if (!elem)
    return kRestrictionNone;

  // An <optgroup> is not exposed directly in the AX tree.
  if (IsA<HTMLOptGroupElement>(elem))
    return kRestrictionNone;

  // According to ARIA, all elements of the base markup can be disabled.
  // According to CORE-AAM, any focusable descendant of aria-disabled
  // ancestor is also disabled.
  bool is_disabled;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kDisabled,
                                    is_disabled)) {
    // Has aria-disabled, overrides native markup determining disabled.
    if (is_disabled)
      return kRestrictionDisabled;
  } else if (elem->IsDisabledFormControl() ||
             (CanSetFocusAttribute() && IsDescendantOfDisabledNode())) {
    // No aria-disabled, but other markup says it's disabled.
    return kRestrictionDisabled;
  }

  // Check aria-readonly if supported by current role.
  bool is_read_only;
  if (SupportsARIAReadOnly() &&
      HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kReadOnly,
                                    is_read_only)) {
    // ARIA overrides other readonly state markup.
    return is_read_only ? kRestrictionReadOnly : kRestrictionNone;
  }

  // Only editable fields can be marked @readonly (unlike @aria-readonly).
  auto* text_area_element = DynamicTo<HTMLTextAreaElement>(*elem);
  if (text_area_element && text_area_element->IsReadOnly())
    return kRestrictionReadOnly;
  if (const auto* input = DynamicTo<HTMLInputElement>(*elem)) {
    if (input->IsTextField() && input->IsReadOnly())
      return kRestrictionReadOnly;
  }

  // If a grid cell does not have it's own ARIA input restriction,
  // fall back on parent grid's readonly state.
  // See ARIA specification regarding grid/treegrid and readonly.
  if (IsTableCellLikeRole()) {
    AXObject* row = ParentObjectUnignored();
    if (row && row->IsTableRowLikeRole()) {
      AXObject* table = row->ParentObjectUnignored();
      if (table && table->IsTableLikeRole() &&
          (table->RoleValue() == ax::mojom::blink::Role::kGrid ||
           table->RoleValue() == ax::mojom::blink::Role::kTreeGrid)) {
        if (table->Restriction() == kRestrictionReadOnly)
          return kRestrictionReadOnly;
      }
    }
  }

  // This is a node that is not readonly and not disabled.
  return kRestrictionNone;
}

AccessibilityExpanded AXNodeObject::IsExpanded() const {
  if (!SupportsARIAExpanded())
    return kExpandedUndefined;

  if (RoleValue() == ax::mojom::blink::Role::kPopUpButton && GetNode() &&
      IsA<HTMLSelectElement>(*GetNode())) {
    return To<HTMLSelectElement>(GetNode())->PopupIsVisible()
               ? kExpandedExpanded
               : kExpandedCollapsed;
  }

  if (GetNode() && IsA<HTMLSummaryElement>(*GetNode())) {
    if (GetNode()->parentNode() &&
        IsA<HTMLDetailsElement>(GetNode()->parentNode())) {
      return To<Element>(GetNode()->parentNode())
                     ->FastHasAttribute(html_names::kOpenAttr)
                 ? kExpandedExpanded
                 : kExpandedCollapsed;
    }
  }

  bool expanded = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kExpanded, expanded)) {
    return expanded ? kExpandedExpanded : kExpandedCollapsed;
  }

  return kExpandedUndefined;
}

bool AXNodeObject::IsRequired() const {
  auto* form_control = DynamicTo<HTMLFormControlElement>(GetNode());
  if (form_control && form_control->IsRequired())
    return true;

  if (AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kRequired))
    return true;

  return false;
}

bool AXNodeObject::CanvasHasFallbackContent() const {
  if (IsDetached())
    return false;
  Node* node = this->GetNode();
  return IsA<HTMLCanvasElement>(node) && node->hasChildren();
}

int AXNodeObject::HeadingLevel() const {
  // headings can be in block flow and non-block flow
  Node* node = this->GetNode();
  if (!node)
    return 0;

  if (RoleValue() == ax::mojom::blink::Role::kHeading) {
    uint32_t level;
    if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kLevel, level)) {
      if (level >= 1 && level <= 9)
        return level;
    }
  }

  auto* element = DynamicTo<HTMLElement>(node);
  if (!element)
    return 0;

  if (element->HasTagName(html_names::kH1Tag))
    return 1;

  if (element->HasTagName(html_names::kH2Tag))
    return 2;

  if (element->HasTagName(html_names::kH3Tag))
    return 3;

  if (element->HasTagName(html_names::kH4Tag))
    return 4;

  if (element->HasTagName(html_names::kH5Tag))
    return 5;

  if (element->HasTagName(html_names::kH6Tag))
    return 6;

  if (RoleValue() == ax::mojom::blink::Role::kHeading)
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

  // Helper lambda for calculating hierarchical levels by counting ancestor
  // nodes that match a target role.
  auto accumulateLevel = [&](int initial_level,
                             ax::mojom::blink::Role target_role) {
    int level = initial_level;
    for (AXObject* parent = ParentObject(); parent;
         parent = parent->ParentObject()) {
      if (parent->RoleValue() == target_role)
        level++;
    }
    return level;
  };

  switch (RoleValue()) {
    case ax::mojom::blink::Role::kComment:
      // Comment: level is based on counting comment ancestors until the root.
      return accumulateLevel(1, ax::mojom::blink::Role::kComment);
    case ax::mojom::blink::Role::kListItem:
      level = accumulateLevel(0, ax::mojom::blink::Role::kList);
      // When level count is 0 due to this list item not having an ancestor of
      // Role::kList, not nested in list groups, this list item has a level
      // of 1.
      return level == 0 ? 1 : level;
    case ax::mojom::blink::Role::kTabList:
      return accumulateLevel(1, ax::mojom::blink::Role::kTabList);
    case ax::mojom::blink::Role::kTreeItem: {
      // Hierarchy leveling starts at 1, to match the aria-level spec.
      // We measure tree hierarchy by the number of groups that the item is
      // within.
      level = 1;
      for (AXObject* parent = ParentObject(); parent;
           parent = parent->ParentObject()) {
        ax::mojom::blink::Role parent_role = parent->RoleValue();
        if (parent_role == ax::mojom::blink::Role::kGroup)
          level++;
        else if (parent_role == ax::mojom::blink::Role::kTree)
          break;
      }
      return level;
    }
    default:
      return 0;
  }

  return 0;
}

String AXNodeObject::AutoComplete() const {
  // Check cache for auto complete state.
  if (AXObjectCache().GetAutofillState(AXObjectID()) ==
      WebAXAutofillState::kAutocompleteAvailable)
    return "list";

  if (IsNativeTextControl() || IsARIATextControl()) {
    const AtomicString& aria_auto_complete =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kAutocomplete)
            .LowerASCII();
    // Illegal values must be passed through, according to CORE-AAM.
    if (!aria_auto_complete.IsNull())
      return aria_auto_complete == "none" ? String() : aria_auto_complete;
  }

  if (auto* input = DynamicTo<HTMLInputElement>(GetNode())) {
    if (input->DataList())
      return "list";
  }

  return String();
}

// TODO(nektar): Consider removing this method in favor of
// AXInlineTextBox::GetDocumentMarkers, or add document markers to the tree data
// instead of nodes objects.
void AXNodeObject::SerializeMarkerAttributes(ui::AXNodeData* node_data) const {
  if (!GetNode() || !GetDocument() || !GetDocument()->View())
    return;

  auto* text_node = DynamicTo<Text>(GetNode());
  if (!text_node)
    return;

  std::vector<int32_t> marker_types;
  std::vector<int32_t> marker_starts;
  std::vector<int32_t> marker_ends;

  // First use ARIA markers for spelling/grammar if available.
  base::Optional<DocumentMarker::MarkerType> aria_marker_type =
      GetAriaSpellingOrGrammarMarker();
  if (aria_marker_type) {
    AXRange range = AXRange::RangeOfContents(*this);
    marker_types.push_back(ToAXMarkerType(aria_marker_type.value()));
    marker_starts.push_back(range.Start().TextOffset());
    marker_ends.push_back(range.End().TextOffset());
  }

  DocumentMarkerController& marker_controller = GetDocument()->Markers();
  const DocumentMarker::MarkerTypes markers_used_by_accessibility(
      DocumentMarker::kSpelling | DocumentMarker::kGrammar |
      DocumentMarker::kTextMatch | DocumentMarker::kActiveSuggestion |
      DocumentMarker::kSuggestion | DocumentMarker::kTextFragment);
  const DocumentMarkerVector markers =
      marker_controller.MarkersFor(*text_node, markers_used_by_accessibility);
  for (const DocumentMarker* marker : markers) {
    if (aria_marker_type == marker->GetType())
      continue;

    const Position start_position(*GetNode(), marker->StartOffset());
    const Position end_position(*GetNode(), marker->EndOffset());
    if (!start_position.IsValidFor(*GetDocument()) ||
        !end_position.IsValidFor(*GetDocument())) {
      continue;
    }

    marker_types.push_back(ToAXMarkerType(marker->GetType()));
    auto start_pos =
        AXPosition::FromPosition(start_position, TextAffinity::kDownstream,
                                 AXPositionAdjustmentBehavior::kMoveLeft);
    auto end_pos =
        AXPosition::FromPosition(end_position, TextAffinity::kDownstream,
                                 AXPositionAdjustmentBehavior::kMoveRight);
    marker_starts.push_back(start_pos.TextOffset());
    marker_ends.push_back(end_pos.TextOffset());
  }

  if (marker_types.empty())
    return;

  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kMarkerTypes, marker_types);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kMarkerStarts, marker_starts);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kMarkerEnds, marker_ends);
}

AXObject* AXNodeObject::InPageLinkTarget() const {
  if (!IsAnchor() || !GetDocument())
    return AXObject::InPageLinkTarget();

  const Element* anchor = AnchorElement();
  if (!anchor)
    return AXObject::InPageLinkTarget();

  KURL link_url = anchor->HrefURL();
  if (!link_url.IsValid())
    return AXObject::InPageLinkTarget();

  KURL document_url = GetDocument()->Url();
  if (!document_url.IsValid() ||
      !EqualIgnoringFragmentIdentifier(document_url, link_url)) {
    return AXObject::InPageLinkTarget();
  }

  String fragment = link_url.FragmentIdentifier();
  TreeScope& tree_scope = anchor->GetTreeScope();
  Node* target = tree_scope.FindAnchor(fragment);
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
    case ax::mojom::blink::Role::kListBox:
    case ax::mojom::blink::Role::kMenu:
    case ax::mojom::blink::Role::kScrollBar:
    case ax::mojom::blink::Role::kTree:
      if (orientation == kAccessibilityOrientationUndefined)
        orientation = kAccessibilityOrientationVertical;

      return orientation;
    case ax::mojom::blink::Role::kMenuBar:
    case ax::mojom::blink::Role::kSlider:
    case ax::mojom::blink::Role::kSplitter:
    case ax::mojom::blink::Role::kTabList:
    case ax::mojom::blink::Role::kToolbar:
      if (orientation == kAccessibilityOrientationUndefined)
        orientation = kAccessibilityOrientationHorizontal;

      return orientation;
    case ax::mojom::blink::Role::kComboBoxGrouping:
    case ax::mojom::blink::Role::kComboBoxMenuButton:
    case ax::mojom::blink::Role::kRadioGroup:
    case ax::mojom::blink::Role::kTreeGrid:
      return orientation;
    default:
      return AXObject::Orientation();
  }
}

AXObject::AXObjectVector AXNodeObject::RadioButtonsInGroup() const {
  AXObjectVector radio_buttons;
  if (!node_ || RoleValue() != ax::mojom::blink::Role::kRadioButton)
    return radio_buttons;

  if (auto* node_radio_button = DynamicTo<HTMLInputElement>(node_.Get())) {
    HeapVector<Member<HTMLInputElement>> html_radio_buttons =
        FindAllRadioButtonsWithSameName(node_radio_button);
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
  if (parent && parent->RoleValue() == ax::mojom::blink::Role::kRadioGroup) {
    for (AXObject* child : parent->ChildrenIncludingIgnored()) {
      DCHECK(child);
      if (child->RoleValue() == ax::mojom::blink::Role::kRadioButton &&
          child->AccessibilityIsIncludedInTree()) {
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
  if (!radio_button || radio_button->type() != input_type_names::kRadio)
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

ax::mojom::blink::WritingDirection AXNodeObject::GetTextDirection() const {
  if (!GetLayoutObject())
    return AXObject::GetTextDirection();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXObject::GetTextDirection();

  if (style->IsHorizontalWritingMode()) {
    switch (style->Direction()) {
      case TextDirection::kLtr:
        return ax::mojom::blink::WritingDirection::kLtr;
      case TextDirection::kRtl:
        return ax::mojom::blink::WritingDirection::kRtl;
    }
  } else {
    switch (style->Direction()) {
      case TextDirection::kLtr:
        return ax::mojom::blink::WritingDirection::kTtb;
      case TextDirection::kRtl:
        return ax::mojom::blink::WritingDirection::kBtt;
    }
  }

  return AXNodeObject::GetTextDirection();
}

ax::mojom::blink::TextPosition AXNodeObject::GetTextPosition() const {
  if (!GetLayoutObject())
    return AXObject::GetTextPosition();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXObject::GetTextPosition();

  switch (style->VerticalAlign()) {
    case EVerticalAlign::kBaseline:
    case EVerticalAlign::kMiddle:
    case EVerticalAlign::kTextTop:
    case EVerticalAlign::kTextBottom:
    case EVerticalAlign::kTop:
    case EVerticalAlign::kBottom:
    case EVerticalAlign::kBaselineMiddle:
    case EVerticalAlign::kLength:
      return AXObject::GetTextPosition();
    case EVerticalAlign::kSub:
      return ax::mojom::blink::TextPosition::kSubscript;
    case EVerticalAlign::kSuper:
      return ax::mojom::blink::TextPosition::kSuperscript;
  }
}

void AXNodeObject::GetTextStyleAndTextDecorationStyle(
    int32_t* text_style,
    ax::mojom::blink::TextDecorationStyle* text_overline_style,
    ax::mojom::blink::TextDecorationStyle* text_strikethrough_style,
    ax::mojom::blink::TextDecorationStyle* text_underline_style) const {
  if (!GetLayoutObject()) {
    AXObject::GetTextStyleAndTextDecorationStyle(
        text_style, text_overline_style, text_strikethrough_style,
        text_underline_style);
    return;
  }
  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style) {
    AXObject::GetTextStyleAndTextDecorationStyle(
        text_style, text_overline_style, text_strikethrough_style,
        text_underline_style);
    return;
  }

  *text_style = 0;
  *text_overline_style = ax::mojom::blink::TextDecorationStyle::kNone;
  *text_strikethrough_style = ax::mojom::blink::TextDecorationStyle::kNone;
  *text_underline_style = ax::mojom::blink::TextDecorationStyle::kNone;

  if (style->GetFontWeight() == BoldWeightValue())
    *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kBold);
  if (style->GetFontDescription().Style() == ItalicSlopeValue())
    *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kItalic);

  for (const auto& decoration : style->AppliedTextDecorations()) {
    if (EnumHasFlags(decoration.Lines(), TextDecoration::kOverline)) {
      *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kOverline);
      *text_overline_style =
          TextDecorationStyleToAXTextDecorationStyle(decoration.Style());
    }
    if (EnumHasFlags(decoration.Lines(), TextDecoration::kLineThrough)) {
      *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kLineThrough);
      *text_strikethrough_style =
          TextDecorationStyleToAXTextDecorationStyle(decoration.Style());
    }
    if (EnumHasFlags(decoration.Lines(), TextDecoration::kUnderline)) {
      *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kUnderline);
      *text_underline_style =
          TextDecorationStyleToAXTextDecorationStyle(decoration.Style());
    }
  }
}

ax::mojom::blink::TextAlign AXNodeObject::GetTextAlign() const {
  // Object attributes are not applied to text objects.
  if (IsTextObject() || !GetLayoutObject())
    return ax::mojom::blink::TextAlign::kNone;

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return ax::mojom::blink::TextAlign::kNone;

  switch (style->GetTextAlign()) {
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
    case ETextAlign::kStart:
      return ax::mojom::blink::TextAlign::kLeft;
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
    case ETextAlign::kEnd:
      return ax::mojom::blink::TextAlign::kRight;
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
      return ax::mojom::blink::TextAlign::kCenter;
    case ETextAlign::kJustify:
      return ax::mojom::blink::TextAlign::kJustify;
  }
}

float AXNodeObject::GetTextIndent() const {
  // Text-indent applies to lines or blocks, but not text.
  if (IsTextObject() || !GetLayoutObject())
    return 0.0f;
  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return 0.0f;

  const blink::LayoutBlock* layout_block =
      GetLayoutObject()->InclusiveContainingBlock();
  if (!layout_block)
    return 0.0f;
  float text_indent = layout_block->TextIndentOffset().ToFloat();
  return text_indent / kCssPixelsPerMillimeter;
}

String AXNodeObject::ImageDataUrl(const IntSize& max_size) const {
  Node* node = GetNode();
  if (!node)
    return String();

  ImageBitmapOptions* options = ImageBitmapOptions::Create();
  ImageBitmap* image_bitmap = nullptr;
  if (auto* image = DynamicTo<HTMLImageElement>(node)) {
    image_bitmap = MakeGarbageCollected<ImageBitmap>(
        image, base::Optional<IntRect>(), options);
  } else if (auto* canvas = DynamicTo<HTMLCanvasElement>(node)) {
    image_bitmap = MakeGarbageCollected<ImageBitmap>(
        canvas, base::Optional<IntRect>(), options);
  } else if (auto* video = DynamicTo<HTMLVideoElement>(node)) {
    image_bitmap = MakeGarbageCollected<ImageBitmap>(
        video, base::Optional<IntRect>(), options);
  }
  if (!image_bitmap)
    return String();

  scoped_refptr<StaticBitmapImage> bitmap_image = image_bitmap->BitmapImage();
  if (!bitmap_image)
    return String();

  sk_sp<SkImage> image =
      bitmap_image->PaintImageForCurrentFrame().GetSwSkImage();
  if (!image || image->width() <= 0 || image->height() <= 0)
    return String();

  // Determine the width and height of the output image, using a proportional
  // scale factor such that it's no larger than |maxSize|, if |maxSize| is not
  // empty. It only resizes the image to be smaller (if necessary), not
  // larger.
  float x_scale =
      max_size.Width() ? max_size.Width() * 1.0 / image->width() : 1.0;
  float y_scale =
      max_size.Height() ? max_size.Height() * 1.0 / image->height() : 1.0;
  float scale = std::min(x_scale, y_scale);
  if (scale >= 1.0)
    scale = 1.0;
  int width = std::round(image->width() * scale);
  int height = std::round(image->height() * scale);

  // Draw the image into a bitmap in native format.
  SkBitmap bitmap;
  SkPixmap unscaled_pixmap;
  if (scale == 1.0 && image->peekPixels(&unscaled_pixmap)) {
    bitmap.installPixels(unscaled_pixmap);
  } else {
    bitmap.allocPixels(
        SkImageInfo::MakeN32(width, height, kPremul_SkAlphaType));
    SkCanvas canvas(bitmap, SkSurfaceProps{});
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.drawImageRect(image, SkRect::MakeIWH(width, height),
                         SkSamplingOptions());
  }

  // Copy the bits into a buffer in RGBA_8888 unpremultiplied format
  // for encoding.
  SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                       kUnpremul_SkAlphaType);
  size_t row_bytes = info.minRowBytes();
  Vector<char> pixel_storage(
      SafeCast<wtf_size_t>(info.computeByteSize(row_bytes)));
  SkPixmap pixmap(info, pixel_storage.data(), row_bytes);
  if (!SkImage::MakeFromBitmap(bitmap)->readPixels(pixmap, 0, 0))
    return String();

  // Encode as a PNG and return as a data url.
  std::unique_ptr<ImageDataBuffer> buffer = ImageDataBuffer::Create(pixmap);

  if (!buffer)
    return String();

  return buffer->ToDataURL(kMimeTypePng, 1.0);
}

const AtomicString& AXNodeObject::AccessKey() const {
  auto* element = DynamicTo<Element>(GetNode());
  if (!element)
    return g_null_atom;
  return element->FastGetAttribute(html_names::kAccesskeyAttr);
}

int AXNodeObject::TextLength() const {
  if (!IsTextControl())
    return -1;
  return GetValueForControl().length();
}

RGBA32 AXNodeObject::ColorValue() const {
  auto* input = DynamicTo<HTMLInputElement>(GetNode());
  if (!input || !IsColorWell())
    return AXObject::ColorValue();

  const AtomicString& type = input->getAttribute(kTypeAttr);
  if (!EqualIgnoringASCIICase(type, "color"))
    return AXObject::ColorValue();

  // HTMLInputElement::value always returns a string parseable by Color.
  Color color;
  bool success = color.SetFromString(input->value());
  DCHECK(success);
  return color.Rgb();
}

RGBA32 AXNodeObject::BackgroundColor() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return Color::kTransparent;

  if (IsWebArea()) {
    LocalFrameView* view = DocumentFrameView();
    if (view)
      return view->BaseBackgroundColor().Rgb();
    else
      return Color::kWhite;
  }

  const ComputedStyle* style = layout_object->Style();
  if (!style || !style->HasBackground())
    return Color::kTransparent;

  return style->VisitedDependentColor(GetCSSPropertyBackgroundColor()).Rgb();
}

RGBA32 AXNodeObject::GetColor() const {
  if (!GetLayoutObject() || IsColorWell())
    return AXObject::GetColor();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXObject::GetColor();

  Color color = style->VisitedDependentColor(GetCSSPropertyColor());
  return color.Rgb();
}

String AXNodeObject::FontFamily() const {
  if (!GetLayoutObject())
    return AXObject::FontFamily();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXObject::FontFamily();

  const SimpleFontData* primary_font = style->GetFont().PrimaryFont();
  if (!primary_font)
    return AXObject::FontFamily();

  return primary_font->PlatformData().FontFamilyName();
}

// Font size is in pixels.
float AXNodeObject::FontSize() const {
  if (!GetLayoutObject())
    return AXObject::FontSize();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXObject::FontSize();

  return style->ComputedFontSize();
}

float AXNodeObject::FontWeight() const {
  if (!GetLayoutObject())
    return AXObject::FontWeight();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXObject::FontWeight();

  return style->GetFontWeight();
}

ax::mojom::blink::AriaCurrentState AXNodeObject::GetAriaCurrentState() const {
  const AtomicString& attribute_value =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kCurrent);
  if (attribute_value.IsNull())
    return ax::mojom::blink::AriaCurrentState::kNone;
  if (attribute_value.IsEmpty() ||
      EqualIgnoringASCIICase(attribute_value, "false"))
    return ax::mojom::blink::AriaCurrentState::kFalse;
  if (EqualIgnoringASCIICase(attribute_value, "true"))
    return ax::mojom::blink::AriaCurrentState::kTrue;
  if (EqualIgnoringASCIICase(attribute_value, "page"))
    return ax::mojom::blink::AriaCurrentState::kPage;
  if (EqualIgnoringASCIICase(attribute_value, "step"))
    return ax::mojom::blink::AriaCurrentState::kStep;
  if (EqualIgnoringASCIICase(attribute_value, "location"))
    return ax::mojom::blink::AriaCurrentState::kLocation;
  if (EqualIgnoringASCIICase(attribute_value, "date"))
    return ax::mojom::blink::AriaCurrentState::kDate;
  if (EqualIgnoringASCIICase(attribute_value, "time"))
    return ax::mojom::blink::AriaCurrentState::kTime;
  // An unknown value should return true.
  if (!attribute_value.IsEmpty())
    return ax::mojom::blink::AriaCurrentState::kTrue;

  return AXObject::GetAriaCurrentState();
}

ax::mojom::blink::InvalidState AXNodeObject::GetInvalidState() const {
  const AtomicString& attribute_value =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kInvalid);
  if (EqualIgnoringASCIICase(attribute_value, "false"))
    return ax::mojom::blink::InvalidState::kFalse;
  if (EqualIgnoringASCIICase(attribute_value, "true"))
    return ax::mojom::blink::InvalidState::kTrue;
  // "spelling" and "grammar" are also invalid values: they are exposed via
  // Markers() as if they are native errors, but also use the invalid entry
  // state on the node itself, therefore they are treated like "true".
  // in terms of the node's invalid state
  // A yet unknown value.
  if (!attribute_value.IsEmpty())
    return ax::mojom::blink::InvalidState::kOther;

  if (GetElement()) {
    ListedElement* form_control = ListedElement::From(*GetElement());
    if (form_control) {
      if (form_control->IsNotCandidateOrValid())
        return ax::mojom::blink::InvalidState::kFalse;
      else
        return ax::mojom::blink::InvalidState::kTrue;
    }
  }
  return AXObject::GetInvalidState();
}

int AXNodeObject::PosInSet() const {
  if (RoleValue() == ax::mojom::blink::Role::kPopUpButton && GetNode() &&
      !AXObjectCache().UseAXMenuList()) {
    if (auto* select_element = DynamicTo<HTMLSelectElement>(*GetNode()))
      return 1 + select_element->selectedIndex();
  }

  if (SupportsARIASetSizeAndPosInSet()) {
    uint32_t pos_in_set;
    if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kPosInSet, pos_in_set))
      return pos_in_set;
  }
  return 0;
}

int AXNodeObject::SetSize() const {
  if (RoleValue() == ax::mojom::blink::Role::kPopUpButton && GetNode() &&
      !AXObjectCache().UseAXMenuList()) {
    if (auto* select_element = DynamicTo<HTMLSelectElement>(*GetNode()))
      return static_cast<int>(select_element->length());
  }

  if (SupportsARIASetSizeAndPosInSet()) {
    int32_t set_size;
    if (HasAOMPropertyOrARIAAttribute(AOMIntProperty::kSetSize, set_size))
      return set_size;
  }
  return 0;
}

String AXNodeObject::AriaInvalidValue() const {
  if (GetInvalidState() == ax::mojom::blink::InvalidState::kOther)
    return GetAOMPropertyOrARIAAttribute(AOMStringProperty::kInvalid);

  return String();
}

bool AXNodeObject::ValueForRange(float* out_value) const {
  float value_now;
  if (HasAOMPropertyOrARIAAttribute(AOMFloatProperty::kValueNow, value_now)) {
    *out_value = value_now;
    return true;
  }

  if (IsNativeSlider() || IsNativeSpinButton()) {
    *out_value = To<HTMLInputElement>(*GetNode()).valueAsNumber();
    return std::isfinite(*out_value);
  }

  if (auto* meter = DynamicTo<HTMLMeterElement>(GetNode())) {
    *out_value = meter->value();
    return true;
  }

  // In ARIA 1.1, default values for aria-valuenow were changed as below.
  // - scrollbar, slider : half way between aria-valuemin and aria-valuemax
  // - separator : 50
  // - spinbutton : 0
  switch (AriaRoleAttribute()) {
    case ax::mojom::blink::Role::kScrollBar:
    case ax::mojom::blink::Role::kSlider: {
      float min_value, max_value;
      if (MinValueForRange(&min_value) && MaxValueForRange(&max_value)) {
        *out_value = (min_value + max_value) / 2.0f;
        return true;
      }
      FALLTHROUGH;
    }
    case ax::mojom::blink::Role::kSplitter: {
      *out_value = 50.0f;
      return true;
    }
    case ax::mojom::blink::Role::kSpinButton: {
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
    *out_value = static_cast<float>(To<HTMLInputElement>(*GetNode()).Maximum());
    return std::isfinite(*out_value);
  }

  if (auto* meter = DynamicTo<HTMLMeterElement>(GetNode())) {
    *out_value = meter->max();
    return true;
  }

  // In ARIA 1.1, default value of scrollbar, separator and slider
  // for aria-valuemax were changed to 100.
  switch (AriaRoleAttribute()) {
    case ax::mojom::blink::Role::kScrollBar:
    case ax::mojom::blink::Role::kSplitter:
    case ax::mojom::blink::Role::kSlider: {
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
    *out_value = static_cast<float>(To<HTMLInputElement>(*GetNode()).Minimum());
    return std::isfinite(*out_value);
  }

  if (auto* meter = DynamicTo<HTMLMeterElement>(GetNode())) {
    *out_value = meter->min();
    return true;
  }

  // In ARIA 1.1, default value of scrollbar, separator and slider
  // for aria-valuemin were changed to 0.
  switch (AriaRoleAttribute()) {
    case ax::mojom::blink::Role::kScrollBar:
    case ax::mojom::blink::Role::kSplitter:
    case ax::mojom::blink::Role::kSlider: {
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
    // AT may want to know whether a step value was explicitly provided or not,
    // so return false if there was not one set.
    if (!To<HTMLInputElement>(*GetNode())
             .FastGetAttribute(html_names::kStepAttr)) {
      *out_value = 0.0f;
      return false;
    }

    auto step =
        To<HTMLInputElement>(*GetNode()).CreateStepRange(kRejectAny).Step();
    *out_value = step.ToString().ToFloat();
    return std::isfinite(*out_value);
  }

  switch (AriaRoleAttribute()) {
    case ax::mojom::blink::Role::kScrollBar:
    case ax::mojom::blink::Role::kSplitter:
    case ax::mojom::blink::Role::kSlider: {
      *out_value = 0.0f;
      return true;
    }
    default:
      break;
  }

  return false;
}

KURL AXNodeObject::Url() const {
  if (IsAnchor()) {
    const Element* anchor = AnchorElement();

    if (const auto* html_anchor = DynamicTo<HTMLAnchorElement>(anchor)) {
      return html_anchor->Href();
    }

    // Some non-HTML elements, most notably SVG <a> elements, can act as
    // links/anchors.
    if (anchor)
      return anchor->HrefURL();
  }

  if (IsWebArea() && GetDocument())
    return GetDocument()->Url();

  auto* html_image_element = DynamicTo<HTMLImageElement>(GetNode());
  if (IsImage() && html_image_element) {
    // Using ImageSourceURL handles both src and srcset.
    String source_url = html_image_element->ImageSourceURL();
    String stripped_image_source_url =
        StripLeadingAndTrailingHTMLSpaces(source_url);
    if (!stripped_image_source_url.IsEmpty())
      return GetDocument()->CompleteURL(stripped_image_source_url);
  }

  if (IsInputImage())
    return To<HTMLInputElement>(GetNode())->Src();

  return KURL();
}

AXObject* AXNodeObject::ChooserPopup() const {
  // When color & date chooser popups are visible, they can be found in the tree
  // as a WebArea child of the <input> control itself.
  switch (native_role_) {
    case ax::mojom::blink::Role::kColorWell:
    case ax::mojom::blink::Role::kDate:
    case ax::mojom::blink::Role::kDateTime: {
      for (const auto& child : ChildrenIncludingIgnored()) {
        if (child->IsWebArea())
          return child;
      }
      return nullptr;
    }
    default:
      return nullptr;
  }
}

String AXNodeObject::GetValueForControl() const {
  // TODO(crbug.com/1165853): Remove this method completely and compute value on
  // the browser side.
  Node* node = GetNode();
  if (!node)
    return String();

  if (const auto* select_element = DynamicTo<HTMLSelectElement>(*node)) {
    if (!select_element->UsesMenuList())
      return String();

    // In most cases, we want to return what's actually displayed inside the
    // <select> element on screen, unless there is an ARIA label overriding it.
    int selected_index = select_element->SelectedListIndex();
    const HeapVector<Member<HTMLElement>>& list_items =
        select_element->GetListItems();
    if (selected_index >= 0 &&
        static_cast<wtf_size_t>(selected_index) < list_items.size()) {
      const AtomicString& overridden_description =
          list_items[selected_index]->FastGetAttribute(
              html_names::kAriaLabelAttr);
      if (!overridden_description.IsNull())
        return overridden_description;
    }

    // We don't retrieve the element's value attribute on purpose. The value
    // attribute might be sanitized and might be different from what is actually
    // displayed inside the <select> element on screen.
    return select_element->InnerElement().innerText();
  }

  if (IsTextControl()) {
    if (!IsA<HTMLTextAreaElement>(*node) && !IsA<HTMLInputElement>(*node)) {
      // The text control is a contenteditable.
      auto* element = DynamicTo<Element>(node);
      return element ? element->GetInnerTextWithoutUpdate() : String();
    }

    // For an Input or a textarea: We should not simply return the "value"
    // attribute because it might be sanitized in some input control types, e.g.
    // email fields. If we do that, then "selectionStart" and "selectionEnd"
    // indices will not match with the text in the sanitized value.
    String inner_text = ToTextControl(*node).InnerEditorValue();
    if (!IsPasswordFieldAndShouldHideValue())
      return inner_text;

    if (!GetLayoutObject())
      return inner_text;

    const ComputedStyle* style = GetLayoutObject()->Style();
    if (!style)
      return inner_text;

    unsigned int unmasked_text_length = inner_text.length();
    if (!unmasked_text_length)
      return String();

    UChar mask_character = 0;
    switch (style->TextSecurity()) {
      case ETextSecurity::kNone:
        break;  // Fall through to the non-password branch.
      case ETextSecurity::kDisc:
        mask_character = kBulletCharacter;
        break;
      case ETextSecurity::kCircle:
        mask_character = kWhiteBulletCharacter;
        break;
      case ETextSecurity::kSquare:
        mask_character = kBlackSquareCharacter;
        break;
    }
    if (!mask_character)
      return inner_text;

    StringBuilder masked_text;
    masked_text.ReserveCapacity(unmasked_text_length);
    for (unsigned int i = 0; i < unmasked_text_length; ++i)
      masked_text.Append(mask_character);
    return masked_text.ToString();
  }

  if (IsRangeValueSupported()) {
    String aria_value_text =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kValueText)
            .GetString();
    if (!aria_value_text.IsEmpty())
      return aria_value_text;
  }

  if (GetLayoutObject() && GetLayoutObject()->IsFileUploadControl())
    return To<LayoutFileUploadControl>(GetLayoutObject())->FileTextValue();

  // Handle other HTML input elements that aren't text controls, like date and
  // time controls, by returning their value converted to text, with the
  // exception of checkboxes and radio buttons (which would return "on"), and
  // buttons which will return their name.
  // https://html.spec.whatwg.org/C/#dom-input-value
  if (const auto* input = DynamicTo<HTMLInputElement>(node)) {
    if (input->type() == input_type_names::kFile)
      return input->FileStatusText();

    if (input->type() != input_type_names::kButton &&
        input->type() != input_type_names::kCheckbox &&
        input->type() != input_type_names::kImage &&
        input->type() != input_type_names::kRadio &&
        input->type() != input_type_names::kReset &&
        input->type() != input_type_names::kSubmit) {
      return input->value();
    }
  }

  // An ARIA combobox can get value from inner contents.
  if (AriaRoleAttribute() == ax::mojom::blink::Role::kComboBoxMenuButton) {
    AXObjectSet visited;
    return TextFromDescendants(visited, false);
  }

  return String();
}

ax::mojom::blink::Role AXNodeObject::AriaRoleAttribute() const {
  return aria_role_;
}

bool AXNodeObject::HasAriaAttribute() const {
  Element* element = GetElement();
  if (!element)
    return false;

  // Explicit ARIA role should be considered an aria attribute.
  if (AriaRoleAttribute() != ax::mojom::blink::Role::kUnknown)
    return true;

  AttributeCollection attributes = element->AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    // Attributes cache their uppercase names.
    if (attr.GetName().LocalNameUpper().StartsWith("ARIA-"))
      return true;
  }

  return false;
}

void AXNodeObject::AriaDescribedbyElements(AXObjectVector& describedby) const {
  AccessibilityChildrenFromAOMProperty(AOMRelationListProperty::kDescribedBy,
                                       describedby);
}

void AXNodeObject::AriaOwnsElements(AXObjectVector& owns) const {
  AccessibilityChildrenFromAOMProperty(AOMRelationListProperty::kOwns, owns);
}

// TODO(accessibility): Aria-dropeffect and aria-grabbed are deprecated in
// aria 1.1 Also those properties are expected to be replaced by a new feature
// in a future version of WAI-ARIA. After that we will re-implement them
// following new spec.
bool AXNodeObject::SupportsARIADragging() const {
  const AtomicString& grabbed = GetAttribute(html_names::kAriaGrabbedAttr);
  return EqualIgnoringASCIICase(grabbed, "true") ||
         EqualIgnoringASCIICase(grabbed, "false");
}

ax::mojom::blink::Dropeffect AXNodeObject::ParseDropeffect(
    String& dropeffect) const {
  if (EqualIgnoringASCIICase(dropeffect, "copy"))
    return ax::mojom::blink::Dropeffect::kCopy;
  if (EqualIgnoringASCIICase(dropeffect, "execute"))
    return ax::mojom::blink::Dropeffect::kExecute;
  if (EqualIgnoringASCIICase(dropeffect, "link"))
    return ax::mojom::blink::Dropeffect::kLink;
  if (EqualIgnoringASCIICase(dropeffect, "move"))
    return ax::mojom::blink::Dropeffect::kMove;
  if (EqualIgnoringASCIICase(dropeffect, "popup"))
    return ax::mojom::blink::Dropeffect::kPopup;
  return ax::mojom::blink::Dropeffect::kNone;
}

void AXNodeObject::Dropeffects(
    Vector<ax::mojom::blink::Dropeffect>& dropeffects) const {
  if (!HasAttribute(html_names::kAriaDropeffectAttr))
    return;

  Vector<String> str_dropeffects;
  TokenVectorFromAttribute(str_dropeffects, html_names::kAriaDropeffectAttr);

  if (str_dropeffects.IsEmpty()) {
    dropeffects.push_back(ax::mojom::blink::Dropeffect::kNone);
    return;
  }

  for (auto&& str : str_dropeffects) {
    dropeffects.push_back(ParseDropeffect(str));
  }
}

ax::mojom::blink::HasPopup AXNodeObject::HasPopup() const {
  const AtomicString& has_popup =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kHasPopUp);
  if (!has_popup.IsNull()) {
    if (EqualIgnoringASCIICase(has_popup, "false"))
      return ax::mojom::blink::HasPopup::kFalse;

    if (EqualIgnoringASCIICase(has_popup, "listbox"))
      return ax::mojom::blink::HasPopup::kListbox;

    if (EqualIgnoringASCIICase(has_popup, "tree"))
      return ax::mojom::blink::HasPopup::kTree;

    if (EqualIgnoringASCIICase(has_popup, "grid"))
      return ax::mojom::blink::HasPopup::kGrid;

    if (EqualIgnoringASCIICase(has_popup, "dialog"))
      return ax::mojom::blink::HasPopup::kDialog;

    // To provide backward compatibility with ARIA 1.0 content,
    // user agents MUST treat an aria-haspopup value of true
    // as equivalent to a value of menu.
    if (EqualIgnoringASCIICase(has_popup, "true") ||
        EqualIgnoringASCIICase(has_popup, "menu"))
      return ax::mojom::blink::HasPopup::kMenu;
  }

  // ARIA 1.1 default value of haspopup for combobox is "listbox".
  if (RoleValue() == ax::mojom::blink::Role::kComboBoxMenuButton ||
      RoleValue() == ax::mojom::blink::Role::kTextFieldWithComboBox)
    return ax::mojom::blink::HasPopup::kListbox;

  if (AXObjectCache().GetAutofillState(AXObjectID()) !=
      WebAXAutofillState::kNoSuggestions) {
    return ax::mojom::blink::HasPopup::kMenu;
  }

  return AXObject::HasPopup();
}

// Returns the nearest block-level LayoutBlockFlow ancestor
static LayoutBlockFlow* NonInlineBlockFlow(LayoutObject* object) {
  LayoutObject* current = object;
  while (current) {
    auto* block_flow = DynamicTo<LayoutBlockFlow>(current);
    if (block_flow && !block_flow->IsAtomicInlineLevel())
      return block_flow;
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
// Modify or take an action on an object.
//

bool AXNodeObject::OnNativeSetValueAction(const String& string) {
  if (!GetNode() || !GetNode()->IsElementNode())
    return false;
  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsBoxModelObject())
    return false;

  auto* html_input_element = DynamicTo<HTMLInputElement>(*GetNode());
  if (html_input_element && layout_object->IsTextFieldIncludingNG()) {
    html_input_element->setValue(
        string, TextFieldEventBehavior::kDispatchInputAndChangeEvent);
    return true;
  }

  if (auto* text_area_element = DynamicTo<HTMLTextAreaElement>(*GetNode())) {
    DCHECK(layout_object->IsTextAreaIncludingNG());
    text_area_element->setValue(
        string, TextFieldEventBehavior::kDispatchInputAndChangeEvent);
    return true;
  }

  if (HasContentEditableAttributeSet()) {
    ExceptionState exception_state(v8::Isolate::GetCurrent(),
                                   ExceptionState::kUnknownContext, nullptr,
                                   nullptr);
    To<HTMLElement>(GetNode())->setInnerText(string, exception_state);
    if (exception_state.HadException()) {
      exception_state.ClearException();
      return false;
    }
    return true;
  }

  return false;
}

//
// New AX name calculation.
//

String AXNodeObject::GetName(ax::mojom::blink::NameFrom& name_from,
                             AXObjectVector* name_objects) const {
  String name = AXObject::GetName(name_from, name_objects);
  if (RoleValue() == ax::mojom::blink::Role::kSpinButton &&
      DatetimeAncestor()) {
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
                                     ax::mojom::blink::NameFrom& name_from,
                                     AXRelatedObjectVector* related_objects,
                                     NameSources* name_sources) const {
  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (name_sources)
    DCHECK(related_objects);

  bool found_text_alternative = false;

  if (!GetNode() && !GetLayoutObject())
    return String();

  // Exclude offscreen objects inside a portal.
  // NOTE: If an object is found to be offscreen, this also omits its children,
  // which may not be offscreen in some cases.
  Page* page = GetNode() ? GetNode()->GetDocument().GetPage() : nullptr;
  if (page && page->InsidePortal()) {
    LayoutRect bounds = GetBoundsInFrameCoordinates();
    IntSize document_size =
        GetNode()->GetDocument().GetLayoutView()->GetLayoutSize();
    bool is_visible = bounds.Intersects(LayoutRect(IntPoint(), document_size));
    if (!is_visible)
      return String();
  }

  // Step 2E from: http://www.w3.org/TR/accname-aam-1.1 -- value from control.
  // This must occur before 2C, because 2C is not applied if 2E will be:
  // 2C: "If traversal of the current node is due to recursion and the current
  // node is an embedded control as defined in step 2E, ignore aria-label and
  // skip to rule 2E".
  // Note that 2E only applies the label is for "another widget", therefore, the
  // value cannot be used to label the original control, even if aria-labelledby
  // points to itself. The easiest way to check this is by testing whether this
  // node has already been visited.
  if (recursive && !visited.Contains(this)) {
    String value_for_name = GetValueContributionToName();
    if (!value_for_name.IsNull())
      return value_for_name;
  }

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1 -- aria-label.
  String text_alternative = AriaTextAlternative(
      recursive, in_aria_labelled_by_traversal, visited, name_from,
      related_objects, name_sources, &found_text_alternative);
  if (found_text_alternative && !name_sources)
    return text_alternative;

  // Step 2D from: http://www.w3.org/TR/accname-aam-1.1  -- native markup.
  text_alternative =
      NativeTextAlternative(visited, name_from, related_objects, name_sources,
                            &found_text_alternative);
  const bool has_text_alternative =
      !text_alternative.IsEmpty() ||
      name_from == ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty;
  if (has_text_alternative && !name_sources)
    return text_alternative;

  // Step 2F / 2G from: http://www.w3.org/TR/accname-aam-1.1 -- from content.
  if (in_aria_labelled_by_traversal || SupportsNameFromContents(recursive)) {
    Node* node = GetNode();
    if (!IsA<HTMLSelectElement>(node)) {  // Avoid option descendant text
      name_from = ax::mojom::blink::NameFrom::kContents;
      if (name_sources) {
        name_sources->push_back(NameSource(found_text_alternative));
        name_sources->back().type = name_from;
      }

      if (auto* text_node = DynamicTo<Text>(node))
        text_alternative = text_node->data();
      else if (IsA<HTMLBRElement>(node))
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
  name_from = ax::mojom::blink::NameFrom::kTitle;
  if (name_sources) {
    name_sources->push_back(NameSource(found_text_alternative, kTitleAttr));
    name_sources->back().type = name_from;
  }
  const AtomicString& title = GetAttribute(kTitleAttr);
  if (!title.IsEmpty()) {
    text_alternative = title;
    name_from = ax::mojom::blink::NameFrom::kTitle;
    if (name_sources) {
      found_text_alternative = true;
      name_sources->back().text = text_alternative;
    } else {
      return text_alternative;
    }
  }

  name_from = ax::mojom::blink::NameFrom::kUninitialized;

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
    ax::mojom::blink::NameFrom last_used_name_from,
    ax::mojom::blink::NameFrom name_from) {
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
    case ax::mojom::blink::NameFrom::kNone:
    case ax::mojom::blink::NameFrom::kUninitialized:
    case ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty:
    case ax::mojom::blink::NameFrom::kContents:
      break;
    case ax::mojom::blink::NameFrom::kAttribute:
    case ax::mojom::blink::NameFrom::kCaption:
    case ax::mojom::blink::NameFrom::kPlaceholder:
    case ax::mojom::blink::NameFrom::kRelatedElement:
    case ax::mojom::blink::NameFrom::kTitle:
    case ax::mojom::blink::NameFrom::kValue:
      return true;
  }
  switch (name_from) {
    case ax::mojom::blink::NameFrom::kNone:
    case ax::mojom::blink::NameFrom::kUninitialized:
    case ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty:
    case ax::mojom::blink::NameFrom::kContents:
      break;
    case ax::mojom::blink::NameFrom::kAttribute:
    case ax::mojom::blink::NameFrom::kCaption:
    case ax::mojom::blink::NameFrom::kPlaceholder:
    case ax::mojom::blink::NameFrom::kRelatedElement:
    case ax::mojom::blink::NameFrom::kTitle:
    case ax::mojom::blink::NameFrom::kValue:
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
  ax::mojom::blink::NameFrom last_used_name_from =
      ax::mojom::blink::NameFrom::kUninitialized;

  AXObjectVector children;

  HeapVector<Member<AXObject>> owned_children;
  AXObjectCache().GetAriaOwnedChildren(this, owned_children);

  // TODO(aleventhal) Why isn't this just using cached children?
  AXNodeObject* parent = const_cast<AXNodeObject*>(this);
  for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node_); child;
       child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
    auto* child_text_node = DynamicTo<Text>(child);
    if (child_text_node &&
        child_text_node->wholeText().ContainsOnlyWhitespaceOrEmpty()) {
      // skip over empty text nodes
      continue;
    }
    AXObject* child_obj = AXObjectCache().GetOrCreate(child, parent);
    if (child_obj && !AXObjectCache().IsAriaOwned(child_obj))
      children.push_back(child_obj);
  }
  for (const auto& owned_child : owned_children)
    children.push_back(owned_child);

  for (AXObject* child : children) {
    constexpr size_t kMaxDescendantsForTextAlternativeComputation = 100;
    if (visited.size() > kMaxDescendantsForTextAlternativeComputation + 1)
      break;  // Need to add 1 because the root naming node is in the list.
    // If a child is a continuation, we should ignore attributes like
    // hidden and presentational. See LAYOUT TREE WALKING ALGORITHM in
    // ax_layout_object.cc for more information on continuations.
    bool is_continuation = child->GetLayoutObject() &&
                           child->GetLayoutObject()->IsElementContinuation();

    // Don't recurse into children that are explicitly hidden.
    // Note that we don't call IsInertOrAriaHidden because that would return
    // true if any ancestor is hidden, but we need to be able to compute the
    // accessible name of object inside hidden subtrees (for example, if
    // aria-labelledby points to an object that's hidden).
    if (!is_continuation &&
        (child->AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kHidden) ||
         child->IsHiddenForTextAlternativeCalculation()))
      continue;

    ax::mojom::blink::NameFrom child_name_from =
        ax::mojom::blink::NameFrom::kUninitialized;
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
  // Try both spellings, but prefer aria-labelledby, which is the official spec.
  const QualifiedName& attr =
      HasAttribute(html_names::kAriaLabeledbyAttr) &&
              !HasAttribute(html_names::kAriaLabelledbyAttr)
          ? html_names::kAriaLabeledbyAttr
          : html_names::kAriaLabelledbyAttr;
  HeapVector<Member<Element>> elements_from_attribute;
  Vector<String> ids;
  ElementsFromAttribute(elements_from_attribute, attr, ids);
  if (elements_from_attribute.size() > 0)
    return false;

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1
  const AtomicString& aria_label =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kLabel);
  if (!aria_label.IsEmpty())
    return false;

  // Based on
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
  // 5.1/5.5 Text inputs, Other labelable Elements
  auto* html_element = DynamicTo<HTMLElement>(GetNode());
  if (html_element && html_element->IsLabelable()) {
    if (html_element->labels() && html_element->labels()->length() > 0)
      return true;
  }

  return false;
}

void AXNodeObject::GetRelativeBounds(AXObject** out_container,
                                     FloatRect& out_bounds_in_container,
                                     SkMatrix44& out_container_transform,
                                     bool* clips_children) const {
  if (GetLayoutObject()) {
    AXObject::GetRelativeBounds(out_container, out_bounds_in_container,
                                out_container_transform, clips_children);
    return;
  }

#if DCHECK_IS_ON()
  DCHECK(!getting_bounds_) << "GetRelativeBounds reentrant: " << ToString(true);
  base::AutoReset<bool> reentrancy_protector(&getting_bounds_, true);
#endif

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

  Element* element = GetElement();
  // If it's in a canvas but doesn't have an explicit rect, or has display:
  // contents set, get the bounding rect of its children.
  if ((GetNode()->parentElement() &&
       GetNode()->parentElement()->IsInCanvasSubtree()) ||
      (element && element->HasDisplayContentsStyle())) {
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
    if (IsA<AXLayoutObject>(position_provider)) {
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

bool AXNodeObject::HasValidHTMLTableStructureAndLayout() const {
  // Is it a visible <table> with a table-like role and layout?
  if (!IsTableLikeRole() || !GetLayoutObject() ||
      !GetLayoutObject()->IsTable() || !IsA<HTMLTableElement>(GetNode()))
    return false;

  // Check for any invalid children, as far as W3C table validity is concerned.
  // * If no invalid children exist, this will be considered a valid table,
  //   and AddTableChildren() can be used to add the children in rendered order.
  // * If any invalid children exist, this table will be considered invalid.
  //   In that case the children will still be added via AddNodeChildren(),
  //   so that no content is lost.
  // See comments in AddTableChildren() for more information about valid tables.
  for (Element* child = ElementTraversal::FirstChild(*GetElement()); child;
       child = ElementTraversal::NextSibling(*child)) {
    if (!IsA<HTMLTableSectionElement>(child) &&
        !IsA<HTMLTableCaptionElement>(child) &&
        !child->HasTagName(html_names::kColgroupTag)) {
      return false;
    }
  }

  return true;
}

void AXNodeObject::AddTableChildren() {
  // Add the caption (if any) and table sections in the visible order.
  //
  // Implementation notes:
  //
  // * In a valid table, there is always at least one section child DOM node.
  //   For example, if the HTML of the web page includes <tr>s as direct
  //   children of a <table>, Blink will insert a <tbody> as a child of the
  //   table, and parent of the <tr> elements.
  //
  // * Rendered order can differ from DOM order:
  //   The valid DOM order of <table> children is specified here:
  //   https://html.spec.whatwg.org/multipage/tables.html#the-table-element,
  //   "... optionally a caption element, followed by zero or more
  //   colgroup elements, followed optionally by a thead element, followed by
  //   either zero or more tbody elements or one or more tr elements, followed
  //   optionally by a tfoot element"
  //   However, even if the DOM children occur in an incorrect order, Blink
  //   automatically renders them as if they were in the correct order.
  //   The following code ensures that the children are added to the AX tree in
  //   the same order as Blink renders them.

  DCHECK(HasValidHTMLTableStructureAndLayout());
  auto* html_table_element = To<HTMLTableElement>(GetNode());
  AddNodeChild(html_table_element->caption());
  AddNodeChild(html_table_element->tHead());
  for (Node* node : *html_table_element->tBodies())
    AddNodeChild(node);
  AddNodeChild(html_table_element->tFoot());
}

int AXNodeObject::TextOffsetInFormattingContext(int offset) const {
  DCHECK_GE(offset, 0);
  if (IsDetached())
    return 0;

  // When a node has the first-letter CSS style applied to it, it is split into
  // two parts (two branches) in the layout tree. The "first-letter part"
  // contains its first letter and any surrounding Punctuation. The "remaining
  // part" contains the rest of the text.
  //
  // We need to ensure that we retrieve the correct layout object: either the
  // one for the "first-letter part" or the one for the "remaining part",
  // depending of the value of |offset|.
  const LayoutObject* layout_obj =
      GetNode() ? AssociatedLayoutObjectOf(*GetNode(), offset)
                : GetLayoutObject();
  if (!layout_obj)
    return AXObject::TextOffsetInFormattingContext(offset);

  // We support calculating the text offset from the start of the formatting
  // contexts of the following layout objects, provided that they are at
  // inline-level, (display=inline) or "display=inline-block":
  //
  // (Note that in the following examples, the paragraph is the formatting
  // context.
  //
  // Layout replaced, e.g. <p><img></p>.
  // Layout inline with a layout text child, e.g. <p><a href="#">link</a></p>.
  // Layout block flow, e.g. <p><b style="display: inline-block;"></b></p>.
  // Layout text, e.g. <p>Hello</p>.
  // Layout br (subclass of layout text), e.g. <p><br></p>.

  if (layout_obj->IsLayoutInline()) {
    // The NGOffsetMapping class doesn't map layout inline objects to their text
    // mappings because such an operation could be ambiguous. An inline object
    // may have another inline object inside it. For example,
    // <span><span>Inner</span outer</span>. We need to recursively retrieve the
    // first layout text or layout replaced child so that any potential
    // ambiguity would be removed.
    const AXObject* first_child = FirstChildIncludingIgnored();
    return first_child ? first_child->TextOffsetInFormattingContext(offset)
                       : offset;
  }

  // TODO(crbug.com/567964): LayoutObject::IsAtomicInlineLevel() also includes
  // block-level replaced elements. We need to explicitly exclude them via
  // LayoutObject::IsInline().
  const bool is_atomic_inline_level =
      layout_obj->IsInline() && layout_obj->IsAtomicInlineLevel();
  if (!is_atomic_inline_level && !layout_obj->IsText()) {
    // Not in a formatting context in which text offsets are meaningful.
    return AXObject::TextOffsetInFormattingContext(offset);
  }

  // TODO(crbug.com/1149171): NGInlineOffsetMappingBuilder does not properly
  // compute offset mappings for empty LayoutText objects. Other text objects
  // (such as some list markers) are not affected.
  if (const LayoutText* layout_text = DynamicTo<LayoutText>(layout_obj)) {
    if (layout_text->GetText().IsEmpty())
      return AXObject::TextOffsetInFormattingContext(offset);
  }

  LayoutBlockFlow* formatting_context =
      NGOffsetMapping::GetInlineFormattingContextOf(*layout_obj);
  if (!formatting_context || formatting_context == layout_obj)
    return AXObject::TextOffsetInFormattingContext(offset);

  // If "formatting_context" is not a Layout NG object, the offset mappings will
  // be computed on demand and cached.
  const NGOffsetMapping* inline_offset_mapping =
      NGInlineNode::GetOffsetMapping(formatting_context);
  if (!inline_offset_mapping)
    return AXObject::TextOffsetInFormattingContext(offset);

  const base::span<const NGOffsetMappingUnit> mapping_units =
      inline_offset_mapping->GetMappingUnitsForLayoutObject(*layout_obj);
  if (mapping_units.empty())
    return AXObject::TextOffsetInFormattingContext(offset);
  return int{mapping_units.front().TextContentStart()} + offset;
}

//
// Inline text boxes.
//

void AXNodeObject::LoadInlineTextBoxes() {
  if (!GetLayoutObject())
    return;

  if (GetLayoutObject()->IsText()) {
    ClearChildren();
    AddInlineTextBoxChildren(true);
    children_dirty_ = false;  // Avoid adding these children twice.
    return;
  }

  for (const auto& child : ChildrenIncludingIgnored()) {
    child->LoadInlineTextBoxes();
  }
}

void AXNodeObject::AddInlineTextBoxChildren(bool force) {
  Document* document = GetDocument();
  if (!document) {
    NOTREACHED();
    return;
  }

  Settings* settings = document->GetSettings();
  if (!force &&
      (!settings || !settings->GetInlineTextBoxAccessibilityEnabled())) {
    return;
  }

  if (!GetLayoutObject() || !GetLayoutObject()->IsText())
    return;

  if (GetLayoutObject()->NeedsLayout()) {
    // If a LayoutText or a LayoutBR needs layout, its inline text boxes are
    // either nonexistent or invalid, so defer until the layout happens and the
    // layoutObject calls AXObjectCacheImpl::inlineTextBoxesUpdated.
    return;
  }

  if (LastKnownIsIgnoredValue()) {
    // Inline textboxes are included if and only if the parent is unignored.
    // If the parent is ignored but included in tree, the inline textbox is
    // still withheld.
    return;
  }

  auto* layout_text = To<LayoutText>(GetLayoutObject());
  for (scoped_refptr<AbstractInlineTextBox> box =
           layout_text->FirstAbstractInlineTextBox();
       box.get(); box = box->NextInlineTextBox()) {
    AXObject* ax_box = AXObjectCache().GetOrCreate(box.get(), this);
    if (!ax_box)
      continue;

    children_.push_back(ax_box);
  }
}

void AXNodeObject::AddValidationMessageChild() {
  DCHECK(IsWebArea()) << "Validation message must be child of root";
  // First child requirement enables easy checking to see if a children changed
  // event is needed in AXObjectCacheImpl::ValidationMessageObjectIfInvalid().
  DCHECK_EQ(children_.size(), 0U)
      << "Validation message must be the first child";
  AddChildAndCheckIncluded(AXObjectCache().ValidationMessageObjectIfInvalid(
      /* suppress children changed, already processing that */ false));
}

void AXNodeObject::AddImageMapChildren() {
  HTMLMapElement* map = GetMapForImage(GetLayoutObject());
  if (!map)
    return;

  HTMLImageElement* curr_image_element = DynamicTo<HTMLImageElement>(GetNode());
  DCHECK(curr_image_element);
  DCHECK(curr_image_element->IsLink());
  String usemap = curr_image_element->FastGetAttribute(html_names::kUsemapAttr);
  DCHECK(!usemap.IsEmpty());

  // Even though several images can point to the same map via usemap, only
  // use one reported via HTMLImageMapElement::ImageElement(), which is always
  // the first image in the DOM that matches the #usemap, even if there are
  // changes to the DOM. Only allow map children for the primary image.
  // This avoids two problems:
  // 1. Focusing the same area but in a different image scrolls the page to
  //    the first image that uses that map. Safari does the same thing, and
  //    Firefox does something similar (but seems to prefer the last image).
  // 2. When an object has multiple parents, serialization errors occur.
  // While allowed in the spec, using multiple images with the same map is not
  // handled well in browsers (problem #1), and serializer support for multiple
  // parents of the same area children is messy.

  // Get the primary image, which is the first image using this map.
  HTMLImageElement* primary_image_element = map->ImageElement();
  DCHECK(primary_image_element);
#if DCHECK_IS_ON()
  // Prove that this is the same as getting the first image using this map.
  String usemap_selector = "img[usemap=\"";
  usemap_selector = usemap_selector + usemap + "\"]";
  Element* first_image_with_this_usemap =
      GetDocument()->QuerySelector(AtomicString(usemap_selector));
  DCHECK(primary_image_element) << "No match for " << usemap_selector;
  DCHECK_EQ(primary_image_element, first_image_with_this_usemap);
#endif

  // Is this the primary image for this map?
  if (primary_image_element != curr_image_element) {
    // No, the current image (for |this|) is not the primary image.
    // Therefore, do not add area children to it.
    AXObject* ax_primary_image =
        AXObjectCache().GetOrCreate(primary_image_element);
    if (ax_primary_image &&
        ax_primary_image->ChildCountIncludingIgnored() == 0 &&
        Traversal<HTMLAreaElement>::FirstWithin(*map)) {
      // The primary image still needs to add the area children, and there's at
      // least one to add.
      AXObjectCache().ChildrenChanged(primary_image_element);
    }
    return;
  }

  // Yes, this is the primary image.
  HTMLAreaElement* first_area = Traversal<HTMLAreaElement>::FirstWithin(*map);
  if (first_area) {
    // If the <area> children were part of a different parent, notify that
    // parent that its children have changed.
    if (AXObject* ax_preexisting = AXObjectCache().Get(first_area)) {
      if (AXObject* ax_previous_parent = ax_preexisting->CachedParentObject()) {
        DCHECK_NE(ax_previous_parent, this);
        DCHECK(ax_previous_parent->GetNode());
        AXObjectCache().ChildrenChangedWithCleanLayout(
            ax_previous_parent->GetNode(), ax_previous_parent);
          ax_previous_parent->ClearChildren();
      }
    }

    // Add the area children to |this|.
    for (HTMLAreaElement& area :
         Traversal<HTMLAreaElement>::DescendantsOf(*map)) {
      // Add an <area> element for this child if it has a link and is visible.
      AddChildAndCheckIncluded(AXObjectCache().GetOrCreate(&area, this));
    }
  }
}

void AXNodeObject::AddPopupChildren() {
  if (!AXObjectCache().UseAXMenuList()) {
    auto* html_select_element = DynamicTo<HTMLSelectElement>(GetNode());
    if (html_select_element && html_select_element->UsesMenuList())
      AddChildAndCheckIncluded(html_select_element->PopupRootAXObject());
    return;
  }

  auto* html_input_element = DynamicTo<HTMLInputElement>(GetNode());
  if (!html_input_element)
    return;
  AddChildAndCheckIncluded(html_input_element->PopupRootAXObject());
}

bool AXNodeObject::CanAddLayoutChild(LayoutObject& child) {
  if (child.IsAnonymous())
    return true;

  // An non-anonymous layout object (has a DOM node) is only reached when a
  // pseudo element is inside another pseudo element.
  // This is because layout object traversal only occurs for pseudo element
  // subtrees -- see AXObject::ShouldUseLayoutObjectTraversalForChildren().
  // The only way a node will occur inside of that subtree is if it's another
  // pseudo element.
  DCHECK(child.GetNode()->IsPseudoElement());

  // Only add this inner pseudo element if it hasn't been added elsewhere.
  // An example is ::before with ::first-letter.
  AXObject* ax_preexisting = AXObjectCache().Get(&child);
  return !ax_preexisting || !ax_preexisting->CachedParentObject() ||
         ax_preexisting->CachedParentObject() == this;
}

#if DCHECK_IS_ON()
#define CHECK_NO_OTHER_PARENT_FOR(child)                                \
  AXObject* ax_preexisting = AXObjectCache().Get(child);                \
  DCHECK(!ax_preexisting || !ax_preexisting->CachedParentObject() ||    \
         ax_preexisting->CachedParentObject() == this)                  \
      << "Newly added child can't have a different preexisting parent:" \
      << "\nChild = " << ax_preexisting->ToString(true, true)           \
      << "\nNew parent = " << ToString(true, true)                      \
      << "\nPreexisting parent = "                                      \
      << ax_preexisting->CachedParentObject()->ToString(true, true);
#else
#define CHECK_NO_OTHER_PARENT_FOR(child) (void(0))
#endif

void AXNodeObject::AddLayoutChildren() {
  // Children are added this way only for pseudo-element subtrees.
  // See AXObject::ShouldUseLayoutObjectTraversalForChildren().
  DCHECK(GetLayoutObject());
  LayoutObject* child = GetLayoutObject()->SlowFirstChild();
  while (child) {
    DCHECK(AXObjectCacheImpl::IsPseudoElementDescendant(*child));
    if (CanAddLayoutChild(*child)) {
      CHECK_NO_OTHER_PARENT_FOR(child);
      // All added pseudo element desecendants are included in the tree.
      AddChildAndCheckIncluded(AXObjectCache().GetOrCreate(child, this));
    }
    child = child->NextSibling();
  }
}

void AXNodeObject::AddNodeChildren() {
  if (!node_)
    return;

  for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node_); child;
       child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
    AddNodeChild(child);
  }
}

void AXNodeObject::AddAccessibleNodeChildren() {
  Element* element = GetElement();
  if (!element)
    return;

  AccessibleNode* accessible_node = element->ExistingAccessibleNode();
  if (!accessible_node)
    return;

  for (const auto& child : accessible_node->GetChildren()) {
    CHECK_NO_OTHER_PARENT_FOR(child);
    AddChildAndCheckIncluded(AXObjectCache().GetOrCreate(child, this));
  }
}

void AXNodeObject::AddOwnedChildren() {
  AXObjectVector owned_children;
  AXObjectCache().GetAriaOwnedChildren(this, owned_children);

  DCHECK(owned_children.size() == 0 || AXRelationCache::IsValidOwner(this))
      << "This object is not allowed to use aria-owns, but is: "
      << ToString(true, true);

  // Always include owned children.
  for (const auto& owned_child : owned_children) {
    DCHECK(AXRelationCache::IsValidOwnedChild(owned_child))
        << "This object is not allowed to be owned, but is: "
        << owned_child->ToString(true, true);
    AddChildAndCheckIncluded(owned_child, true);
  }
}

void AXNodeObject::AddChildrenImpl() {
#define CHECK_ATTACHED()                                                  \
  if (IsDetached()) {                                                     \
    NOTREACHED() << "Detached adding children: " << ToString(true, true); \
    return;                                                               \
  }

  DCHECK(children_dirty_);

  if (!CanHaveChildren()) {
    NOTREACHED()
        << "Should not reach AddChildren() if CanHaveChildren() is false "
        << ToString(true, true);
    return;
  }

  if (ui::CanHaveInlineTextBoxChildren(RoleValue())) {
    AddInlineTextBoxChildren();
    CHECK_ATTACHED();
    return;
  }

  if (IsA<HTMLImageElement>(GetNode())) {
    AddImageMapChildren();
    CHECK_ATTACHED();
    return;
  }

  // If validation message exists, always make it the first child of the root,
  // to enable easy checking of whether it's a known child of the root.
  if (IsWebArea())
    AddValidationMessageChild();
  CHECK_ATTACHED();

  if (HasValidHTMLTableStructureAndLayout())
    AddTableChildren();
  else if (ShouldUseLayoutObjectTraversalForChildren())
    AddLayoutChildren();
  else
    AddNodeChildren();
  CHECK_ATTACHED();

  AddPopupChildren();
  CHECK_ATTACHED();

  AddAccessibleNodeChildren();
  CHECK_ATTACHED();

  AddOwnedChildren();
  CHECK_ATTACHED();
}

void AXNodeObject::AddChildren() {
#if DCHECK_IS_ON()
  DCHECK(!IsDetached());
  DCHECK(!is_adding_children_) << " Reentering method on " << GetNode();
  base::AutoReset<bool> reentrancy_protector(&is_adding_children_, true);
  // If the need to add more children in addition to existing children arises,
  // childrenChanged should have been called, which leads to children_dirty_
  // being true, then UpdateChildrenIfNecessary() clears the children before
  // calling AddChildren().
  DCHECK_EQ(children_.size(), 0U)
      << "\nParent still has " << children_.size() << " children before adding:"
      << "\nParent is " << ToString(true, true) << "\nFirst child is "
      << children_[0]->ToString(true, true);
#endif

  AddChildrenImpl();
  children_dirty_ = false;

#if DCHECK_IS_ON()
  // All added children must be attached.
  for (const auto& child : children_) {
    DCHECK(!child->IsDetached())
        << "A brand new child was detached: " << child->ToString(true, true)
        << "\n ... of parent " << ToString(true, true);
  }
#endif
}

// Add non-owned children that are backed with a DOM node.
void AXNodeObject::AddNodeChild(Node* node) {
  if (!node)
    return;

  AXObject* ax_child = AXObjectCache().Get(node);
  // Should not have another parent unless owned.
  if (AXObjectCache().IsAriaOwned(ax_child))
    return;  // Do not add owned children to their natural parent.

#if DCHECK_IS_ON()
  AXObject* ax_cached_parent =
      ax_child ? ax_child->CachedParentObject() : nullptr;
  size_t num_children_before_add = children_.size();
#endif

  if (!ax_child) {
    ax_child = AXObjectCache().GetOrCreate(node, this);
    if (!ax_child)
      return;
  }

  AddChild(ax_child);

#if DCHECK_IS_ON()
  bool did_add_child = children_.size() == num_children_before_add + 1 &&
                       children_[0] == ax_child;
  if (did_add_child) {
    DCHECK(!ax_cached_parent || ax_cached_parent->AXObjectID() == AXObjectID())
        << "Newly added child shouldn't have a different preexisting parent:"
        << "\nChild = " << ax_child->ToString(true, true)
        << "\nNew parent = " << ToString(true, true)
        << "\nPreexisting parent = " << ax_cached_parent->ToString(true, true);
  }
#endif
}

#if DCHECK_IS_ON()
void AXNodeObject::CheckValidChild(AXObject* child) {
  DCHECK(!child->IsDetached())
      << "Cannot add a detached child: " << child->ToString(true, true);

  Node* child_node = child->GetNode();

  // An HTML image can only have area children.
  DCHECK(!IsA<HTMLImageElement>(GetNode()) || IsA<HTMLAreaElement>(child_node))
      << "Image elements can only have area children, had "
      << child->ToString(true, true);

  // <area> children should only be added via AddImageMapChildren(), as the
  // children of an <image usemap>, and never alone or as children of a <map>.
  DCHECK(IsA<HTMLImageElement>(GetNode()) || !IsA<HTMLAreaElement>(child_node))
      << "Area elements can only be added by image parents: "
      << child->ToString(true, true) << " had a parent of "
      << ToString(true, true);

  // An option or popup for a <select size=1> must only be added via an
  // overridden AddChildren() on AXMenuList/AXMenuListPopup.
  // These AXObjects must be added in an overridden AddChildren() method, and
  // that will only occur if AXObjectCacheImpl::UseAXMenuList() returns true.
  DCHECK(!IsA<AXMenuListOption>(child))
      << "Adding menulist option child in wrong place: "
      << "\nChild: " << child->ToString(true, true)
      << "\nParent: " << child->ParentObject()->ToString(true, true)
      << "\nUseAXMenuList()=" << AXObjectCacheImpl::UseAXMenuList();

  // An popup for a <select size=1> must only be added via an overridden
  // AddChildren() on AXMenuList.
  DCHECK(!IsA<AXMenuListPopup>(child))
      << "Adding menulist popup in wrong place: "
      << "\nChild: " << child->ToString(true, true)
      << "\nParent: " << child->ParentObject()->ToString(true, true)
      << "\nUseAXMenuList()=" << AXObjectCacheImpl::UseAXMenuList()
      << "\nShouldCreateAXMenuListOptionFor()="
      << AXObjectCacheImpl::ShouldCreateAXMenuListOptionFor(child_node);
}
#endif

void AXNodeObject::AddChild(AXObject* child, bool is_from_aria_owns) {
  if (!child)
    return;

#if DCHECK_IS_ON()
  CheckValidChild(child);
#endif

  unsigned int index = children_.size();
  InsertChild(child, index, is_from_aria_owns);
}

void AXNodeObject::AddChildAndCheckIncluded(AXObject* child,
                                            bool is_from_aria_owns) {
  if (!child)
    return;
  DCHECK(child->AccessibilityIsIncludedInTree())
      << "A parent " << ToString(true, true) << "\n    ... tried to add child "
      << child->ToString(true, true);
  AddChild(child, is_from_aria_owns);
}

void AXNodeObject::InsertChild(AXObject* child,
                               unsigned index,
                               bool is_from_aria_owns) {
  if (!child)
    return;

  DCHECK(CanHaveChildren());
  DCHECK(!child->IsDetached())
      << "Cannot add a detached child: " << child->ToString(true, true);
  // Enforce expected aria-owns status:
  // - Don't add a non-aria-owned child when called from AddOwnedChildren().
  // - Don't add an aria-owned child to its natural parent, because it will
  //   already be the child of the element with aria-owns.
  DCHECK_EQ(AXObjectCache().IsAriaOwned(child), is_from_aria_owns);

  // Set the parent:
  // - For a new object it will have already been set.
  // - For a reused, older object, it may need to be changed to a new parent.
  child->SetParent(this);

#if DCHECK_IS_ON()
  child->EnsureCorrectParentComputation();
#endif

  // Update cached values preemptively, but don't allow children changed to be
  // called if ignored change, we are already recomputing children and don't
  // want to recurse.
  child->UpdateCachedAttributeValuesIfNeeded(false);

  if (!child->LastKnownIsIncludedInTreeValue()) {
    DCHECK(!is_from_aria_owns)
        << "Owned elements must be in tree: " << child->ToString(true, true)
        << "\nRecompute included in tree: "
        << child->ComputeAccessibilityIsIgnoredButIncludedInTree();
    // Child is ignored and not in the tree.
    // Recompute the child's children now as we skip over the ignored object.
    child->SetNeedsToUpdateChildren();

    // Get the ignored child's children and add to children of ancestor
    // included in tree. This will recurse if necessary, skipping levels of
    // unignored descendants as it goes.
    const auto& children = child->ChildrenIncludingIgnored();
    wtf_size_t length = children.size();
    int new_index = index;
    for (wtf_size_t i = 0; i < length; ++i) {
      // If the child was owned, it will be added elsewhere as a direct
      // child of the object owning it.
      if (!AXObjectCache().IsAriaOwned(children[i])) {
        DCHECK(!children[i]->IsDetached()) << "Cannot add a detached child: "
                                           << children[i]->ToString(true, true);
        children_.insert(new_index++, children[i]);
      }
    }
  } else {
    children_.insert(index, child);
  }
}

bool AXNodeObject::CanHaveChildren() const {
  // If this is an AXLayoutObject, then it's okay if this object
  // doesn't have a node - there are some layoutObjects that don't have
  // associated nodes, like scroll areas and css-generated text.
  if (!GetNode() && !IsAXLayoutObject())
    return false;

  DCHECK(!IsA<HTMLMapElement>(GetNode()));

  // Placeholder gets exposed as an attribute on the input accessibility node,
  // so there's no need to add its text children. Placeholder text is a separate
  // node that gets removed when it disappears, so this will only be present if
  // the placeholder is visible.
  if (GetElement() && GetElement()->ShadowPseudoId() ==
                          shadow_element_names::kPseudoInputPlaceholder) {
    return false;
  }

  switch (native_role_) {
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kMenuListOption:
    case ax::mojom::blink::Role::kMenuItem:
    case ax::mojom::blink::Role::kMenuItemCheckBox:
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kProgressIndicator:
    case ax::mojom::blink::Role::kRadioButton:
    case ax::mojom::blink::Role::kScrollBar:
    // case ax::mojom::blink::Role::kSearchBox:
    case ax::mojom::blink::Role::kSlider:
    case ax::mojom::blink::Role::kSplitter:
    case ax::mojom::blink::Role::kSwitch:
    case ax::mojom::blink::Role::kTab:
    // case ax::mojom::blink::Role::kTextField:
    case ax::mojom::blink::Role::kToggleButton:
      return false;
    case ax::mojom::blink::Role::kPopUpButton:
      return true;
    case ax::mojom::blink::Role::kLineBreak:
    case ax::mojom::blink::Role::kStaticText:
      return AXObjectCache().InlineTextBoxAccessibilityEnabled();
    case ax::mojom::blink::Role::kImage:
      // Can turn into an image map if gains children later.
      return GetNode() && GetNode()->IsLink();
    default:
      break;
  }

  switch (AriaRoleAttribute()) {
    case ax::mojom::blink::Role::kImage:
      return false;
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kMath:  // role="math" is flat, unlike <math>
    case ax::mojom::blink::Role::kMenuListOption:
    case ax::mojom::blink::Role::kMenuItem:
    case ax::mojom::blink::Role::kMenuItemCheckBox:
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kPopUpButton:
    case ax::mojom::blink::Role::kProgressIndicator:
    case ax::mojom::blink::Role::kRadioButton:
    case ax::mojom::blink::Role::kScrollBar:
    case ax::mojom::blink::Role::kSlider:
    case ax::mojom::blink::Role::kSplitter:
    case ax::mojom::blink::Role::kSwitch:
    case ax::mojom::blink::Role::kTab:
    case ax::mojom::blink::Role::kToggleButton: {
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

//
// Properties of the object's owning document or page.
//

double AXNodeObject::EstimatedLoadingProgress() const {
  if (!GetDocument())
    return 0;

  if (IsLoaded())
    return 1.0;

  if (LocalFrame* frame = GetDocument()->GetFrame())
    return frame->Loader().Progress().EstimatedProgress();
  return 0;
}

//
// DOM and Render tree access.
//

Element* AXNodeObject::ActionElement() const {
  Node* node = this->GetNode();
  if (!node)
    return nullptr;

  auto* element = DynamicTo<Element>(node);
  if (element && IsClickable())
    return element;

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
    if (IsA<HTMLAnchorElement>(*node))
      return To<Element>(node);

    if (LayoutObject* layout_object = node->GetLayoutObject()) {
      AXObject* ax_object = cache.GetOrCreate(layout_object);
      if (ax_object && ax_object->IsAnchor())
        return To<Element>(node);
    }
  }

  return nullptr;
}

Document* AXNodeObject::GetDocument() const {
  if (!GetNode())
    return nullptr;
  return &GetNode()->GetDocument();
}

// TODO(chrishall): consider merging this with AXObject::Language in followup.
AtomicString AXNodeObject::Language() const {
  if (!GetNode())
    return AXObject::Language();

  // If it's the root, get the computed language for the document element,
  // because the root LayoutObject doesn't have the right value.
  if (RoleValue() == ax::mojom::blink::Role::kRootWebArea) {
    Element* document_element = GetDocument()->documentElement();
    if (!document_element)
      return g_empty_atom;

    // Ensure we return only the first language tag. ComputeInheritedLanguage
    // consults ContentLanguage which can be set from 2 different sources.
    // DocumentLoader::DidInstallNewDocument from HTTP headers which truncates
    // until the first comma.
    // HttpEquiv::Process from <meta> tag which does not truncate.
    // TODO(chrishall): Consider moving this comma handling to setter side.
    AtomicString lang = document_element->ComputeInheritedLanguage();
    Vector<String> languages;
    String(lang).Split(',', languages);
    if (!languages.IsEmpty())
      return AtomicString(languages[0].StripWhiteSpace());
  }

  return AXObject::Language();
}

bool AXNodeObject::HasAttribute(const QualifiedName& attribute) const {
  Element* element = GetElement();
  if (!element)
    return false;
  if (element->FastHasAttribute(attribute))
    return true;
  return HasInternalsAttribute(*element, attribute);
}

const AtomicString& AXNodeObject::GetAttribute(
    const QualifiedName& attribute) const {
  Element* element = GetElement();
  if (!element)
    return g_null_atom;
  const AtomicString& value = element->FastGetAttribute(attribute);
  if (!value.IsNull())
    return value;
  return GetInternalsAttribute(*element, attribute);
}

bool AXNodeObject::HasInternalsAttribute(Element& element,
                                         const QualifiedName& attribute) const {
  if (!element.DidAttachInternals())
    return false;
  return element.EnsureElementInternals().HasAttribute(attribute);
}

const AtomicString& AXNodeObject::GetInternalsAttribute(
    Element& element,
    const QualifiedName& attribute) const {
  if (!element.DidAttachInternals())
    return g_null_atom;
  return element.EnsureElementInternals().FastGetAttribute(attribute);
}

AXObject* AXNodeObject::CorrespondingControlAXObjectForLabelElement() const {
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

AXObject* AXNodeObject::CorrespondingLabelAXObject() const {
  HTMLLabelElement* label_element = LabelElementContainer();
  if (!label_element)
    return nullptr;

  return AXObjectCache().GetOrCreate(label_element);
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
  // Checking if node is focusable in a native focus action requires that we
  // have updated style and layout tree, since the focus check relies on the
  // existence of layout objects to determine the result. However, these layout
  // objects may have been deferred by display-locking.
  Document* document = GetDocument();
  Node* node = GetNode();
  if (document && node)
    document->UpdateStyleAndLayoutTreeForNode(node);

  if (!CanSetFocusAttribute())
    return false;

  if (IsWebArea()) {
    // If another Frame has focused content (e.g. nested iframe), then we
    // need to clear focus for the other Document Frame.
    // Here we set the focused element via the FocusController so that the
    // other Frame loses focus, and the target Document Element gains focus.
    // This fixes a scenario with Narrator Item Navigation when the user
    // navigates from the outer UI to the document when the last focused
    // element was within a nested iframe before leaving the document frame.
    Page* page = document->GetPage();
    // Elements inside a portal should not be focusable.
    if (page && !page->InsidePortal()) {
      page->GetFocusController().SetFocusedElement(document->documentElement(),
                                                   document->GetFrame());
    } else {
      document->ClearFocusedElement();
    }
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
  if (document->FocusedElement() == element) {
    document->ClearFocusedElement();

    // Calling ClearFocusedElement could result in changes to the document,
    // like this AXObject becoming detached.
    if (IsDetached())
      return false;
  }

  // If the object is not natively focusable but can be focused using an ARIA
  // active descendant, perform a native click instead. This will enable Web
  // apps that set accessibility focus using an active descendant to capture and
  // act on the click event. Otherwise, there is no other way to inform the app
  // that an AT has requested the focus to be changed, except if the app is
  // using AOM. To be extra safe, exclude objects that are clickable themselves.
  // This won't prevent anyone from having a click handler on the object's
  // container.
  if (!IsClickable() && CanBeActiveDescendant()) {
    return OnNativeClickAction();
  }

  element->focus();
  return true;
}

bool AXNodeObject::OnNativeIncrementAction() {
  LocalFrame* frame = GetDocument() ? GetDocument()->GetFrame() : nullptr;
  LocalFrame::NotifyUserActivation(
      frame, mojom::blink::UserActivationNotificationType::kInteraction);
  AlterSliderOrSpinButtonValue(true);
  return true;
}

bool AXNodeObject::OnNativeDecrementAction() {
  LocalFrame* frame = GetDocument() ? GetDocument()->GetFrame() : nullptr;
  LocalFrame::NotifyUserActivation(
      frame, mojom::blink::UserActivationNotificationType::kInteraction);
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

  DCHECK(!IsDetached()) << "Avoid ChildrenChanged() on detached node: "
                        << ToString(true, true);

  // When children changed on a <map> that means we need to forward the
  // children changed to the <img> that parents the <area> elements.
  // TODO(accessibility) Consider treating <img usemap> as aria-owns so that
  // we get implementation "for free" vai relation cache, etc.
  if (HTMLMapElement* map_element = DynamicTo<HTMLMapElement>(GetNode())) {
    HTMLImageElement* image_element = map_element->ImageElement();
    if (image_element) {
      AXObject* ax_image = AXObjectCache().Get(image_element);
      if (ax_image) {
        ax_image->ChildrenChanged();
        return;
      }
    }
  }

  // Always update current object, in case it wasn't included in the tree but
  // now is. In that case, the LastKnownIsIncludedInTreeValue() won't have been
  // updated yet, so we can't use that. Unfortunately, this is not a safe time
  // to get the current included in tree value, therefore, we'll play it safe
  // and update the children in two places sometimes.
  SetNeedsToUpdateChildren();

  // If this node is not in the tree, update the children of the first ancesor
  // that is included in the tree.
  if (!LastKnownIsIncludedInTreeValue()) {
    // The first object (this or ancestor) that is included in the tree is the
    // one whose children may have changed.
    // Can be null, e.g. if <title> contents change
    if (AXObject* node_to_update = ParentObjectIncludedInTree())
      node_to_update->SetNeedsToUpdateChildren();
  }

  // If this node's children are not part of the accessibility tree then
  // skip notification and walking up the ancestors.
  // Cases where this happens:
  // - an ancestor has only presentational children, or
  // - this or an ancestor is a leaf node
  // Uses |cached_is_descendant_of_leaf_node_| to avoid updating cached
  // attributes for eachc change via | UpdateCachedAttributeValuesIfNeeded()|.
  if (!CanHaveChildren() || LastKnownIsDescendantOfLeafNode())
    return;

  // TODO(aleventhal) Consider removing.
  if (IsDetached()) {
    NOTREACHED() << "None of the above calls should be able to detach |this|: "
                 << ToString(true, true);
    return;
  }

  AXObjectCache().PostNotification(this,
                                   ax::mojom::blink::Event::kChildrenChanged);
}

void AXNodeObject::SelectedOptions(AXObjectVector& options) const {
  if (auto* select = DynamicTo<HTMLSelectElement>(GetNode())) {
    for (auto* const option : *select->selectedOptions()) {
      AXObject* ax_option = AXObjectCache().GetOrCreate(option);
      if (ax_option)
        options.push_back(ax_option);
    }
    return;
  }

  const AXObjectVector& children = ChildrenIncludingIgnored();
  if (RoleValue() == ax::mojom::blink::Role::kComboBoxGrouping ||
      RoleValue() == ax::mojom::blink::Role::kComboBoxMenuButton) {
    for (const auto& obj : children) {
      if (obj->RoleValue() == ax::mojom::blink::Role::kListBox) {
        obj->SelectedOptions(options);
        return;
      }
    }
  }

  for (const auto& obj : children) {
    if (obj->IsSelected() == kSelectedStateTrue)
      options.push_back(obj);
  }
}

void AXNodeObject::SelectionChanged() {
  // Post the selected text changed event on the first ancestor that's
  // focused (to handle form controls, ARIA text boxes and contentEditable),
  // or the web area if the selection is just in the document somewhere.
  if (IsFocused() || IsWebArea()) {
    AXObjectCache().PostNotification(
        this, ax::mojom::blink::Event::kTextSelectionChanged);
    if (GetDocument()) {
      AXObject* document_object = AXObjectCache().GetOrCreate(GetDocument());
      AXObjectCache().PostNotification(
          document_object, ax::mojom::blink::Event::kDocumentSelectionChanged);
    }
  } else {
    AXObject::SelectionChanged();  // Calls selectionChanged on parent.
  }
}

AXObject* AXNodeObject::ErrorMessage() const {
  // Check for aria-errormessage.
  Element* existing_error_message =
      GetAOMPropertyOrARIAAttribute(AOMRelationProperty::kErrorMessage);
  if (existing_error_message)
    return AXObjectCache().GetOrCreate(existing_error_message);

  // Check for visible validationMessage. This can only be visible for a focused
  // control. Corollary: if there is a visible validationMessage alert box, then
  // it is related to the current focus.
  if (this != AXObjectCache().FocusedObject())
    return nullptr;

  return AXObjectCache().ValidationMessageObjectIfInvalid(true);
}

// According to the standard, the figcaption should only be the first or
// last child: https://html.spec.whatwg.org/#the-figcaption-element
static Element* GetChildFigcaption(const Node& node) {
  Element* element = ElementTraversal::FirstChild(node);
  if (!element)
    return nullptr;
  if (element->HasTagName(html_names::kFigcaptionTag))
    return element;

  element = ElementTraversal::LastChild(node);
  if (element->HasTagName(html_names::kFigcaptionTag))
    return element;

  return nullptr;
}

// Based on
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
String AXNodeObject::NativeTextAlternative(
    AXObjectSet& visited,
    ax::mojom::blink::NameFrom& name_from,
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

  // 5.1/5.5 Text inputs, Other labelable Elements
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  auto* html_element = DynamicTo<HTMLElement>(GetNode());
  if (html_element && html_element->IsLabelable()) {
    name_from = ax::mojom::blink::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLLabel;
    }

    LabelsNodeList* labels = nullptr;
    if (AXObjectCache().MayHaveHTMLLabel(*html_element))
      labels = html_element->labels();
    if (labels && labels->length() > 0) {
      HeapVector<Member<Element>> label_elements;
      for (unsigned label_index = 0; label_index < labels->length();
           ++label_index) {
        Element* label = labels->item(label_index);
        if (name_sources) {
          if (!label->FastGetAttribute(html_names::kForAttr).IsEmpty() &&
              label->FastGetAttribute(html_names::kForAttr) ==
                  html_element->GetIdAttribute()) {
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
  const auto* input_element = DynamicTo<HTMLInputElement>(GetNode());
  if (input_element && input_element->IsTextButton()) {
    // value attribute.
    name_from = ax::mojom::blink::NameFrom::kValue;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kValueAttr));
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
        name_from = ax::mojom::blink::NameFrom::kContents;
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
      input_element->getAttribute(kTypeAttr) == input_type_names::kImage) {
    // alt attr
    const AtomicString& alt = input_element->getAttribute(kAltAttr);
    const bool is_empty = alt.IsEmpty() && !alt.IsNull();
    name_from = is_empty ? ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty
                         : ax::mojom::blink::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kAltAttr));
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

    // value attribute.
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kValueAttr));
      name_sources->back().type = name_from;
    }
    name_from = ax::mojom::blink::NameFrom::kAttribute;
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
      name_sources->push_back(NameSource(*found_text_alternative, kTitleAttr));
      name_sources->back().type = name_from;
    }
    name_from = ax::mojom::blink::NameFrom::kTitle;
    const AtomicString& title = input_element->getAttribute(kTitleAttr);
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
    name_from = ax::mojom::blink::NameFrom::kValue;
    text_alternative =
        input_element->GetLocale().QueryString(IDS_FORM_SUBMIT_LABEL);
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kTypeAttr));
      NameSource& source = name_sources->back();
      source.attribute_value = input_element->getAttribute(kTypeAttr);
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
    name_from = ax::mojom::blink::NameFrom::kPlaceholder;
    if (name_sources) {
      name_sources->push_back(
          NameSource(*found_text_alternative, html_names::kPlaceholderAttr));
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
            html_element->FastGetAttribute(html_names::kPlaceholderAttr);
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }
  }

  // Input type=file. Not part of the spec, but Blink implements it
  // as a single control that has both a button ("Choose file...") and
  // some text showing the filename, and we need to concatenate both into
  // the name of the button.
  if (input_element && input_element->type() == input_type_names::kFile) {
    name_from = ax::mojom::blink::NameFrom::kValue;

    String displayed_file_path = GetValueForControl();
    String upload_button_text = input_element->UploadButton()->value();
    if (!displayed_file_path.IsEmpty()) {
      text_alternative = displayed_file_path + ", " + upload_button_text;
    } else {
      text_alternative = upload_button_text;
    }
    *found_text_alternative = true;

    if (name_sources) {
      name_sources->push_back(NameSource(true, kValueAttr));
      name_sources->back().type = name_from;
      name_sources->back().text = text_alternative;
    } else {
      return text_alternative;
    }
  }

  // Also check for aria-placeholder.
  if (IsTextControl()) {
    name_from = ax::mojom::blink::NameFrom::kPlaceholder;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative,
                                         html_names::kAriaPlaceholderAttr));
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
  if (GetNode()->HasTagName(html_names::kFigureTag)) {
    // figcaption
    name_from = ax::mojom::blink::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLFigcaption;
    }
    Element* figcaption = GetChildFigcaption(*(GetNode()));
    if (figcaption) {
      AXObject* figcaption_ax_object = AXObjectCache().GetOrCreate(figcaption);
      if (figcaption_ax_object) {
        text_alternative =
            RecursiveTextAlternative(*figcaption_ax_object, false, visited);

        if (related_objects) {
          local_related_objects.push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(
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
  if (IsA<HTMLImageElement>(GetNode()) || IsA<HTMLAreaElement>(GetNode()) ||
      (GetLayoutObject() && GetLayoutObject()->IsSVGImage())) {
    // alt
    const AtomicString& alt = GetAttribute(kAltAttr);
    const bool is_empty = alt.IsEmpty() && !alt.IsNull();
    name_from = is_empty ? ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty
                         : ax::mojom::blink::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kAltAttr));
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
  if (auto* table_element = DynamicTo<HTMLTableElement>(GetNode())) {
    // caption
    name_from = ax::mojom::blink::NameFrom::kCaption;
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
              MakeGarbageCollected<NameSourceRelatedObject>(caption_ax_object,
                                                            text_alternative));
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
    name_from = ax::mojom::blink::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(
          NameSource(*found_text_alternative, html_names::kSummaryAttr));
      name_sources->back().type = name_from;
    }
    const AtomicString& summary = GetAttribute(html_names::kSummaryAttr);
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
    name_from = ax::mojom::blink::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLTitleElement;
    }
    auto* container_node = To<ContainerNode>(GetNode());
    Element* title = ElementTraversal::FirstChild(
        *container_node, HasTagName(svg_names::kTitleTag));

    if (title) {
      AXObject* title_ax_object = AXObjectCache().GetOrCreate(title);
      if (title_ax_object && !visited.Contains(title_ax_object)) {
        text_alternative =
            RecursiveTextAlternative(*title_ax_object, false, visited);
        if (related_objects) {
          local_related_objects.push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(title_ax_object,
                                                            text_alternative));
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
  if (auto* html_field_set_element =
          DynamicTo<HTMLFieldSetElement>(GetNode())) {
    name_from = ax::mojom::blink::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLLegend;
    }
    HTMLElement* legend = html_field_set_element->Legend();
    if (legend) {
      AXObject* legend_ax_object = AXObjectCache().GetOrCreate(legend);
      // Avoid an infinite loop
      if (legend_ax_object && !visited.Contains(legend_ax_object)) {
        text_alternative =
            RecursiveTextAlternative(*legend_ax_object, false, visited);

        if (related_objects) {
          local_related_objects.push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(legend_ax_object,
                                                            text_alternative));
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
      name_from = ax::mojom::blink::NameFrom::kAttribute;
      if (name_sources) {
        name_sources->push_back(
            NameSource(found_text_alternative, html_names::kAriaLabelAttr));
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

      text_alternative = document->title();
      bool is_empty_title_element =
          text_alternative.IsEmpty() && document->TitleElement();
      if (is_empty_title_element)
        name_from = ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty;
      else
        name_from = ax::mojom::blink::NameFrom::kRelatedElement;

      if (name_sources) {
        name_sources->push_back(NameSource(*found_text_alternative));
        name_sources->back().type = name_from;
        name_sources->back().native_source = kAXTextFromNativeHTMLTitleElement;
      }

      Element* title_element = document->TitleElement();
      AXObject* title_ax_object = AXObjectCache().GetOrCreate(
          title_element, AXObjectCache().Get(document));
      if (title_ax_object) {
        if (related_objects) {
          local_related_objects.push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(title_ax_object,
                                                            text_alternative));
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

String AXNodeObject::Description(
    ax::mojom::blink::NameFrom name_from,
    ax::mojom::blink::DescriptionFrom& description_from,
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

  if (RoleValue() == ax::mojom::blink::Role::kSpinButton &&
      DatetimeAncestor()) {
    // Fields inside a datetime control need to merge the field description
    // with the description of the <input> element.
    const AXObject* datetime_ancestor = DatetimeAncestor();
    ax::mojom::blink::NameFrom datetime_ancestor_name_from;
    datetime_ancestor->GetName(datetime_ancestor_name_from, nullptr);
    description_objects->clear();
    String ancestor_description = DatetimeAncestor()->Description(
        datetime_ancestor_name_from, description_from, description_objects);
    if (!result.IsEmpty() && !ancestor_description.IsEmpty())
      return result + " " + ancestor_description;
    if (!ancestor_description.IsEmpty())
      return ancestor_description;
  }

  return result;
}

// Based on
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
String AXNodeObject::Description(
    ax::mojom::blink::NameFrom name_from,
    ax::mojom::blink::DescriptionFrom& description_from,
    DescriptionSources* description_sources,
    AXRelatedObjectVector* related_objects) const {
  // If descriptionSources is non-null, relatedObjects is used in filling it in,
  // so it must be non-null as well.
  // Important: create a DescriptionSource for every *potential* description
  // source, even if it ends up not being present.
  // When adding a new description_from type:
  // * Also add it to AXValueNativeSourceType here:
  //   blink/public/devtools_protocol/browser_protocol.pdl
  // * Update InspectorTypeBuilderHelper to map the new enum to
  //   the browser_protocol enum in NativeSourceType():
  //   blink/renderer/modules/accessibility/inspector_type_builder_helper.cc
  // * Update devtools_frontend to add a new string for the new type of
  //   description. See AXNativeSourceTypes at:
  //   devtools-frontend/src/front_end/accessibility/AccessibilityStrings.js
  if (description_sources)
    DCHECK(related_objects);

  if (!GetNode())
    return String();

  String description;
  bool found_description = false;

  description_from = ax::mojom::blink::DescriptionFrom::kRelatedElement;
  if (description_sources) {
    description_sources->push_back(
        DescriptionSource(found_description, html_names::kAriaDescribedbyAttr));
    description_sources->back().type = description_from;
  }

  // aria-describedby overrides any other accessible description, from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  Element* element = GetElement();
  if (!element)
    return String();

  Vector<String> ids;
  HeapVector<Member<Element>> elements_from_attribute;
  ElementsFromAttribute(elements_from_attribute,
                        html_names::kAriaDescribedbyAttr, ids);
  if (!elements_from_attribute.IsEmpty()) {
    // TODO(meredithl): Determine description sources when |aria_describedby| is
    // the empty string, in order to make devtools work with attr-associated
    // elements.
    if (description_sources) {
      description_sources->back().attribute_value =
          GetAttribute(html_names::kAriaDescribedbyAttr);
    }
    AXObjectSet visited;
    description = TextFromElements(true, visited, elements_from_attribute,
                                   related_objects);

    for (auto& element : elements_from_attribute)
      ids.push_back(element->GetIdAttribute());

    TokenVectorFromAttribute(ids, html_names::kAriaDescribedbyAttr);
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

  // aria-description overrides any HTML-based accessible description,
  // but not aria-describedby.
  const AtomicString& aria_desc =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kDescription);
  if (!aria_desc.IsNull()) {
    description_from = ax::mojom::blink::DescriptionFrom::kAttribute;
    description = aria_desc;
    if (description_sources) {
      found_description = true;
      description_sources->back().text = description;
    } else {
      return description;
    }
  }

  const auto* input_element = DynamicTo<HTMLInputElement>(GetNode());

  // value, 5.2.2 from: http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (name_from != ax::mojom::blink::NameFrom::kValue && input_element &&
      input_element->IsTextButton()) {
    description_from = ax::mojom::blink::DescriptionFrom::kAttribute;
    if (description_sources) {
      description_sources->push_back(
          DescriptionSource(found_description, kValueAttr));
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

  if (RoleValue() == ax::mojom::blink::Role::kRuby) {
    description_from = ax::mojom::blink::DescriptionFrom::kRelatedElement;
    if (description_sources) {
      description_sources->push_back(DescriptionSource(found_description));
      description_sources->back().type = description_from;
      description_sources->back().native_source =
          kAXTextFromNativeHTMLRubyAnnotation;
    }
    AXObject* ruby_annotation_ax_object = nullptr;
    for (const auto& child : children_) {
      if (child->RoleValue() == ax::mojom::blink::Role::kRubyAnnotation &&
          child->GetNode() &&
          child->GetNode()->HasTagName(html_names::kRtTag)) {
        ruby_annotation_ax_object = child;
        break;
      }
    }
    if (ruby_annotation_ax_object) {
      AXObjectSet visited;
      description =
          RecursiveTextAlternative(*ruby_annotation_ax_object, true, visited);
      if (related_objects) {
        related_objects->push_back(
            MakeGarbageCollected<NameSourceRelatedObject>(
                ruby_annotation_ax_object, description));
      }
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

  // table caption, 5.9.2 from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  auto* table_element = DynamicTo<HTMLTableElement>(element);
  if (name_from != ax::mojom::blink::NameFrom::kCaption && table_element) {
    description_from = ax::mojom::blink::DescriptionFrom::kRelatedElement;
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
        if (related_objects) {
          related_objects->push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(caption_ax_object,
                                                            description));
        }

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
  if (name_from != ax::mojom::blink::NameFrom::kContents &&
      IsA<HTMLSummaryElement>(GetNode())) {
    description_from = ax::mojom::blink::DescriptionFrom::kContents;
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
  if (name_from != ax::mojom::blink::NameFrom::kTitle) {
    description_from = ax::mojom::blink::DescriptionFrom::kTitle;
    if (description_sources) {
      description_sources->push_back(
          DescriptionSource(found_description, kTitleAttr));
      description_sources->back().type = description_from;
    }
    const AtomicString& title = GetAttribute(kTitleAttr);
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

  description_from = ax::mojom::blink::DescriptionFrom::kUninitialized;

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

String AXNodeObject::Placeholder(ax::mojom::blink::NameFrom name_from) const {
  if (name_from == ax::mojom::blink::NameFrom::kPlaceholder)
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

String AXNodeObject::Title(ax::mojom::blink::NameFrom name_from) const {
  if (name_from == ax::mojom::blink::NameFrom::kTitle)
    return String();

  if (const auto* element = GetElement()) {
    String title = element->title();
    if (!title.IsEmpty())
      return title;
  }

  return String();
}

String AXNodeObject::PlaceholderFromNativeAttribute() const {
  Node* node = GetNode();
  if (!node || !blink::IsTextControl(*node))
    return String();
  return ToTextControl(node)->StrippedPlaceholder();
}

String AXNodeObject::GetValueContributionToName() const {
  if (CanSetValueAttribute()) {
    if (IsTextControl())
      return GetValueForControl();

    if (IsRangeValueSupported()) {
      const AtomicString& aria_valuetext =
          GetAOMPropertyOrARIAAttribute(AOMStringProperty::kValueText);
      if (!aria_valuetext.IsNull())
        return aria_valuetext.GetString();
      float value;
      if (ValueForRange(&value))
        return String::Number(value);
    }
  }

  // "If the embedded control has role combobox or listbox, return the text
  // alternative of the chosen option."
  if (UseNameFromSelectedOption()) {
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

  return String();
}

bool AXNodeObject::UseNameFromSelectedOption() const {
  // Assumes that the node was reached via recursion in the name calculation.
  switch (RoleValue()) {
    // Step 2E from: http://www.w3.org/TR/accname-aam-1.1
    case ax::mojom::blink::Role::kComboBoxGrouping:
    case ax::mojom::blink::Role::kComboBoxMenuButton:
    case ax::mojom::blink::Role::kListBox:
      return true;
    // This can be either a button widget with a non-false value of
    // aria-haspopup or a select element with size of 1.
    case ax::mojom::blink::Role::kPopUpButton:
      return DynamicTo<HTMLSelectElement>(*GetNode());
    default:
      return false;
  }
}

void AXNodeObject::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  AXObject::Trace(visitor);
}

}  // namespace blink
