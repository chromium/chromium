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

#include <algorithm>
#include <memory>
#include <queue>

#include "base/auto_reset.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/forms/radio_input_type.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_dlist_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
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
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/svg_desc_element.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/core/svg/svg_title_element.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/transform.h"

namespace {

// It is not easily possible to find out if an element is the target of an
// in-page link.
// As a workaround, we consider the following to be potential targets:
// - <a name>
// - <foo id> -- an element with an id that is not SVG, a <label> or <optgroup>.
//     <label> does not make much sense as an in-page link target.
//     Exposing <optgroup> is redundant, as the group is already exposed via a
//     child in its shadow DOM, which contains the accessible name.
//   #document -- this is always a potential link target via <a name="#">.
//   This is a compromise that does not include too many elements, and
//   has minimal impact on tests.
bool IsPotentialInPageLinkTarget(blink::Node& node) {
  auto* element = blink::DynamicTo<blink::Element>(&node);
  if (!element) {
    // The document itself is a potential link target, e.g. via <a name="#">.
    return blink::IsA<blink::Document>(node);
  }

  // We exclude elements that are in the shadow DOM. They cannot be linked by a
  // document fragment from the main page:as they have their own id namespace.
  if (element->ContainingShadowRoot())
    return false;

  // SVG elements are unlikely link targets, and we want to avoid creating
  // a lot of noise in the AX tree or breaking tests unnecessarily.
  if (element->IsSVGElement())
    return false;

  // <a name>
  if (auto* anchor = blink::DynamicTo<blink::HTMLAnchorElement>(element)) {
    if (anchor->HasName())
      return true;
  }

  // <foo id> not in an <optgroup> or <label>.
  if (element->HasID() && !blink::IsA<blink::HTMLLabelElement>(element) &&
      !blink::IsA<blink::HTMLOptGroupElement>(element)) {
    return true;
  }

  return false;
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
}

enum class AXAction {
  kActionIncrement = 0,
  kActionDecrement,
};

blink::KeyboardEvent* CreateKeyboardEvent(
    blink::LocalDOMWindow* local_dom_window,
    blink::WebInputEvent::Type type,
    AXAction action,
    blink::AccessibilityOrientation orientation,
    ax::mojom::blink::WritingDirection text_direction) {
  blink::WebKeyboardEvent key(type,
                              blink::WebInputEvent::Modifiers::kNoModifiers,
                              base::TimeTicks::Now());

  if (action == AXAction::kActionIncrement) {
    if (orientation == blink::kAccessibilityOrientationVertical) {
      key.dom_key = ui::DomKey::ARROW_UP;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_UP);
      key.native_key_code = key.windows_key_code = blink::VKEY_UP;
    } else if (text_direction == ax::mojom::blink::WritingDirection::kRtl) {
      key.dom_key = ui::DomKey::ARROW_LEFT;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_LEFT);
      key.native_key_code = key.windows_key_code = blink::VKEY_LEFT;
    } else {  // horizontal and left to right
      key.dom_key = ui::DomKey::ARROW_RIGHT;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_RIGHT);
      key.native_key_code = key.windows_key_code = blink::VKEY_RIGHT;
    }
  } else if (action == AXAction::kActionDecrement) {
    if (orientation == blink::kAccessibilityOrientationVertical) {
      key.dom_key = ui::DomKey::ARROW_DOWN;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_DOWN);
      key.native_key_code = key.windows_key_code = blink::VKEY_DOWN;
    } else if (text_direction == ax::mojom::blink::WritingDirection::kRtl) {
      key.dom_key = ui::DomKey::ARROW_RIGHT;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_RIGHT);
      key.native_key_code = key.windows_key_code = blink::VKEY_RIGHT;
    } else {  // horizontal and left to right
      key.dom_key = ui::DomKey::ARROW_LEFT;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_LEFT);
      key.native_key_code = key.windows_key_code = blink::VKEY_LEFT;
    }
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

String GetTitle(blink::Element* element) {
  if (!element)
    return String();

  if (blink::SVGElement* svg_element =
          blink::DynamicTo<blink::SVGElement>(element)) {
    // Don't use title() in SVG, as it calls innerText() which updates layout.
    // Unfortunately, this must duplicate some logic from SVGElement::title().
    if (svg_element->InUseShadowTree()) {
      String title = GetTitle(svg_element->OwnerShadowHost());
      if (!title.empty())
        return title;
    }
    // If we aren't an instance in a <use> or the <use> title was not found,
    // then find the first <title> child of this element. If a title child was
    // found, return the text contents.
    if (auto* title_element =
            blink::Traversal<blink::SVGTitleElement>::FirstChild(*element)) {
      return title_element->GetInnerTextWithoutUpdate();
    }
    return String();
  }

  return element->title();
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
      aria_role_(ax::mojom::blink::Role::kUnknown),
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

  if (!RuntimeEnabledFeatures::
          SynthesizedKeyboardEventsForAccessibilityActionsEnabled()) {
    // If synthesized keyboard events are disabled, we need to set the value
    // directly here.

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
  }

  // If we have synthesized keyboard events enabled, we generate a keydown
  // event:
  // * For a native slider, the dispatch of the event will reach
  // RangeInputType::HandleKeydownEvent(), where the value will be set and the
  // AXObjectCache notified. The corresponding keydown/up JS events will be
  // fired so the website doesn't know it was produced by an AT action.
  // * For an ARIA slider, the corresponding keydown/up JS events will be
  // fired. It is expected that the handlers for those events manage the
  // update of the slider value.

  AXAction action =
      increase ? AXAction::kActionIncrement : AXAction::kActionDecrement;
  LocalDOMWindow* local_dom_window = GetDocument()->domWindow();
  AccessibilityOrientation orientation = Orientation();
  ax::mojom::blink::WritingDirection text_direction = GetTextDirection();

  // A kKeyDown event is kRawKeyDown + kChar events. We cannot synthesize it
  // because the KeyboardEvent constructor will prevent it, to force us to
  // decide if we must produce both events. In our case, we don't have to
  // produce a kChar event because we are synthesizing arrow key presses, and
  // only keys that output characters are expected to produce kChar events.
  KeyboardEvent* keydown =
      CreateKeyboardEvent(local_dom_window, WebInputEvent::Type::kRawKeyDown,
                          action, orientation, text_direction);
  GetNode()->DispatchEvent(*keydown);

  // The keydown handler may have caused the node to be removed.
  if (!GetNode())
    return;

  KeyboardEvent* keyup =
      CreateKeyboardEvent(local_dom_window, WebInputEvent::Type::kKeyUp, action,
                          orientation, text_direction);

  // Add a 100ms delay between keydown and keyup to make events look less
  // evidently synthesized.
  GetDocument()
      ->GetTaskRunner(TaskType::kUserInteraction)
      ->PostDelayedTask(
          FROM_HERE,
          WTF::BindOnce(
              [](Node* node, KeyboardEvent* evt) { node->DispatchEvent(*evt); },
              WrapWeakPersistent(GetNode()), WrapPersistent(keyup)),
          base::Milliseconds(100));
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
  DCHECK(GetDocument());

  if (IsPresentational()) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXPresentational));
    return kIgnoreObject;
  }

  // Objects inside a portal should be ignored. Portals don't directly expose
  // their contents as the contents are not focusable (portals do not currently
  // support input events). Portals do use their contents to compute a default
  // accessible name.
  if (GetDocument()->GetPage() && GetDocument()->GetPage()->InsidePortal())
    return kIgnoreObject;

  Node* node = GetNode();
  if (!node) {
    // Nodeless pseudo element images are included, even if they don't have CSS
    // alt text. This can allow auto alt to be applied to them.
    if (IsImage())
      return kIncludeObject;
    return kDefaultBehavior;
  }

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

  Element* element = GetElement();
  if (!element)
    return kDefaultBehavior;

  if (IsA<SVGElement>(node)) {
    // The symbol element is used to define graphical templates which can be
    // instantiated by a use element but which are not rendered directly. We
    // don't want to include these template objects, or their subtrees, where
    // they appear in the DOM. Any associated semantic information (e.g. the
    // title child of a symbol) may participate in the text alternative
    // computation where it is instantiated by the use element.
    // https://svgwg.org/svg2-draft/struct.html#SymbolElement
    if (Traversal<SVGSymbolElement>::FirstAncestorOrSelf(*node))
      return kIgnoreObject;

    // The SVG-AAM states that user agents MUST provide an accessible object
    // for rendered SVG elements that have at least one direct child title or
    // desc element that is not empty after trimming whitespace. But it also
    // says, "User agents MAY include elements with these child elements without
    // checking for valid text content." So just check for their existence in
    // order to be performant. https://w3c.github.io/svg-aam/#include_elements
    if (ElementTraversal::FirstChild(
            *To<ContainerNode>(node), [](auto& element) {
              return element.HasTagName(svg_names::kTitleTag) ||
                     element.HasTagName(svg_names::kDescTag);
            })) {
      return kIncludeObject;
    }

    // If we return kDefaultBehavior here, the logic related to inclusion of
    // clickable objects, links, controls, etc. will not be reached. We handle
    // SVG elements early to ensure properties in a <symbol> subtree do not
    // result in inclusion.
  }

  if (IsTableLikeRole() || IsTableRowLikeRole() || IsTableCellLikeRole())
    return kIncludeObject;

  // All focusable elements except the <body> are included.
  if (!IsA<HTMLBodyElement>(node) && CanSetFocusAttribute())
    return kIncludeObject;

  if (IsLink())
    return kIncludeObject;

  // A click handler might be placed on an otherwise ignored non-empty block
  // element, e.g. a div. We shouldn't ignore such elements because if an AT
  // sees the |ax::mojom::blink::DefaultActionVerb::kClickAncestor|, it will
  // look for the clickable ancestor and it expects to find one.
  if (IsClickable())
    return kIncludeObject;

  if (IsHeading())
    return kIncludeObject;

  // Header and footer tags may also be exposed as landmark roles but not
  // always.
  if (node->HasTagName(html_names::kHeaderTag) ||
      node->HasTagName(html_names::kFooterTag))
    return kIncludeObject;

  // All controls are accessible.
  if (IsControl())
    return kIncludeObject;

  // Anything with an explicit ARIA role should be included.
  if (AriaRoleAttribute() != ax::mojom::blink::Role::kUnknown)
    return kIncludeObject;

  // Anything with CSS alt should be included.
  // Descendants are pruned: IsRelevantPseudoElementDescendant() returns false.
  // Note: this is duplicated from AXLayoutObject because CSS alt text may apply
  // to both Elements and pseudo-elements.
  absl::optional<String> alt_text = GetCSSAltText(GetNode());
  if (alt_text && !alt_text->empty())
    return kIncludeObject;

  // Don't ignored legends, because JAWS uses them to determine redundant text.
  if (IsA<HTMLLegendElement>(node))
    return kIncludeObject;

  // Anything that is an editable root should not be ignored. However, one
  // cannot just call `AXObject::IsEditable()` since that will include the
  // contents of an editable region too. Only the editable root should always be
  // exposed.
  if (IsEditableRoot())
    return kIncludeObject;

  static const HashSet<ax::mojom::blink::Role> always_included_computed_roles =
      {
          ax::mojom::blink::Role::kAbbr,
          ax::mojom::blink::Role::kApplication,
          ax::mojom::blink::Role::kArticle,
          ax::mojom::blink::Role::kBanner,
          ax::mojom::blink::Role::kBlockquote,
          ax::mojom::blink::Role::kComplementary,
          ax::mojom::blink::Role::kContentDeletion,
          ax::mojom::blink::Role::kContentInfo,
          ax::mojom::blink::Role::kContentInsertion,
          ax::mojom::blink::Role::kDescriptionList,
          ax::mojom::blink::Role::kDescriptionListDetail,
          ax::mojom::blink::Role::kDescriptionListTerm,
          ax::mojom::blink::Role::kDetails,
          ax::mojom::blink::Role::kDialog,
          ax::mojom::blink::Role::kDocAcknowledgments,
          ax::mojom::blink::Role::kDocAfterword,
          ax::mojom::blink::Role::kDocAppendix,
          ax::mojom::blink::Role::kDocBibliography,
          ax::mojom::blink::Role::kDocChapter,
          ax::mojom::blink::Role::kDocConclusion,
          ax::mojom::blink::Role::kDocCredits,
          ax::mojom::blink::Role::kDocEndnotes,
          ax::mojom::blink::Role::kDocEpilogue,
          ax::mojom::blink::Role::kDocErrata,
          ax::mojom::blink::Role::kDocForeword,
          ax::mojom::blink::Role::kDocGlossary,
          ax::mojom::blink::Role::kDocIntroduction,
          ax::mojom::blink::Role::kDocPart,
          ax::mojom::blink::Role::kDocPreface,
          ax::mojom::blink::Role::kDocPrologue,
          ax::mojom::blink::Role::kDocToc,
          ax::mojom::blink::Role::kFigcaption,
          ax::mojom::blink::Role::kFigure,
          ax::mojom::blink::Role::kFooter,
          ax::mojom::blink::Role::kForm,
          ax::mojom::blink::Role::kHeader,
          ax::mojom::blink::Role::kList,
          ax::mojom::blink::Role::kListItem,
          ax::mojom::blink::Role::kMain,
          ax::mojom::blink::Role::kMark,
          ax::mojom::blink::Role::kMath,
          ax::mojom::blink::Role::kMathMLMath,
          // Don't ignore MathML nodes by default, since MathML relies on child
          // positions to determine semantics (e.g. numerator is the first
          // child of a fraction).
          ax::mojom::blink::Role::kMathMLFraction,
          ax::mojom::blink::Role::kMathMLIdentifier,
          ax::mojom::blink::Role::kMathMLMultiscripts,
          ax::mojom::blink::Role::kMathMLNoneScript,
          ax::mojom::blink::Role::kMathMLNumber,
          ax::mojom::blink::Role::kMathMLOperator,
          ax::mojom::blink::Role::kMathMLOver,
          ax::mojom::blink::Role::kMathMLPrescriptDelimiter,
          ax::mojom::blink::Role::kMathMLRoot,
          ax::mojom::blink::Role::kMathMLRow,
          ax::mojom::blink::Role::kMathMLSquareRoot,
          ax::mojom::blink::Role::kMathMLStringLiteral,
          ax::mojom::blink::Role::kMathMLSub,
          ax::mojom::blink::Role::kMathMLSubSup,
          ax::mojom::blink::Role::kMathMLSup,
          ax::mojom::blink::Role::kMathMLTable,
          ax::mojom::blink::Role::kMathMLTableCell,
          ax::mojom::blink::Role::kMathMLTableRow,
          ax::mojom::blink::Role::kMathMLText,
          ax::mojom::blink::Role::kMathMLUnder,
          ax::mojom::blink::Role::kMathMLUnderOver,
          ax::mojom::blink::Role::kMeter,
          ax::mojom::blink::Role::kNavigation,
          ax::mojom::blink::Role::kPluginObject,
          ax::mojom::blink::Role::kProgressIndicator,
          ax::mojom::blink::Role::kRegion,
          ax::mojom::blink::Role::kRuby,
          ax::mojom::blink::Role::kSearch,
          ax::mojom::blink::Role::kSection,
          ax::mojom::blink::Role::kSplitter,
          ax::mojom::blink::Role::kSubscript,
          ax::mojom::blink::Role::kSuperscript,
          ax::mojom::blink::Role::kTime,
      };

  if (always_included_computed_roles.find(RoleValue()) !=
      always_included_computed_roles.end())
    return kIncludeObject;

  // Using the title or accessibility description (so we
  // check if there's some kind of accessible name for the element)
  // to decide an element's visibility is not as definitive as
  // previous checks, so this should remain as one of the last.
  if (HasAriaAttribute() || !GetAttribute(kTitleAttr).empty())
    return kIncludeObject;

  if (IsImage() && !IsA<SVGElement>(node)) {
    String alt = GetAttribute(kAltAttr);
    // A null alt attribute means the attribute is not present. We assume this
    // is a mistake, and expose the image so that it can be repaired.
    // In contrast, alt="" is treated as intentional markup to ignore the image.
    if (!alt.empty() || alt.IsNull())
      return kIncludeObject;
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXEmptyAlt));
    return kIgnoreObject;
  }

  // Process potential in-page link targets
  if (IsPotentialInPageLinkTarget(*element))
    return kIncludeObject;

  // <span> tags are inline tags and not meant to convey information if they
  // have no other ARIA information on them. If we don't ignore them, they may
  // emit signals expected to come from their parent.
  if (IsA<HTMLSpanElement>(node)) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return kIgnoreObject;
  }

  // Ignore labels that are already used to name a control.
  // See IsRedundantLabel() for more commentary.
  if (HTMLLabelElement* label = DynamicTo<HTMLLabelElement>(node)) {
    if (IsRedundantLabel(label)) {
      if (ignored_reasons) {
        ignored_reasons->push_back(
            IgnoredReason(kAXLabelFor, AXObjectCache().Get(label->control())));
      }
      return kIgnoreObject;
    }
    return kIncludeObject;
  }

  return kDefaultBehavior;
}

