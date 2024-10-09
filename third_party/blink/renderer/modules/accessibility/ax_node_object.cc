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
#include <array>
#include <memory>
#include <optional>
#include <queue>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
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
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_output_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/forms/radio_input_type.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_directory_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_dlist_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/html_menu_element.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_paragraph_element.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_summary_element.h"
#include "third_party/blink/renderer/core/html/html_table_caption_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_col_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/html_time_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/inline/abstract_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/svg_a_element.h"
#include "third_party/blink/renderer/core/svg/svg_desc_element.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/core/svg/svg_title_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"
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
#include "third_party/skia/include/core/SkImage.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {
namespace {

bool ShouldUseLayoutNG(const blink::LayoutObject& layout_object) {
  return layout_object.IsInline() &&
         layout_object.IsInLayoutNGInlineFormattingContext();
}

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

bool IsLinkable(const blink::AXObject& object) {
  if (!object.GetLayoutObject()) {
    return false;
  }

  // See https://wiki.mozilla.org/Accessibility/AT-Windows-API for the elements
  // Mozilla considers linkable.
  return object.IsLink() || object.IsImage() ||
         object.GetLayoutObject()->IsText();
}

bool IsImageOrAltText(blink::LayoutObject* layout_object, blink::Node* node) {
  DCHECK(layout_object);
  if (layout_object->IsImage()) {
    return true;
  }
  if (IsA<blink::HTMLImageElement>(node)) {
    return true;
  }
  auto* html_input_element = DynamicTo<blink::HTMLInputElement>(node);
  return html_input_element && html_input_element->HasFallbackContent();
}

bool ShouldIgnoreListItem(blink::Node* node) {
  DCHECK(node);

  // http://www.w3.org/TR/wai-aria/complete#presentation
  // A list item is presentational if its parent is a native list but
  // it has an explicit ARIA role set on it that's anything other than "list".
  blink::Element* parent = blink::FlatTreeTraversal::ParentElement(*node);
  if (!parent) {
    return false;
  }

  if (IsA<blink::HTMLMenuElement>(*parent) ||
      IsA<blink::HTMLUListElement>(*parent) ||
      IsA<blink::HTMLOListElement>(*parent)) {
    AtomicString role =
        blink::AXObject::AriaAttribute(*parent, blink::html_names::kRoleAttr);
    if (!role.empty() && role != "list" && role != "directory") {
      return true;
    }
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

    blink::AXObject* ax_object = cache.Get(node);
    if (ax_object && !IsNeutralWithinTable(ax_object))
      return ax_object;
  }
}

// Return the first LayoutTableSection if maybe_table is a non-anonymous
// table. If non-null, set table_out to the containing table.
blink::LayoutTableSection* FirstTableSection(
    blink::LayoutObject* maybe_table,
    blink::LayoutTable** table_out = nullptr) {
  if (auto* table = DynamicTo<blink::LayoutTable>(maybe_table)) {
    if (table->GetNode()) {
      if (table_out) {
        *table_out = table;
      }
      return table->FirstSection();
    }
  }
  if (table_out) {
    *table_out = nullptr;
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

  NOTREACHED_IN_MIGRATION();
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

bool CanHaveInlineTextBoxChildren(const blink::AXObject* obj) {
  if (!ui::CanHaveInlineTextBoxChildren(obj->RoleValue())) {
    return false;
  }

  // Requires a layout object for there to be any inline text boxes.
  if (!obj->GetLayoutObject()) {
    return false;
  }

  // Inline text boxes are included if and only if the parent is unignored.
  // If the parent is ignored but included in tree, the inline textbox is
  // still withheld.
  return !obj->IsIgnored();
}

bool HasLayoutText(const blink::AXObject* obj) {
  // This method should only be used when layout is clean.
#if DCHECK_IS_ON()
  DCHECK(obj->GetDocument()->Lifecycle().GetState() >=
         blink::DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle "
      << obj->GetDocument()->Lifecycle().ToString();
#endif

  // If no layout object, could be display:none or display locked.
  if (!obj->GetLayoutObject()) {
    return false;
  }

  if (blink::DisplayLockUtilities::LockedAncestorPreventingPaint(
          *obj->GetLayoutObject())) {
    return false;
  }

  // Only text has inline textbox children.
  if (!obj->GetLayoutObject()->IsText()) {
    return false;
  }

  // TODO(accessibility): Unclear why text would need layout if it's not display
  // locked and the document is currently in a clean layout state.
  // It seems to be fairly rare, but is creating some crashes, and there is
  // no repro case yet.
  if (obj->GetLayoutObject()->NeedsLayout()) {
    DCHECK(false) << "LayoutText needed layout but was not display locked: "
                  << obj;
    return false;
  }

  return true;
}

// TODO(crbug.com/371011661): Use single list marker representation for a11y.
// Accessibility is treating list markers in two different ways:
// 1. As a regular list marker object;
// 2. As a object of type none, thus ignoring the list marker, and adding the
// text as its child. Regardless of the way being used for a particular list
// item, we need to know how to connect the list marker with the next text on
// the line. `layout_object`represents the node being investigated, and `parent`
// may contain the parent of this object, if it is included in the tree.
const LayoutObject* GetListMarker(const LayoutObject& layout_object,
                                  const AXObject* parent) {
  if (layout_object.IsLayoutOutsideListMarker()) {
    // This  is the default case: this LayoutObject represents a list marker.
    return &layout_object;
  }
  if (parent && parent->RoleValue() == ax::mojom::blink::Role::kNone &&
      parent->GetLayoutObject() &&
      parent->GetLayoutObject()->IsLayoutOutsideListMarker()) {
    // The parent of the node being investigated is a list marker, so it will be
    // used in the computation to connect things in the same line.
    return parent->GetLayoutObject();
  }
  return nullptr;
}

}  // namespace

using html_names::kAltAttr;
using html_names::kTitleAttr;
using html_names::kTypeAttr;
using html_names::kValueAttr;
using mojom::blink::FormControlType;

// In ARIA 1.1, default value of aria-level was changed to 2.
const int kDefaultHeadingLevel = 2;

// When an AXNodeObject is created with a Node instead of a LayoutObject it
// means that the LayoutObject is purposely being set to null, as it is not
// relevant for this object in the AX tree.
AXNodeObject::AXNodeObject(Node* node, AXObjectCacheImpl& ax_object_cache)
    : AXObject(ax_object_cache),
      node_(node) {}

AXNodeObject::AXNodeObject(LayoutObject* layout_object,
                           AXObjectCacheImpl& ax_object_cache)
    : AXObject(ax_object_cache),
      node_(layout_object->GetNode()),
      layout_object_(layout_object) {
#if DCHECK_IS_ON()
  layout_object_->SetHasAXObject(true);
#endif
}

AXNodeObject::~AXNodeObject() {
  DCHECK(!node_);
  DCHECK(!layout_object_);
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
              [](Node* node, KeyboardEvent* evt) {
                if (node) {
                  node->DispatchEvent(*evt);
                }
              },
              WrapWeakPersistent(GetNode()), WrapPersistent(keyup)),
          base::Milliseconds(100));
}

AXObject* AXNodeObject::ActiveDescendant() const {
  Element* element = GetElement();
  if (!element)
    return nullptr;

  if (RoleValue() == ax::mojom::blink::Role::kMenuListPopup) {
    if (HTMLSelectElement* select =
            DynamicTo<HTMLSelectElement>(parent_->GetNode())) {
      // TODO(accessibility): as a simplification, just expose the active
      // descendant of a <select size=1> at all times, like we do for other
      // active descendant situations,
      return select->PopupIsVisible() || select->IsFocusedElementInDocument()
                 ? AXObjectCache().Get(select->OptionToBeShown())
                 : nullptr;
    }
  }

  if (auto* select = DynamicTo<HTMLSelectElement>(GetNode())) {
    if (!select->UsesMenuList()) {
      return AXObjectCache().Get(select->ActiveSelectionEnd());
    }
  }

  Element* descendant =
      GetAOMPropertyOrARIAAttribute(AOMRelationProperty::kActiveDescendant);
  if (!descendant)
    return nullptr;

  AXObject* ax_descendant = AXObjectCache().Get(descendant);
  return ax_descendant && ax_descendant->IsVisible() ? ax_descendant : nullptr;
}

bool IsExemptFromInlineBlockCheck(ax::mojom::blink::Role role) {
  return role == ax::mojom::blink::Role::kSvgRoot ||
         role == ax::mojom::blink::Role::kCanvas ||
         role == ax::mojom::blink::Role::kEmbeddedObject;
}

AXObjectInclusion AXNodeObject::ShouldIncludeBasedOnSemantics(
    IgnoredReasons* ignored_reasons) const {
  DCHECK(GetDocument());

  // All nodes must have an unignored parent within their tree under
  // the root node of the web area, so force that node to always be unignored.
  if (IsA<Document>(GetNode())) {
    return kIncludeObject;
  }

  if (IsPresentational()) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXPresentational));
    return kIgnoreObject;
  }

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
  if (!element) {
    return kDefaultBehavior;
  }

  if (IsExcludedByFormControlsFilter()) {
    if (ignored_reasons) {
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    }
    return kIgnoreObject;
  }

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

    // Include non-empty SVG root as clients may want to treat it as an image.
    if (IsA<SVGSVGElement>(node) && GetLayoutObject() &&
        GetLayoutObject()->IsSVGRoot() && element->firstElementChild()) {
      return kIncludeObject;
    }

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

    // If setting enabled, do not ignore SVG grouping (<g>) elements.
    if (IsA<SVGGElement>(node)) {
      Settings* settings = GetDocument()->GetSettings();
      if (settings->GetAccessibilityIncludeSvgGElement()) {
        return kIncludeObject;
      }
    }

    // If we return kDefaultBehavior here, the logic related to inclusion of
    // clickable objects, links, controls, etc. will not be reached. We handle
    // SVG elements early to ensure properties in a <symbol> subtree do not
    // result in inclusion.
  }

  if (IsTableLikeRole() || IsTableRowLikeRole() || IsTableCellLikeRole() ||
      element->HasTagName(html_names::kTheadTag) ||
      element->HasTagName(html_names::kTfootTag)) {
    return kIncludeObject;
  }

  if (IsA<HTMLHtmlElement>(node)) {
    if (ignored_reasons) {
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    }
    return kIgnoreObject;
  }

  // All focusable elements except the <body> and <html> are included.
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

  if (IsA<HTMLOptGroupElement>(node)) {
    return kIncludeObject;
  }

  // Anything with an explicit ARIA role should be included.
  if (RawAriaRole() != ax::mojom::blink::Role::kUnknown) {
    return kIncludeObject;
  }

  // Anything with CSS alt should be included.
  // Descendants are pruned: IsRelevantPseudoElementDescendant() returns false.
  std::optional<String> alt_text = GetCSSAltText(GetElement());
  if (alt_text && !alt_text->empty())
    return kIncludeObject;

  // Anything that is an editable root should not be ignored. However, one
  // cannot just call `AXObject::IsEditable()` since that will include the
  // contents of an editable region too. Only the editable root should always be
  // exposed.
  if (IsEditableRoot())
    return kIncludeObject;

  // Don't ignored legends, because JAWS uses them to determine redundant text.
  if (IsA<HTMLLegendElement>(node)) {
    if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
      // When a <legend> is used inside an <optgroup>, it is used to set the
      // name of the <optgroup> and shouldn't be redundantly repeated.
      for (auto* ancestor = node->parentNode(); ancestor;
           ancestor = ancestor->parentNode()) {
        if (IsA<HTMLOptGroupElement>(ancestor)) {
          return kIgnoreObject;
        }
      }
    }
    return kIncludeObject;
  }

  static constexpr auto always_included_computed_roles =
      base::MakeFixedFlatSet<ax::mojom::blink::Role>({
          ax::mojom::blink::Role::kAbbr,
          ax::mojom::blink::Role::kApplication,
          ax::mojom::blink::Role::kArticle,
          ax::mojom::blink::Role::kAudio,
          ax::mojom::blink::Role::kBanner,
          ax::mojom::blink::Role::kBlockquote,
          ax::mojom::blink::Role::kCode,
          ax::mojom::blink::Role::kComplementary,
          ax::mojom::blink::Role::kContentDeletion,
          ax::mojom::blink::Role::kContentInfo,
          ax::mojom::blink::Role::kContentInsertion,
          ax::mojom::blink::Role::kDefinition,
          ax::mojom::blink::Role::kDescriptionList,
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
          ax::mojom::blink::Role::kEmphasis,
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
          // Don't ignore MathML nodes by default, since MathML
          // relies on child positions to determine semantics
          // (e.g. numerator is the first child of a fraction).
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
          ax::mojom::blink::Role::kMenuListOption,
          ax::mojom::blink::Role::kMenuListPopup,
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
          ax::mojom::blink::Role::kStrong,
          ax::mojom::blink::Role::kTerm,
          ax::mojom::blink::Role::kTime,
          ax::mojom::blink::Role::kVideo,
      });

  if (base::Contains(always_included_computed_roles, RoleValue())) {
    return kIncludeObject;
  }

  // An <hgroup> element has the "group" aria role.
  if (GetNode()->HasTagName(html_names::kHgroupTag)) {
    return kIncludeObject;
  }

  // Using the title or accessibility description (so we
  // check if there's some kind of accessible name for the element)
  // to decide an element's visibility is not as definitive as
  // previous checks, so this should remain as one of the last.
  if (HasAriaAttribute() ||
      !GetElement()->FastGetAttribute(kTitleAttr).empty()) {
    return kIncludeObject;
  }

  if (IsImage() && !IsA<SVGElement>(node)) {
    String alt = GetElement()->FastGetAttribute(kAltAttr);
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

  if (AXObjectCache().GetAXMode().has_mode(ui::AXMode::kInlineTextBoxes)) {
    // We are including inline block elements since we might rely on these for
    // NextOnLine/PreviousOnLine computations.
    //
    // If we have an element with inline
    // block specified, we should include. There are some roles where we
    // shouldn't include even if inline block, or we'll get test failures.
    //
    // We also only want to include in the tree if the inline block element has
    // siblings.
    // Otherwise we will include nodes that we don't need for anything.
    // Consider a structure where we have a subtree of 12 layers, where each
    // layer has an inline-block node with a single child that points to the
    // next layer. All nodes have a single child, meaning that this child has no
    // siblings.
    if (!IsExemptFromInlineBlockCheck(native_role_) && GetLayoutObject() &&
        GetLayoutObject()->IsInline() &&
        GetLayoutObject()->IsAtomicInlineLevel() &&
        node->parentNode()->childElementCount() > 1) {
      return kIncludeObject;
    }
  }

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
            IgnoredReason(kAXLabelFor, AXObjectCache().Get(label->Control())));
      }
      return kIgnoreObject;
    }
    return kIncludeObject;
  }

  // The SVG-AAM says the foreignObject element is normally presentational.
  if (IsA<SVGForeignObjectElement>(node)) {
    if (ignored_reasons) {
      ignored_reasons->push_back(IgnoredReason(kAXPresentational));
    }
    return kIgnoreObject;
  }

  return kDefaultBehavior;
}

bool AXNodeObject::ComputeIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  Node* node = GetNode();

  if (ShouldIgnoreForHiddenOrInert(ignored_reasons)) {
    if (IsAriaHidden()) {
      return true;
    }
    // Keep structure of <select size=1> even when collapsed.
    if (const AXObject* ax_menu_list = ParentObject()->AncestorMenuList()) {
      return ax_menu_list->IsIgnored();
    }

    // Fallback elements inside of a <canvas> are invisible, but are not ignored
    if (IsHiddenViaStyle() || !node || !node->parentElement() ||
        !node->parentElement()->IsInCanvasSubtree()) {
      return true;
    }
  }

  // Handle content that is either visible or in a canvas subtree.

  AXObjectInclusion include = ShouldIncludeBasedOnSemantics(ignored_reasons);
  if (include == kIncludeObject) {
    return false;
  }
  if (include == kIgnoreObject) {
    return true;
  }

  if (!GetLayoutObject()) {
    // Text without a layout object that has reached this point is not
    // explicitly hidden, e.g. is in a <canvas> fallback or is display locked.
    if (IsA<Text>(node)) {
      return false;
    }
    if (ignored_reasons) {
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    }
    return true;
  }

  // Inner editor element of editable area with empty text provides bounds
  // used to compute the character extent for index 0. This is the same as
  // what the caret's bounds would be if the editable area is focused.
  if (node) {
    const TextControlElement* text_control = EnclosingTextControl(node);
    if (text_control) {
      // Keep only the inner editor element and it's children.
      // If inline textboxes are being loaded, then the inline textbox for the
      // text wil be included by AXNodeObject::AddInlineTextboxChildren().
      // By only keeping the inner editor and its text, it makes finding the
      // inner editor simpler on the browser side.
      // See BrowserAccessibility::GetTextFieldInnerEditorElement().
      // TODO(accessibility) In the future, we may want to keep all descendants
      // of the inner text element -- right now we only include one internally
      // used container, it's text, and possibly the text's inlinext text box.
      return text_control->InnerEditorElement() != node &&
             text_control->InnerEditorElement() != NodeTraversal::Parent(*node);
    }
  }

  // A LayoutEmbeddedContent is an iframe element or embedded object element or
  // something like that. We don't want to ignore those.
  if (GetLayoutObject()->IsLayoutEmbeddedContent()) {
    return false;
  }

  if (node && node->IsInUserAgentShadowRoot()) {
    Element* host = node->OwnerShadowHost();
    if (auto* containing_media_element = DynamicTo<HTMLMediaElement>(host)) {
      if (!containing_media_element->ShouldShowControls()) {
        return true;
      }
    }
    if (IsA<HTMLOptGroupElement>(host)) {
      return false;
    }
  }

  if (IsCanvas()) {
    if (CanvasHasFallbackContent()) {
      return false;
    }

    // A 1x1 canvas is too small for the user to see and thus ignored.
    const auto* canvas = DynamicTo<LayoutHTMLCanvas>(GetLayoutObject());
    if (canvas && (canvas->Size().height <= 1 || canvas->Size().width <= 1)) {
      if (ignored_reasons) {
        ignored_reasons->push_back(IgnoredReason(kAXProbablyPresentational));
      }
      return true;
    }

    // Otherwise fall through; use presence of help text, title, or description
    // to decide.
  }

  if (GetLayoutObject()->IsBR()) {
    return false;
  }

  if (GetLayoutObject()->IsText()) {
    if (GetLayoutObject()->IsInListMarker()) {
      // Ignore TextAlternative of the list marker for SUMMARY because:
      //  - TextAlternatives for disclosure-* are triangle symbol characters
      //  used to visually indicate the expansion state.
      //  - It's redundant. The host DETAILS exposes the expansion state.
      // Also ignore text descendants of any non-ignored list marker because the
      // text descendants do not provide any extra information than the
      // TextAlternative on the list marker. Besides, with 'speak-as', they will
      // be inconsistent with the list marker.
      const AXObject* list_marker_object =
          ContainerListMarkerIncludingIgnored();
      if (list_marker_object &&
          (list_marker_object->GetLayoutObject()->IsListMarkerForSummary() ||
           !list_marker_object->IsIgnored())) {
        if (ignored_reasons) {
          ignored_reasons->push_back(IgnoredReason(kAXPresentational));
        }
        return true;
      }
    }

    // Ignore text inside of an ignored <label>.
    // To save processing, only walk up the ignored objects.
    // This means that other interesting objects inside the <label> will
    // cause the text to be unignored.
    if (IsUsedForLabelOrDescription()) {
      const AXObject* ancestor = ParentObject();
      while (ancestor && ancestor->IsIgnored()) {
        if (ancestor->RoleValue() == ax::mojom::blink::Role::kLabelText) {
          if (ignored_reasons) {
            ignored_reasons->push_back(IgnoredReason(kAXPresentational));
          }
          return true;
        }
        ancestor = ancestor->ParentObject();
      }
    }
    return false;
  }

  if (GetLayoutObject()->IsListMarker()) {
    // Ignore TextAlternative of the list marker for SUMMARY because:
    //  - TextAlternatives for disclosure-* are triangle symbol characters used
    //    to visually indicate the expansion state.
    //  - It's redundant. The host DETAILS exposes the expansion state.
    if (GetLayoutObject()->IsListMarkerForSummary()) {
      if (ignored_reasons) {
        ignored_reasons->push_back(IgnoredReason(kAXPresentational));
      }
      return true;
    }
    return false;
  }

  // Positioned elements and scrollable containers are important for determining
  // bounding boxes, so don't ignore them unless they are pseudo-content.
  if (!GetLayoutObject()->IsPseudoElement()) {
    if (IsScrollableContainer()) {
      return false;
    }
    if (GetLayoutObject()->IsPositioned()) {
      return false;
    }
  }

  // Ignore a block flow (display:block, display:inline-block), unless it
  // directly parents inline children.
  // This effectively trims a lot of uninteresting divs out of the tree.
  if (auto* block_flow = DynamicTo<LayoutBlockFlow>(GetLayoutObject())) {
    if (block_flow->ChildrenInline() && block_flow->FirstChild()) {
      return false;
    }
  }

  // By default, objects should be ignored so that the AX hierarchy is not
  // filled with unnecessary items.
  if (ignored_reasons) {
    ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
  }
  return true;
}