// static
absl::optional<String> AXNodeObject::GetCSSAltText(const Node* node) {
  // CSS alt text rules allow text to be assigned to ::before/::after content.
  // For example, the following CSS assigns "bullet" text to bullet.png:
  // .something::before {
  //   content: url(bullet.png) / "bullet";
  // }

  if (!node || !node->GetComputedStyle() ||
      node->GetComputedStyle()->ContentBehavesAsNormal()) {
    return absl::nullopt;
  }

  const ComputedStyle* style = node->GetComputedStyle();
  if (node->IsPseudoElement()) {
    for (const ContentData* content_data = style->GetContentData();
         content_data; content_data = content_data->Next()) {
      if (content_data->IsAltText())
        return To<AltTextContentData>(content_data)->GetText();
    }
    return absl::nullopt;
  }

  // If the content property is used on a non-pseudo element, match the
  // behaviour of LayoutObject::CreateObject and only honour the style if
  // there is exactly one piece of content, which is an image.
  const ContentData* content_data = style->GetContentData();
  if (content_data && content_data->IsImage() && content_data->Next() &&
      content_data->Next()->IsAltText()) {
    return To<AltTextContentData>(content_data->Next())->GetText();
  }

  return absl::nullopt;
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

  if (DisplayLockUtilities::IsDisplayLockedPreventingPaint(GetNode())) {
    if (IsAriaHidden() ||
        DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
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

// There should only be one banner/contentInfo per page. If header/footer are
// being used within an article, aside, nave, section, blockquote, details,
// fieldset, figure, td, or main, then it should not be exposed as whole
// page's banner/contentInfo.
static HashSet<QualifiedName>& GetLandmarkRolesNotAllowed() {
  DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, landmark_roles_not_allowed, ());
  if (landmark_roles_not_allowed.empty()) {
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

  const auto* row = DynamicTo<HTMLTableRowElement>(cell->parentNode());
  if (!row)
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
    return ax::mojom::blink::Role::kGenericContainer;

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

ax::mojom::blink::Role AXNodeObject::RoleFromLayoutObjectOrNode() const {
  return ax::mojom::blink::Role::kGenericContainer;
}

// Does not check ARIA role, but does check some ARIA properties, specifically
// @aria-haspopup/aria-pressed via ButtonType().
// TODO(accessibility) Ensure that if the native role needs to change, that the
// object is destroyed and a new one is created. Examples are changes to
// IsClickable(), DataList(), aria-pressed, the parent's tag, @role.
ax::mojom::blink::Role AXNodeObject::NativeRoleIgnoringAria() const {
  if (!GetNode()) {
    // Can be null in the case of pseudo content.
    return RoleFromLayoutObjectOrNode();
  }

  if (GetNode()->IsPseudoElement() && GetCSSAltText(GetNode())) {
    const ComputedStyle* style = GetNode()->GetComputedStyle();
    ContentData* content_data = style->GetContentData();
    // We just check the first item of the content list to determine the
    // appropriate role, should only ever be image or text.
    // TODO(accessibility) Is it possible to use CSS alt text on an HTML tag
    // with strong semantics? If so, why are we overriding the role here?
    // We only need to ensure the accessible name gets the CSS alt text.
    // Note: by doing this, we are often hiding child pseudo element content
    // because IsRelevantPseudoElementDescendant() returns false when an
    // ancestor has CSS alt text.
    if (content_data->IsImage())
      return ax::mojom::blink::Role::kImage;

    return ax::mojom::blink::Role::kStaticText;
  }

  if (GetNode()->IsTextNode())
    return ax::mojom::blink::Role::kStaticText;

  const HTMLSelectMenuElement* owner_select_menu =
      HTMLSelectMenuElement::OwnerSelectMenu(GetNode());
  if (owner_select_menu) {
    HTMLSelectMenuElement::PartType part_type =
        owner_select_menu->AssignedPartType(GetNode());
    if (part_type == HTMLSelectMenuElement::PartType::kButton) {
      return ax::mojom::blink::Role::kComboBoxMenuButton;
    } else if (part_type == HTMLSelectMenuElement::PartType::kListBox) {
      return ax::mojom::blink::Role::kListBox;
    } else if (part_type == HTMLSelectMenuElement::PartType::kOption) {
      return ax::mojom::blink::Role::kListBoxOption;
    }
  }

  if (IsA<HTMLImageElement>(GetNode()))
    return ax::mojom::blink::Role::kImage;

  // <a> or <svg:a>.
  if (IsA<HTMLAnchorElement>(GetNode()) || IsA<SVGAElement>(GetNode())) {
    // Assume that an anchor element is a Role::kLink if it has an href or a
    // click event listener.
    if (GetNode()->IsLink() || IsClickable())
      return ax::mojom::blink::Role::kLink;

    // According to the SVG-AAM, a non-link 'a' element should be exposed like
    // a 'g' if it does not descend from a 'text' element and like a 'tspan'
    // if it does. This is consistent with the SVG spec which states that an
    // 'a' within 'text' acts as an inline element, whereas it otherwise acts
    // as a container element.
    if (IsA<SVGAElement>(GetNode()) &&
        !Traversal<SVGTextElement>::FirstAncestor(*GetNode())) {
      return ax::mojom::blink::Role::kGroup;
    }

    return ax::mojom::blink::Role::kGenericContainer;
  }

  if (IsA<HTMLPortalElement>(*GetNode()))
    return ax::mojom::blink::Role::kPortal;

  if (IsA<HTMLButtonElement>(*GetNode()))
    return ButtonRoleType();

  if (IsA<HTMLDetailsElement>(*GetNode()))
    return ax::mojom::blink::Role::kDetails;

  if (IsA<HTMLSummaryElement>(*GetNode())) {
    ContainerNode* parent = LayoutTreeBuilderTraversal::Parent(*GetNode());
    if (ToHTMLSlotElementIfSupportsAssignmentOrNull(parent))
      parent = LayoutTreeBuilderTraversal::Parent(*parent);
    if (parent && IsA<HTMLDetailsElement>(parent))
      return ax::mojom::blink::Role::kDisclosureTriangle;
    return ax::mojom::blink::Role::kGenericContainer;
  }

  // Chrome exposes both table markup and table CSS as a table, letting
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
        type == input_type_names::kMonth || type == input_type_names::kWeek) {
      return ax::mojom::blink::Role::kDateTime;
    }
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
      return ax::mojom::blink::Role::kComboBoxSelect;
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
    return RoleFromLayoutObjectOrNode();

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

  // Mapping of MathML elements. See https://w3c.github.io/mathml-aam/
  if (auto* element = DynamicTo<MathMLElement>(GetNode())) {
    if (element->HasTagName(mathml_names::kMathTag)) {
      return RuntimeEnabledFeatures::MathMLCoreEnabled()
                 ? ax::mojom::blink::Role::kMathMLMath
                 : ax::mojom::blink::Role::kMath;
    }
    if (element->HasTagName(mathml_names::kMfracTag))
      return ax::mojom::blink::Role::kMathMLFraction;
    if (element->HasTagName(mathml_names::kMiTag))
      return ax::mojom::blink::Role::kMathMLIdentifier;
    if (element->HasTagName(mathml_names::kMmultiscriptsTag))
      return ax::mojom::blink::Role::kMathMLMultiscripts;
    if (element->HasTagName(mathml_names::kMnTag))
      return ax::mojom::blink::Role::kMathMLNumber;
    if (element->HasTagName(mathml_names::kMoTag))
      return ax::mojom::blink::Role::kMathMLOperator;
    if (element->HasTagName(mathml_names::kMoverTag))
      return ax::mojom::blink::Role::kMathMLOver;
    if (element->HasTagName(mathml_names::kMunderTag))
      return ax::mojom::blink::Role::kMathMLUnder;
    if (element->HasTagName(mathml_names::kMunderoverTag))
      return ax::mojom::blink::Role::kMathMLUnderOver;
    if (element->HasTagName(mathml_names::kMrootTag))
      return ax::mojom::blink::Role::kMathMLRoot;
    if (element->HasTagName(mathml_names::kMrowTag) ||
        element->HasTagName(mathml_names::kAnnotationXmlTag) ||
        element->HasTagName(mathml_names::kMactionTag) ||
        element->HasTagName(mathml_names::kMerrorTag) ||
        element->HasTagName(mathml_names::kMpaddedTag) ||
        element->HasTagName(mathml_names::kMphantomTag) ||
        element->HasTagName(mathml_names::kMstyleTag) ||
        element->HasTagName(mathml_names::kSemanticsTag)) {
      return ax::mojom::blink::Role::kMathMLRow;
    }
    if (element->HasTagName(mathml_names::kMprescriptsTag))
      return ax::mojom::blink::Role::kMathMLPrescriptDelimiter;
    if (element->HasTagName(mathml_names::kNoneTag))
      return ax::mojom::blink::Role::kMathMLNoneScript;
    if (element->HasTagName(mathml_names::kMsqrtTag))
      return ax::mojom::blink::Role::kMathMLSquareRoot;
    if (element->HasTagName(mathml_names::kMsTag))
      return ax::mojom::blink::Role::kMathMLStringLiteral;
    if (element->HasTagName(mathml_names::kMsubTag))
      return ax::mojom::blink::Role::kMathMLSub;
    if (element->HasTagName(mathml_names::kMsubsupTag))
      return ax::mojom::blink::Role::kMathMLSubSup;
    if (element->HasTagName(mathml_names::kMsupTag))
      return ax::mojom::blink::Role::kMathMLSup;
    if (element->HasTagName(mathml_names::kMtableTag))
      return ax::mojom::blink::Role::kMathMLTable;
    if (element->HasTagName(mathml_names::kMtdTag))
      return ax::mojom::blink::Role::kMathMLTableCell;
    if (element->HasTagName(mathml_names::kMtrTag))
      return ax::mojom::blink::Role::kMathMLTableRow;
    if (element->HasTagName(mathml_names::kMtextTag) ||
        element->HasTagName(mathml_names::kAnnotationTag)) {
      return ax::mojom::blink::Role::kMathMLText;
    }
  }

  if (GetNode()->HasTagName(html_names::kRpTag) ||
      GetNode()->HasTagName(html_names::kRtTag)) {
    return ax::mojom::blink::Role::kRubyAnnotation;
  }

  if (IsA<HTMLFormElement>(*GetNode())) {
    // Only treat <form> as role="form" when it has an accessible name, which
    // can only occur when the name is assigned by the author via aria-label,
    // aria-labelledby, or title. Otherwise, treat as a <section>.
    return IsNameFromAuthorAttribute() ? ax::mojom::blink::Role::kForm
                                       : ax::mojom::blink::Role::kSection;
  }

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

  if (GetNode()->HasTagName(html_names::kSubTag))
    return ax::mojom::blink::Role::kSubscript;

  if (GetNode()->HasTagName(html_names::kSupTag))
    return ax::mojom::blink::Role::kSuperscript;

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

  if (GetNode()->HasTagName(html_names::kSectionTag)) {
    // Treat a named <section> as role="region".
    return IsNameFromAuthorAttribute() ? ax::mojom::blink::Role::kRegion
                                       : ax::mojom::blink::Role::kSection;
  }

  if (GetNode()->HasTagName(html_names::kAddressTag))
    return ax::mojom::blink::Role::kGroup;

  if (IsA<HTMLDialogElement>(*GetNode()))
    return ax::mojom::blink::Role::kDialog;

  // The HTML element.
  if (IsA<HTMLHtmlElement>(GetNode()))
    return RoleFromLayoutObjectOrNode();

  // Treat <iframe>, <frame> and <fencedframe> the same.
  if (IsFrame(GetNode()))
    return ax::mojom::blink::Role::kIframe;

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

  if (IsA<HTMLTableCaptionElement>(GetNode()))
    return ax::mojom::blink::Role::kCaption;

  if (GetNode()->HasTagName(html_names::kFigcaptionTag))
    return ax::mojom::blink::Role::kFigcaption;

  if (GetNode()->HasTagName(html_names::kFigureTag))
    return ax::mojom::blink::Role::kFigure;

  if (IsA<HTMLTimeElement>(GetNode()))
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

  return RoleFromLayoutObjectOrNode();
}

ax::mojom::blink::Role AXNodeObject::DetermineAccessibilityRole() {
#if DCHECK_IS_ON()
  base::AutoReset<bool> reentrancy_protector(&is_computing_role_, true);
#endif

  if (IsDetached()) {
    NOTREACHED();
    return ax::mojom::blink::Role::kUnknown;
  }

  native_role_ = NativeRoleIgnoringAria();

  aria_role_ = DetermineAriaRoleAttribute();

  return aria_role_ == ax::mojom::blink::Role::kUnknown ? native_role_
                                                        : aria_role_;
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

void AXNodeObject::Init(AXObject* parent) {
#if DCHECK_IS_ON()
  DCHECK(!initialized_);
  initialized_ = true;
#endif
  AXObject::Init(parent);

  DCHECK(role_ == native_role_ || role_ == aria_role_)
      << "Role must be either the cached native role or cached aria role: "
      << "\n* Final role: " << role_ << "\n* Native role: " << native_role_
      << "\n* Aria role: " << aria_role_ << "\n* Node: " << GetNode();

  DCHECK(node_ || (GetLayoutObject() &&
                   AXObjectCacheImpl::IsRelevantPseudoElementDescendant(
                       *GetLayoutObject())))
      << "Nodeless AXNodeObject can only exist inside a pseudo element: "
      << GetLayoutObject();
}

void AXNodeObject::Detach() {
#if defined(AX_FAIL_FAST_BUILD)
  SANITIZER_CHECK(!is_adding_children_)
      << "Cannot detach |this| during AddChildren(): " << GetNode();
#endif
  AXObject::Detach();
  node_ = nullptr;
}

bool AXNodeObject::IsAXNodeObject() const {
  return true;
}

bool AXNodeObject::IsControl() const {
  Node* node = GetNode();
  if (!node)
    return false;

  auto* element = DynamicTo<Element>(node);
  return ((element && element->IsFormControlElement()) ||
          AXObject::IsARIAControl(AriaRoleAttribute()));
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

bool AXNodeObject::IsFieldset() const {
  return IsA<HTMLFieldSetElement>(GetNode());
}

bool AXNodeObject::IsHovered() const {
  if (Node* node = GetNode())
    return node->IsHovered();
  return false;
}

bool AXNodeObject::IsImageButton() const {
  return IsNativeImage() && IsButton();
}

bool AXNodeObject::IsInputImage() const {
  auto* html_input_element = DynamicTo<HTMLInputElement>(GetNode());
  if (html_input_element && RoleValue() == ax::mojom::blink::Role::kButton)
    return html_input_element->type() == input_type_names::kImage;

  return false;
}

bool AXNodeObject::IsLineBreakingObject() const {
  // According to Blink Editing, objects without an associated DOM node such as
  // pseudo-elements and list bullets, are never considered as paragraph
  // boundaries.
  if (IsDetached() || !GetNode())
    return false;

  // Presentational objects should not contribute any of their semantic meaning
  // to the accessibility tree, including to its text representation.
  if (IsPresentational())
    return false;

  // `IsEnclosingBlock` includes all elements with display block, inline block,
  // table related, flex, grid, list item, flow-root, webkit-box, and display
  // contents. This is the same function used by Blink > Editing for determining
  // paragraph boundaries, i.e. line breaking objects.
  if (IsEnclosingBlock(GetNode()))
    return true;

  // Not all <br> elements have an associated layout object. They might be
  // "visibility: hidden" or within a display locked region. We need to check
  // their DOM node first.
  if (IsA<HTMLBRElement>(GetNode()))
    return true;

  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return AXObject::IsLineBreakingObject();

  if (layout_object->IsBR())
    return true;

  // LayoutText objects could include a paragraph break in their text. This can
  // only occur if line breaks are preserved and a newline character is present
  // in their collapsed text. Text collapsing removes all whitespace found in
  // the HTML file, but a special style rule could be used to preserve line
  // breaks.
  //
  // The best example is the <pre> element:
  // <pre>Line 1
  // Line 2</pre>
  if (const LayoutText* layout_text = DynamicTo<LayoutText>(layout_object)) {
    const ComputedStyle& style = layout_object->StyleRef();
    if (layout_text->HasNonCollapsedText() && style.PreserveNewline() &&
        layout_text->PlainText().find('\n') != WTF::kNotFound) {
      return true;
    }
  }

  // Rely on the ARIA role to figure out if this object is line breaking.
  return AXObject::IsLineBreakingObject();
}

bool AXNodeObject::IsLoaded() const {
  if (!GetDocument())
    return false;

  if (!GetDocument()->IsLoadCompleted())
    return false;

  // Check for a navigation API single-page app navigation in progress.
  if (auto* window = GetDocument()->domWindow()) {
    if (window->navigation()->HasNonDroppedOngoingNavigation())
      return false;
  }

  return true;
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
  Node* node = GetNode();
  if (!node)
    return false;

  if (IsA<HTMLImageElement>(*node) || IsA<HTMLPlugInElement>(*node))
    return true;

  if (const auto* input = DynamicTo<HTMLInputElement>(*node))
    return input->type() == input_type_names::kImage;

  return false;
}

bool AXNodeObject::IsOffScreen() const {
  if (IsDetached())
    return false;
  DCHECK(GetNode());
  // Differs fromAXLayoutObject::IsOffScreen() in that there is no bounding box.
  // However, we know that if it is display-locked that is an indicator that it
  // is currently offscreen, and will likely be onscreen once scrolled to.
  return DisplayLockUtilities::IsDisplayLockedPreventingPaint(GetNode());
}

bool AXNodeObject::IsProgressIndicator() const {
  return RoleValue() == ax::mojom::blink::Role::kProgressIndicator;
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

bool AXNodeObject::IsChildTreeOwner() const {
  return ui::IsChildTreeOwner(native_role_);
}

bool AXNodeObject::IsClickable() const {
  // Determine whether the element is clickable either because there is a
  // mouse button handler or because it has a native element where click
  // performs an action. Disabled nodes are never considered clickable.
  // Note: we can't call |node->WillRespondToMouseClickEvents()| because that
  // triggers a style recalc and can delete this.

  // Treat mouse button listeners on the |window|, |document| as if they're on
  // the |documentElement|.
  if (GetNode() == GetDocument()->documentElement()) {
    return GetNode()->HasAnyEventListeners(
               event_util::MouseButtonEventTypes()) ||
           GetDocument()->HasAnyEventListeners(
               event_util::MouseButtonEventTypes()) ||
           GetDocument()->domWindow()->HasAnyEventListeners(
               event_util::MouseButtonEventTypes());
  }

  // Look for mouse listeners only on element nodes, e.g. skip text nodes.
  const Element* element = GetElement();
  if (!element)
    return false;

  if (IsDisabled())
    return false;

  if (element->HasAnyEventListeners(event_util::MouseButtonEventTypes()))
    return true;

  if (HasContentEditableAttributeSet())
    return true;

  // Certain user-agent shadow DOM elements are expected to be clickable but
  // they do not have event listeners attached or a clickable native role. We
  // whitelist them here.
  if (element->ShadowPseudoId() ==
      shadow_element_names::kPseudoCalendarPickerIndicator) {
    return true;
  }

  // Only use native roles. For ARIA elements, require a click listener.
  return ui::IsClickable(native_role_);
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
  if (!ui::IsSelectRequiredOrImplicit(RoleValue()))
    return kSelectedStateUndefined;

  // Selection follows focus, but ONLY in single selection containers, and only
  // if aria-selected was not present to override.
  return IsSelectedFromFocus() ? kSelectedStateTrue : kSelectedStateFalse;
}

bool AXNodeObject::IsSelectedFromFocusSupported() const {
  // The selection should only follow the focus when the aria-selected attribute
  // is marked as required or implied for this element in the ARIA specs.
  // If this object can't follow the focus, then we can't say that it's selected
  // nor that it's not.
  // TODO(crbug.com/1143483): Consider allowing more roles.
  if (!ui::IsSelectRequiredOrImplicit(RoleValue()))
    return false;

  // https://www.w3.org/TR/wai-aria-1.1/#aria-selected
  // Any explicit assignment of aria-selected takes precedence over the implicit
  // selection based on focus.
  bool is_selected;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kSelected, is_selected))
    return false;

  // Selection follows focus only when in a single selection container.
  const AXObject* container = ContainerWidget();
  if (!container || container->IsMultiSelectable())
    return false;

  // TODO(crbug.com/1143451): https://www.w3.org/TR/wai-aria-1.1/#aria-selected
  // If any DOM element in the widget is explicitly marked as selected, the user
  // agent MUST NOT convey implicit selection for the widget.
  return true;
}

// In single selection containers, selection follows focus unless aria_selected
// is set to false. This is only valid for a subset of elements.
bool AXNodeObject::IsSelectedFromFocus() const {
  if (!IsSelectedFromFocusSupported())
    return false;

  // A tab item can also be selected if it is associated to a focused tabpanel
  // via the aria-labelledby attribute.
  if (IsTabItem() && IsTabItemSelected())
    return true;

  // If this object is not accessibility focused, then it is not selected from
  // focus.
  AXObject* focused_object = AXObjectCache().FocusedObject();
  if (focused_object != this &&
      (!focused_object || focused_object->ActiveDescendant() != this))
    return false;

  return true;
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
  if (IsDisabled())
    return kRestrictionDisabled;

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

  auto* element = GetElement();
  if (!element)
    return kExpandedUndefined;

  if (HTMLSelectMenuElement* select_menu =
          HTMLSelectMenuElement::OwnerSelectMenu(element)) {
    if (select_menu->ButtonPart() == element) {
      return select_menu->open() ? kExpandedExpanded : kExpandedCollapsed;
    }
  }

  if (RoleValue() == ax::mojom::blink::Role::kComboBoxSelect &&
      IsA<HTMLSelectElement>(*element)) {
    return To<HTMLSelectElement>(element)->PopupIsVisible()
               ? kExpandedExpanded
               : kExpandedCollapsed;
  }

  // For form controls that act as triggering elements for popovers of type
  // kAuto, then set aria-expanded=false when the popover is hidden, and
  // aria-expanded=true when it is showing.
  if (auto* form_control = DynamicTo<HTMLFormControlElement>(element)) {
    if (auto popover = form_control->popoverTargetElement().element;
        popover && popover->PopoverType() == PopoverValueType::kAuto) {
      return popover->popoverOpen() ? kExpandedExpanded : kExpandedCollapsed;
    }
  }

  if (IsA<HTMLSummaryElement>(*element)) {
    if (element->parentNode() &&
        IsA<HTMLDetailsElement>(element->parentNode())) {
      return To<Element>(element->parentNode())
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
  Node* node = GetNode();
  return IsA<HTMLCanvasElement>(node) && node->hasChildren();
}

int AXNodeObject::HeadingLevel() const {
  // headings can be in block flow and non-block flow
  Node* node = GetNode();
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
    if (level >= 1)
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
}

String AXNodeObject::AutoComplete() const {
  // Check cache for auto complete state.
  if (AXObjectCache().GetAutofillState(AXObjectID()) ==
      WebAXAutofillState::kAutocompleteAvailable)
    return "list";

  if (IsAtomicTextField() || IsARIATextField()) {
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
  std::vector<int32_t> highlight_types;
  std::vector<int32_t> marker_starts;
  std::vector<int32_t> marker_ends;

  // First use ARIA markers for spelling/grammar if available.
  absl::optional<DocumentMarker::MarkerType> aria_marker_type =
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
      DocumentMarker::kSuggestion | DocumentMarker::kTextFragment |
      DocumentMarker::kCustomHighlight);
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

    int32_t highlight_type =
        static_cast<int32_t>(ax::mojom::blink::HighlightType::kNone);
    if (marker->GetType() == DocumentMarker::kCustomHighlight) {
      const auto& highlight_marker = To<CustomHighlightMarker>(*marker);
      highlight_type =
          ToAXHighlightType(highlight_marker.GetHighlight()->type());
    }

    marker_types.push_back(ToAXMarkerType(marker->GetType()));
    highlight_types.push_back(static_cast<int32_t>(highlight_type));
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
      ax::mojom::blink::IntListAttribute::kHighlightTypes, highlight_types);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kMarkerStarts, marker_starts);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kMarkerEnds, marker_ends);
}

AXObject* AXNodeObject::InPageLinkTarget() const {
  if (!IsLink() || !GetDocument())
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
  AXObject* ax_target = AXObjectCache().GetOrCreate(target);
  if (!ax_target || !IsPotentialInPageLinkTarget(*ax_target->GetNode()))
    return AXObject::InPageLinkTarget();

#if DCHECK_IS_ON()
  // Link targets always have an element, unless it is the document itself,
  // e.g. via <a href="#">.
  DCHECK(ax_target->IsWebArea() || ax_target->GetElement())
      << "The link target is expected to be a document or an element: "
      << ax_target->ToString(true, true) << "\n* URL fragment = " << fragment;
#endif

  // Usually won't be ignored, but could be e.g. if aria-hidden.
  if (ax_target->AccessibilityIsIgnored())
    return nullptr;

  return ax_target;
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

// According to the standard, the figcaption should only be the first or
// last child: https://html.spec.whatwg.org/#the-figcaption-element
AXObject* AXNodeObject::GetChildFigcaption() const {
  AXObject* child = FirstChildIncludingIgnored();
  if (!child)
    return nullptr;
  if (child->RoleValue() == ax::mojom::blink::Role::kFigcaption)
    return child;

  child = LastChildIncludingIgnored();
  if (child->RoleValue() == ax::mojom::blink::Role::kFigcaption)
    return child;

  return nullptr;
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
  AXObject* parent = ParentObjectUnignored();
  if (parent && parent->RoleValue() == ax::mojom::blink::Role::kRadioGroup) {
    for (AXObject* child : parent->UnignoredChildren()) {
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

ax::mojom::blink::TextPosition AXNodeObject::GetTextPositionFromRole() const {
  // Check for role="subscript" or role="superscript" on the element, or if
  // static text, on the containing element.
  AXObject* obj = nullptr;
  if (RoleValue() == ax::mojom::blink::Role::kStaticText)
    obj = ParentObject();
  else
    obj = const_cast<AXNodeObject*>(this);

  if (obj->RoleValue() == ax::mojom::blink::Role::kSubscript)
    return ax::mojom::blink::TextPosition::kSubscript;
  if (obj->RoleValue() == ax::mojom::blink::Role::kSuperscript)
    return ax::mojom::blink::TextPosition::kSuperscript;

  if (!GetLayoutObject() || !GetLayoutObject()->IsInline())
    return ax::mojom::blink::TextPosition::kNone;

  // We could have an inline element which descends from a subscript or
  // superscript.
  if (auto* parent = obj->ParentObjectUnignored())
    return static_cast<AXNodeObject*>(parent)->GetTextPositionFromRole();

  return ax::mojom::blink::TextPosition::kNone;
}

ax::mojom::blink::TextPosition AXNodeObject::GetTextPosition() const {
  if (GetNode()) {
    const auto& text_position = GetTextPositionFromRole();
    if (text_position != ax::mojom::blink::TextPosition::kNone)
      return text_position;
  }

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
    if (EnumHasFlags(decoration.Lines(), TextDecorationLine::kOverline)) {
      *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kOverline);
      *text_overline_style =
          TextDecorationStyleToAXTextDecorationStyle(decoration.Style());
    }
    if (EnumHasFlags(decoration.Lines(), TextDecorationLine::kLineThrough)) {
      *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kLineThrough);
      *text_strikethrough_style =
          TextDecorationStyleToAXTextDecorationStyle(decoration.Style());
    }
    if (EnumHasFlags(decoration.Lines(), TextDecorationLine::kUnderline)) {
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

String AXNodeObject::ImageDataUrl(const gfx::Size& max_size) const {
  Node* node = GetNode();
  if (!node)
    return String();

  ImageBitmapOptions* options = ImageBitmapOptions::Create();
  ImageBitmap* image_bitmap = nullptr;
  if (auto* image = DynamicTo<HTMLImageElement>(node)) {
    image_bitmap =
        MakeGarbageCollected<ImageBitmap>(image, absl::nullopt, options);
  } else if (auto* canvas = DynamicTo<HTMLCanvasElement>(node)) {
    image_bitmap =
        MakeGarbageCollected<ImageBitmap>(canvas, absl::nullopt, options);
  } else if (auto* video = DynamicTo<HTMLVideoElement>(node)) {
    image_bitmap =
        MakeGarbageCollected<ImageBitmap>(video, absl::nullopt, options);
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
      max_size.width() ? max_size.width() * 1.0 / image->width() : 1.0;
  float y_scale =
      max_size.height() ? max_size.height() * 1.0 / image->height() : 1.0;
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
      base::checked_cast<wtf_size_t>(info.computeByteSize(row_bytes)));
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

RGBA32 AXNodeObject::ColorValue() const {
  auto* input = DynamicTo<HTMLInputElement>(GetNode());
  if (!input || !IsColorWell())
    return AXObject::ColorValue();

  const AtomicString& type = input->getAttribute(kTypeAttr);
  if (!EqualIgnoringASCIICase(type, "color"))
    return AXObject::ColorValue();

  // HTMLInputElement::Value always returns a string parseable by Color.
  Color color;
  bool success = color.SetFromString(input->Value());
  DCHECK(success);
  return color.Rgb();
}

RGBA32 AXNodeObject::BackgroundColor() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return Color::kTransparent.Rgb();

  if (IsWebArea()) {
    LocalFrameView* view = DocumentFrameView();
    if (view)
      return view->BaseBackgroundColor().Rgb();
    else
      return Color::kWhite.Rgb();
  }

  const ComputedStyle* style = layout_object->Style();
  if (!style || !style->HasBackground())
    return Color::kTransparent.Rgb();

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

const AtomicString& AXNodeObject::ComputedFontFamily() const {
  if (!GetLayoutObject())
    return AXObject::ComputedFontFamily();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXObject::ComputedFontFamily();

  const FontDescription& font_description = style->GetFontDescription();
  return font_description.FirstFamily().FamilyName();
}

String AXNodeObject::FontFamilyForSerialization() const {
  if (!GetLayoutObject())
    return AXObject::FontFamilyForSerialization();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXObject::FontFamilyForSerialization();

  const SimpleFontData* primary_font = style->GetFont().PrimaryFont();
  if (!primary_font)
    return AXObject::FontFamilyForSerialization();

  // Note that repeatedly querying this can be expensive - only use this when
  // serializing. For other comparisons consider using `ComputedFontFamily`.
  return primary_font->PlatformData().FontFamilyName();
}

// Blink font size is provided in pixels.
// Platform APIs may convert to another unit (IA2 converts to points).
float AXNodeObject::FontSize() const {
  if (!GetLayoutObject())
    return AXObject::FontSize();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXObject::FontSize();

  // Font size should not be affected by scale transform or page zoom, because
  // users of authoring tools may want to check that their text is formatted
  // with the font size they expected.
  // E.g. use SpecifiedFontSize() instead of ComputedFontSize(), and do not
  // multiply by style->Scale()->Transform()->Y();
  return style->SpecifiedFontSize();
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
  if (attribute_value.empty() ||
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
  if (!attribute_value.empty())
    return ax::mojom::blink::AriaCurrentState::kTrue;

  return AXObject::GetAriaCurrentState();
}

ax::mojom::blink::InvalidState AXNodeObject::GetInvalidState() const {
  // First check aria-invalid.
  const AtomicString& attribute_value =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kInvalid);
  // aria-invalid="false".
  if (EqualIgnoringASCIICase(attribute_value, "false"))
    return ax::mojom::blink::InvalidState::kFalse;
  // In most cases, aria-invalid="spelling"| "grammar" are used on inline text
  // elements, and are exposed via Markers() as if they are native errors.
  // Therefore, they are exposed as InvalidState:kNone here in order to avoid
  // exposing the state twice, and to prevent superfluous "invalid"
  // announcements in some screen readers.
  // On text fields, they are simply exposed as if aria-invalid="true".
  if (EqualIgnoringASCIICase(attribute_value, "spelling") ||
      EqualIgnoringASCIICase(attribute_value, "grammar")) {
    return RoleValue() == ax::mojom::blink::Role::kTextField
               ? ax::mojom::blink::InvalidState::kTrue
               : ax::mojom::blink::InvalidState::kNone;
  }
  // Any other non-empty value is considered true.
  if (!attribute_value.empty()) {
    return ax::mojom::blink::InvalidState::kTrue;
  }

  // Next check for native the invalid state.
  if (GetElement()) {
    ListedElement* form_control = ListedElement::From(*GetElement());
    if (form_control) {
      return IsValidFormControl(form_control)
                 ? ax::mojom::blink::InvalidState::kFalse
                 : ax::mojom::blink::InvalidState::kTrue;
    }
  }

  return AXObject::GetInvalidState();
}

bool AXNodeObject::IsValidFormControl(ListedElement* form_control) const {
  // If the control is marked with a custom error, the form control is invalid.
  if (form_control->CustomError())
    return false;

  // If the form control checks for validity, and has passed the checks,
  // then consider it valid.
  if (form_control->IsNotCandidateOrValid())
    return true;

  // The control is invalid, as far as CSS is concerned.
  // However, we ignore a failed check inside of an empty required text field,
  // in order to avoid redundant verbalizations (screen reader already says
  // required).
  if (IsAtomicTextField() && IsRequired() && GetValueForControl().length() == 0)
    return true;

  return false;
}

int AXNodeObject::PosInSet() const {
  if (RoleValue() == ax::mojom::blink::Role::kComboBoxSelect && GetNode() &&
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
  if (RoleValue() == ax::mojom::blink::Role::kComboBoxSelect && GetNode() &&
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
      [[fallthrough]];
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
  // for aria-valuemax were changed to 100. This change was made for
  // progressbar in ARIA 1.2.
  switch (AriaRoleAttribute()) {
    case ax::mojom::blink::Role::kProgressIndicator:
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
  // for aria-valuemin were changed to 0. This change was made for
  // progressbar in ARIA 1.2.
  switch (AriaRoleAttribute()) {
    case ax::mojom::blink::Role::kProgressIndicator:
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
  if (IsLink())  // <area>, <link>, <html:a> or <svg:a>
    return GetElement()->HrefURL();

  if (IsWebArea() && GetDocument())
    return GetDocument()->Url();

  auto* html_image_element = DynamicTo<HTMLImageElement>(GetNode());
  if (IsImage() && html_image_element) {
    // Using ImageSourceURL handles both src and srcset.
    String source_url = html_image_element->ImageSourceURL();
    String stripped_image_source_url =
        StripLeadingAndTrailingHTMLSpaces(source_url);
    if (!stripped_image_source_url.empty())
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
    return select_element->InnerElement().GetInnerTextWithoutUpdate();
  }

  if (IsAtomicTextField()) {
    // This is an "<input type=text>" or a "<textarea>": We should not simply
    // return the "value" attribute because it might be sanitized in some input
    // control types, e.g. email fields. If we do that, then "selectionStart"
    // and "selectionEnd" indices will not match with the text in the sanitized
    // value.
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
      return input->Value();
    }
  }

  // An ARIA combobox can get value from inner contents.
  if (RoleValue() == ax::mojom::blink::Role::kComboBoxMenuButton) {
    AXObjectSet visited;
    return TextFromDescendants(visited, nullptr, false);
  }

  return String();
}

String AXNodeObject::SlowGetValueForControlIncludingContentEditable() const {
  if (IsNonAtomicTextField()) {
    Element* element = GetElement();
    return element ? element->GetInnerTextWithoutUpdate() : String();
  }
  return GetValueForControl();
}

ax::mojom::blink::Role AXNodeObject::AriaRoleAttribute() const {
  return aria_role_;
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
  TokenVectorFromAttribute(GetElement(), str_dropeffects,
                           html_names::kAriaDropeffectAttr);

  if (str_dropeffects.empty()) {
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
      RoleValue() == ax::mojom::blink::Role::kTextFieldWithComboBox) {
    return ax::mojom::blink::HasPopup::kListbox;
  }

  if (AXObjectCache().GetAutofillState(AXObjectID()) !=
      WebAXAutofillState::kNoSuggestions) {
    return ax::mojom::blink::HasPopup::kMenu;
  }

  return AXObject::HasPopup();
}

bool AXNodeObject::IsEditableRoot() const {
  const Node* node = GetNode();
  if (IsDetached() || !node)
    return false;
#if DCHECK_IS_ON()  // Required in order to get Lifecycle().ToString()
  DCHECK(GetDocument());
  DCHECK_GE(GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kStyleClean)
      << "Unclean document style at lifecycle state "
      << GetDocument()->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  // Catches the case where the 'contenteditable' attribute is set on an atomic
  // text field (which shouldn't have any effect).
  if (IsAtomicTextField())
    return false;

  // The DOM inside native text fields is an implementation detail that should
  // not be exposed to platform accessibility APIs.
  if (EnclosingTextControl(node))
    return false;

  if (IsRootEditableElement(*node))
    return true;

  // Catches the case where a contenteditable is inside another contenteditable.
  // This is especially important when the two nested contenteditables have
  // different attributes, e.g. "true" vs. "plaintext-only".
  if (HasContentEditableAttributeSet())
    return true;

  return false;
}

bool AXNodeObject::HasContentEditableAttributeSet() const {
  if (IsDetached() || !GetNode())
    return false;

  const auto* html_element = DynamicTo<HTMLElement>(GetNode());
  if (!html_element)
    return false;

  ContentEditableType normalized_value =
      html_element->contentEditableNormalized();
  return normalized_value == ContentEditableType::kContentEditable ||
         normalized_value == ContentEditableType::kPlaintextOnly;
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
    html_input_element->SetValue(
        string, TextFieldEventBehavior::kDispatchInputAndChangeEvent);
    return true;
  }

  if (auto* text_area_element = DynamicTo<HTMLTextAreaElement>(*GetNode())) {
    DCHECK(layout_object->IsTextAreaIncludingNG());
    text_area_element->SetValue(
        string, TextFieldEventBehavior::kDispatchInputAndChangeEvent);
    return true;
  }

  if (HasContentEditableAttributeSet()) {
    To<HTMLElement>(GetNode())->setInnerText(string);
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
    if (!input_name.empty())
      return name + " " + input_name;
  }

  return name;
}

String AXNodeObject::TextAlternative(
    bool recursive,
    const AXObject* aria_label_or_description_root,
    AXObjectSet& visited,
    ax::mojom::blink::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources) const {
  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  DCHECK(!name_sources || related_objects);

  bool found_text_alternative = false;

  if (!GetNode() && !GetLayoutObject())
    return String();

  // Exclude offscreen objects inside a portal.
  // NOTE: If an object is found to be offscreen, this also omits its children,
  // which may not be offscreen in some cases.
  Page* page = GetNode() ? GetNode()->GetDocument().GetPage() : nullptr;
  if (page && page->InsidePortal()) {
    LayoutRect bounds = GetBoundsInFrameCoordinates();
    gfx::Size document_size =
        GetNode()->GetDocument().GetLayoutView()->GetLayoutSize();
    bool is_visible =
        bounds.Intersects(LayoutRect(gfx::Point(), document_size));
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
    String value_for_name = GetValueContributionToName(visited);
    if (!value_for_name.IsNull())
      return value_for_name;
  }

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1 -- aria-label.
  String text_alternative = AriaTextAlternative(
      recursive, aria_label_or_description_root, visited, name_from,
      related_objects, name_sources, &found_text_alternative);
  if (found_text_alternative && !name_sources)
    return text_alternative;

  // Step 2D from: http://www.w3.org/TR/accname-aam-1.1  -- native markup.
  text_alternative =
      NativeTextAlternative(visited, name_from, related_objects, name_sources,
                            &found_text_alternative);
  const bool has_text_alternative =
      !text_alternative.empty() ||
      name_from == ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty;
  if (has_text_alternative && !name_sources)
    return text_alternative;

  // Step 2F / 2G from: http://www.w3.org/TR/accname-aam-1.1 -- from content.
  if (aria_label_or_description_root || SupportsNameFromContents(recursive)) {
    Node* node = GetNode();
    if (!IsA<HTMLSelectElement>(node)) {  // Avoid option descendant text
      name_from = ax::mojom::blink::NameFrom::kContents;
      if (name_sources) {
        name_sources->push_back(NameSource(found_text_alternative));
        name_sources->back().type = name_from;
      }

      if (auto* text_node = DynamicTo<Text>(node)) {
        text_alternative = text_node->data();
      } else if (IsA<HTMLBRElement>(node)) {
        text_alternative = String("\n");
      } else {
        text_alternative =
            TextFromDescendants(visited, aria_label_or_description_root, false);
      }

      if (!text_alternative.empty()) {
        if (name_sources) {
          found_text_alternative = true;
          name_sources->back().text = text_alternative;
        } else {
          return text_alternative;
        }
      }
    }
  }

  // Step 2I from: http://www.w3.org/TR/accname-aam-1.1
  name_from = ax::mojom::blink::NameFrom::kTitle;
  const AtomicString& title = GetAttribute(kTitleAttr);
  String titleText = text_alternative = TextAlternativeFromTitleAttribute(
      title, name_from, name_sources, &found_text_alternative);
  if (!title.empty()) {
    if (name_sources) {
      text_alternative = titleText;
    } else {
      return titleText;
    }
  }

  name_from = ax::mojom::blink::NameFrom::kNone;
  if (name_sources && found_text_alternative) {
    for (NameSource& name_source : *name_sources) {
      if (!name_source.text.IsNull() && !name_source.superseded) {
        name_from = name_source.type;
        if (!name_source.related_objects.empty())
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
  if (previous->IsControl() || next->IsControl())
    return true;

  if (!RuntimeEnabledFeatures::LayoutNGBlockInInlineEnabled())
    return false;

  // When |previous| and |next| are in same inline formatting context, we
  // may have block-in-inline between |previous| and |next|.
  // For <div>abc<p aria-hidden=true>...</p>def</div>, we have following
  // layout tree:
  //    LayoutBlockFlow {DIV}, children are inline
  //      LayoutText "abc"  <= previous
  //      LayoutBlockFlow (anonymous) block-in-inline wrapper
  //        LayoutBlockFlow {P}
  //          ...
  //      LayoutText "def" <= next
  // When block-in-inline disabled, layout tree is:
  //    LayoutBlockFlow {DIV}, children are block
  //      LayoutBlockFlow (anonymous)
  //        LayoutText "abc"  <= previous
  //      LayoutBlockFlow (anonymous) block-in-inline wrapper
  //        LayoutBlockFlow {P}
  //          ...
  //      LayoutBlockFlow (anonymous)
  //        LayoutText "def" <= next
  // See accessibility/name-calc-aria-hidden.html
  const auto* next_layout_object = next->GetLayoutObject();
  for (auto* layout_object = previous->GetLayoutObject();
       layout_object && layout_object != next_layout_object;
       layout_object = layout_object->NextInPreOrder()) {
    if (layout_object->IsBlockInInline())
      return true;
  }
  return false;
}

String AXNodeObject::TextFromDescendants(
    AXObjectSet& visited,
    const AXObject* aria_label_or_description_root,
    bool recursive) const {
  if (!CanHaveChildren())
    return recursive ? String() : GetElement()->GetInnerTextWithoutUpdate();

  StringBuilder accumulated_text;
  AXObject* previous = nullptr;
  ax::mojom::blink::NameFrom last_used_name_from =
      ax::mojom::blink::NameFrom::kNone;

  // Ensure that if this node needs to invalidate its children (e.g. due to
  // included in tree status change), that we do it now, rather than while
  // traversing the children.
  UpdateCachedAttributeValuesIfNeeded();

  const AXObjectVector& children = ChildrenIncludingIgnored();
#if defined(AX_FAIL_FAST_BUILD)
  base::AutoReset<bool> auto_reset(&is_computing_text_from_descendants_, true);
#endif
  for (AXObject* child : children) {
    constexpr size_t kMaxDescendantsForTextAlternativeComputation = 100;
    if (visited.size() > kMaxDescendantsForTextAlternativeComputation)
      break;

    if (child->IsHiddenForTextAlternativeCalculation(
            aria_label_or_description_root)) {
      continue;
    }

    ax::mojom::blink::NameFrom child_name_from =
        ax::mojom::blink::NameFrom::kNone;
    String result;
    if (child->IsPresentational()) {
      result = child->TextFromDescendants(visited,
                                          aria_label_or_description_root, true);
    } else {
      result = RecursiveTextAlternative(*child, aria_label_or_description_root,
                                        visited, child_name_from);
    }

    if (!result.empty() && previous && accumulated_text.length() &&
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
    if (!result.empty())
      last_used_name_from = child_name_from;
  }

  return accumulated_text.ToString();
}

// static
bool AXNodeObject::IsNameFromLabelElement(HTMLElement* control) {
  // This duplicates some logic from TextAlternative()/NativeTextAlternative(),
  // but is necessary because IsNameFromLabelElement() needs to be called from
  // ComputeAccessibilityIsIgnored(), which isn't allowed to call
  // AXObjectCache().GetOrCreate() in random places in the tree.

  if (!control)
    return false;

  // aria-label and aria-labelledby take precedence over <label>.
  if (IsNameFromAriaAttribute(control))
    return false;

  // <label> will be used. It contains the control or points via <label for>.
  // Based on https://www.w3.org/TR/html-aam-1.0
  // 5.1/5.5 Text inputs, Other labelable Elements
  auto* labels = control->labels();
  return labels && labels->length();
}

// static
bool AXNodeObject::IsRedundantLabel(HTMLLabelElement* label) {
  // Determine if label is redundant:
  // - Labelling a checkbox or radio.
  // - Text was already used to name the checkbox/radio.
  // - No interesting content in the label (focusable or semantically useful)
  // TODO(accessibility) Consider moving this logic to the browser side.
  // TODO(accessibility) Consider this for more controls, such as textboxes.
  // There isn't a clear history why this is only done for checkboxes, and not
  // other controls such as textboxes. It may be because the checkbox/radio
  // itself is small, so this makes a nicely sized combined click target. Most
  // ATs do not already have features to combine labels and controls, e.g.
  // removing redundant announcements caused by having text and named controls
  // as separate objects.
  HTMLInputElement* input = DynamicTo<HTMLInputElement>(label->control());
  if (!input)
    return false;

  if (!input->GetLayoutObject() ||
      input->GetLayoutObject()->Style()->Visibility() != EVisibility::kVisible)
    return false;

  if (!input->IsCheckable())
    return false;

  if (!IsNameFromLabelElement(input))
    return false;

  DCHECK_NE(input->labels()->length(), 0U);

  // Look for any first child element that is not the input control itself.
  // This could be important semantically.
  Element* first_child = ElementTraversal::FirstChild(*label);
  if (!first_child)
    return true;  // No element children.

  if (first_child != input)
    return false;  // Has an element child that is not the input control.

  // The first child was the input control.
  // If there's another child, then it won't be the input control.
  return ElementTraversal::NextSibling(*first_child) == nullptr;
}

void AXNodeObject::GetRelativeBounds(AXObject** out_container,
                                     gfx::RectF& out_bounds_in_container,
                                     gfx::Transform& out_container_transform,
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
  out_bounds_in_container = gfx::RectF();
  out_container_transform.MakeIdentity();

  // First check if it has explicit bounds, for example if this element is tied
  // to a canvas path. When explicit coordinates are provided, the ID of the
  // explicit container element that the coordinates are relative to must be
  // provided too.
  if (!explicit_element_rect_.IsEmpty()) {
    *out_container = AXObjectCache().ObjectFromAXID(explicit_container_id_);
    if (*out_container) {
      out_bounds_in_container = gfx::RectF(explicit_element_rect_);
      return;
    }
  }

  Element* element = GetElement();
  // If it's in a canvas but doesn't have an explicit rect, or has display:
  // contents set, get the bounding rect of its children.
  if ((GetNode()->parentElement() &&
       GetNode()->parentElement()->IsInCanvasSubtree()) ||
      (element && element->HasDisplayContentsStyle())) {
    Vector<gfx::RectF> rects;
    for (Node& child : NodeTraversal::ChildrenOf(*GetNode())) {
      if (child.IsHTMLElement()) {
        if (AXObject* obj = AXObjectCache().Get(&child)) {
          AXObject* container;
          gfx::RectF bounds;
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
      for (auto& rect : rects)
        out_bounds_in_container.Union(rect);
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
      if (*out_container) {
        out_bounds_in_container.set_size(
            gfx::SizeF(out_bounds_in_container.width(),
                       std::min(10.0f, out_bounds_in_container.height())));
      }
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
    if (layout_text->GetText().empty())
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
  return static_cast<int>(mapping_units.front().TextContentStart()) + offset;
}

//
// Inline text boxes.
//

void AXNodeObject::LoadInlineTextBoxes() {
  std::queue<AXID> work_queue;
  work_queue.push(AXObjectID());

  while (!work_queue.empty()) {
    AXObject* work_obj = AXObjectCache().ObjectFromAXID(work_queue.front());
    work_queue.pop();
    if (!work_obj || !work_obj->AccessibilityIsIncludedInTree())
      continue;

    if (ui::CanHaveInlineTextBoxChildren(work_obj->RoleValue())) {
      if (work_obj->CachedChildrenIncludingIgnored().empty()) {
        // We only need to add inline textbox children if they aren't present.
        // Although some platforms (e.g. Android), load inline text boxes
        // on subtrees that may later be stale, once they are stale, the old
        // inline text boxes are cleared because SetNeedsToUpdateChildren()
        // calls ClearChildren().
        work_obj->ForceAddInlineTextBoxChildren();
      }
    } else {
      for (const auto& child : work_obj->ChildrenIncludingIgnored())
        work_queue.push(child->AXObjectID());
    }
  }
}

void AXNodeObject::ForceAddInlineTextBoxChildren() {
  AddInlineTextBoxChildren(true /*force*/);
  children_dirty_ = false;  // Avoid adding these children twice.
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
  HTMLMapElement* map = GetMapForImage(GetNode());
  if (!map)
    return;

  HTMLImageElement* curr_image_element = DynamicTo<HTMLImageElement>(GetNode());
  DCHECK(curr_image_element);
  DCHECK(curr_image_element->IsLink());
  String usemap = curr_image_element->FastGetAttribute(html_names::kUsemapAttr);
  DCHECK(!usemap.empty());

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

  // Is this the primary image for this map?
  if (primary_image_element != curr_image_element) {
    // No, the current image (for |this|) is not the primary image.
    // Therefore, do not add area children to it.
    AXObject* ax_primary_image =
        AXObjectCache().GetOrCreate(primary_image_element);
    if (ax_primary_image &&
        ax_primary_image->ChildCountIncludingIgnored() == 0 &&
        NodeTraversal::FirstChild(*map)) {
      // The primary image still needs to add the area children, and there's at
      // least one to add.
      AXObjectCache().ChildrenChanged(primary_image_element);
    }
    return;
  }

  // Yes, this is the primary image.

  // If the children were part of a different parent, notify that parent that
  // its children have changed.
  if (AXObject* ax_previous_parent = AXObjectCache().GetAXImageForMap(*map)) {
    if (ax_previous_parent != this) {
      DCHECK(ax_previous_parent->GetNode());
      AXObjectCache().ChildrenChangedWithCleanLayout(
          ax_previous_parent->GetNode(), ax_previous_parent);
      ax_previous_parent->ClearChildren();
    }
  }

  // Add the children to |this|.
  Node* child = LayoutTreeBuilderTraversal::FirstChild(*map);
  while (child) {
    AddChildAndCheckIncluded(AXObjectCache().GetOrCreate(child, this));
    child = LayoutTreeBuilderTraversal::NextSibling(*child);
  }
}

void AXNodeObject::AddPopupChildren() {
  auto* html_select_element = DynamicTo<HTMLSelectElement>(GetNode());
  if (html_select_element) {
    if (!AXObjectCache().UseAXMenuList() && html_select_element->UsesMenuList())
      AddChildAndCheckIncluded(html_select_element->PopupRootAXObject());
    return;
  }

  auto* html_input_element = DynamicTo<HTMLInputElement>(GetNode());
  if (html_input_element) {
    AddChildAndCheckIncluded(html_input_element->PopupRootAXObject());
    return;
  }
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

  // ---------------------------------------------------------------------------
  // Under certain circumstances the LayoutTreeBuilderTraversal and LayoutObject
  // trees do not match, e.g. for the combination of ::before/::after and
  // ::marker pseudo elements in legacy layout.
  // In this case, there is a danger that the AXObject created for |child| will
  // be added in two places.Unfortunately, this requires a slow check.
  // For more info, see discussion here:
  // https://crrev.com/c/chromium/src/+/3591572/9/third_party/blink/renderer/modules/accessibility/ax_node_object.cc#3973
  // TODO(accessibility) Remove this once legacy layout is completely removed,
  // as this problem will go away.
  AXObject* ax_dom_parent = AXObjectCache().SafeGet(
      LayoutTreeBuilderTraversal::Parent(*child.GetNode()));
  if (ax_dom_parent &&
      !ax_dom_parent->ShouldUseLayoutObjectTraversalForChildren()) {
    DCHECK_NE(ax_dom_parent, this);
    for (Node* child_node =
             LayoutTreeBuilderTraversal::FirstChild(*ax_dom_parent->GetNode());
         child_node;
         child_node = LayoutTreeBuilderTraversal::NextSibling(*child_node)) {
      if (child_node == child.GetNode()) {
        // Different AX parent would have the same AX child (via
        // LayoutTreeBuilderTraversal).
        return false;
      }
    }
  }
  // ---------------------------------------------------------------------------

  return true;
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
  if (!GetLayoutObject()) {
    DCHECK(GetNode());
    DCHECK(GetNode()->IsPseudoElement());
    return;  // Can't add children for hidden or display-locked pseudo elements.
  }
  LayoutObject* child = GetLayoutObject()->SlowFirstChild();
  while (child) {
    if (CanAddLayoutChild(*child)) {
      CHECK_NO_OTHER_PARENT_FOR(child);
      // All added pseudo element desecendants are included in the tree.
      if (AXObject* ax_child = AXObjectCache().GetOrCreate(child, this)) {
        DCHECK(AXObjectCacheImpl::IsRelevantPseudoElementDescendant(*child));
        AddChildAndCheckIncluded(ax_child);
      }
    }
    child = child->NextSibling();
  }
}

void AXNodeObject::AddNodeChildren() {
  if (!node_)
    return;

  // Ignore DOM children of frame/iframe: they do not act as fallbacks and
  // are never part of layout.
  if (IsA<HTMLFrameElementBase>(GetNode()))
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
  AXObjectCache().ValidatedAriaOwnedChildren(this, owned_children);

  DCHECK(owned_children.size() == 0 || AXRelationCache::IsValidOwner(this))
      << "This object is not allowed to use aria-owns, but it is.\n"
      << ToString(true, true);

  // Always include owned children.
  for (const auto& owned_child : owned_children) {
    DCHECK(AXRelationCache::IsValidOwnedChild(owned_child))
        << "This object is not allowed to be owned, but it is.\n"
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
        << "Should not reach AddChildren() if CanHaveChildren() is false.\n"
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
  // If the need to add more children in addition to existing children arises,
  // childrenChanged should have been called, which leads to children_dirty_
  // being true, then UpdateChildrenIfNecessary() clears the children before
  // calling AddChildren().
  DCHECK(children_.empty())
      << "\nParent still has " << children_.size() << " children before adding:"
      << "\nParent is " << ToString(true, true) << "\nFirst child is "
      << children_[0]->ToString(true, true);
#endif

#if defined(AX_FAIL_FAST_BUILD)
  SANITIZER_CHECK(!is_computing_text_from_descendants_)
      << "Should not attempt to simultaneously compute text from descendants "
         "and add children on: "
      << ToString(true, true);
  SANITIZER_CHECK(!is_adding_children_)
      << " Reentering method on " << GetNode();
  base::AutoReset<bool> reentrancy_protector(&is_adding_children_, true);
#endif

  AddChildrenImpl();
  children_dirty_ = false;

#if DCHECK_IS_ON()
  // All added children must be attached.
  for (const auto& child : children_) {
    DCHECK(!child->IsDetached()) << "A brand new child was detached.\n"
                                 << child->ToString(true, true)
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
  DCHECK(!child->IsDetached()) << "Cannot add a detached child.\n"
                               << child->ToString(true, true);

  Node* child_node = child->GetNode();

  // <area> children should only be added via AddImageMapChildren(), as the
  // descendants of an <image usemap> -- never alone or as children of a <map>.
  if (IsA<HTMLAreaElement>(child_node)) {
    AXObject* ancestor = this;
    while (ancestor && !IsA<HTMLImageElement>(ancestor->GetNode()))
      ancestor = ancestor->CachedParentObject();
    DCHECK(ancestor && IsA<HTMLImageElement>(ancestor->GetNode()))
        << "Area elements can only be added by image parents: "
        << child->ToString(true, true) << " had a parent of "
        << ToString(true, true);
  }

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

  DCHECK(!IsA<HTMLFrameElementBase>(GetNode()) ||
         IsA<Document>(child->GetNode()))
      << "Cannot have a non-document child of a frame or iframe."
      << "\nChild: " << child->ToString(true, true)
      << "\nParent: " << child->ParentObject()->ToString(true, true);
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

    // Get the ignored child's children and add to children of ancestor
    // included in tree. This will recurse if necessary, skipping levels of
    // unignored descendants as it goes.
    const auto& children = child->ChildrenIncludingIgnored();
    wtf_size_t length = children.size();
    int new_index = index;
    for (wtf_size_t i = 0; i < length; ++i) {
      if (children[i]->IsDetached()) {
        // TODO(accessibility) Restore to CHECK().
#if defined(AX_FAIL_FAST_BUILD)
        SANITIZER_NOTREACHED()
            << "Cannot add a detached child: "
            << "\n* Child: " << children[i]->ToString(true, true)
            << "\n* Parent: " << child->ToString(true, true)
            << "\n* Grandparent: " << ToString(true, true);
#endif
        continue;
      }
      // If the child was owned, it will be added elsewhere as a direct
      // child of the object owning it.
      if (!AXObjectCache().IsAriaOwned(children[i]))
        children_.insert(new_index++, children[i]);
    }
  } else {
    children_.insert(index, child);
  }
}

bool AXNodeObject::CanHaveChildren() const {
  DCHECK(!IsDetached());
  // Notes:
  // * Native text fields expose any children they might have, complying
  // with browser-side expectations that editable controls have children
  // containing the actual text content.
  // * ARIA roles with childrenPresentational:true in the ARIA spec expose
  // their contents to the browser side, allowing platforms to decide whether
  // to make them a leaf, ensuring that focusable content cannot be hidden,
  // and improving stability in Blink.
  bool result = !GetElement() || CanHaveChildren(*GetElement());
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
    case ax::mojom::blink::Role::kSlider:
    case ax::mojom::blink::Role::kSplitter:
    case ax::mojom::blink::Role::kSwitch:
    case ax::mojom::blink::Role::kTab:
      DCHECK(!result) << "Expected to disallow children for " << GetElement();
      break;
    case ax::mojom::blink::Role::kComboBoxSelect:
    case ax::mojom::blink::Role::kPopUpButton:
    case ax::mojom::blink::Role::kStaticText:
      // Note: these can have AXInlineTextBox children, but when adding them, we
      // also check AXObjectCache().InlineTextBoxAccessibilityEnabled().
      DCHECK(result) << "Expected to allow children for " << GetElement()
                     << " on role " << native_role_;
      break;
    default:
      break;
  }
  return result;
}

// static
bool AXNodeObject::CanHaveChildren(Element& element) {
  DCHECK(!IsA<HTMLMapElement>(element));
  // Placeholder gets exposed as an attribute on the input accessibility node,
  // so there's no need to add its text children. Placeholder text is a separate
  // node that gets removed when it disappears, so this will only be present if
  // the placeholder is visible.
  if (element.ShadowPseudoId() ==
      shadow_element_names::kPseudoInputPlaceholder) {
    return false;
  }

  if (IsA<HTMLBRElement>(element) &&
      (!element.GetLayoutObject() || !element.GetLayoutObject()->IsBR())) {
    // A <br> element that is not treated as a line break could occur when the
    // <br> element has DOM children. A <br> does not usually have DOM children,
    // but there is nothing preventing a script from creating this situation.
    // This anomalous child content is not rendered, and therefore AXObjects
    // should not be created for the children. Enforcing that <br>s to only have
    // children when they are line breaks also helps create consistency: any AX
    // child of a <br> will always be an AXInlineTextBox.
    return false;
  }

  if (IsA<HTMLHRElement>(element))
    return false;

  if (auto* input = DynamicTo<HTMLInputElement>(&element)) {
    // False for checkbox, radio and range.
    return !input->IsCheckable() && input->type() != input_type_names::kRange;
  }

  if (IsA<HTMLOptionElement>(element))
    return false;

  if (IsA<HTMLProgressElement>(element))
    return false;

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
  const AXObject* current = this;

  if (blink::IsA<blink::Document>(current->GetNode()))
    return nullptr;  // Do not expose action element for document.

  // In general, we look an action element up only for AXObjects that have a
  // backing Element. We make an exception for text nodes and pseudo elements
  // because we also want these to expose a default action when any of their
  // ancestors is clickable. We have found Windows ATs relying on this behavior
  // (see https://crbug.com/1382034).
  DCHECK(current->GetElement() || current->IsTextObject() ||
         current->ShouldUseLayoutObjectTraversalForChildren());

  while (current) {
    // Handles clicks or is a textfield and is not a disabled form control.
    if (current->IsClickable()) {
      Element* click_element = current->GetElement();
      DCHECK(click_element) << "Only elements are clickable";
      // Only return if the click element is a DOM ancestor as well, because
      // the click handler won't propagate down via aria-owns.
      if (!GetNode() || click_element->contains(GetNode()))
        return click_element;
      return nullptr;
    }
    current = current->ParentObject();
  }

  return nullptr;
}

Element* AXNodeObject::AnchorElement() const {
  // Search up the DOM tree for an anchor. This can be anything that has the
  // linked state, such as an HTMLAnchorElement or role=link/doc-backlink.
  const AXObject* current = this;
  while (current) {
    if (current->IsLink()) {
      DCHECK(current->GetElement())
          << "An AXObject* that is a link should always have an element.";
      return current->GetElement();
    }
    current = current->ParentObject();
  }

  return nullptr;
}

Document* AXNodeObject::GetDocument() const {
  if (!GetNode())
    return nullptr;
  return &GetNode()->GetDocument();
}

Node* AXNodeObject::GetNode() const {
  if (IsDetached()) {
    DCHECK(!node_);
    return nullptr;
  }

  DCHECK(!GetLayoutObject() || GetLayoutObject()->GetNode() == node_)
      << "If there is an associated layout object, its node should match the "
         "associated node of this accessibility object.\n"
      << ToString(true, true);
  return node_;
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
    if (!languages.empty())
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

bool AXNodeObject::OnNativeFocusAction() {
  // Checking if node is focusable in a native focus action requires that we
  // have updated style and layout tree, since the focus check relies on the
  // existence of layout objects to determine the result. However, these layout
  // objects may have been deferred by display-locking.
  Document* document = GetDocument();
  Node* node = GetNode();
  if (!document || !node)
    return false;

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

  if (base::FeatureList::IsEnabled(blink::features::kSimulateClickOnAXFocus)) {
    // If the object is not natively focusable but can be focused using an ARIA
    // active descendant, perform a native click instead. This will enable Web
    // apps that set accessibility focus using an active descendant to capture
    // and act on the click event. Otherwise, there is no other way to inform
    // the app that an AT has requested the focus to be changed, except if the
    // app is using AOM. To be extra safe, exclude objects that are clickable
    // themselves. This won't prevent anyone from having a click handler on the
    // object's container.
    //
    // This code is in the process of being removed. See the comment above
    // |kSimulateClickOnAXFocus| in `blink/common/features.cc`.
    if (!IsClickable()) {
      return OnNativeClickAction();
    }
  }

  element->Focus();

  // Calling NotifyUserActivation here allows the browser to activate features
  // that need user activation, such as showing an autofill suggestion.
  LocalFrame::NotifyUserActivation(
      document->GetFrame(),
      mojom::blink::UserActivationNotificationType::kInteraction);

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

void AXNodeObject::ChildrenChangedWithCleanLayout() {
  DCHECK(!IsDetached()) << "Don't call on detached node: "
                        << ToString(true, true);
  DCHECK(GetNode() || GetLayoutObject());

  // When children changed on a <map> that means we need to forward the
  // children changed to the <img> that parents the <area> elements.
  // TODO(accessibility) Consider treating <img usemap> as aria-owns so that
  // we get implementation "for free" vai relation cache, etc.
  if (HTMLMapElement* map_element = DynamicTo<HTMLMapElement>(GetNode())) {
    HTMLImageElement* image_element = map_element->ImageElement();
    if (image_element) {
      AXObject* ax_image = AXObjectCache().Get(image_element);
      if (ax_image) {
        ax_image->ChildrenChangedWithCleanLayout();
        return;
      }
    }
  }

  // Always invalidate |children_| even if it was invalidated before, because
  // now layout is clean.
  SetNeedsToUpdateChildren();

  // The caller, AXObjectCacheImpl::ChildrenChangedWithCleanLayout(), is only
  // Between the time that AXObjectCacheImpl::ChildrenChanged() determines
  // which included parent to use and now, it's possible that the parent will
  // no longer be ignored. This is rare, but is covered by this test:
  // external/wpt/accessibility/crashtests/delayed-ignored-change.html/
  //
  // If this object is no longer included in the tree, then our parent needs to
  // recompute its included-in-the-tree children vector. (And if our parent
  // isn't included in the tree either, it will recursively update its parent
  // and so on.)
  //
  // The first ancestor that's included in the tree will
  // be the one that actually fires the ChildrenChanged
  // event notification.
  if (!LastKnownIsIncludedInTreeValue()) {
    if (AXObject* ax_parent = CachedParentObject()) {
      ax_parent->ChildrenChangedWithCleanLayout();
      return;
    }
  }

  // TODO(accessibility) Move this up.
  if (!CanHaveChildren())
    return;

  DCHECK(!IsDetached()) << "None of the above should be able to detach |this|: "
                        << ToString(true, true);

  AXObjectCache().MarkAXObjectDirtyWithCleanLayout(this);
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

//
// Notifications that this object may have changed.
//

void AXNodeObject::HandleAriaExpandedChanged() {
  // Find if a parent of this object should handle aria-expanded changes.
  AXObject* container_parent = ParentObject();
  while (container_parent) {
    bool found_parent = false;

    switch (container_parent->RoleValue()) {
      case ax::mojom::blink::Role::kLayoutTable:
      case ax::mojom::blink::Role::kTree:
      case ax::mojom::blink::Role::kTreeGrid:
      case ax::mojom::blink::Role::kGrid:
      case ax::mojom::blink::Role::kTable:
        found_parent = true;
        break;
      default:
        break;
    }

    if (found_parent)
      break;

    container_parent = container_parent->ParentObject();
  }

  // Post that the row count changed.
  if (container_parent) {
    AXObjectCache().PostNotification(container_parent,
                                     ax::mojom::blink::Event::kRowCountChanged);
  }

  // Post that the specific row either collapsed or expanded.
  AccessibilityExpanded expanded = IsExpanded();
  if (!expanded)
    return;

  if (RoleValue() == ax::mojom::blink::Role::kRow ||
      RoleValue() == ax::mojom::blink::Role::kTreeItem) {
    ax::mojom::blink::Event notification =
        ax::mojom::blink::Event::kRowExpanded;
    if (expanded == kExpandedCollapsed)
      notification = ax::mojom::blink::Event::kRowCollapsed;

    AXObjectCache().PostNotification(this, notification);
  } else {
    AXObjectCache().PostNotification(this,
                                     ax::mojom::blink::Event::kExpandedChanged);
  }
}

void AXNodeObject::HandleActiveDescendantChanged() {
  if (!GetLayoutObject() || !GetNode() || !GetDocument())
    return;

  Node* focused_node = GetDocument()->FocusedElement();
  if (focused_node == GetNode()) {
    AXObject* active_descendant = ActiveDescendant();
    if (active_descendant && active_descendant->IsSelectedFromFocus()) {
      // In single selection containers, selection follows focus, so a selection
      // changed event must be fired. This ensures the AT is notified that the
      // selected state has changed, so that it does not read "unselected" as
      // the user navigates through the items.
      AXObjectCache().HandleAriaSelectedChangedWithCleanLayout(
          active_descendant->GetNode());
    }

    // Mark this node dirty. AXEventGenerator will automatically infer
    // that the active descendant changed.
    AXObjectCache().MarkAXObjectDirtyWithCleanLayout(this);
  }
}

AXObject* AXNodeObject::ErrorMessage() const {
  if (GetInvalidState() == ax::mojom::blink::InvalidState::kFalse)
    return nullptr;

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

String AXNodeObject::TextAlternativeFromTitleAttribute(
    const AtomicString& title,
    ax::mojom::blink::NameFrom& name_from,
    NameSources* name_sources,
    bool* found_text_alternative) const {
  String text_alternative;
  if (name_sources) {
    name_sources->push_back(NameSource(*found_text_alternative, kTitleAttr));
    name_sources->back().type = name_from;
  }
  name_from = ax::mojom::blink::NameFrom::kTitle;
  if (!title.IsNull()) {
    text_alternative = title;
    if (name_sources) {
      NameSource& source = name_sources->back();
      source.attribute_value = title;
      source.attribute_value = title;
      source.text = text_alternative;
      *found_text_alternative = true;
    }
  }
  return text_alternative;
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
          if (!label->FastGetAttribute(html_names::kForAttr).empty() &&
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
    String value = input_element->Value();
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
    const bool is_empty = alt.empty() && !alt.IsNull();
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
    String value = input_element->Value();
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
    const AtomicString& title = input_element->getAttribute(kTitleAttr);
    String titleText = text_alternative = TextAlternativeFromTitleAttribute(
        title, name_from, name_sources, found_text_alternative);
    if (!titleText.IsNull()) {
      if (name_sources) {
        text_alternative = titleText;
      } else {
        return titleText;
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
    // title attr
    name_from = ax::mojom::blink::NameFrom::kAttribute;
    const AtomicString& title = html_element->getAttribute(kTitleAttr);
    String titleText = text_alternative = TextAlternativeFromTitleAttribute(
        title, name_from, name_sources, found_text_alternative);
    if (!titleText.IsNull()) {
      if (name_sources) {
        text_alternative = titleText;
      } else {
        return titleText;
      }
    }

    name_from = ax::mojom::blink::NameFrom::kPlaceholder;
    if (name_sources) {
      name_sources->push_back(
          NameSource(*found_text_alternative, html_names::kPlaceholderAttr));
      NameSource& source = name_sources->back();
      source.type = name_from;
    }
    const String placeholder = PlaceholderFromNativeAttribute();
    if (!placeholder.empty()) {
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
    String upload_button_text = input_element->UploadButton()->Value();
    if (!displayed_file_path.empty()) {
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
  if (IsTextField()) {
    name_from = ax::mojom::blink::NameFrom::kPlaceholder;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative,
                                         html_names::kAriaPlaceholderAttr));
      NameSource& source = name_sources->back();
      source.type = name_from;
    }
    const AtomicString& aria_placeholder =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kPlaceholder);
    if (!aria_placeholder.empty()) {
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

  // 5.8 img or area Element
  if (IsA<HTMLImageElement>(GetNode()) || IsA<HTMLAreaElement>(GetNode()) ||
      (GetLayoutObject() && GetLayoutObject()->IsSVGImage())) {
    // alt
    const AtomicString& alt = GetAttribute(kAltAttr);
    const bool is_empty = alt.empty() && !alt.IsNull();
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
            RecursiveTextAlternative(*caption_ax_object, nullptr, visited);
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
      name_sources->back().native_source = kAXTextFromNativeTitleElement;
    }
    auto* container_node = To<ContainerNode>(GetNode());
    Element* title = ElementTraversal::FirstChild(
        *container_node, HasTagName(svg_names::kTitleTag));

    if (title) {
      // TODO(accessibility): In most cases <desc> and <title> can participate
      // in the recursive text alternative calculation. However when the <desc>
      // or <title> is the child of a <use>, |AXObjectCache::GetOrCreate| will
      // fail when |AXObject::ComputeNonARIAParent| returns null because the
      // <use> element's subtree isn't visited by LayoutTreeBuilderTraversal. In
      // addition, while aria-label and other text alternative sources are are
      // technically valid on SVG <desc> and <title>, it is not clear if user
      // agents must expose their values. Therefore until we hear otherwise,
      // just use the inner text. See https://github.com/w3c/svgwg/issues/867
      text_alternative = title->GetInnerTextWithoutUpdate();
      if (!text_alternative.empty()) {
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
    // The SVG-AAM says that the xlink:title participates as a name source
    // for links.
    if (IsA<SVGAElement>(GetNode())) {
      name_from = ax::mojom::blink::NameFrom::kAttribute;
      if (name_sources) {
        name_sources->push_back(
            NameSource(*found_text_alternative, xlink_names::kTitleAttr));
        name_sources->back().type = name_from;
      }

      const AtomicString& title_attr =
          DynamicTo<Element>(GetNode())->FastGetAttribute(
              xlink_names::kTitleAttr);
      if (!title_attr.empty()) {
        text_alternative = title_attr;
        if (name_sources) {
          NameSource& source = name_sources->back();
          source.text = text_alternative;
          source.attribute_value = title_attr;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
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
            RecursiveTextAlternative(*legend_ax_object, nullptr, visited);

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
    Document* document = GetDocument();
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
        if (!aria_label.empty()) {
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
          text_alternative.empty() && document->TitleElement();
      if (is_empty_title_element)
        name_from = ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty;
      else
        name_from = ax::mojom::blink::NameFrom::kRelatedElement;

      if (name_sources) {
        name_sources->push_back(NameSource(*found_text_alternative));
        NameSource& source = name_sources->back();
        source.type = name_from;
        source.native_source = kAXTextFromNativeTitleElement;
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
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

  result = result.SimplifyWhiteSpace(IsHTMLSpace<UChar>);

  if (RoleValue() == ax::mojom::blink::Role::kSpinButton &&
      DatetimeAncestor()) {
    // Fields inside a datetime control need to merge the field description
    // with the description of the <input> element.
    const AXObject* datetime_ancestor = DatetimeAncestor();
    ax::mojom::blink::NameFrom datetime_ancestor_name_from;
    datetime_ancestor->GetName(datetime_ancestor_name_from, nullptr);
    if (description_objects)
      description_objects->clear();
    String ancestor_description = DatetimeAncestor()->Description(
        datetime_ancestor_name_from, description_from, description_objects);
    if (!result.empty() && !ancestor_description.empty())
      return result + " " + ancestor_description;
    if (!ancestor_description.empty())
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
  if (ElementsFromAttribute(element, elements_from_attribute,
                            html_names::kAriaDescribedbyAttr, ids)) {
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

    for (auto& member_element : elements_from_attribute)
      ids.push_back(member_element->GetIdAttribute());

    TokenVectorFromAttribute(element, ids, html_names::kAriaDescribedbyAttr);
    AXObjectCache().UpdateReverseTextRelations(this, ids);

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
    description_from = ax::mojom::blink::DescriptionFrom::kAriaDescription;
    description = aria_desc;
    if (description_sources) {
      found_description = true;
      description_sources->back().text = description;
    } else {
      return description;
    }
  }

  // SVG-AAM specifies additional description sources when ARIA sources have not
  // been found. https://w3c.github.io/svg-aam/#mapping_additional_nd
  if (IsA<SVGElement>(GetNode())) {
    return SVGDescription(name_from, description_from, description_sources,
                          related_objects);
  }

  const auto* input_element = DynamicTo<HTMLInputElement>(GetNode());

  // value, 5.2.2 from: https://www.w3.org/TR/html-aam-1.0/
  if (name_from != ax::mojom::blink::NameFrom::kValue && input_element &&
      input_element->IsTextButton()) {
    description_from = ax::mojom::blink::DescriptionFrom::kButtonLabel;
    if (description_sources) {
      description_sources->push_back(
          DescriptionSource(found_description, kValueAttr));
      description_sources->back().type = description_from;
    }
    String value = input_element->Value();
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
    description_from = ax::mojom::blink::DescriptionFrom::kRubyAnnotation;
    if (description_sources) {
      description_sources->push_back(DescriptionSource(found_description));
      description_sources->back().type = description_from;
      description_sources->back().native_source =
          kAXTextFromNativeHTMLRubyAnnotation;
    }
    AXObject* ruby_annotation_ax_object = nullptr;
    for (const auto& child : children_) {
      if (child->RoleValue() == ax::mojom::blink::Role::kRubyAnnotation &&
          child->GetNode() && IsA<HTMLRTElement>(child->GetNode())) {
        ruby_annotation_ax_object = child;
        break;
      }
    }
    if (ruby_annotation_ax_object) {
      AXObjectSet visited;
      description =
          RecursiveTextAlternative(*ruby_annotation_ax_object, this, visited);
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

  // table caption, 5.9.2 from: https://www.w3.org/TR/html-aam-1.0/
  auto* table_element = DynamicTo<HTMLTableElement>(element);
  if (name_from != ax::mojom::blink::NameFrom::kCaption && table_element) {
    description_from = ax::mojom::blink::DescriptionFrom::kTableCaption;
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
            RecursiveTextAlternative(*caption_ax_object, nullptr, visited);
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

  // summary, 5.8.2 from: https://www.w3.org/TR/html-aam-1.0/
  if (name_from != ax::mojom::blink::NameFrom::kContents &&
      IsA<HTMLSummaryElement>(GetNode())) {
    description_from = ax::mojom::blink::DescriptionFrom::kSummary;
    if (description_sources) {
      description_sources->push_back(DescriptionSource(found_description));
      description_sources->back().type = description_from;
    }

    AXObjectSet visited;
    description = TextFromDescendants(visited, nullptr, false);

    if (!description.empty()) {
      if (description_sources) {
        found_description = true;
        description_sources->back().text = description;
      } else {
        return description;
      }
    }
  }

  // title attribute, from: https://www.w3.org/TR/html-aam-1.0/
  if (name_from != ax::mojom::blink::NameFrom::kTitle) {
    description_from = ax::mojom::blink::DescriptionFrom::kTitle;
    if (description_sources) {
      description_sources->push_back(
          DescriptionSource(found_description, kTitleAttr));
      description_sources->back().type = description_from;
    }
    const AtomicString& title = GetAttribute(kTitleAttr);
    if (!title.empty()) {
      description = title;
      if (description_sources) {
        found_description = true;
        description_sources->back().text = description;
      } else {
        return description;
      }
    }
  }

  description_from = ax::mojom::blink::DescriptionFrom::kNone;

  if (found_description) {
    DCHECK(description_sources)
        << "Should only reach here if description_sources are tracked";
    // Use the first non-null description.
    // TODO(accessibility) Why do we need to check superceded if that will
    // always be the first one?
    for (DescriptionSource& description_source : *description_sources) {
      if (!description_source.text.IsNull() && !description_source.superseded) {
        description_from = description_source.type;
        if (!description_source.related_objects.empty())
          *related_objects = description_source.related_objects;
        return description_source.text;
      }
    }
  }

  return String();
}

String AXNodeObject::SVGDescription(
    ax::mojom::blink::NameFrom name_from,
    ax::mojom::blink::DescriptionFrom& description_from,
    DescriptionSources* description_sources,
    AXRelatedObjectVector* related_objects) const {
  DCHECK(IsA<SVGElement>(GetNode()));
  String description;
  bool found_description = false;
  Element* element = GetElement();

  description_from = ax::mojom::blink::DescriptionFrom::kSvgDescElement;
  if (description_sources) {
    description_sources->push_back(DescriptionSource(found_description));
    description_sources->back().type = description_from;
    description_sources->back().native_source = kAXTextFromNativeSVGDescElement;
  }
  if (Element* desc = ElementTraversal::FirstChild(
          *element, HasTagName(svg_names::kDescTag))) {
    // TODO(accessibility): In most cases <desc> and <title> can participate in
    // the recursive text alternative calculation. However when the <desc> or
    // <title> is the child of a <use>, |AXObjectCache::GetOrCreate| will fail
    // when |AXObject::ComputeNonARIAParent| returns null because the <use>
    // element's subtree isn't visited by LayoutTreeBuilderTraversal. In
    // addition, while aria-label and other text alternative sources are are
    // technically valid on SVG <desc> and <title>, it is not clear if user
    // agents must expose their values. Therefore until we hear otherwise, just
    // use the inner text. See https://github.com/w3c/svgwg/issues/867
    description = desc->GetInnerTextWithoutUpdate();
    if (!description.empty()) {
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

  // If we haven't found a description source yet and the title is present,
  // SVG-AAM states to use the <title> if ARIA label attributes are used to
  // provide the accessible name.
  if (IsNameFromAriaAttribute(element)) {
    description_from = ax::mojom::blink::DescriptionFrom::kTitle;
    if (description_sources) {
      description_sources->push_back(DescriptionSource(found_description));
      description_sources->back().type = description_from;
      description_sources->back().native_source = kAXTextFromNativeTitleElement;
    }
    if (Element* title = ElementTraversal::FirstChild(
            *element, HasTagName(svg_names::kTitleTag))) {
      // TODO(accessibility): In most cases <desc> and <title> can participate
      // in the recursive text alternative calculation. However when the <desc>
      // or <title> is the child of a <use>, |AXObjectCache::GetOrCreate| will
      // fail when |AXObject::ComputeNonARIAParent| returns null because the
      // <use> element's subtree isn't visited by LayoutTreeBuilderTraversal. In
      // addition, while aria-label and other text alternative sources are are
      // technically valid on SVG <desc> and <title>, it is not clear if user
      // agents must expose their values. Therefore until we hear otherwise,
      // just use the inner text. See https://github.com/w3c/svgwg/issues/867
      description = title->GetInnerTextWithoutUpdate();
      if (!description.empty()) {
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

  // In the case of an SVG <a>, the last description source is the xlink:title
  // attribute, if it didn't serve as the name source.
  if (IsA<SVGAElement>(GetNode()) &&
      name_from != ax::mojom::blink::NameFrom::kAttribute) {
    description_from = ax::mojom::blink::DescriptionFrom::kTitle;
    const AtomicString& title_attr =
        DynamicTo<Element>(GetNode())->FastGetAttribute(
            xlink_names::kTitleAttr);
    if (!title_attr.empty()) {
      description = title_attr;
      if (description_sources) {
        found_description = true;
        description_sources->back().text = description;
      } else {
        return description;
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
  if (!native_placeholder.empty())
    return native_placeholder;

  const AtomicString& aria_placeholder =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kPlaceholder);
  if (!aria_placeholder.empty())
    return aria_placeholder;

  return String();
}

String AXNodeObject::Title(ax::mojom::blink::NameFrom name_from) const {
  if (name_from == ax::mojom::blink::NameFrom::kTitle)
    return String();  // Already exposed the title in the name field.

  return GetTitle(GetElement());
}

String AXNodeObject::PlaceholderFromNativeAttribute() const {
  Node* node = GetNode();
  if (!node || !blink::IsTextControl(*node))
    return String();
  return ToTextControl(node)->StrippedPlaceholder();
}

String AXNodeObject::GetValueContributionToName(AXObjectSet& visited) const {
  if (IsTextField())
    return SlowGetValueForControlIncludingContentEditable();

  if (IsRangeValueSupported()) {
    const AtomicString& aria_valuetext =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kValueText);
    if (!aria_valuetext.IsNull())
      return aria_valuetext.GetString();
    float value;
    if (ValueForRange(&value))
      return String::Number(value);
  }

  // "If the embedded control has role combobox or listbox, return the text
  // alternative of the chosen option."
  if (UseNameFromSelectedOption()) {
    StringBuilder accumulated_text;
    AXObjectVector selected_options;
    SelectedOptions(selected_options);
    for (const auto& child : selected_options) {
      if (visited.insert(child).is_new_entry) {
        if (accumulated_text.length())
          accumulated_text.Append(" ");
        accumulated_text.Append(child->ComputedName());
      }
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
    case ax::mojom::blink::Role::kComboBoxSelect:
    case ax::mojom::blink::Role::kListBox:
      return true;
    default:
      return false;
  }
}

void AXNodeObject::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  AXObject::Trace(visitor);
}

}  // namespace blink