// static
std::optional<String> AXNodeObject::GetCSSAltText(const Element* element) {
  // CSS alt text rules allow text to be assigned to ::before/::after content.
  // For example, the following CSS assigns "bullet" text to bullet.png:
  // .something::before {
  //   content: url(bullet.png) / "bullet";
  // }

  if (!element) {
    return std::nullopt;
  }
  const ComputedStyle* style = element->GetComputedStyle();
  if (!style || style->ContentBehavesAsNormal()) {
    return std::nullopt;
  }

  if (element->IsPseudoElement()) {
    for (const ContentData* content_data = style->GetContentData();
         content_data; content_data = content_data->Next()) {
      if (auto* css_alt = DynamicTo<AltTextContentData>(content_data)) {
        return css_alt->ConcatenateAltText();
      }
    }
    return std::nullopt;
  }

  // If the content property is used on a non-pseudo element, match the
  // behaviour of LayoutObject::CreateObject and only honour the style if
  // there is exactly one piece of content, which is an image.
  const ContentData* content_data = style->GetContentData();
  if (content_data && content_data->IsImage() && content_data->Next() &&
      content_data->Next()->IsAltText()) {
    return To<AltTextContentData>(content_data->Next())->ConcatenateAltText();
  }

  return std::nullopt;
}

// The following lists are for deciding whether the tags aside,
// header and footer can be interpreted as roles complementary, banner and
// contentInfo or if they should be interpreted as generic, sectionheader, or
// sectionfooter.
// This function only handles the complementary, banner, and contentInfo roles,
// which belong to the landmark roles set.
static HashSet<ax::mojom::blink::Role>& GetLandmarkIsNotAllowedAncestorRoles(
    ax::mojom::blink::Role landmark) {
  // clang-format off
  DEFINE_STATIC_LOCAL(
      // https://html.spec.whatwg.org/multipage/dom.html#sectioning-content-2
      // The aside element should not assume the complementary role when nested
      // within the following sectioning content elements.
      HashSet<ax::mojom::blink::Role>, complementary_is_not_allowed_roles,
      ({
        ax::mojom::blink::Role::kArticle,
        ax::mojom::blink::Role::kComplementary,
        ax::mojom::blink::Role::kNavigation,
        ax::mojom::blink::Role::kSection
      }));
      // https://w3c.github.io/html-aam/#el-header-ancestorbody
      // The header and footer elements should not assume the banner and
      // contentInfo roles, respectively, when nested within any of the
      // sectioning content elements or the main element.
  DEFINE_STATIC_LOCAL(
      HashSet<ax::mojom::blink::Role>, landmark_is_not_allowed_roles,
      ({
        ax::mojom::blink::Role::kArticle,
        ax::mojom::blink::Role::kComplementary,
        ax::mojom::blink::Role::kMain,
        ax::mojom::blink::Role::kNavigation,
        ax::mojom::blink::Role::kSection
      }));
  // clang-format on

  if (landmark == ax::mojom::blink::Role::kComplementary) {
    return complementary_is_not_allowed_roles;
  }
  return landmark_is_not_allowed_roles;
}

bool AXNodeObject::IsDescendantOfLandmarkDisallowedElement() const {
  if (!GetNode())
    return false;

  auto role_names = GetLandmarkIsNotAllowedAncestorRoles(RoleValue());

  for (AXObject* parent = ParentObjectUnignored(); parent;
       parent = parent->ParentObjectUnignored()) {
    if (role_names.Contains(parent->RoleValue())) {
      return true;
    }
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

  const AtomicString& scope =
      GetElement()->FastGetAttribute(html_names::kScopeAttr);
  if (EqualIgnoringASCIICase(scope, "row") ||
      EqualIgnoringASCIICase(scope, "rowgroup"))
    return ax::mojom::blink::Role::kRowHeader;
  if (EqualIgnoringASCIICase(scope, "col") ||
      EqualIgnoringASCIICase(scope, "colgroup"))
    return ax::mojom::blink::Role::kColumnHeader;

  return DecideRoleFromSiblings(GetElement());
}

unsigned AXNodeObject::ColumnCount() const {
  if (RawAriaRole() != ax::mojom::blink::Role::kUnknown) {
    return AXObject::ColumnCount();
  }

  if (const auto* table = DynamicTo<LayoutTable>(GetLayoutObject())) {
    return table->EffectiveColumnCount();
  }

  return AXObject::ColumnCount();
}

unsigned AXNodeObject::RowCount() const {
  if (RawAriaRole() != ax::mojom::blink::Role::kUnknown) {
    return AXObject::RowCount();
  }

  LayoutTable* table;
  auto* table_section = FirstTableSection(GetLayoutObject(), &table);
  if (!table_section) {
    return AXObject::RowCount();
  }

  unsigned row_count = 0;
  while (table_section) {
    row_count += table_section->NumRows();
    table_section = table->NextSection(table_section);
  }
  return row_count;
}

unsigned AXNodeObject::ColumnIndex() const {
  auto* cell = DynamicTo<LayoutTableCell>(GetLayoutObject());
  if (cell && cell->GetNode()) {
    return cell->Table()->AbsoluteColumnToEffectiveColumn(
        cell->AbsoluteColumnIndex());
  }

  return AXObject::ColumnIndex();
}

unsigned AXNodeObject::RowIndex() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->GetNode()) {
    return AXObject::RowIndex();
  }

  unsigned row_index = 0;
  const LayoutTableSection* row_section = nullptr;
  const LayoutTable* table = nullptr;
  if (const auto* row = DynamicTo<LayoutTableRow>(layout_object)) {
    row_index = row->RowIndex();
    row_section = row->Section();
    table = row->Table();
  } else if (const auto* cell = DynamicTo<LayoutTableCell>(layout_object)) {
    row_index = cell->RowIndex();
    row_section = cell->Section();
    table = cell->Table();
  } else {
    return AXObject::RowIndex();
  }

  if (!table || !row_section) {
    return AXObject::RowIndex();
  }

  // Since our table might have multiple sections, we have to offset our row
  // appropriately.
  const LayoutTableSection* section = table->FirstSection();
  while (section && section != row_section) {
    row_index += section->NumRows();
    section = table->NextSection(section);
  }

  return row_index;
}

unsigned AXNodeObject::ColumnSpan() const {
  auto* cell = DynamicTo<LayoutTableCell>(GetLayoutObject());
  if (!cell) {
    return AXObject::ColumnSpan();
  }

  LayoutTable* table = cell->Table();
  unsigned absolute_first_col = cell->AbsoluteColumnIndex();
  unsigned absolute_last_col = absolute_first_col + cell->ColSpan() - 1;
  unsigned effective_first_col =
      table->AbsoluteColumnToEffectiveColumn(absolute_first_col);
  unsigned effective_last_col =
      table->AbsoluteColumnToEffectiveColumn(absolute_last_col);
  return effective_last_col - effective_first_col + 1;
}

unsigned AXNodeObject::RowSpan() const {
  auto* cell = DynamicTo<LayoutTableCell>(GetLayoutObject());
  return cell ? cell->ResolvedRowSpan() : AXObject::RowSpan();
}

ax::mojom::blink::SortDirection AXNodeObject::GetSortDirection() const {
  if (RoleValue() != ax::mojom::blink::Role::kRowHeader &&
      RoleValue() != ax::mojom::blink::Role::kColumnHeader) {
    return ax::mojom::blink::SortDirection::kNone;
  }

  if (const AtomicString& aria_sort =
          AriaTokenAttribute(html_names::kAriaSortAttr)) {
    if (EqualIgnoringASCIICase(aria_sort, "none")) {
      return ax::mojom::blink::SortDirection::kNone;
    }
    if (EqualIgnoringASCIICase(aria_sort, "ascending")) {
      return ax::mojom::blink::SortDirection::kAscending;
    }
    if (EqualIgnoringASCIICase(aria_sort, "descending")) {
      return ax::mojom::blink::SortDirection::kDescending;
    }
    // Technically, illegal values should be exposed as is, but this does
    // not seem to be worth the implementation effort at this time.
    return ax::mojom::blink::SortDirection::kOther;
  }
  return ax::mojom::blink::SortDirection::kNone;
}

AXObject* AXNodeObject::CellForColumnAndRow(unsigned target_column_index,
                                            unsigned target_row_index) const {
  LayoutTable* table;
  auto* table_section = FirstTableSection(GetLayoutObject(), &table);
  if (!table_section) {
    return AXObject::CellForColumnAndRow(target_column_index, target_row_index);
  }

  unsigned row_offset = 0;
  while (table_section) {
    // Iterate backwards through the rows in case the desired cell has a rowspan
    // and exists in a previous row.
    for (LayoutTableRow* row = table_section->LastRow(); row;
         row = row->PreviousRow()) {
      unsigned row_index = row->RowIndex() + row_offset;
      for (LayoutTableCell* cell = row->LastCell(); cell;
           cell = cell->PreviousCell()) {
        unsigned absolute_first_col = cell->AbsoluteColumnIndex();
        unsigned absolute_last_col = absolute_first_col + cell->ColSpan() - 1;
        unsigned effective_first_col =
            table->AbsoluteColumnToEffectiveColumn(absolute_first_col);
        unsigned effective_last_col =
            table->AbsoluteColumnToEffectiveColumn(absolute_last_col);
        unsigned row_span = cell->ResolvedRowSpan();
        if (target_column_index >= effective_first_col &&
            target_column_index <= effective_last_col &&
            target_row_index >= row_index &&
            target_row_index < row_index + row_span) {
          return AXObjectCache().Get(cell);
        }
      }
    }

    row_offset += table_section->NumRows();
    table_section = table->NextSection(table_section);
  }

  return nullptr;
}

bool AXNodeObject::FindAllTableCellsWithRole(ax::mojom::blink::Role role,
                                             AXObjectVector& cells) const {
  LayoutTable* table;
  auto* table_section = FirstTableSection(GetLayoutObject(), &table);
  if (!table_section) {
    return false;
  }

  while (table_section) {
    for (LayoutTableRow* row = table_section->FirstRow(); row;
         row = row->NextRow()) {
      for (LayoutTableCell* cell = row->FirstCell(); cell;
           cell = cell->NextCell()) {
        AXObject* ax_cell = AXObjectCache().Get(cell);
        if (ax_cell && ax_cell->RoleValue() == role) {
          cells.push_back(ax_cell);
        }
      }
    }

    table_section = table->NextSection(table_section);
  }

  return true;
}

void AXNodeObject::ColumnHeaders(AXObjectVector& headers) const {
  if (!FindAllTableCellsWithRole(ax::mojom::blink::Role::kColumnHeader,
                                 headers)) {
    AXObject::ColumnHeaders(headers);
  }
}

void AXNodeObject::RowHeaders(AXObjectVector& headers) const {
  if (!FindAllTableCellsWithRole(ax::mojom::blink::Role::kRowHeader, headers)) {
    AXObject::RowHeaders(headers);
  }
}

AXObject* AXNodeObject::HeaderObject() const {
  auto* row = DynamicTo<LayoutTableRow>(GetLayoutObject());
  if (!row) {
    return nullptr;
  }

  for (LayoutTableCell* cell = row->FirstCell(); cell;
       cell = cell->NextCell()) {
    AXObject* ax_cell = cell ? AXObjectCache().Get(cell) : nullptr;
    if (ax_cell && ax_cell->RoleValue() == ax::mojom::blink::Role::kRowHeader) {
      return ax_cell;
    }
  }

  return nullptr;
}

// The following is a heuristic used to determine if a
// <table> should be with ax::mojom::blink::Role::kTable or
// ax::mojom::blink::Role::kLayoutTable.
// Only "data" tables should be exposed as tables.
// Unfortunately, there is no determinsistic or precise way to differentiate a
// layout table vs a data table. Fortunately, CSS authoring techniques have
// improved a lot and mostly supplanted the practice of using tables for layout.
bool AXNodeObject::IsDataTable() const {
  DCHECK(!IsDetached());

  auto* table_element = DynamicTo<HTMLTableElement>(GetNode());
  if (!table_element) {
    return false;
  }

  if (!GetLayoutObject()) {
    // The table is not rendered, so the author has no reason to use the table
    // for layout. Treat as a data table by default as there is not enough
    // information to decide otherwise.
    // One useful result of this is that a table inside a canvas fallback is
    // treated as a data table.
    return true;
  }

  // If it has an ARIA role, it's definitely a data table.
  if (HasAriaAttribute(html_names::kRoleAttr)) {
    return true;
  }

  // When a section of the document is contentEditable, all tables should be
  // treated as data tables, otherwise users may not be able to work with rich
  // text editors that allow creating and editing tables.
  if (GetNode() && blink::IsEditable(*GetNode())) {
    return true;
  }

  // If there is a caption element, summary, THEAD, or TFOOT section, it's most
  // certainly a data table
  if (!table_element->Summary().empty() || table_element->tHead() ||
      table_element->tFoot() || table_element->caption()) {
    return true;
  }

  // if someone used "rules" attribute than the table should appear
  if (!table_element->Rules().empty()) {
    return true;
  }

  // if there's a colgroup or col element, it's probably a data table.
  if (Traversal<HTMLTableColElement>::FirstChild(*table_element)) {
    return true;
  }

  // If there are at least 20 rows, we'll call it a data table.
  HTMLTableRowsCollection* rows = table_element->rows();
  int num_rows = rows->length();
  if (num_rows >= AXObjectCacheImpl::kDataTableHeuristicMinRows) {
    return true;
  }
  if (num_rows <= 0) {
    return false;
  }

  int num_cols_in_first_body = rows->Item(0)->cells()->length();
  // If there's only one cell, it's not a good AXTable candidate.
  if (num_rows == 1 && num_cols_in_first_body == 1) {
    return false;
  }

  // Store the background color of the table to check against cell's background
  // colors.
  const ComputedStyle* table_style = GetLayoutObject()->Style();
  if (!table_style) {
    return false;
  }

  Color table_bg_color =
      table_style->VisitedDependentColor(GetCSSPropertyBackgroundColor());
  bool has_cell_spacing = table_style->HorizontalBorderSpacing() &&
                          table_style->VerticalBorderSpacing();

  // check enough of the cells to find if the table matches our criteria
  // Criteria:
  //   1) must have at least one valid cell (and)
  //   2) at least half of cells have borders (or)
  //   3) at least half of cells have different bg colors than the table, and
  //      there is cell spacing
  unsigned valid_cell_count = 0;
  unsigned bordered_cell_count = 0;
  unsigned background_difference_cell_count = 0;
  unsigned cells_with_top_border = 0;
  unsigned cells_with_bottom_border = 0;
  unsigned cells_with_left_border = 0;
  unsigned cells_with_right_border = 0;

  std::array<Color, 5> alternating_row_colors;
  int alternating_row_color_count = 0;
  for (int row = 0; row < num_rows; ++row) {
    HTMLTableRowElement* row_element = rows->Item(row);
    int n_cols = row_element->cells()->length();
    for (int col = 0; col < n_cols; ++col) {
      const Element* cell = row_element->cells()->item(col);
      if (!cell) {
        continue;
      }
      // Any <th> tag -> treat as data table.
      if (cell->HasTagName(html_names::kThTag)) {
        return true;
      }

      // Check for an explicitly assigned a "data" table attribute.
      auto* cell_elem = DynamicTo<HTMLTableCellElement>(*cell);
      if (cell_elem) {
        if (!cell_elem->Headers().empty() || !cell_elem->Abbr().empty() ||
            !cell_elem->Axis().empty() ||
            !cell_elem->FastGetAttribute(html_names::kScopeAttr).empty()) {
          return true;
        }
      }

      LayoutObject* cell_layout_object = cell->GetLayoutObject();
      if (!cell_layout_object || !cell_layout_object->IsLayoutBlock()) {
        continue;
      }

      const LayoutBlock* cell_layout_block =
          To<LayoutBlock>(cell_layout_object);
      if (cell_layout_block->Size().width < 1 ||
          cell_layout_block->Size().height < 1) {
        continue;
      }

      valid_cell_count++;

      const ComputedStyle* computed_style = cell_layout_block->Style();
      if (!computed_style) {
        continue;
      }

      // If the empty-cells style is set, we'll call it a data table.
      if (computed_style->EmptyCells() == EEmptyCells::kHide) {
        return true;
      }

      // If a cell has matching bordered sides, call it a (fully) bordered cell.
      if ((cell_layout_block->BorderTop() > 0 &&
           cell_layout_block->BorderBottom() > 0) ||
          (cell_layout_block->BorderLeft() > 0 &&
           cell_layout_block->BorderRight() > 0)) {
        bordered_cell_count++;
      }

      // Also keep track of each individual border, so we can catch tables where
      // most cells have a bottom border, for example.
      if (cell_layout_block->BorderTop() > 0) {
        cells_with_top_border++;
      }
      if (cell_layout_block->BorderBottom() > 0) {
        cells_with_bottom_border++;
      }
      if (cell_layout_block->BorderLeft() > 0) {
        cells_with_left_border++;
      }
      if (cell_layout_block->BorderRight() > 0) {
        cells_with_right_border++;
      }

      // If the cell has a different color from the table and there is cell
      // spacing, then it is probably a data table cell (spacing and colors take
      // the place of borders).
      Color cell_color = computed_style->VisitedDependentColor(
          GetCSSPropertyBackgroundColor());
      if (has_cell_spacing && table_bg_color != cell_color &&
          !cell_color.IsFullyTransparent()) {
        background_difference_cell_count++;
      }

      // If we've found 10 "good" cells, we don't need to keep searching.
      if (bordered_cell_count >= 10 || background_difference_cell_count >= 10) {
        return true;
      }

      // For the first 5 rows, cache the background color so we can check if
      // this table has zebra-striped rows.
      if (row < 5 && row == alternating_row_color_count) {
        LayoutObject* layout_row = cell_layout_block->Parent();
        if (!layout_row || !layout_row->IsBoxModelObject() ||
            !layout_row->IsTableRow()) {
          continue;
        }
        const ComputedStyle* row_computed_style = layout_row->Style();
        if (!row_computed_style) {
          continue;
        }
        Color row_color = row_computed_style->VisitedDependentColor(
            GetCSSPropertyBackgroundColor());
        alternating_row_colors[alternating_row_color_count] = row_color;
        alternating_row_color_count++;
      }
    }
  }

  // if there is less than two valid cells, it's not a data table
  if (valid_cell_count <= 1) {
    return false;
  }

  // half of the cells had borders, it's a data table
  unsigned needed_cell_count = valid_cell_count / 2;
  if (bordered_cell_count >= needed_cell_count ||
      cells_with_top_border >= needed_cell_count ||
      cells_with_bottom_border >= needed_cell_count ||
      cells_with_left_border >= needed_cell_count ||
      cells_with_right_border >= needed_cell_count) {
    return true;
  }

  // half had different background colors, it's a data table
  if (background_difference_cell_count >= needed_cell_count) {
    return true;
  }

  // Check if there is an alternating row background color indicating a zebra
  // striped style pattern.
  if (alternating_row_color_count > 2) {
    Color first_color = alternating_row_colors[0];
    for (int k = 1; k < alternating_row_color_count; k++) {
      // If an odd row was the same color as the first row, its not alternating.
      if (k % 2 == 1 && alternating_row_colors[k] == first_color) {
        return false;
      }
      // If an even row is not the same as the first row, its not alternating.
      if (!(k % 2) && alternating_row_colors[k] != first_color) {
        return false;
      }
    }
    return true;
  }

  return false;
}

// TODO(accessibility) Consider combining with NativeRoleIgnoringAria().
ax::mojom::blink::Role AXNodeObject::RoleFromLayoutObjectOrNode() const {
  if (!GetLayoutObject()) {
    return ax::mojom::blink::Role::kGenericContainer;
  }

  DCHECK(GetLayoutObject());

  if (GetLayoutObject()->IsListMarker()) {
    Node* list_item = GetLayoutObject()->GeneratingNode();
    if (list_item && ShouldIgnoreListItem(list_item)) {
      return ax::mojom::blink::Role::kNone;
    }
    return ax::mojom::blink::Role::kListMarker;
  }

  if (GetLayoutObject()->IsListItem()) {
    return ax::mojom::blink::Role::kListItem;
  }
  if (GetLayoutObject()->IsBR()) {
    return ax::mojom::blink::Role::kLineBreak;
  }
  if (GetLayoutObject()->IsText()) {
    return ax::mojom::blink::Role::kStaticText;
  }

  Node* node = GetNode();  // Can be null in the case of pseudo content.

  // Chrome exposes both table markup and table CSS as a tables, letting
  // the screen reader determine what to do for CSS tables. If this line
  // is reached, then it is not an HTML table, and therefore will only be
  // considered a data table if ARIA markup indicates it is a table.
  // Additionally, as pseudo elements don't have any structure it doesn't make
  // sense to report their table-related layout roles that could be set via the
  // display property.
  if (node && !node->IsPseudoElement()) {
    if (GetLayoutObject()->IsTable()) {
      return ax::mojom::blink::Role::kLayoutTable;
    }
    if (GetLayoutObject()->IsTableSection()) {
      return DetermineTableSectionRole();
    }
    if (GetLayoutObject()->IsTableRow()) {
      return DetermineTableRowRole();
    }
    if (GetLayoutObject()->IsTableCell()) {
      return DetermineTableCellRole();
    }
  }

  if (IsImageOrAltText(GetLayoutObject(), node)) {
    if (IsA<HTMLInputElement>(node)) {
      return ButtonRoleType();
    }
    return ax::mojom::blink::Role::kImage;
  }

  if (IsA<HTMLCanvasElement>(node)) {
    return ax::mojom::blink::Role::kCanvas;
  }

  if (IsA<LayoutView>(GetLayoutObject())) {
    return ParentObject() ? ax::mojom::blink::Role::kGroup
                          : ax::mojom::blink::Role::kRootWebArea;
  }

  if (node && node->IsSVGElement()) {
    if (GetLayoutObject()->IsSVGImage()) {
      return ax::mojom::blink::Role::kImage;
    }
    if (IsA<SVGSVGElement>(node)) {
      // Exposing a nested <svg> as a group (rather than a generic container)
      // increases the likelihood that an author-provided name will be presented
      // by assistive technologies. Note that this mapping is not yet in the
      // SVG-AAM, which currently maps all <svg> elements as graphics-document.
      // See https://github.com/w3c/svg-aam/issues/18.
      return GetLayoutObject()->IsSVGRoot() ? ax::mojom::blink::Role::kSvgRoot
                                            : ax::mojom::blink::Role::kGroup;
    }
    if (GetLayoutObject()->IsSVGShape()) {
      return ax::mojom::blink::Role::kGraphicsSymbol;
    }
    if (GetLayoutObject()->IsSVGForeignObject()) {
      return ax::mojom::blink::Role::kGroup;
    }
    if (IsA<SVGUseElement>(node)) {
      return ax::mojom::blink::Role::kGraphicsObject;
    }
  }

  if (GetLayoutObject()->IsHR()) {
    return ax::mojom::blink::Role::kSplitter;
  }

  // Minimum role:
  // TODO(accessibility) if (AXObjectCache().IsInternalUICheckerOn()) assert,
  // because it is a bad code smell and usually points to other problems.
  if (GetElement() && !HasAriaAttribute(html_names::kRoleAttr)) {
    if (IsPopup() != ax::mojom::blink::IsPopup::kNone ||
        GetElement()->FastHasAttribute(html_names::kAutofocusAttr) ||
        GetElement()->FastHasAttribute(html_names::kDraggableAttr)) {
      return ax::mojom::blink::Role::kGroup;
    }
    if (RuntimeEnabledFeatures::AccessibilityMinRoleTabbableEnabled()) {
      if (GetElement()->IsKeyboardFocusable(
              Element::UpdateBehavior::kNoneForAccessibility)) {
        return ax::mojom::blink::Role::kGroup;
      }
    }
  }

  if (IsA<HTMLPermissionElement>(node)) {
    return ax::mojom::blink::Role::kButton;
  }

  // Anything that needs to be exposed but doesn't have a more specific role
  // should be considered a generic container. Examples are layout blocks with
  // no node, in-page link targets, and plain elements such as a <span> with
  // an aria- property.
  return ax::mojom::blink::Role::kGenericContainer;
}

// Does not check ARIA role, but does check some ARIA properties, specifically
// @aria-haspopup/aria-pressed via ButtonType().
ax::mojom::blink::Role AXNodeObject::NativeRoleIgnoringAria() const {
  if (!GetNode()) {
    // Can be null in the case of pseudo content.
    return RoleFromLayoutObjectOrNode();
  }

  if (GetNode()->IsPseudoElement() && GetCSSAltText(GetElement())) {
    const ComputedStyle* style = GetElement()->GetComputedStyle();
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

  if (IsA<HTMLImageElement>(GetNode()))
    return ax::mojom::blink::Role::kImage;

  // <a> or <svg:a>.
  if (IsA<HTMLAnchorElement>(GetNode()) || IsA<SVGAElement>(GetNode())) {
    // Assume that an anchor element is a Role::kLink if it has an href or a
    // click event listener.
    if (GetNode()->IsLink() ||
        GetNode()->HasAnyEventListeners(event_util::MouseButtonEventTypes())) {
      return ax::mojom::blink::Role::kLink;
    }

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

  if (IsA<SVGGElement>(*GetNode())) {
    return ax::mojom::blink::Role::kGroup;
  }

  if (IsA<HTMLButtonElement>(*GetNode()))
    return ButtonRoleType();

  if (IsA<HTMLDetailsElement>(*GetNode()))
    return ax::mojom::blink::Role::kDetails;

  if (IsA<HTMLSummaryElement>(*GetNode())) {
    ContainerNode* parent = LayoutTreeBuilderTraversal::Parent(*GetNode());
    if (ToHTMLSlotElementIfSupportsAssignmentOrNull(parent))
      parent = LayoutTreeBuilderTraversal::Parent(*parent);
    if (HTMLDetailsElement* parent_details =
            DynamicTo<HTMLDetailsElement>(parent)) {
      if (parent_details->GetName().empty()) {
        return ax::mojom::blink::Role::kDisclosureTriangle;
      } else {
        return ax::mojom::blink::Role::kDisclosureTriangleGrouped;
      }
    }
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
    FormControlType type = input->FormControlType();
    if (input->DataList() && type != FormControlType::kInputColor) {
      return ax::mojom::blink::Role::kTextFieldWithComboBox;
    }
    switch (type) {
      case FormControlType::kInputButton:
      case FormControlType::kInputReset:
      case FormControlType::kInputSubmit:
        return ButtonRoleType();
      case FormControlType::kInputCheckbox:
        return ax::mojom::blink::Role::kCheckBox;
      case FormControlType::kInputDate:
        return ax::mojom::blink::Role::kDate;
      case FormControlType::kInputDatetimeLocal:
      case FormControlType::kInputMonth:
      case FormControlType::kInputWeek:
        return ax::mojom::blink::Role::kDateTime;
      case FormControlType::kInputFile:
        return ax::mojom::blink::Role::kButton;
      case FormControlType::kInputRadio:
        return ax::mojom::blink::Role::kRadioButton;
      case FormControlType::kInputNumber:
        return ax::mojom::blink::Role::kSpinButton;
      case FormControlType::kInputRange:
        return ax::mojom::blink::Role::kSlider;
      case FormControlType::kInputSearch:
        return ax::mojom::blink::Role::kSearchBox;
      case FormControlType::kInputColor:
        return ax::mojom::blink::Role::kColorWell;
      case FormControlType::kInputTime:
        return ax::mojom::blink::Role::kInputTime;
      case FormControlType::kInputImage:
        return ax::mojom::blink::Role::kButton;
      default:
        return ax::mojom::blink::Role::kTextField;
    }
  }

  if (auto* select_element = DynamicTo<HTMLSelectElement>(*GetNode())) {
    if (select_element->UsesMenuList() && !select_element->IsMultiple()) {
      return ax::mojom::blink::Role::kComboBoxSelect;
    } else {
      return ax::mojom::blink::Role::kListBox;
    }
  }

  if (ParentObjectIfPresent() && ParentObjectIfPresent()->RoleValue() ==
                                     ax::mojom::blink::Role::kComboBoxSelect) {
    return ax::mojom::blink::Role::kMenuListPopup;
  }

  if (auto* option = DynamicTo<HTMLOptionElement>(*GetNode())) {
    HTMLSelectElement* select_element = option->OwnerSelectElement();
    if (select_element && select_element->UsesMenuList() &&
        !select_element->IsMultiple()) {
      return ax::mojom::blink::Role::kMenuListOption;
    } else {
      return ax::mojom::blink::Role::kListBoxOption;
    }
  }

  if (IsA<HTMLOptGroupElement>(GetNode())) {
    return ax::mojom::blink::Role::kGroup;
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

  if (IsA<HTMLLIElement>(*GetNode())) {
    if (ShouldIgnoreListItem(GetNode())) {
      return ax::mojom::blink::Role::kNone;
    }
    return ax::mojom::blink::Role::kListItem;
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

  if (GetNode()->HasTagName(html_names::kRubyTag)) {
    return ax::mojom::blink::Role::kRuby;
  }

  if (IsA<HTMLDListElement>(*GetNode()))
    return ax::mojom::blink::Role::kDescriptionList;

  if (IsA<HTMLDirectoryElement>(*GetNode())) {
    return ax::mojom::blink::Role::kList;
  }

  if (IsA<HTMLAudioElement>(*GetNode()))
    return ax::mojom::blink::Role::kAudio;
  if (IsA<HTMLVideoElement>(*GetNode()))
    return ax::mojom::blink::Role::kVideo;

  if (GetNode()->HasTagName(html_names::kDdTag))
    return ax::mojom::blink::Role::kDefinition;

  if (GetNode()->HasTagName(html_names::kDfnTag))
    return ax::mojom::blink::Role::kTerm;

  if (GetNode()->HasTagName(html_names::kDtTag))
    return ax::mojom::blink::Role::kTerm;

  // Mapping of MathML elements. See https://w3c.github.io/mathml-aam/
  if (auto* element = DynamicTo<MathMLElement>(GetNode())) {
    if (element->HasTagName(mathml_names::kMathTag)) {
      return ax::mojom::blink::Role::kMathMLMath;
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

  if (GetNode()->HasTagName(html_names::kSearchTag)) {
    return ax::mojom::blink::Role::kSearch;
  }

  if (GetNode()->HasTagName(html_names::kDelTag) ||
      GetNode()->HasTagName(html_names::kSTag)) {
    return ax::mojom::blink::Role::kContentDeletion;
  }

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

  if (GetNode()->HasTagName(html_names::kSectionTag)) {
    return ax::mojom::blink::Role::kSection;
  }

  if (GetNode()->HasTagName(html_names::kAddressTag))
    return ax::mojom::blink::Role::kGroup;

  if (GetNode()->HasTagName(html_names::kHgroupTag)) {
    return ax::mojom::blink::Role::kGroup;
  }

  if (IsA<HTMLDialogElement>(*GetNode()))
    return ax::mojom::blink::Role::kDialog;

  // The HTML element.
  if (IsA<HTMLHtmlElement>(GetNode()))
    return ax::mojom::blink::Role::kGenericContainer;

  // Treat <iframe>, <frame> and <fencedframe> the same.
  if (IsFrame(GetNode()))
    return ax::mojom::blink::Role::kIframe;

  if (GetNode()->HasTagName(html_names::kHeaderTag)) {
    return ax::mojom::blink::Role::kHeader;
  }

  if (GetNode()->HasTagName(html_names::kFooterTag)) {
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

ax::mojom::blink::Role AXNodeObject::DetermineRoleValue() {
#if DCHECK_IS_ON()
  base::AutoReset<bool> reentrancy_protector(&is_computing_role_, true);
#endif

  if (IsDetached()) {
    NOTREACHED_IN_MIGRATION()
        << "Do not compute role on detached object: " << this;
    return ax::mojom::blink::Role::kUnknown;
  }

  native_role_ = NativeRoleIgnoringAria();

  aria_role_ = DetermineAriaRole();

  return aria_role_ == ax::mojom::blink::Role::kUnknown ? native_role_
                                                        : aria_role_;
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
        blink::AXObject::AriaAttribute(*element, html_names::kRoleAttr);
    if (EqualIgnoringASCIICase(sibling_aria_role, role))
      return element;
  }

  return nullptr;
}

Element* AXNodeObject::MenuItemElementForMenu() const {
  if (RawAriaRole() != ax::mojom::blink::Role::kMenu) {
    return nullptr;
  }

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
#if DCHECK_IS_ON()
  if (layout_object_) {
    layout_object_->SetHasAXObject(false);
  }
#endif
  layout_object_ = nullptr;
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
          ui::IsControl(RawAriaRole()));
}

bool AXNodeObject::IsAutofillAvailable() const {
  // Autofill suggestion availability is stored in AXObjectCache.
  WebAXAutofillSuggestionAvailability suggestion_availability =
      AXObjectCache().GetAutofillSuggestionAvailability(AXObjectID());
  return suggestion_availability ==
         WebAXAutofillSuggestionAvailability::kAutofillAvailable;
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
  if (html_input_element && RoleValue() == ax::mojom::blink::Role::kButton) {
    return html_input_element->FormControlType() ==
           FormControlType::kInputImage;
  }

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
    if (layout_text->HasNonCollapsedText() && style.ShouldPreserveBreaks() &&
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
    case ax::mojom::blink::Role::kTabList:
      bool multiselectable;
      if (AriaBooleanAttribute(html_names::kAriaMultiselectableAttr,
                               &multiselectable)) {
        return multiselectable;
      }
      break;
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
    return input->FormControlType() == FormControlType::kInputImage;

  return false;
}

bool AXNodeObject::IsVisible() const {
  // Any descendant of a <select size=1> should be considered invisible if
  // the select is collapsed.
  if (RoleValue() == ax::mojom::blink::Role::kMenuListPopup) {
    CHECK(parent_);
    return parent_->IsExpanded() == kExpandedExpanded;
  }

  if (IsRoot()) {
    return true;
  }

  // Anything else inside of a collapsed select is also invisible.
  if (const AXObject* ax_select = ParentObject()->AncestorMenuList()) {
    // If the select is invisible, so is everything inside of it.
    if (!ax_select->IsVisible()) {
      return false;
    }
    // Inside of a collapsed select:
    // - The selected option's subtree is visible.
    // - Everything else is invisible.
    if (ax_select->IsExpanded() == kExpandedCollapsed) {
      if (const AXObject* ax_option = AncestorMenuListOption()) {
        return ax_option->IsSelected() == kSelectedStateTrue;
      }
      return false;
    }
  }

  return AXObject::IsVisible();
}

bool AXNodeObject::IsLinked() const {
  if (!IsLinkable(*this)) {
    return false;
  }

  if (auto* anchor = DynamicTo<HTMLAnchorElementBase>(AnchorElement())) {
    return !anchor->Href().IsEmpty();
  }
  return false;
}

bool AXNodeObject::IsVisited() const {
  return GetLayoutObject() && GetLayoutObject()->Style()->IsLink() &&
         GetLayoutObject()->Style()->InsideLink() ==
             EInsideLink::kInsideVisitedLink;
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
    return input->FormControlType() == FormControlType::kInputRange;
  return false;
}

bool AXNodeObject::IsNativeSpinButton() const {
  if (const auto* input = DynamicTo<HTMLInputElement>(GetNode()))
    return input->FormControlType() == FormControlType::kInputNumber;
  return false;
}

bool AXNodeObject::IsEmbeddingElement() const {
  return ui::IsEmbeddingElement(native_role_);
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

AccessibilitySelectedState AXNodeObject::IsSelected() const {
  if (!GetNode() || !IsSubWidget()) {
    return kSelectedStateUndefined;
  }

  // The aria-selected attribute overrides automatic behaviors.
  bool is_selected;
  if (AriaBooleanAttribute(html_names::kAriaSelectedAttr, &is_selected)) {
    return is_selected ? kSelectedStateTrue : kSelectedStateFalse;
  }

  // The selection should only follow the focus when the aria-selected attribute
  // is marked as required or implied for this element in the ARIA specs.
  // If this object can't follow the focus, then we can't say that it's selected
  // nor that it's not.
  if (!ui::IsSelectRequiredOrImplicit(RoleValue()))
    return kSelectedStateUndefined;

  if (auto* option_element = DynamicTo<HTMLOptionElement>(GetNode())) {
    if (!CanSetSelectedAttribute()) {
      return kSelectedStateUndefined;
    }
    return (option_element->Selected()) ? kSelectedStateTrue
                                        : kSelectedStateFalse;
  }
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
  if (AriaBooleanAttribute(html_names::kAriaSelectedAttr, &is_selected)) {
    return false;
  }

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

// Returns true if the object is marked user-select:none
bool AXNodeObject::IsNotUserSelectable() const {
  if (!GetLayoutObject()) {
    return false;
  }

  if (IsA<PseudoElement>(GetClosestElement())) {
    return true;
  }

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style) {
    return false;
  }

  return (style->UsedUserSelect() == EUserSelect::kNone);
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
    AXObject* tab_panel = AXObjectCache().Get(element);

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

  // Only editable fields can be marked @readonly (unlike @aria-readonly).
  auto* text_area_element = DynamicTo<HTMLTextAreaElement>(*elem);
  if (text_area_element && text_area_element->IsReadOnly())
    return kRestrictionReadOnly;
  if (const auto* input = DynamicTo<HTMLInputElement>(*elem)) {
    if (input->IsTextField() && input->IsReadOnly())
      return kRestrictionReadOnly;
  }

  // Check aria-readonly if supported by current role.
  bool is_read_only;
  if (SupportsARIAReadOnly() &&
      AriaBooleanAttribute(html_names::kAriaReadonlyAttr, &is_read_only)) {
    // ARIA overrides other readonly state markup.
    return is_read_only ? kRestrictionReadOnly : kRestrictionNone;
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

  if (RoleValue() == ax::mojom::blink::Role::kComboBoxSelect) {
    DCHECK(IsA<HTMLSelectElement>(element));
    bool is_expanded = To<HTMLSelectElement>(element)->PopupIsVisible();
    return is_expanded ? kExpandedExpanded : kExpandedCollapsed;
  }

  // For form controls that act as triggering elements for popovers, then set
  // aria-expanded=false when the popover is hidden, and aria-expanded=true when
  // it is showing.
  if (auto* form_control = DynamicTo<HTMLFormControlElement>(element)) {
    if (auto popover = form_control->popoverTargetElement().popover) {
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
  if (AriaBooleanAttribute(html_names::kAriaExpandedAttr, &expanded)) {
    return expanded ? kExpandedExpanded : kExpandedCollapsed;
  }

  return kExpandedUndefined;
}

bool AXNodeObject::IsRequired() const {
  auto* form_control = DynamicTo<HTMLFormControlElement>(GetNode());
  if (form_control && form_control->IsRequired())
    return true;

  if (IsAriaAttributeTrue(html_names::kAriaRequiredAttr)) {
    return true;
  }

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
    int32_t level;
    if (AriaIntAttribute(html_names::kAriaLevelAttr, &level)) {
      if (level >= 1 && level <= 9) {
        return level;
      }
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

  // TODO(accessibility) For kDisclosureTriangle, kDisclosureTriangleGrouping,
  // if IsAccessibilityExposeSummaryAsHeadingEnabled(), we should expose
  // a default heading level that makes sense in the context of the document.
  // Will likely be easier to do on the browser side.
  if (ui::IsHeading(RoleValue())) {
    return 5;
  }

  return 0;
}

unsigned AXNodeObject::HierarchicalLevel() const {
  Element* element = GetElement();
  if (!element)
    return 0;

  int32_t level;
  if (AriaIntAttribute(html_names::kAriaLevelAttr, &level)) {
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
  if (AXObjectCache().GetAutofillSuggestionAvailability(AXObjectID()) ==
      WebAXAutofillSuggestionAvailability::kAutocompleteAvailable) {
    return "list";
  }

  if (IsAtomicTextField() || IsARIATextField()) {
    const AtomicString& aria_auto_complete =
        AriaTokenAttribute(html_names::kAriaAutocompleteAttr);
    // Illegal values must be passed through, according to CORE-AAM.
    if (aria_auto_complete) {
      return aria_auto_complete == "none" ? String()
                                          : aria_auto_complete.LowerASCII();
      ;
    }
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
  std::optional<DocumentMarker::MarkerType> aria_marker_type =
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

ax::mojom::blink::ListStyle AXNodeObject::GetListStyle() const {
  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object) {
    return AXObject::GetListStyle();
  }

  const ComputedStyle* computed_style = layout_object->Style();
  if (!computed_style) {
    return AXObject::GetListStyle();
  }

  const StyleImage* style_image = computed_style->ListStyleImage();
  if (style_image && !style_image->ErrorOccurred()) {
    return ax::mojom::blink::ListStyle::kImage;
  }

  if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleSpeakAsDescriptorEnabled()) {
    if (!computed_style->ListStyleType()) {
      return ax::mojom::blink::ListStyle::kNone;
    }
    if (computed_style->ListStyleType()->IsString()) {
      return ax::mojom::blink::ListStyle::kOther;
    }

    DCHECK(computed_style->ListStyleType()->IsCounterStyle());
    const CounterStyle& counter_style =
        ListMarker::GetCounterStyle(*GetDocument(), *computed_style);
    switch (counter_style.EffectiveSpeakAs()) {
      case CounterStyleSpeakAs::kBullets: {
        // See |ua_counter_style_map.cc| for predefined symbolic counter styles.
        UChar symbol = counter_style.GenerateTextAlternative(0)[0];
        switch (symbol) {
          case 0x2022:
            return ax::mojom::blink::ListStyle::kDisc;
          case 0x25E6:
            return ax::mojom::blink::ListStyle::kCircle;
          case 0x25A0:
            return ax::mojom::blink::ListStyle::kSquare;
          default:
            return ax::mojom::blink::ListStyle::kOther;
        }
      }
      case CounterStyleSpeakAs::kNumbers:
        return ax::mojom::blink::ListStyle::kNumeric;
      case CounterStyleSpeakAs::kWords:
        return ax::mojom::blink::ListStyle::kOther;
      case CounterStyleSpeakAs::kAuto:
      case CounterStyleSpeakAs::kReference:
        NOTREACHED_IN_MIGRATION();
        return ax::mojom::blink::ListStyle::kOther;
    }
  }

  switch (ListMarker::GetListStyleCategory(*GetDocument(), *computed_style)) {
    case ListMarker::ListStyleCategory::kNone:
      return ax::mojom::blink::ListStyle::kNone;
    case ListMarker::ListStyleCategory::kSymbol: {
      const AtomicString& counter_style_name =
          computed_style->ListStyleType()->GetCounterStyleName();
      if (counter_style_name == keywords::kDisc) {
        return ax::mojom::blink::ListStyle::kDisc;
      }
      if (counter_style_name == keywords::kCircle) {
        return ax::mojom::blink::ListStyle::kCircle;
      }
      if (counter_style_name == keywords::kSquare) {
        return ax::mojom::blink::ListStyle::kSquare;
      }
      return ax::mojom::blink::ListStyle::kOther;
    }
    case ListMarker::ListStyleCategory::kLanguage: {
      const AtomicString& counter_style_name =
          computed_style->ListStyleType()->GetCounterStyleName();
      if (counter_style_name == keywords::kDecimal) {
        return ax::mojom::blink::ListStyle::kNumeric;
      }
      if (counter_style_name == "decimal-leading-zero") {
        // 'decimal-leading-zero' may be overridden by custom counter styles. We
        // return kNumeric only when we are using the predefined counter style.
        if (ListMarker::GetCounterStyle(*GetDocument(), *computed_style)
                .IsPredefined()) {
          return ax::mojom::blink::ListStyle::kNumeric;
        }
      }
      return ax::mojom::blink::ListStyle::kOther;
    }
    case ListMarker::ListStyleCategory::kStaticString:
      return ax::mojom::blink::ListStyle::kOther;
  }
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

  String fragment = link_url.FragmentIdentifier().ToString();
  TreeScope& tree_scope = anchor->GetTreeScope();
  Node* target = tree_scope.FindAnchor(fragment);
  AXObject* ax_target = AXObjectCache().Get(target);
  if (!ax_target || !IsPotentialInPageLinkTarget(*ax_target->GetNode()))
    return AXObject::InPageLinkTarget();

#if DCHECK_IS_ON()
  // Link targets always have an element, unless it is the document itself,
  // e.g. via <a href="#">.
  DCHECK(ax_target->IsWebArea() || ax_target->GetElement())
      << "The link target is expected to be a document or an element: "
      << ax_target << "\n* URL fragment = " << fragment;
#endif

  // Usually won't be ignored, but could be e.g. if aria-hidden.
  if (ax_target->IsIgnored())
    return nullptr;

  return ax_target;
}

const AtomicString& AXNodeObject::EffectiveTarget() const {
  // The "target" attribute defines the target browser context and is supported
  // on <a>, <area>, <base>, and <form>. Valid values are: "frame_name", "self",
  // "blank", "top", and "parent", where "frame_name" is the value of the "name"
  // attribute on any enclosing iframe.
  //
  // <area> is a subclass of <a>, while <base> provides the document's base
  // target that any <a>'s or any <area>'s target can override.
  // `HtmlAnchorElement::GetEffectiveTarget()` will take <base> into account.
  //
  // <form> is out of scope, because it affects the target to which the form is
  // submitted, and could also be overridden by a "formTarget" attribute on e.g.
  // a form's submit button. However, screen reader users have no need to know
  // to which target (browser context) a form would be submitted.
  const auto* anchor = DynamicTo<HTMLAnchorElementBase>(GetNode());
  if (anchor) {
    const AtomicString self_value("_self");
    const AtomicString& effective_target = anchor->GetEffectiveTarget();
    if (effective_target != self_value) {
      return anchor->GetEffectiveTarget();
    }
  }
  return AXObject::EffectiveTarget();
}

AccessibilityOrientation AXNodeObject::Orientation() const {
  const AtomicString& aria_orientation =
      AriaTokenAttribute(html_names::kAriaOrientationAttr);
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
      AXObject* ax_radio_button = AXObjectCache().Get(radio_button);
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
          child->IsIncludedInTree()) {
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
  if (!radio_button ||
      radio_button->FormControlType() != FormControlType::kInputRadio) {
    return all_radio_buttons;
  }

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

  switch (style->GetWritingDirection().InlineEnd()) {
    case PhysicalDirection::kRight:
      return ax::mojom::blink::WritingDirection::kLtr;
    case PhysicalDirection::kLeft:
      return ax::mojom::blink::WritingDirection::kRtl;
    case PhysicalDirection::kDown:
      return ax::mojom::blink::WritingDirection::kTtb;
    case PhysicalDirection::kUp:
      return ax::mojom::blink::WritingDirection::kBtt;
  }

  NOTREACHED_IN_MIGRATION();
  return AXObject::GetTextDirection();
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

  if (style->GetFontWeight() == kBoldWeightValue) {
    *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kBold);
  }
  if (style->GetFontDescription().Style() == kItalicSlopeValue) {
    *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kItalic);
  }

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
        MakeGarbageCollected<ImageBitmap>(image, std::nullopt, options);
  } else if (auto* canvas = DynamicTo<HTMLCanvasElement>(node)) {
    image_bitmap =
        MakeGarbageCollected<ImageBitmap>(canvas, std::nullopt, options);
  } else if (auto* video = DynamicTo<HTMLVideoElement>(node)) {
    image_bitmap =
        MakeGarbageCollected<ImageBitmap>(video, std::nullopt, options);
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
  if (!SkImages::RasterFromBitmap(bitmap)->readPixels(pixmap, 0, 0)) {
    return String();
  }

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

  if (IsA<Document>(GetNode())) {
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
      AriaTokenAttribute(html_names::kAriaCurrentAttr);
  if (attribute_value.IsNull()) {
    return ax::mojom::blink::AriaCurrentState::kNone;
  }
  if (EqualIgnoringASCIICase(attribute_value, "false")) {
    return ax::mojom::blink::AriaCurrentState::kFalse;
  }
  if (EqualIgnoringASCIICase(attribute_value, "page")) {
    return ax::mojom::blink::AriaCurrentState::kPage;
  }
  if (EqualIgnoringASCIICase(attribute_value, "step")) {
    return ax::mojom::blink::AriaCurrentState::kStep;
  }
  if (EqualIgnoringASCIICase(attribute_value, "location")) {
    return ax::mojom::blink::AriaCurrentState::kLocation;
  }
  if (EqualIgnoringASCIICase(attribute_value, "date")) {
    return ax::mojom::blink::AriaCurrentState::kDate;
  }
  if (EqualIgnoringASCIICase(attribute_value, "time")) {
    return ax::mojom::blink::AriaCurrentState::kTime;
  }

  // An unknown value should return true.
  return ax::mojom::blink::AriaCurrentState::kTrue;
}

ax::mojom::blink::InvalidState AXNodeObject::GetInvalidState() const {
  // First check aria-invalid.
  if (const AtomicString& attribute_value =
          AriaTokenAttribute(html_names::kAriaInvalidAttr)) {
    // aria-invalid="false".
    if (EqualIgnoringASCIICase(attribute_value, "false")) {
      return ax::mojom::blink::InvalidState::kFalse;
    }
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
  // A <select size=1> exposes posinset as the index of the selected option.
  if (RoleValue() == ax::mojom::blink::Role::kComboBoxSelect) {
    if (auto* select_element = DynamicTo<HTMLSelectElement>(*GetNode())) {
      return 1 + select_element->selectedIndex();
    }
  }

  if (SupportsARIASetSizeAndPosInSet()) {
    int32_t pos_in_set;
    if (AriaIntAttribute(html_names::kAriaPosinsetAttr, &pos_in_set)) {
      return pos_in_set;
    }
  }
  return 0;
}

int AXNodeObject::SetSize() const {
  if (auto* select_element = DynamicTo<HTMLSelectElement>(GetNode())) {
    return static_cast<int>(select_element->length());
  }

  if (RoleValue() == ax::mojom::blink::Role::kMenuListPopup) {
    return ParentObject()->SetSize();
  }

  if (SupportsARIASetSizeAndPosInSet()) {
    int32_t set_size;
    if (AriaIntAttribute(html_names::kAriaSetsizeAttr, &set_size)) {
      return set_size;
    }
  }
  return 0;
}

bool AXNodeObject::ValueForRange(float* out_value) const {
  float value_now;
  if (AriaFloatAttribute(html_names::kAriaValuenowAttr, &value_now)) {
    // Adjustment when the aria-valuenow is less than aria-valuemin or greater
    // than the aria-valuemax value.
    // See https://w3c.github.io/aria/#authorErrorDefaultValuesTable.
    float min_value, max_value;
    if (MinValueForRange(&min_value)) {
      if (value_now < min_value) {
        *out_value = min_value;
        return true;
      }
    }
    if (MaxValueForRange(&max_value)) {
      if (value_now > max_value) {
        *out_value = max_value;
        return true;
      }
    }

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
  // - meter: A value matching the implicit or explicitly set aria-valuemin.
  // - scrollbar, slider : half way between aria-valuemin and aria-valuemax
  // - separator : 50
  // - spinbutton : 0
  switch (RawAriaRole()) {
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
    case ax::mojom::blink::Role::kMeter: {
      float min_value;
      if (MinValueForRange(&min_value)) {
        *out_value = min_value;
        return true;
      }
      [[fallthrough]];
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
  if (AriaFloatAttribute(html_names::kAriaValuemaxAttr, out_value)) {
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
  switch (RawAriaRole()) {
    case ax::mojom::blink::Role::kMeter:
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
  if (AriaFloatAttribute(html_names::kAriaValueminAttr, out_value)) {
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
  switch (RawAriaRole()) {
    case ax::mojom::blink::Role::kMeter:
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
    auto step_range =
        To<HTMLInputElement>(*GetNode()).CreateStepRange(kRejectAny);
    auto step = step_range.Step().ToString().ToFloat();

    // Provide a step if ATs incrementing slider should move by step, otherwise
    // AT will move by 5%.
    // If there are too few allowed stops (< 20), incrementing/decrementing
    // the slider by 5% could get stuck, and therefore the step is exposed.
    // The step is also exposed if moving by 5% would cause intermittent
    // behavior where sometimes the slider would alternate by 1 or 2 steps.
    // Therefore the final decision is to use the step if there are
    // less than stops in the slider, otherwise, move by 5%.
    float max = step_range.Maximum().ToString().ToFloat();
    float min = step_range.Minimum().ToString().ToFloat();
    int num_stops = base::saturated_cast<int>((max - min) / step);
    constexpr int kNumStopsForFivePercentRule = 40;
    if (num_stops >= kNumStopsForFivePercentRule) {
      // No explicit step, and the step is very small -- don't expose a step
      // so that Talkback will move by 5% increments.
      *out_value = 0.0f;
      return false;
    }

    *out_value = step;
    return std::isfinite(*out_value);
  }

  switch (RawAriaRole()) {
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

  if (IsWebArea()) {
    DCHECK(GetDocument());
    return GetDocument()->Url();
  }

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
  // as a group child of the <input> control itself.
  switch (native_role_) {
    case ax::mojom::blink::Role::kColorWell:
    case ax::mojom::blink::Role::kComboBoxSelect:
    case ax::mojom::blink::Role::kDate:
    case ax::mojom::blink::Role::kDateTime:
    case ax::mojom::blink::Role::kInputTime:
    case ax::mojom::blink::Role::kTextFieldWithComboBox: {
      for (const auto& child : ChildrenIncludingIgnored()) {
        if (IsA<Document>(child->GetNode())) {
          return child.Get();
        }
      }
      return nullptr;
    }
    default:
#if DCHECK_IS_ON()
      for (const auto& child : ChildrenIncludingIgnored()) {
        DCHECK(!IsA<Document>(child->GetNode()) ||
               !child->ParentObject()->IsVisible())
            << "Chooser popup exists for " << native_role_
            << "\n* Child: " << child
            << "\n* Child's immediate parent: " << child->ParentObject();
      }
#endif
      return nullptr;
  }
}

String AXNodeObject::GetValueForControl() const {
  AXObjectSet visited;
  return GetValueForControl(visited);
}

String AXNodeObject::GetValueForControl(AXObjectSet& visited) const {
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
      const AtomicString& overridden_description = AriaAttribute(
          *list_items[selected_index], html_names::kAriaLabelAttr);
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
    unsigned int unmasked_text_length = inner_text.length();
    // If the inner text is empty, we return a null string to let the text
    // alternative algorithm continue searching for an accessible name.
    if (!unmasked_text_length) {
      return String();
    }

    if (!IsPasswordFieldAndShouldHideValue())
      return inner_text;

    if (!GetLayoutObject())
      return inner_text;

    const ComputedStyle* style = GetLayoutObject()->Style();
    if (!style)
      return inner_text;

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
    return AriaAttribute(html_names::kAriaValuetextAttr).GetString();
  }

  // Handle other HTML input elements that aren't text controls, like date and
  // time controls, by returning their value converted to text, with the
  // exception of checkboxes and radio buttons (which would return "on"), and
  // buttons which will return their name.
  // https://html.spec.whatwg.org/C/#dom-input-value
  if (const auto* input = DynamicTo<HTMLInputElement>(node)) {
    if (input->FormControlType() == FormControlType::kInputFile) {
      return input->FileStatusText();
    }

    if (input->FormControlType() != FormControlType::kInputButton &&
        input->FormControlType() != FormControlType::kInputCheckbox &&
        input->FormControlType() != FormControlType::kInputImage &&
        input->FormControlType() != FormControlType::kInputRadio &&
        input->FormControlType() != FormControlType::kInputReset &&
        input->FormControlType() != FormControlType::kInputSubmit) {
      return input->Value();
    }
  }

  if (RoleValue() == ax::mojom::blink::Role::kComboBoxMenuButton) {
    // An ARIA combobox can get value from inner contents.
    return TextFromDescendants(visited, nullptr, false);
  }

  return String();
}

String AXNodeObject::SlowGetValueForControlIncludingContentEditable() const {
  AXObjectSet visited;
  return SlowGetValueForControlIncludingContentEditable(visited);
}

String AXNodeObject::SlowGetValueForControlIncludingContentEditable(
    AXObjectSet& visited) const {
  if (IsNonAtomicTextField()) {
    Element* element = GetElement();
    return element ? element->GetInnerTextWithoutUpdate() : String();
  }
  return GetValueForControl(visited);
}

ax::mojom::blink::Role AXNodeObject::RawAriaRole() const {
  return aria_role_;
}

ax::mojom::blink::HasPopup AXNodeObject::HasPopup() const {
  if (const AtomicString& has_popup =
          AriaTokenAttribute(html_names::kAriaHaspopupAttr)) {
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

  if (AXObjectCache().GetAutofillSuggestionAvailability(AXObjectID()) !=
      WebAXAutofillSuggestionAvailability::kNoSuggestions) {
    return ax::mojom::blink::HasPopup::kMenu;
  }

  return AXObject::HasPopup();
}

ax::mojom::blink::IsPopup AXNodeObject::IsPopup() const {
  if (IsDetached() || !GetElement()) {
    return ax::mojom::blink::IsPopup::kNone;
  }
  const auto* html_element = DynamicTo<HTMLElement>(GetElement());
  if (!html_element) {
    return ax::mojom::blink::IsPopup::kNone;
  }
  if (RoleValue() == ax::mojom::blink::Role::kMenuListPopup) {
    return ax::mojom::blink::IsPopup::kAuto;
  }
  switch (html_element->PopoverType()) {
    case PopoverValueType::kNone:
      return ax::mojom::blink::IsPopup::kNone;
    case PopoverValueType::kAuto:
      return ax::mojom::blink::IsPopup::kAuto;
    case PopoverValueType::kHint:
      return ax::mojom::blink::IsPopup::kHint;
    case PopoverValueType::kManual:
      return ax::mojom::blink::IsPopup::kManual;
  }
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
static LayoutBlockFlow* GetNearestBlockFlow(LayoutObject* object) {
  LayoutObject* current = object;
  while (current) {
    if (auto* block_flow = DynamicTo<LayoutBlockFlow>(current)) {
      return block_flow;
    }
    current = current->Parent();
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// Returns true if |r1| and |r2| are both non-null, both inline, and are
// contained within the same LayoutBlockFlow.
static bool IsInSameBlockFlow(LayoutObject* r1, LayoutObject* r2) {
  if (!r1 || !r2)
    return false;
  if (!r1->IsInline() || !r2->IsInline())
    return false;
  LayoutBlockFlow* b1 = GetNearestBlockFlow(r1);
  LayoutBlockFlow* b2 = GetNearestBlockFlow(r2);
  return b1 && b2 && b1 == b2;
}

//
// Modify or take an action on an object.
//

bool AXNodeObject::OnNativeSetSelectedAction(bool selected) {
  auto* option = DynamicTo<HTMLOptionElement>(GetNode());
  if (!option) {
    return false;
  }

  HTMLSelectElement* select_element = option->OwnerSelectElement();
  if (!select_element) {
    return false;
  }

  if (!CanSetSelectedAttribute()) {
    return false;
  }

  AccessibilitySelectedState is_option_selected = IsSelected();
  if (is_option_selected == kSelectedStateUndefined) {
    return false;
  }

  bool is_selected = (is_option_selected == kSelectedStateTrue) ? true : false;
  if ((is_selected && selected) || (!is_selected && !selected)) {
    return false;
  }

  select_element->SelectOptionByAccessKey(To<HTMLOptionElement>(GetNode()));
  return true;
}

bool AXNodeObject::OnNativeSetValueAction(const String& string) {
  if (!GetNode() || !GetNode()->IsElementNode())
    return false;
  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsBoxModelObject())
    return false;

  auto* html_input_element = DynamicTo<HTMLInputElement>(*GetNode());
  if (html_input_element && layout_object->IsTextField()) {
    html_input_element->SetValue(
        string, TextFieldEventBehavior::kDispatchInputAndChangeEvent);
    return true;
  }

  if (auto* text_area_element = DynamicTo<HTMLTextAreaElement>(*GetNode())) {
    DCHECK(layout_object->IsTextArea());
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
  Node* node = GetNode();

  name_from = ax::mojom::blink::NameFrom::kNone;
  if (!node && !GetLayoutObject()) {
    return String();
  }

  if (IsA<HTMLSlotElement>(node) && node->IsInUserAgentShadowRoot() &&
      !recursive) {
    // User agent slots do not have their own name, but their subtrees can
    // contribute to ancestor names (where recursive == true).
    return String();
  }

  if (GetLayoutObject()) {
    std::optional<String> text_alternative = GetCSSAltText(GetElement());
    if (text_alternative) {
      name_from = ax::mojom::blink::NameFrom::kCssAltText;
      if (name_sources) {
        name_sources->push_back(NameSource(false));
        name_sources->back().type = name_from;
        name_sources->back().text = text_alternative.value();
      }
      return text_alternative.value();
    }
    if (GetLayoutObject()->IsBR()) {
      text_alternative = String("\n");
      found_text_alternative = true;
    } else if (GetLayoutObject()->IsText() &&
               (!recursive || !GetLayoutObject()->IsCounter())) {
      auto* layout_text = To<LayoutText>(GetLayoutObject());
      String visible_text = layout_text->PlainText();  // Actual rendered text.
      // If no text boxes we assume this is unrendered end-of-line whitespace.
      // TODO find robust way to deterministically detect end-of-line space.
      if (visible_text.empty()) {
        // No visible rendered text -- must be whitespace.
        // Either it is useful whitespace for separating words or not.
        if (layout_text->IsAllCollapsibleWhitespace()) {
          if (IsIgnored()) {
            return "";
          }
          // If no textboxes, this was whitespace at the line's end.
          text_alternative = " ";
        } else {
          text_alternative = layout_text->TransformedText();
        }
      } else {
        text_alternative = visible_text;
      }
      found_text_alternative = true;
    } else if (!recursive) {
      if (ListMarker* marker = ListMarker::Get(GetLayoutObject())) {
        text_alternative = marker->TextAlternative(*GetLayoutObject());
        found_text_alternative = true;
      }
    }

    if (found_text_alternative) {
      name_from = ax::mojom::blink::NameFrom::kContents;
      if (name_sources) {
        name_sources->push_back(NameSource(false));
        name_sources->back().type = name_from;
        name_sources->back().text = text_alternative.value();
      }
      // Ensure that text nodes count toward
      // kMaxDescendantsForTextAlternativeComputation when calculating the name
      // for their direct parent (see AXNodeObject::TextFromDescendants).
      visited.insert(this);
      return text_alternative.value();
    }
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
    // TODO(accessibility): Consider using `empty` check instead of `IsNull`.
    if (!value_for_name.IsNull()) {
      name_from = ax::mojom::blink::NameFrom::kValue;
      if (name_sources) {
        name_sources->push_back(NameSource(false));
        name_sources->back().type = ax::mojom::blink::NameFrom::kValue;
        name_sources->back().text = value_for_name;
      }
      return value_for_name;
    }
  }

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1 -- aria-label.
  String text_alternative = AriaTextAlternative(
      recursive, aria_label_or_description_root, visited, name_from,
      related_objects, name_sources, &found_text_alternative);
  if (found_text_alternative && !name_sources)
    return MaybeAppendFileDescriptionToName(text_alternative);

  // Step 2D from: http://www.w3.org/TR/accname-aam-1.1  -- native markup.
  text_alternative =
      NativeTextAlternative(visited, name_from, related_objects, name_sources,
                            &found_text_alternative);
  // An explicitly empty native text alternative can still be overridden if a
  // viable text alternative is found later in the search, so remember that it
  // was explicitly empty here but don't terminate the search yet unless we
  // already found something non-empty.
  const bool has_explicitly_empty_native_text_alternative =
      text_alternative.empty() &&
      name_from == ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty;
  if (!text_alternative.empty() && !name_sources) {
    return MaybeAppendFileDescriptionToName(text_alternative);
  }

  // Step 2F / 2G from: http://www.w3.org/TR/accname-aam-1.1 -- from content.
  if (ShouldIncludeContentInTextAlternative(
          recursive, aria_label_or_description_root, visited)) {
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
        return MaybeAppendFileDescriptionToName(text_alternative);
      }
    }
  }

  // Step 2I from: http://www.w3.org/TR/accname-aam-1.1
  // Use the tooltip text for the name if there was no other accessible name.
  // However, it does not make sense to do this if the object has a role
  // that prohibits name as specified in
  // https://w3c.github.io/aria/#namefromprohibited.
  // Preventing the tooltip for being used in the name causes it to be used for
  // the description instead.
  // This complies with https://w3c.github.io/html-aam/#att-title, which says
  // how to expose a title: "Either the accessible name, or the accessible
  // description, or Not mapped". There's nothing in HTML-AAM that explicitly
  // forbids this, and it seems reasonable for authors to use a tooltip on any
  // visible element without causing an accessibility error or user problem.
  // Note: if this is part of another label or description, it needs to be
  // computed as a name, in order to contribute to that.
  if (aria_label_or_description_root || !IsNameProhibited()) {
    String resulting_text = TextAlternativeFromTooltip(
        name_from, name_sources, &found_text_alternative, &text_alternative,
        related_objects);
    if (!resulting_text.empty()) {
      if (name_sources) {
        text_alternative = resulting_text;
      } else {
        return resulting_text;
      }
    }
  }

  String saved_text_alternative = GetSavedTextAlternativeFromNameSource(
      found_text_alternative, name_from, related_objects, name_sources);
  if (!saved_text_alternative.empty()) {
    return saved_text_alternative;
  }

  if (has_explicitly_empty_native_text_alternative) {
    // If the native text alternative is explicitly empty and we
    // never found another text alternative, then set name_source
    // to reflect the fact that there was an explicitly empty text
    // alternative. This is important because an empty `alt` attribute on an
    // <img> can be used to indicate that the image is presentational and should
    // be ignored by ATs.
    name_from = ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty;
  }

  return String();
}

static bool ShouldInsertSpaceBetweenObjectsIfNeeded(
    AXObject* previous,
    AXObject* next,
    ax::mojom::blink::NameFrom last_used_name_from,
    ax::mojom::blink::NameFrom name_from) {
  LayoutObject* next_layout = next->GetLayoutObject();
  LayoutObject* prev_layout = previous->GetLayoutObject();

  // If we're going between two LayoutObjects that are in separate
  // LayoutBoxes, add whitespace if it wasn't there already. Intuitively if
  // you have <span>Hello</span><span>World</span>, those are part of the same
  // LayoutBox so we should return "HelloWorld", but given
  // <div>Hello</div><div>World</div> the strings are in separate boxes so we
  // should return "Hello World".
  // https://www.w3.org/TR/css-display-3/#the-display-properties
  if (!IsInSameBlockFlow(next_layout, prev_layout)) {
    return true;
  }

  // Even if we are in the same block flow, let's make sure to add whitespace
  // if the layout objects define new formatting contexts for their children,
  // as is the case with the inline-* family of display properties.
  // So we want the following:
  //    <span style="display:inline-block;">Hello</span><span>World</span>
  //    <span style="display:inline-flex;">Hello</span><span>World</span>
  //    <span style="display:inline-grid;">Hello</span><span>World</span>
  //    <span style="display:inline-table;">Hello</span><span>World</span>
  // to return "Hello World". See "inner display type" in the CSS Display 3.0
  // spec: https://www.w3.org/TR/css-display-3/#the-display-properties
  CHECK(next_layout);
  CHECK(prev_layout);
  if (next_layout->IsAtomicInlineLevel() ||
      prev_layout->IsAtomicInlineLevel()) {
    return true;
  }

  // Even if it is in the same inline block flow, if we are using a text
  // alternative such as an ARIA label or HTML title, we should separate
  // the strings. Doing so is consistent with what is stated in the AccName
  // spec and with what is done in other user agents.
  switch (last_used_name_from) {
    case ax::mojom::blink::NameFrom::kNone:
    case ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty:
    case ax::mojom::blink::NameFrom::kContents:
    case ax::mojom::blink::NameFrom::kProhibited:
    case ax::mojom::blink::NameFrom::kProhibitedAndRedundant:
      break;
    case ax::mojom::blink::NameFrom::kAttribute:
    case ax::mojom::blink::NameFrom::kCaption:
    case ax::mojom::blink::NameFrom::kCssAltText:
    case ax::mojom::blink::NameFrom::kPlaceholder:
    case ax::mojom::blink::NameFrom::kRelatedElement:
    case ax::mojom::blink::NameFrom::kTitle:
    case ax::mojom::blink::NameFrom::kValue:
    case ax::mojom::blink::NameFrom::kPopoverAttribute:
      return true;
  }
  switch (name_from) {
    case ax::mojom::blink::NameFrom::kNone:
    case ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty:
    case ax::mojom::blink::NameFrom::kContents:
    case ax::mojom::blink::NameFrom::kProhibited:
    case ax::mojom::blink::NameFrom::kProhibitedAndRedundant:
      break;
    case ax::mojom::blink::NameFrom::kAttribute:
    case ax::mojom::blink::NameFrom::kCaption:
    case ax::mojom::blink::NameFrom::kCssAltText:
    case ax::mojom::blink::NameFrom::kPlaceholder:
    case ax::mojom::blink::NameFrom::kRelatedElement:
    case ax::mojom::blink::NameFrom::kTitle:
    case ax::mojom::blink::NameFrom::kValue:
    case ax::mojom::blink::NameFrom::kPopoverAttribute:
      return true;
  }

  // According to the AccName spec, we need to separate controls from text nodes
  // using a space.
  if (previous->IsControl() || next->IsControl())
    return true;

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
  for (auto* layout_object = previous->GetLayoutObject();
       layout_object && layout_object != next_layout;
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
  if (!CanHaveChildren()) {
    return recursive || !GetElement()
               ? String()
               : GetElement()->GetInnerTextWithoutUpdate();
  }

  StringBuilder accumulated_text;
  AXObject* previous = nullptr;
  ax::mojom::blink::NameFrom last_used_name_from =
      ax::mojom::blink::NameFrom::kNone;

  CHECK(!NeedsToUpdateCachedValues());

  const AXObjectVector& children = ChildrenIncludingIgnored();
#if defined(AX_FAIL_FAST_BUILD)
  base::AutoReset<bool> auto_reset(&is_computing_text_from_descendants_, true);
#endif
  wtf_size_t num_children = children.size();
  for (wtf_size_t index = 0; index < num_children; index++) {
    DCHECK_EQ(children.size(), num_children);
    if (index >= children.size()) {
      // TODO(accessibility) Remove this condition once we solve all causes of
      // the child list being altered during this loop.
      break;
    }
    AXObject* child = children[index];
    DCHECK(child);
    DCHECK(!child->IsDetached()) << child;
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
  // ComputeIsIgnored(), which isn't allowed to call
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
  HTMLInputElement* input = DynamicTo<HTMLInputElement>(label->Control());
  if (!input)
    return false;

  if (!input->GetLayoutObject() ||
      input->GetLayoutObject()->Style()->UsedVisibility() !=
          EVisibility::kVisible) {
    return false;
  }
  if (!input->IsCheckable()) {
    return false;
  }
  if (!IsNameFromLabelElement(input)) {
    return false;
  }

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
  DCHECK(!getting_bounds_) << "GetRelativeBounds reentrant: " << ToString();
  base::AutoReset<bool> reentrancy_protector(&getting_bounds_, true);
#endif

  *out_container = nullptr;
  out_bounds_in_container = gfx::RectF();
  out_container_transform.MakeIdentity();

  if (RoleValue() == ax::mojom::blink::Role::kMenuListOption) {
    // When a <select> is collapsed, the bounds of its options are the same as
    // that of the containing <select>. Falling through will achieve this.
    // TODO(accessibility): Support bounding boxes for <optgroup>. Could
    // union the rect of the first and last option in it.
    auto* select = To<HTMLOptionElement>(GetNode())->OwnerSelectElement();
    if (auto* ax_select = AXObjectCache().Get(select)) {
      if (ax_select->IsExpanded() == kExpandedExpanded) {
        auto* options_bounds = AXObjectCache().GetOptionsBounds(*ax_select);
        if (options_bounds) {
          unsigned int index = static_cast<unsigned int>(
              To<HTMLOptionElement>(GetNode())->index());
          // Some <option> bounding boxes may not be sent, as a performance
          // optimization. For example, only the first 1000 options may have
          // bounding boxes. If no bounding box is available, then we serialize
          // the option with everything except for that information.
          if (index < options_bounds->size()) {
            out_bounds_in_container = gfx::RectF(options_bounds->at(index));
          }
          return;
        }
      }
    }
  }

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
    if (position_provider->GetLayoutObject()) {
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
  auto* table = To<HTMLTableElement>(GetNode());
  auto* thead = table->tHead();
  auto* tfoot = table->tFoot();
  for (Node* node = LayoutTreeBuilderTraversal::FirstChild(*GetElement()); node;
       node = LayoutTreeBuilderTraversal::NextSibling(*node)) {
    if (Element* child = DynamicTo<Element>(node)) {
      if (child == thead || child == tfoot) {
        // Only 1 thead and 1 tfoot are allowed.
        continue;
      }
      if (IsA<HTMLTableSectionElement>(child) &&
          child->HasTagName(html_names::kTbodyTag)) {
        // Multiple <tbody>s are valid, but only 1 thead or tfoot.
        continue;
      }
      if (!child->GetLayoutObject() &&
          child->HasTagName(html_names::kColgroupTag)) {
        continue;
      }
      if (IsA<HTMLTableCaptionElement>(child) && child == table->caption()) {
        continue;  // Only one caption is valid.
      }
    } else if (!node->GetLayoutObject()) {
      continue;
    }
    return false;
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
    // The OffsetMapping class doesn't map layout inline objects to their text
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
    if (layout_text->HasEmptyText()) {
      return AXObject::TextOffsetInFormattingContext(offset);
    }
  }

  LayoutBlockFlow* formatting_context =
      OffsetMapping::GetInlineFormattingContextOf(*layout_obj);
  if (!formatting_context || formatting_context == layout_obj)
    return AXObject::TextOffsetInFormattingContext(offset);

  // If "formatting_context" is not a Layout NG object, the offset mappings will
  // be computed on demand and cached.
  const OffsetMapping* inline_offset_mapping =
      InlineNode::GetOffsetMapping(formatting_context);
  if (!inline_offset_mapping)
    return AXObject::TextOffsetInFormattingContext(offset);

  const base::span<const OffsetMappingUnit> mapping_units =
      inline_offset_mapping->GetMappingUnitsForLayoutObject(*layout_obj);
  if (mapping_units.empty())
    return AXObject::TextOffsetInFormattingContext(offset);
  return static_cast<int>(mapping_units.front().TextContentStart()) + offset;
}

//
// Inline text boxes.
//

bool AXNodeObject::ShouldLoadInlineTextBoxes() const {
  CHECK(!IsDetached());

  if (!CanHaveInlineTextBoxChildren(this)) {
    return false;
  }

  if (!AXObjectCache().GetAXMode().has_mode(ui::AXMode::kInlineTextBoxes)) {
    return false;
  }

#if defined(REDUCE_AX_INLINE_TEXTBOXES)
  // On Android, once an object has loaded inline text boxes, it will keep
  // them refreshed.
  return always_load_inline_text_boxes_;
#else
  // Other platforms keep all inline text boxes in the tree and refreshed,
  // depending on the AXMode.
  return true;
#endif
}

void AXNodeObject::LoadInlineTextBoxes() {
#if DCHECK_IS_ON()
  DCHECK(GetDocument()->Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle "
      << GetDocument()->Lifecycle().ToString();
#endif

  std::queue<AXID> work_queue;
  work_queue.push(AXObjectID());

  while (!work_queue.empty()) {
    AXObject* work_obj = AXObjectCache().ObjectFromAXID(work_queue.front());
    work_queue.pop();
    if (!work_obj || !work_obj->IsIncludedInTree()) {
      continue;
    }

    if (CanHaveInlineTextBoxChildren(work_obj)) {
      if (work_obj->CachedChildrenIncludingIgnored().empty()) {
        // We only need to add inline textbox children if they aren't present.
        // Although some platforms (e.g. Android), load inline text boxes
        // on subtrees that may later be stale, once they are stale, the old
        // inline text boxes are cleared because SetNeedsToUpdateChildren()
        // calls ClearChildren().
        work_obj->LoadInlineTextBoxesHelper();
      }
    } else {
      for (const auto& child : work_obj->ChildrenIncludingIgnored())
        work_queue.push(child->AXObjectID());
    }
  }

  // If the work was deferred via ChildrenChanged(), update accessibility
  // to force that work to be performed now.
  if (!AXObjectCache().lifecycle().StateAllowsImmediateTreeUpdates()) {
    AXObjectCache().UpdateAXForAllDocuments();
  }
}

void AXNodeObject::LoadInlineTextBoxesHelper() {
  // The inline textbox children start empty.
  DCHECK(CachedChildrenIncludingIgnored().empty());

#if defined(REDUCE_AX_INLINE_TEXTBOXES)
  // Keep inline text box children up-to-date for this object in the future.
  // This is only necessary on Android, which tries to skip inline text boxes
  // for most objects.
  always_load_inline_text_boxes_ = true;
#endif

  if (AXObjectCache().lifecycle().StateAllowsImmediateTreeUpdates()) {
    // Can only add new objects while processing deferred events.
    AddInlineTextBoxChildren();
    // Avoid adding these children twice.
    SetNeedsToUpdateChildren(false);
    // If inline text box children were added, mark the node dirty so that the
    // results are serialized.
    if (!CachedChildrenIncludingIgnored().empty()) {
      AXObjectCache().AddDirtyObjectToSerializationQueue(
          this, ax::mojom::blink::EventFrom::kNone,
          ax::mojom::blink::Action::kNone, {});
    }
  } else {
    // Wait until processing deferred events.
    AXObjectCache().ChildrenChanged(this);
  }
}

void AXNodeObject::AddInlineTextBoxChildren() {
  CHECK(GetDocument());
  CHECK(ShouldLoadInlineTextBoxes());
  CHECK(GetLayoutObject());
  GetLayoutObject()->CheckIsNotDestroyed();
  CHECK(GetLayoutObject()->IsText());
  CHECK(!GetLayoutObject()->NeedsLayout());
  CHECK(AXObjectCache().GetAXMode().has_mode(ui::AXMode::kInlineTextBoxes));
  CHECK(!AXObjectCache().GetAXMode().HasExperimentalFlags(
      ui::AXMode::kExperimentalFormControls))
      << "Form controls mode should not have inline text boxes turned on.";
  CHECK(AXObjectCache().lifecycle().StateAllowsImmediateTreeUpdates())
      << AXObjectCache();

  auto* layout_text = To<LayoutText>(GetLayoutObject());
  for (auto* box = layout_text->FirstAbstractInlineTextBox(); box;
       box = box->NextInlineTextBox()) {
    AXObject* ax_box = AXObjectCache().GetOrCreate(box, this);
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
  AddChildAndCheckIncluded(AXObjectCache().ValidationMessageObjectIfInvalid());
}

void AXNodeObject::AddImageMapChildren() {
  HTMLMapElement* map = GetMapForImage(GetNode());
  if (!map)
    return;

  HTMLImageElement* curr_image_element = DynamicTo<HTMLImageElement>(GetNode());
  DCHECK(curr_image_element);
  DCHECK(curr_image_element->IsLink());
  DCHECK(
      !curr_image_element->FastGetAttribute(html_names::kUsemapAttr).empty());

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
    return;
  }

  // Yes, this is the primary image.

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
    if (html_select_element->UsesMenuList()) {
      AddChildAndCheckIncluded(html_select_element->PopupRootAXObject());
    }
    return;
  }

  auto* html_input_element = DynamicTo<HTMLInputElement>(GetNode());
  if (html_input_element) {
    AddChildAndCheckIncluded(html_input_element->PopupRootAXObject());
  }
}

void AXNodeObject::AddPseudoElementChildrenFromLayoutTree() {
  // Children are added this way only for pseudo-element subtrees.
  // See AXObject::ShouldUseLayoutObjectTraversalForChildren().
  if (!IsVisible() || !GetLayoutObject()) {
    DCHECK(GetNode());
    DCHECK(GetNode()->IsPseudoElement());
    return;  // Can't add children for hidden or display-locked pseudo elements.
  }
  LayoutObject* child = GetLayoutObject()->SlowFirstChild();
  while (child) {
    // All added pseudo element descendants are included in the tree.
    if (AXObject* ax_child = AXObjectCache().GetOrCreate(child, this)) {
      DCHECK(AXObjectCacheImpl::IsRelevantPseudoElementDescendant(*child));
      AddChildAndCheckIncluded(ax_child);
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

  // If node is ReadingFlowContainer or if it is display contents and its layout
  // parent is ReadingFlowContainer, then we should follow reading-flow order.
  // In this case, the same list of children will be added as in the simple
  // case using only LayoutTreeBuilderTraversal children, with no additions or
  // removals, but in the order defined in CSS.
  // TODO(crbug.com/346979043): If display: contents is a reading flow item,
  // this order will be different from the reading flow in focus navigation.
  Element* element = GetElement();
  Element* closest_layout_parent =
      element && element->HasDisplayContentsStyle()
          ? LayoutTreeBuilderTraversal::LayoutParentElement(*element)
          : element;
  if (closest_layout_parent &&
      closest_layout_parent->IsReadingFlowContainer()) {
    HeapHashSet<Member<Node>> ax_children_added;
    // Add all reading order items first, in the correct order.
    for (Element* reading_item :
         closest_layout_parent->GetLayoutBox()->ReadingFlowElements()) {
      // Filter to only add node child if it is a direct child of current
      // element.
      if (LayoutTreeBuilderTraversal::Parent(*reading_item) == element) {
        AddNodeChild(reading_item);
        ax_children_added.insert(reading_item);
      }
    }
    // Add all non-reading order items at the end of the reading flow.
    for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node_); child;
         child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
      if (!ax_children_added.Contains(child)) {
        AddNodeChild(child);
#if DCHECK_IS_ON()
        ax_children_added.insert(child);
#endif
      }
    }
#if DCHECK_IS_ON()
    // At this point, the number of AXObject children added should equal the
    // number of LayoutTreeBuilderTraversal children.
    size_t num_layout_tree_children = 0;
    for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node_); child;
         child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
      DCHECK(ax_children_added.Contains(child));
      ++num_layout_tree_children;
    }
    DCHECK_EQ(ax_children_added.size(), num_layout_tree_children);
#endif
  } else {
    for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node_); child;
         child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
      AddNodeChild(child);
    }
  }
}

void AXNodeObject::AddMenuListChildren() {
  auto* select = To<HTMLSelectElement>(GetNode());

  if (select->IsAppearanceBasePicker()) {
    // In appearance: base-select (customizable select), the children of the
    // combobox is the displayed data list.
    AddNodeChild(select->PopoverForAppearanceBase());
    return;
  }

  AddNodeChildren();
}

void AXNodeObject::AddMenuListPopupChildren() {
  auto* select = To<HTMLSelectElement>(ParentObject()->GetNode());

  if (select->IsAppearanceBasePicker()) {
    // In appearance: base-select (customizable select), the children of the
    // popup are all of the natural dom children of the <select>.
    for (Node* child = NodeTraversal::FirstChild(*select); child;
         child = NodeTraversal::NextSibling(*child)) {
      if (child == select->SlottedButton()) {
        // The displayed button does not need to be part of the a11y tree. It
        // is not in the popup, and for accessibility purposes it is redundant
        // with the <select>.
        continue;
      }
      AddNodeChild(child);
    }
    return;
  }

  // In appearance: auto/none, the children of the popup are the flat tree
  // children of the slot associated with the popup.
  AddNodeChildren();
}

void AXNodeObject::AddOwnedChildren() {
  AXObjectVector owned_children;
  AXObjectCache().ValidatedAriaOwnedChildren(this, owned_children);

  DCHECK(owned_children.size() == 0 || AXRelationCache::IsValidOwner(this))
      << "This object is not allowed to use aria-owns, but it is.\n"
      << this;

  // Always include owned children.
  for (const auto& owned_child : owned_children) {
    DCHECK(owned_child->GetNode());
    DCHECK(AXRelationCache::IsValidOwnedChild(*owned_child->GetNode()))
        << "This object is not allowed to be owned, but it is.\n"
        << owned_child;
    AddChildAndCheckIncluded(owned_child, true);
  }
}

void AXNodeObject::AddChildrenImpl() {
#define CHECK_ATTACHED()                                               \
  if (IsDetached()) {                                                  \
    NOTREACHED_IN_MIGRATION() << "Detached adding children: " << this; \
    return;                                                            \
  }

  CHECK(NeedsToUpdateChildren());
  CHECK(CanHaveChildren());

  if (ShouldLoadInlineTextBoxes() && HasLayoutText(this)) {
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

  if (RoleValue() == ax::mojom::blink::Role::kComboBoxSelect) {
    AddMenuListChildren();
  } else if (RoleValue() == ax::mojom::blink::Role::kMenuListPopup) {
    AddMenuListPopupChildren();
  } else if (HasValidHTMLTableStructureAndLayout()) {
    AddTableChildren();
  } else if (ShouldUseLayoutObjectTraversalForChildren()) {
    AddPseudoElementChildrenFromLayoutTree();
  } else {
    AddNodeChildren();
  }
  CHECK_ATTACHED();

  AddPopupChildren();
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
  DCHECK(children_.empty()) << "\nParent still has " << children_.size()
                            << " children before adding:" << "\nParent is "
                            << this << "\nFirst child is " << children_[0];
#endif

#if defined(AX_FAIL_FAST_BUILD)
  SANITIZER_CHECK(!is_computing_text_from_descendants_)
      << "Should not attempt to simultaneously compute text from descendants "
         "and add children on: "
      << this;
  SANITIZER_CHECK(!is_adding_children_) << " Reentering method on " << this;
  base::AutoReset<bool> reentrancy_protector(&is_adding_children_, true);
#endif

  AddChildrenImpl();
  SetNeedsToUpdateChildren(false);

#if DCHECK_IS_ON()
  // All added children must be attached.
  for (const auto& child : children_) {
    DCHECK(!child->IsDetached()) << "A brand new child was detached.\n"
                                 << child << "\n ... of parent " << this;
  }
#endif
}

// Add non-owned children that are backed with a DOM node.
void AXNodeObject::AddNodeChild(Node* node) {
  if (!node)
    return;

  AXObject* ax_child = AXObjectCache().Get(node);
  CHECK(!ax_child || !ax_child->IsDetached());
  // Should not have another parent unless owned.
  if (AXObjectCache().IsAriaOwned(ax_child))
    return;  // Do not add owned children to their natural parent.

  AXObject* ax_cached_parent =
      ax_child ? ax_child->ParentObjectIfPresent() : nullptr;

  if (!ax_child) {
    ax_child =
        AXObjectCache().CreateAndInit(node, node->GetLayoutObject(), this);
    if (!ax_child) {
      return;
    }
    CHECK(!ax_child->IsDetached());
  }

  AddChild(ax_child);

  // If we are adding an included child, check to see that it didn't have a
  // different previous parent, because that indicates something strange is
  // happening -- we shouldn't be stealing AXObjects from other parents here.
  bool did_add_child_as_included =
      children_.size() && children_[children_.size() - 1] == ax_child;
  if (did_add_child_as_included && ax_cached_parent) {
    CHECK(ax_child->IsIncludedInTree());
    DUMP_WILL_BE_CHECK(ax_cached_parent->AXObjectID() == AXObjectID())
        << "Newly added child shouldn't have a different preexisting parent:"
        << "\nChild = " << ax_child << "\nNew parent = " << this
        << "\nPreexisting parent = " << ax_cached_parent;
  }
}

#if DCHECK_IS_ON()
void AXNodeObject::CheckValidChild(AXObject* child) {
  DCHECK(!child->IsDetached()) << "Cannot add a detached child.\n" << child;

  Node* child_node = child->GetNode();

  // <area> children should only be added via AddImageMapChildren(), as the
  // descendants of an <image usemap> -- never alone or as children of a <map>.
  if (IsA<HTMLAreaElement>(child_node)) {
    AXObject* ancestor = this;
    while (ancestor && !IsA<HTMLImageElement>(ancestor->GetNode()))
      ancestor = ancestor->ParentObject();
    DCHECK(ancestor && IsA<HTMLImageElement>(ancestor->GetNode()))
        << "Area elements can only be added by image parents: " << child
        << " had a parent of " << this;
  }

  DCHECK(!IsA<HTMLFrameElementBase>(GetNode()) ||
         IsA<Document>(child->GetNode()))
      << "Cannot have a non-document child of a frame or iframe."
      << "\nChild: " << child << "\nParent: " << child->ParentObject();
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
  DCHECK(child->CachedIsIncludedInTree());
  AddChild(child, is_from_aria_owns);
}

void AXNodeObject::InsertChild(AXObject* child,
                               unsigned index,
                               bool is_from_aria_owns) {
  if (!child)
    return;

  DCHECK(CanHaveChildren());
  DCHECK(!child->IsDetached()) << "Cannot add a detached child: " << child;
  // Enforce expected aria-owns status:
  // - Don't add a non-aria-owned child when called from AddOwnedChildren().
  // - Don't add an aria-owned child to its natural parent, because it will
  //   already be the child of the element with aria-owns.
  DCHECK_EQ(AXObjectCache().IsAriaOwned(child), is_from_aria_owns);

  // Set the parent:
  // - For a new object it will have already been set.
  // - For a reused, older object, it may need to be changed to a new parent.
  child->SetParent(this);

  if (ChildrenNeedToUpdateCachedValues()) {
    child->InvalidateCachedValues();
  }
  // Update cached values preemptively, but don't allow children changed to be
  // called on the parent if the ignored state changes, as we are already
  // recomputing children and don't want to recurse.
  child->UpdateCachedAttributeValuesIfNeeded(
      /*notify_parent_of_ignored_changes*/ false);

  if (!child->IsIncludedInTree()) {
    DCHECK(!is_from_aria_owns)
        << "Owned elements must be in tree: " << child
        << "\nRecompute included in tree: "
        << child->ComputeIsIgnoredButIncludedInTree();

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
            << "Cannot add a detached child: " << "\n* Child: " << children[i]
            << "\n* Parent: " << child << "\n* Grandparent: " << this;
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

  // A child tree has been stitched onto this node, hiding its usual subtree.
  if (child_tree_id()) {
    return false;
  }

  // Notes:
  // * Native text fields expose any children they might have, complying
  // with browser-side expectations that editable controls have children
  // containing the actual text content.
  // * ARIA roles with childrenPresentational:true in the ARIA spec expose
  // their contents to the browser side, allowing platforms to decide whether
  // to make them a leaf, ensuring that focusable content cannot be hidden,
  // and improving stability in Blink.
  bool result = !GetElement() || AXObject::CanHaveChildren(*GetElement());
  switch (native_role_) {
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kListBoxOption:
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
      DCHECK(!result) << "Expected to disallow children for:" << "\n* Node: "
                      << GetNode() << "\n* Layout Object: " << GetLayoutObject()
                      << "\n* Native role: " << native_role_
                      << "\n* Aria role: " << RawAriaRole();
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
      if (!current->GetElement()) {
        // TODO(crbug.com/1524124): Investigate and fix why this gets hit.
        DUMP_WILL_BE_NOTREACHED()
            << "An AXObject* that is a link should always have an element.\n"
            << this << "\n"
            << current;
      }
      return current->GetElement();
    }
    current = current->ParentObject();
  }

  return nullptr;
}

Document* AXNodeObject::GetDocument() const {
  if (GetNode()) {
    return &GetNode()->GetDocument();
  }
  if (GetLayoutObject()) {
    return &GetLayoutObject()->GetDocument();
  }
  return nullptr;
}

Node* AXNodeObject::GetNode() const {
  if (IsDetached()) {
    DCHECK(!node_);
    return nullptr;
  }

  DCHECK(!GetLayoutObject() || GetLayoutObject()->GetNode() == node_)
      << "If there is an associated layout object, its node should match the "
         "associated node of this accessibility object.\n"
      << this;
  return node_.Get();
}

LayoutObject* AXNodeObject::GetLayoutObject() const {
  return layout_object_;
}

bool AXNodeObject::OnNativeBlurAction() {
  Document* document = GetDocument();
  Node* node = GetNode();
  if (!document || !node) {
    return false;
  }

  // An AXObject's node will always be of type `Element`, `Document` or
  // `Text`. If the object we're currently on is associated with the currently
  // focused element or the document object, we want to clear the focus.
  // Otherwise, no modification is needed.
  Element* element = GetElement();
  if (element) {
    element->blur();
    return true;
  }

  if (IsA<Document>(GetNode())) {
    document->ClearFocusedElement();
    return true;
  }

  return false;
}

bool AXNodeObject::OnNativeFocusAction() {
  Document* document = GetDocument();
  Node* node = GetNode();
  if (!document || !node)
    return false;

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
    if (Page* page = document->GetPage()) {
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

  // Forward the focus in an appearance:base-select <select> to the button,
  // which actually handles the focus.
  // TODO(accessibility) Try to remove after crrev.com/c/5800883 lands.
  if (auto* select = DynamicTo<HTMLSelectElement>(element)) {
    if (auto* button = select->SlottedButton()) {
      element = button;
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // If this node is already the currently focused node, then calling
  // focus() won't do anything.  That is a problem when focus is removed
  // from the webpage to chrome, and then returns.  In these cases, we need
  // to do what keyboard and mouse focus do, which is reset focus first.
  if (document->FocusedElement() == element) {
    document->ClearFocusedElement();

    // Calling ClearFocusedElement could result in changes to the document,
    // like this AXObject becoming detached.
    if (IsDetached()) {
      return false;
    }
  }
#endif

  element->Focus(FocusParams(FocusTrigger::kUserGesture));

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

void AXNodeObject::SelectedOptions(AXObjectVector& options) const {
  if (auto* select = DynamicTo<HTMLSelectElement>(GetNode())) {
    for (auto* const option : *select->selectedOptions()) {
      AXObject* ax_option = AXObjectCache().Get(option);
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
    if (active_descendant) {
      if (active_descendant->IsSelectedFromFocus()) {
        // In single selection containers, selection follows focus, so a
        // selection changed event must be fired. This ensures the AT is
        // notified that the selected state has changed, so that it does not
        // read "unselected" as the user navigates through the items.
        AXObjectCache().HandleAriaSelectedChangedWithCleanLayout(
            active_descendant->GetNode());
      } else if (active_descendant->RoleValue() ==
                 ax::mojom::blink::Role::kRow) {
        // Active descendant rows must be marked dirty because that can make
        // them gain accessible name from contents
        // (see AXObject::SupportsNameFromContents).
        AXObjectCache().MarkAXObjectDirtyWithCleanLayout(active_descendant);
      }
    }

    // Mark this node dirty. AXEventGenerator will automatically infer
    // that the active descendant changed.
    AXObjectCache().MarkAXObjectDirtyWithCleanLayout(this);
  }
}

AXObject::AXObjectVector AXNodeObject::ErrorMessage() const {
  if (GetInvalidState() == ax::mojom::blink::InvalidState::kFalse)
    return AXObjectVector();

  AXObjectVector aria_error_messages =
      RelationVectorFromAria(html_names::kAriaErrormessageAttr);
  if (aria_error_messages.size() > 0) {
    return aria_error_messages;
  }

  AXObjectVector html_error_messages = ErrorMessageFromHTML();
  if (html_error_messages.size() > 0) {
    return html_error_messages;
  }

  return AXObjectVector();
}

AXObject::AXObjectVector AXNodeObject::RelationVectorFromAria(
    const QualifiedName& attr_name) const {
  Element* el = GetElement();
  if (!el) {
    return AXObjectVector();
  }

  HeapVector<Member<Element>> elements_from_attribute;
  if (!ElementsFromAttribute(el, elements_from_attribute, attr_name)) {
    return AXObjectVector();
  }

  AXObjectVector objects;
  for (Element* element : elements_from_attribute) {
    AXObject* obj = AXObjectCache().Get(element);
    if (obj && !obj->IsIgnored()) {
      objects.push_back(obj);
    }
  }
  return objects;
}

AXObject::AXObjectVector AXNodeObject::ErrorMessageFromHTML() const {
  // This can only be visible for a focused
  // control. Corollary: if there is a visible validationMessage alert box, then
  // it is related to the current focus.
  if (this != AXObjectCache().FocusedObject()) {
    return AXObjectVector();
  }

  AXObject* native_error_message =
      AXObjectCache().ValidationMessageObjectIfInvalid();
  if (native_error_message && !native_error_message->IsDetached()) {
    CHECK_GE(native_error_message->IndexInParent(), 0);
    return AXObjectVector({native_error_message});
  }

  return AXObjectVector();
}

String AXNodeObject::TextAlternativeFromTooltip(
    ax::mojom::blink::NameFrom& name_from,
    NameSources* name_sources,
    bool* found_text_alternative,
    String* text_alternative,
    AXRelatedObjectVector* related_objects) const {
  if (!GetElement()) {
    return String();
  }
  name_from = ax::mojom::blink::NameFrom::kTitle;
  const AtomicString& title = GetElement()->FastGetAttribute(kTitleAttr);
  String title_text = TextAlternativeFromTitleAttribute(
      title, name_from, name_sources, found_text_alternative);
  // Do not use if empty or if redundant with inner text.
  if (!title_text.empty()) {
    *text_alternative = title_text;
    return title_text;
  }

  auto* form_control = DynamicTo<HTMLFormControlElement>(GetElement());
  if (!form_control) {
    return String();
  }

  auto popover_target = form_control->popoverTargetElement();
  if (!popover_target.popover ||
      popover_target.popover->PopoverType() != PopoverValueType::kHint) {
    return String();
  }

  DCHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());

  name_from = ax::mojom::blink::NameFrom::kPopoverAttribute;
  if (name_sources) {
    name_sources->push_back(
        NameSource(*found_text_alternative, html_names::kPopovertargetAttr));
    name_sources->back().type = name_from;
  }
  AXObject* popover_ax_object = AXObjectCache().Get(popover_target.popover);

  // Hint popovers are used for text if and only if all of the contents are
  // plain, e.g. have no interesting semantic or interactive elements.
  // Otherwise, the hint will be exposed via the kDetails relationship. The
  // motivation for this is that by reusing the simple mechanism of titles,
  // screen reader users can easily access the information of plain hints
  // without having to navigate to it, making the content more accessible.
  // However, in the case of rich hints, a kDetails relationship is required to
  // ensure that users are able to access and interact with the hint as they can
  // navigate to it using commands.
  if (!popover_ax_object || !popover_ax_object->IsPlainContent()) {
    return String();
  }
  AXObjectSet visited;
  String popover_text =
      RecursiveTextAlternative(*popover_ax_object, popover_ax_object, visited);
  // Do not use if redundant with inner text.
  if (popover_text.StripWhiteSpace() ==
      GetElement()->GetInnerTextWithoutUpdate().StripWhiteSpace()) {
    return String();
  }
  *text_alternative = popover_text;
  if (related_objects) {
    related_objects->push_back(MakeGarbageCollected<NameSourceRelatedObject>(
        popover_ax_object, popover_text));
  }

  if (name_sources) {
    NameSource& source = name_sources->back();
    source.related_objects = *related_objects;
    source.text = *text_alternative;
    *found_text_alternative = true;
  }

  return popover_text;
}

String AXNodeObject::TextAlternativeFromTitleAttribute(
    const AtomicString& title,
    ax::mojom::blink::NameFrom& name_from,
    NameSources* name_sources,
    bool* found_text_alternative) const {
  DCHECK(GetElement());
  String text_alternative;
  if (name_sources) {
    name_sources->push_back(NameSource(*found_text_alternative, kTitleAttr));
    name_sources->back().type = name_from;
  }
  name_from = ax::mojom::blink::NameFrom::kTitle;
  if (!title.IsNull() &&
      String(title).StripWhiteSpace() !=
          GetElement()->GetInnerTextWithoutUpdate().StripWhiteSpace()) {
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
// https://www.w3.org/TR/html-aam-1.0/#accessible-name-and-description-computation
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

  if (auto* option_element = DynamicTo<HTMLOptionElement>(GetNode())) {
    if (option_element->HasOneTextChild()) {
      // Use the DisplayLabel() method if there are no interesting children.
      // If there are interesting children, fall through and compute the name
      // from contents rather, so that descendant markup is respected.
      name_from = ax::mojom::blink::NameFrom::kContents;
      text_alternative = option_element->DisplayLabel();
      if (!text_alternative.empty()) {
        if (name_sources) {
          name_sources->push_back(NameSource(*found_text_alternative));
          name_sources->back().type = name_from;
          name_sources->back().text = text_alternative;
          *found_text_alternative = true;
        }
        return text_alternative;
      }
    }
  }

  if (auto* opt_group_element = DynamicTo<HTMLOptGroupElement>(GetNode())) {
    name_from = ax::mojom::blink::NameFrom::kAttribute;
    text_alternative = opt_group_element->GroupLabelText();
    if (!text_alternative.empty()) {
      if (name_sources) {
        name_sources->push_back(NameSource(*found_text_alternative));
        name_sources->back().type = name_from;
        name_sources->back().text = text_alternative;
        *found_text_alternative = true;
      }
      return text_alternative;
    }
  }

  // 5.1/5.5 Text inputs, Other labelable Elements
  // If you change this logic, update AXNodeObject::IsNameFromLabelElement, too.
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
    if (!alt.empty()) {
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

    // title attr or popover
    String resulting_text = TextAlternativeFromTooltip(
        name_from, name_sources, found_text_alternative, &text_alternative,
        related_objects);
    if (!resulting_text.empty()) {
      if (name_sources) {
        text_alternative = resulting_text;
      } else {
        return resulting_text;
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

  // <input type="file">
  if (input_element &&
      input_element->FormControlType() == FormControlType::kInputFile) {
    // Append label of inner shadow root button + value attribute.
    name_from = ax::mojom::blink::NameFrom::kContents;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kValueAttr));
      name_sources->back().type = name_from;
    }
    if (ShadowRoot* shadow_root = input_element->UserAgentShadowRoot()) {
      text_alternative =
          To<HTMLInputElement>(shadow_root->firstElementChild())->Value();
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }
  }

  // 5.1 Text inputs - step 3 (placeholder attribute)
  if (html_element && html_element->IsTextControl()) {
    // title attr
    String resulting_text = TextAlternativeFromTooltip(
        name_from, name_sources, found_text_alternative, &text_alternative,
        related_objects);
    if (!resulting_text.empty()) {
      if (name_sources) {
        text_alternative = resulting_text;
      } else {
        return resulting_text;
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
        AriaAttribute(html_names::kAriaPlaceholderAttr);
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
  if (IsA<HTMLImageElement>(GetNode()) || IsA<HTMLAreaElement>(GetNode())) {
    // alt
    const AtomicString& alt = GetElement()->FastGetAttribute(kAltAttr);
    const bool is_empty = alt.empty() && !alt.IsNull();
    name_from = is_empty ? ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty
                         : ax::mojom::blink::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kAltAttr));
      name_sources->back().type = name_from;
    }
    if (!alt.empty()) {
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
      AXObject* caption_ax_object = AXObjectCache().Get(caption);
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
    const AtomicString& summary =
        GetElement()->FastGetAttribute(html_names::kSummaryAttr);
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
      // TODO(accessibility): In most cases <desc> and <title> can
      // participate in the recursive text alternative calculation. However
      // when the <desc> or <title> is the child of a <use>,
      // |AXObjectCache::GetOrCreate| will fail when
      // |AXObject::ComputeNonARIAParent| returns null because the <use>
      // element's subtree isn't visited by LayoutTreeBuilderTraversal. In
      // addition, while aria-label and other text alternative sources are
      // are technically valid on SVG <desc> and <title>, it is not clear if
      // user agents must expose their values. Therefore until we hear
      // otherwise, just use the inner text. See
      // https://github.com/w3c/svgwg/issues/867
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
      AXObject* legend_ax_object = AXObjectCache().Get(legend);
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
  if (Document* document = DynamicTo<Document>(GetNode())) {
    if (document) {
      name_from = ax::mojom::blink::NameFrom::kAttribute;
      if (name_sources) {
        name_sources->push_back(
            NameSource(found_text_alternative, html_names::kAriaLabelAttr));
        name_sources->back().type = name_from;
      }
      if (Element* document_element = document->documentElement()) {
        const AtomicString& aria_label =
            AriaAttribute(*document_element, html_names::kAriaLabelAttr);
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

// static
String AXNodeObject::GetSavedTextAlternativeFromNameSource(
    bool found_text_alternative,
    ax::mojom::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources) {
  name_from = ax::mojom::blink::NameFrom::kNone;
  if (!name_sources || !found_text_alternative) {
    return String();
  }

  for (NameSource& name_source : *name_sources) {
    if (name_source.text.empty() || name_source.superseded) {
      continue;
    }

    name_from = name_source.type;
    if (!name_source.related_objects.empty()) {
      *related_objects = name_source.related_objects;
    }
    return name_source.text;
  }

  return String();
}

// This is not part of the spec, but we think it's a worthy addition: if the
// labelled input is of type="file", we append the chosen file name to it. We do
// this because this type of input is actually exposed as a button, and buttons
// may not have a "value" field. An unlabelled input is manager later in this
// function, it's named with the default text in the button, 'Choose File', plus
// the file name.
String AXNodeObject::MaybeAppendFileDescriptionToName(
    const String& name) const {
  const auto* input_element = DynamicTo<HTMLInputElement>(GetNode());
  if (!input_element ||
      input_element->FormControlType() != FormControlType::kInputFile) {
    return name;
  }

  String displayed_file_path = GetValueForControl();
  if (!displayed_file_path.empty()) {
    if (GetTextDirection() == ax::mojom::blink::WritingDirection::kRtl)
      return name + " :" + displayed_file_path;
    else
      return name + ": " + displayed_file_path;
  }
  return name;
}

bool AXNodeObject::ShouldIncludeContentInTextAlternative(
    bool recursive,
    const AXObject* aria_label_or_description_root,
    AXObjectSet& visited) const {
  if (!aria_label_or_description_root && !SupportsNameFromContents(recursive)) {
    return false;
  }

  // Avoid option descendent text.
  if (IsA<HTMLSelectElement>(GetNode())) {
    return false;
  }

  // A textfield's name should not include its value (see crbug.com/352665697),
  // unless aria-labelledby explicitly references its own content.
  //
  // Example from aria-labelledby-on-input.html:
  //   <input id="time" value="10" aria-labelledby="message time unit"/>
  //
  // When determining the name for the <input>, we parse the list of IDs in
  // aria-labelledby. When "time" is reached, aria_label_or_description_root
  // points to the element we are naming (the <input>) and 'this' refers to the
  // element we are currently traversing, which is the element with id="time"
  // (so, aria_label_or_description_root == this). In this case, since the
  // author explicitly included the input id, the value of the input should be
  // included in the name.
  if (IsTextField() && aria_label_or_description_root != this) {
    return false;
  }
  return true;
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

  HeapVector<Member<Element>> elements_from_attribute;
  if (ElementsFromAttribute(element, elements_from_attribute,
                            html_names::kAriaDescribedbyAttr)) {
    // TODO(meredithl): Determine description sources when |aria_describedby| is
    // the empty string, in order to make devtools work with attr-associated
    // elements.
    if (description_sources) {
      description_sources->back().attribute_value =
          AriaAttribute(html_names::kAriaDescribedbyAttr);
    }
    AXObjectSet visited;
    description = TextFromElements(true, visited, elements_from_attribute,
                                   related_objects);

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
      AriaAttribute(html_names::kAriaDescriptionAttr);
  if (aria_desc) {
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
    String svg_description = SVGDescription(
        name_from, description_from, description_sources, related_objects);
    if (!svg_description.empty()) {
      return svg_description;
    }
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
          child->GetNode() &&
          child->GetNode()->HasTagName(html_names::kRtTag)) {
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
      AXObject* caption_ax_object = AXObjectCache().Get(caption);
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
    const AtomicString& title = GetElement()->FastGetAttribute(kTitleAttr);
    if (!title.empty() &&
        String(title).StripWhiteSpace() !=
            GetElement()->GetInnerTextWithoutUpdate().StripWhiteSpace()) {
      description = title;
      if (description_sources) {
        found_description = true;
        description_sources->back().text = description;
      } else {
        return description;
      }
    }
  }

  // For form controls that act as triggering elements for popovers of type
  // kHint, then set aria-describedby to the popover.
  if (name_from != ax::mojom::blink::NameFrom::kPopoverAttribute) {
    if (auto* form_control = DynamicTo<HTMLFormControlElement>(element)) {
      auto popover_target = form_control->popoverTargetElement();
      if (popover_target.popover &&
          popover_target.popover->PopoverType() == PopoverValueType::kHint) {
        DCHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
        description_from = ax::mojom::blink::DescriptionFrom::kPopoverAttribute;
        if (description_sources) {
          description_sources->push_back(DescriptionSource(
              found_description, html_names::kPopovertargetAttr));
          description_sources->back().type = description_from;
        }
        AXObject* popover_ax_object =
            AXObjectCache().Get(popover_target.popover);
        if (popover_ax_object && popover_ax_object->IsPlainContent()) {
          AXObjectSet visited;
          description = RecursiveTextAlternative(*popover_ax_object,
                                                 popover_ax_object, visited);
          if (related_objects) {
            related_objects->push_back(
                MakeGarbageCollected<NameSourceRelatedObject>(popover_ax_object,
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
  }

  // There was a name, but it is prohibited for this role. Move to description.
  if (name_from == ax::mojom::blink::NameFrom::kProhibited) {
    description_from = ax::mojom::blink::DescriptionFrom::kProhibitedNameRepair;
    ax::mojom::blink::NameFrom orig_name_from_without_prohibited;
    HeapHashSet<Member<const AXObject>> visited;
    description = TextAlternative(false, nullptr, visited,
                                  orig_name_from_without_prohibited,
                                  related_objects, nullptr);
    DCHECK(!description.empty());
    if (description_sources) {
      description_sources->push_back(DescriptionSource(found_description));
      DescriptionSource& source = description_sources->back();
      source.type = description_from;
      source.related_objects = *related_objects;
      source.text = description;
      found_description = true;
    } else {
      return description;
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
      AriaAttribute(html_names::kAriaPlaceholderAttr);
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
        AriaAttribute(html_names::kAriaValuetextAttr);
    if (aria_valuetext) {
      return aria_valuetext.GetString();
    }
    float value;
    if (ValueForRange(&value))
      return String::Number(value);
  }

  // "If the embedded control has role combobox or listbox, return the text
  // alternative of the chosen option."
  if (UseNameFromSelectedOption()) {
    AXObjectVector selected_options;
    SelectedOptions(selected_options);
    if (selected_options.size() == 0) {
      // Per https://www.w3.org/TR/wai-aria/#combobox, a combobox gets its
      // value in the following way:
      // "If the combobox element is a host language element that provides a
      // value, such as an HTML input element, the value of the combobox is the
      // value of that element. Otherwise, the value of the combobox is
      // represented by its descendant elements and can be determined using the
      // same method used to compute the name of a button from its descendant
      // content."
      //
      // Section 2C of the accname computation steps for the combobox/listbox
      // case (https://w3c.github.io/accname/#comp_embedded_control) only
      // mentions getting the text alternative from the chosen option, which
      // doesn't precisely fit for combobox, but a clarification is coming; see
      // https://github.com/w3c/accname/issues/232 and
      // https://github.com/w3c/accname/issues/200.
      return SlowGetValueForControlIncludingContentEditable(visited);
    } else {
      StringBuilder accumulated_text;
      for (const auto& child : selected_options) {
        if (visited.insert(child).is_new_entry) {
          if (accumulated_text.length()) {
            accumulated_text.Append(" ");
          }
          accumulated_text.Append(child->ComputedName());
        }
      }
      return accumulated_text.ToString();
    }
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

ScrollableArea* AXNodeObject::GetScrollableAreaIfScrollable() const {
  if (IsA<Document>(GetNode())) {
    return DocumentFrameView()->LayoutViewport();
  }

  if (auto* box = DynamicTo<LayoutBox>(GetLayoutObject())) {
    PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea();
    if (scrollable_area && scrollable_area->HasOverflow()) {
      return scrollable_area;
    }
  }

  return nullptr;
}

AXObject* AXNodeObject::AccessibilityHitTest(const gfx::Point& point) const {
  // Must be called for the document's root or a popup's root.
  if (!IsA<Document>(GetNode())) {
    return nullptr;
  }

  CHECK(GetLayoutObject());

  // Must be called with lifecycle >= pre-paint clean
  DCHECK_GE(GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  DCHECK(GetLayoutObject()->IsLayoutView());
  PaintLayer* layer = To<LayoutBox>(GetLayoutObject())->Layer();
  DCHECK(layer);

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(point);
  HitTestResult hit_test_result = HitTestResult(request, location);
  layer->HitTest(location, hit_test_result, PhysicalRect(InfiniteIntRect()));

  Node* node = hit_test_result.InnerNode();
  if (!node) {
    return nullptr;
  }

  if (auto* area = DynamicTo<HTMLAreaElement>(node)) {
    return AccessibilityImageMapHitTest(area, point);
  }

  if (auto* option = DynamicTo<HTMLOptionElement>(node)) {
    node = option->OwnerSelectElement();
    if (!node) {
      return nullptr;
    }
  }

  // If |node| is in a user-agent shadow tree, reassign it as the host to hide
  // details in the shadow tree. Previously this was implemented by using
  // Retargeting (https://dom.spec.whatwg.org/#retarget), but this caused
  // elements inside regular shadow DOMs to be ignored by screen reader. See
  // crbug.com/1111800 and crbug.com/1048959.
  const TreeScope& tree_scope = node->GetTreeScope();
  if (auto* shadow_root = DynamicTo<ShadowRoot>(tree_scope.RootNode())) {
    if (shadow_root->IsUserAgent()) {
      node = &shadow_root->host();
    }
  }

  LayoutObject* obj = node->GetLayoutObject();
  AXObject* result = AXObjectCache().Get(obj);
  if (!result) {
    return nullptr;
  }

  // Allow the element to perform any hit-testing it might need to do to reach
  // non-layout children.
  result = result->ElementAccessibilityHitTest(point);

  while (result && result->IsIgnored()) {
    CHECK(!result->IsDetached());
    // If this element is the label of a control, a hit test should return the
    // control. The label is ignored because it's already reflected in the name.
    if (auto* label = DynamicTo<HTMLLabelElement>(result->GetNode())) {
      if (HTMLElement* control = label->Control()) {
        if (AXObject* ax_control = AXObjectCache().Get(control)) {
          result = ax_control;
          break;
        }
      }
    }

    result = result->ParentObject();
  }

  return result->IsIncludedInTree() ? result
                                    : result->ParentObjectIncludedInTree();
}

AXObject* AXNodeObject::AccessibilityImageMapHitTest(
    HTMLAreaElement* area,
    const gfx::Point& point) const {
  if (!area) {
    return nullptr;
  }

  AXObject* parent = AXObjectCache().Get(area->ImageElement());
  if (!parent) {
    return nullptr;
  }

  PhysicalOffset physical_point(point);
  for (const auto& child : parent->ChildrenIncludingIgnored()) {
    if (child->GetBoundsInFrameCoordinates().Contains(physical_point)) {
      return child.Get();
    }
  }

  return nullptr;
}

AXObject* AXNodeObject::GetFirstInlineBlockOrDeepestInlineAXChildInLayoutTree(
    AXObject* start_object,
    bool first) const {
  if (!start_object) {
    return nullptr;
  }

  // Return the deepest last child that is included.
  // Uses LayoutTreeBuildTraversaler to get children, in order to avoid getting
  // children unconnected to the line, e.g. via aria-owns. Doing this first also
  // avoids the issue that |start_object| may not be included in the tree.
  AXObject* result = start_object;
  Node* current_node = start_object->GetNode();
  while (current_node) {
    // If we find a node that is inline-block, we want to return it rather than
    // getting the deepest child for that. This is because these are now always
    // being included in the tree and the Next/PreviousOnLine could be set on
    // the inline-block element. We exclude list markers since those technically
    // fulfill the inline-block condition.
    AXObject* ax_object = start_object->AXObjectCache().Get(current_node);
    if (ax_object && ax_object->IsIncludedInTree() &&
        !current_node->IsMarkerPseudoElement()) {
      if (ax_object->GetLayoutObject() &&
          ax_object->GetLayoutObject()->IsInline() &&
          ax_object->GetLayoutObject()->IsAtomicInlineLevel()) {
        return ax_object;
      }
    }

    current_node = first ? LayoutTreeBuilderTraversal::FirstChild(*current_node)
                         : LayoutTreeBuilderTraversal::LastChild(*current_node);
    if (!current_node) {
      break;
    }

    AXObject* tentative_child = start_object->AXObjectCache().Get(current_node);

    if (tentative_child && tentative_child->IsIncludedInTree()) {
      result = tentative_child;
    }
  }

  // Have reached the end of LayoutTreeBuilderTraversal. From here on, traverse
  // AXObjects to get deepest descendant of pseudo element or static text,
  // such as an AXInlineTextBox.

  // Relevant static text or pseudo element is always included.
  if (!result->IsIncludedInTree()) {
    return nullptr;
  }

  // Already a leaf: return current result.
  if (!result->ChildCountIncludingIgnored()) {
    return result;
  }

  // Get deepest AXObject descendant.
  return first ? result->DeepestFirstChildIncludingIgnored()
               : result->DeepestLastChildIncludingIgnored();
}

void AXNodeObject::MaybeResetCache() const {
  uint64_t generation = AXObjectCache().GenerationalCacheId();
  if (!generational_cache_) {
    generational_cache_ = MakeGarbageCollected<GenerationalCache>();
  }
  DCHECK(AXObjectCache().IsFrozen());
  if (generation != generational_cache_->generation) {
    generational_cache_->generation = generation;
    generational_cache_->next_on_line = nullptr;
    generational_cache_->previous_on_line = nullptr;
  } else {
#if DCHECK_IS_ON()
    // AXObjects cannot be detached while the tree is frozen.
    // These are sanity checks. Limited to DCHECK enabled builds due to
    // potential performance impact with to the sheer volume of calls.
    if (AXObject* next = generational_cache_->next_on_line) {
      CHECK(!next->IsDetached());
    }
    if (AXObject* previous = generational_cache_->previous_on_line) {
      CHECK(!previous->IsDetached());
    }
#endif
  }
}

void AXNodeObject::GenerationalCache::Trace(Visitor* visitor) const {
  visitor->Trace(next_on_line);
  visitor->Trace(previous_on_line);
}

AXObject* AXNodeObject::SetNextOnLine(AXObject* next_on_line) const {
  CHECK(generational_cache_) << "Must call MaybeResetCache ahead of this call";
  generational_cache_->next_on_line = next_on_line;
  return next_on_line;
}

AXObject* AXNodeObject::SetPreviousOnLine(AXObject* previous_on_line) const {
  CHECK(generational_cache_) << "Must call MaybeResetCache ahead of this call";
  generational_cache_->previous_on_line = previous_on_line;
  return previous_on_line;
}

AXObject* AXNodeObject::NextOnLine() const {
  // If this is the last object on the line, nullptr is returned. Otherwise, all
  // inline AXNodeObjects, regardless of role and tree depth, are connected to
  // the next inline text box on the same line. If there is no inline text box,
  // they are connected to the next leaf AXObject.
  DCHECK(!IsDetached());

  MaybeResetCache();
  if (generational_cache_->next_on_line) {
    return generational_cache_->next_on_line;
  }

  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object) {
    return SetNextOnLine(nullptr);
  }

  if (!AXObjectCache().IsFrozen() ||
      !AXObjectCache().HasCachedDataForNodesOnLine()) {
    // TODO(crbug.com/372084397): Solve race condition for web AX API and frozen
    // states of accessibility lifecycle.
    // Not all serialization data comes from the regular flow (see
    // third_party/blink/renderer/modules/accessibility/ax_object_cache_lifecycle.h).
    // Some serialization requests come through a special test API (see
    // third_party/blink/public/web/web_ax_object.h), which requires us to force
    // the data to be computed in case it is not computed yet.
    AXObjectCache().ComputeNodesOnLine(layout_object);
  }

  if (DisplayLockUtilities::LockedAncestorPreventingPaint(*layout_object)) {
    return SetNextOnLine(nullptr);
  }

  if (const auto* list_marker =
          GetListMarker(*layout_object, ParentObjectIfPresent())) {
    // A list marker should be followed by a list item on the same line.
    // Note that pseudo content is always included in the tree, so
    // NextSiblingIncludingIgnored() will succeed.
    auto* ax_list_marker = AXObjectCache().Get(list_marker);
    if (ax_list_marker && ax_list_marker->IsIncludedInTree()) {
      return SetNextOnLine(
          GetFirstInlineBlockOrDeepestInlineAXChildInLayoutTree(
              ax_list_marker->NextSiblingIncludingIgnored(), true));
    }
    return SetNextOnLine(nullptr);
  }

  if (!ShouldUseLayoutNG(*layout_object)) {
    return SetNextOnLine(nullptr);
  }

  if (!layout_object->IsInLayoutNGInlineFormattingContext()) {
    return SetNextOnLine(nullptr);
  }

  // Obtain the next LayoutObject that is in the same line, which was previously
  // computed in `AXObjectCacheImpl::ComputeNodesOnLine()`. If one does not
  // exist, move to children and Repeate the process.
  // If a LayoutObject is found, in the next loop we compute if it has an
  // AXObject that is included in the tree. If so, connect them.
  const LayoutObject* next_layout_object = nullptr;
  while (layout_object) {
    next_layout_object = AXObjectCache().CachedNextOnLine(layout_object);
    if (next_layout_object) {
      break;
    }
    const auto* child = layout_object->SlowLastChild();
    if (!child) {
      break;
    }
    layout_object = child;
  }

  while (next_layout_object) {
    AXObject* result = AXObjectCache().Get(next_layout_object);

    // We want to continue searching for the next inline leaf if the
    // current one is inert or aria-hidden.
    // We don't necessarily want to keep searching in the case of any ignored
    // node, because we anticipate that there might be scenarios where a
    // descendant of the ignored node is not ignored and would be returned by
    // the call to `GetFirstInlineBlockOrDeepestInlineAXChildInLayoutTree`
    bool should_keep_looking =
        result ? result->IsInert() || result->IsAriaHidden() : false;

    result =
        GetFirstInlineBlockOrDeepestInlineAXChildInLayoutTree(result, true);
    if (result && !should_keep_looking) {
      return SetNextOnLine(result);
    }

    if (!should_keep_looking) {
      break;
    }
    next_layout_object = AXObjectCache().CachedNextOnLine(next_layout_object);
  }

  return SetNextOnLine(nullptr);
}

AXObject* AXNodeObject::PreviousOnLine() const {
  // If this is the first object on the line, nullptr is returned. Otherwise,
  // all inline AXNodeObjects, regardless of role and tree depth, are connected
  // to the previous inline text box on the same line. If there is no inline
  // text box, they are connected to the previous leaf AXObject.
  DCHECK(!IsDetached());

  MaybeResetCache();
  if (generational_cache_->previous_on_line) {
    return generational_cache_->previous_on_line;
  }

  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object) {
    return SetPreviousOnLine(nullptr);
  }

  if (!AXObjectCache().IsFrozen() ||
      !AXObjectCache().HasCachedDataForNodesOnLine()) {
    // See AXNodeObject::NextOnLine() for reasoning of this call.
    AXObjectCache().ComputeNodesOnLine(layout_object);
  }

  if (!ShouldUseLayoutNG(*layout_object)) {
    return SetPreviousOnLine(nullptr);
  }

  if (DisplayLockUtilities::LockedAncestorPreventingPaint(*layout_object)) {
    return SetPreviousOnLine(nullptr);
  }

  AXObject* previous_sibling = IsIncludedInTree()
                                   ? PreviousSiblingIncludingIgnored()
                                   : nullptr;
  if (previous_sibling && previous_sibling->GetLayoutObject() &&
      previous_sibling->GetLayoutObject()->IsLayoutOutsideListMarker()) {
    // A list item should be preceded by a list marker on the same line.
    return SetPreviousOnLine(
        GetFirstInlineBlockOrDeepestInlineAXChildInLayoutTree(previous_sibling,
                                                              false));
  }

  if (layout_object->IsLayoutOutsideListMarker() ||
      !layout_object->IsInLayoutNGInlineFormattingContext()) {
    return SetPreviousOnLine(nullptr);
  }

  // Obtain the previous LayoutObject that is in the same line, which was
  // previously computed in `AXObjectCacheImpl::ComputeNodesOnLine()`. If one
  // does not exist, move to children and repeate the process. If a LayoutObject
  // is found, in the next loop we compute if it has an AXObject that is
  // included in the tree. If so, connect them.
  const LayoutObject* previous_layout_object = nullptr;
  while (layout_object) {
    previous_layout_object =
        AXObjectCache().CachedPreviousOnLine(layout_object);

    if (previous_layout_object) {
      break;
    }
    const auto* child = layout_object->SlowFirstChild();
    if (!child) {
      break;
    }
    layout_object = child;
  }

  while (previous_layout_object) {
    AXObject* result = AXObjectCache().Get(previous_layout_object);

    // We want to continue searching for the next inline leaf if the
    // current one is inert or aria-hidden.
    // We don't necessarily want to keep searching in the case of any ignored
    // node, because we anticipate that there might be scenarios where a
    // descendant of the ignored node is not ignored and would be returned by
    // the call to `GetFirstInlineBlockOrDeepestInlineAXChildInLayoutTree`
    bool should_keep_looking =
        result ? result->IsInert() || result->IsAriaHidden() : false;

    result =
        GetFirstInlineBlockOrDeepestInlineAXChildInLayoutTree(result, false);
    if (result && !should_keep_looking) {
      return SetPreviousOnLine(result);
    }

    // We want to continue searching for the previous inline leaf if the
    // current one is inert.
    if (!should_keep_looking) {
      break;
    }
    previous_layout_object =
        AXObjectCache().CachedPreviousOnLine(previous_layout_object);
  }

  return SetPreviousOnLine(nullptr);
}

void AXNodeObject::HandleAutofillSuggestionAvailabilityChanged(
    WebAXAutofillSuggestionAvailability suggestion_availability) {
  if (GetLayoutObject()) {
    // Autofill suggestion availability is stored in AXObjectCache.
    AXObjectCache().SetAutofillSuggestionAvailability(AXObjectID(),
                                                      suggestion_availability);
  }
}

void AXNodeObject::GetWordBoundaries(Vector<int>& word_starts,
                                     Vector<int>& word_ends) const {
  if (!GetLayoutObject() || !GetLayoutObject()->IsListMarker()) {
    return;
  }

  String text_alternative;
  if (ListMarker* marker = ListMarker::Get(GetLayoutObject())) {
    text_alternative = marker->TextAlternative(*GetLayoutObject());
  }
  if (text_alternative.ContainsOnlyWhitespaceOrEmpty()) {
    return;
  }

  Vector<AbstractInlineTextBox::WordBoundaries> boundaries;
  AbstractInlineTextBox::GetWordBoundariesForText(boundaries, text_alternative);
  word_starts.reserve(boundaries.size());
  word_ends.reserve(boundaries.size());
  for (const auto& boundary : boundaries) {
    word_starts.push_back(boundary.start_index);
    word_ends.push_back(boundary.end_index);
  }
}

void AXNodeObject::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  visitor->Trace(layout_object_);
  visitor->Trace(generational_cache_);
  AXObject::Trace(visitor);
}

}  // namespace blink
