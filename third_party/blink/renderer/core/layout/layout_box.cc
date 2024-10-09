/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_box.h"

#include <math.h>

#include <algorithm>
#include <utility>

#include "base/memory/values_equivalent.h"
#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_child.h"
#include "third_party/blink/renderer/core/layout/custom/layout_custom.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/forms/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/forms/layout_text_control.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_utils.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/measure_cache.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/text_utils.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/box_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/outline_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_size.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

using mojom::blink::FormControlType;

// Used by flexible boxes when flexing this element and by table cells.
typedef WTF::HashMap<const LayoutBox*, LayoutUnit> OverrideSizeMap;

// Size of border belt for autoscroll. When mouse pointer in border belt,
// autoscroll is started.
static const int kAutoscrollBeltSize = 20;
static const unsigned kBackgroundObscurationTestMaxDepth = 4;

struct SameSizeAsLayoutBox : public LayoutBoxModelObject {
  LayoutPoint frame_location_;
  PhysicalSize frame_size_;
  PhysicalSize previous_size;
  MinMaxSizes intrinsic_logical_widths;
  Member<void*> min_max_sizes_cache;
  Member<void*> cache;
  HeapVector<Member<const LayoutResult>, 1> layout_results;
  wtf_size_t first_fragment_item_index_;
  Member<void*> members[2];
};

ASSERT_SIZE(LayoutBox, SameSizeAsLayoutBox);

namespace {

LayoutUnit TextAreaIntrinsicInlineSize(const HTMLTextAreaElement& textarea,
                                       const LayoutBox& box) {
  // Always add the scrollbar thickness for 'overflow:auto'.
  const auto& style = box.StyleRef();
  int scrollbar_thickness = 0;
  if (style.OverflowBlockDirection() == EOverflow::kScroll ||
      style.OverflowBlockDirection() == EOverflow::kAuto) {
    scrollbar_thickness = layout_text_control::ScrollbarThickness(box);
  }

  return LayoutUnit(ceilf(layout_text_control::GetAvgCharWidth(style) *
                          textarea.cols())) +
         scrollbar_thickness;
}

LayoutUnit TextFieldIntrinsicInlineSize(const HTMLInputElement& input,
                                        const LayoutBox& box) {
  int factor;
  const bool includes_decoration = input.SizeShouldIncludeDecoration(factor);
  if (factor <= 0)
    factor = 20;

  const float char_width = layout_text_control::GetAvgCharWidth(box.StyleRef());
  float float_result = char_width * factor;

  float max_char_width = 0.f;
  const Font& font = box.StyleRef().GetFont();
  if (layout_text_control::HasValidAvgCharWidth(font)) {
    max_char_width = font.PrimaryFont()->MaxCharWidth();
  }

  // For text inputs, IE adds some extra width.
  if (max_char_width > char_width)
    float_result += max_char_width - char_width;

  LayoutUnit result(ceilf(float_result));
  if (includes_decoration) {
    const auto* spin_button =
        To<HTMLElement>(input.UserAgentShadowRoot()->getElementById(
            shadow_element_names::kIdSpinButton));
    if (LayoutBox* spin_box =
            spin_button ? spin_button->GetLayoutBox() : nullptr) {
      const Length& logical_width = spin_box->StyleRef().LogicalWidth();
      result += spin_box->BorderAndPaddingInlineSize();
      // Since the width of spin_box is not calculated yet,
      // spin_box->LogicalWidth() returns 0. Use the computed logical
      // width instead.
      if (logical_width.IsPercent()) {
        if (logical_width.Value() != 100.f) {
          result +=
              result * logical_width.Value() / (100 - logical_width.Value());
        }
      } else {
        result += logical_width.Value();
      }
    }
  }
  return result;
}

LayoutUnit TextAreaIntrinsicBlockSize(const HTMLTextAreaElement& textarea,
                                      const LayoutBox& box) {
  // Only add the scrollbar thickness for 'overflow: scroll'.
  int scrollbar_thickness = 0;
  if (box.StyleRef().OverflowInlineDirection() == EOverflow::kScroll) {
    scrollbar_thickness = layout_text_control::ScrollbarThickness(box);
  }

  const auto* inner_editor = textarea.InnerEditorElement();
  const LayoutUnit line_height =
      inner_editor && inner_editor->GetLayoutBox()
          ? inner_editor->GetLayoutBox()->FirstLineHeight()
          : box.FirstLineHeight();

  return line_height * textarea.rows() + scrollbar_thickness;
}

LayoutUnit TextFieldIntrinsicBlockSize(const HTMLInputElement& input,
                                       const LayoutBox& box) {
  const auto* inner_editor = input.InnerEditorElement();
  // inner_editor's LayoutBox can be nullptr because web authors can set
  // display:none to ::-webkit-textfield-decoration-container element.
  const LayoutBox& target_box = (inner_editor && inner_editor->GetLayoutBox())
                                    ? *inner_editor->GetLayoutBox()
                                    : box;
  return target_box.FirstLineHeight();
}

LayoutUnit FileUploadControlIntrinsicInlineSize(const HTMLInputElement& input,
                                                const LayoutBox& box) {
  // This should match to margin-inline-end of ::-webkit-file-upload-button UA
  // style.
  constexpr int kAfterButtonSpacing = 4;
  // Figure out how big the filename space needs to be for a given number of
  // characters (using "0" as the nominal character).
  constexpr int kDefaultWidthNumChars = 34;
  constexpr UChar kCharacter = '0';
  const String character_as_string = String(&kCharacter, 1u);
  const float min_default_label_width =
      kDefaultWidthNumChars *
      ComputeTextWidth(character_as_string, box.StyleRef());

  const String label =
      input.GetLocale().QueryString(IDS_FORM_FILE_NO_FILE_LABEL);
  float default_label_width = ComputeTextWidth(label, box.StyleRef());
  if (HTMLInputElement* button = input.UploadButton()) {
    if (auto* button_box = button->GetLayoutBox()) {
      const ComputedStyle& button_style = button_box->StyleRef();
      WritingMode mode = button_style.GetWritingMode();
      ConstraintSpaceBuilder builder(mode, button_style.GetWritingDirection(),
                                     /* is_new_fc */ true);
      LayoutUnit max = BlockNode(button_box)
                           .ComputeMinMaxSizes(mode, SizeType::kIntrinsic,
                                               builder.ToConstraintSpace())
                           .sizes.max_size;
      default_label_width +=
          max + (kAfterButtonSpacing * box.StyleRef().EffectiveZoom());
    }
  }
  return LayoutUnit(
      ceilf(std::max(min_default_label_width, default_label_width)));
}

LayoutUnit SliderIntrinsicInlineSize(const LayoutBox& box) {
  constexpr int kDefaultTrackLength = 129;
  return LayoutUnit(kDefaultTrackLength * box.StyleRef().EffectiveZoom());
}

LogicalSize ThemePartIntrinsicSize(const LayoutBox& box,
                                   WebThemeEngine::Part part) {
  const auto& style = box.StyleRef();
  PhysicalSize size(
      WebThemeEngineHelper::GetNativeThemeEngine()->GetSize(part));
  size.Scale(style.EffectiveZoom());
  return size.ConvertToLogical(style.GetWritingMode());
}

LayoutUnit ListBoxDefaultItemHeight(const LayoutBox& box) {
  constexpr int kDefaultPaddingBottom = 1;

  const SimpleFontData* font_data = box.StyleRef().GetFont().PrimaryFont();
  if (!font_data)
    return LayoutUnit();
  return LayoutUnit(font_data->GetFontMetrics().Height() +
                    kDefaultPaddingBottom);
}

// TODO(crbug.com/1040826): This function is written in LayoutObject API
// so that this works in both of the legacy layout and LayoutNG. We
// should have LayoutNG-specific code.
LayoutUnit ListBoxItemBlockSize(const HTMLSelectElement& select,
                                const LayoutBox& box) {
  const auto& items = select.GetListItems();
  if (items.empty() || box.ShouldApplySizeContainment())
    return ListBoxDefaultItemHeight(box);

  LayoutUnit max_block_size;
  for (Element* element : items) {
    if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(element))
      element = &optgroup->OptGroupLabelElement();
    LayoutUnit item_block_size;
    if (auto* layout_box = element->GetLayoutBox()) {
      item_block_size = box.StyleRef().IsHorizontalWritingMode()
                            ? layout_box->Size().height
                            : layout_box->Size().width;
    } else {
      item_block_size = ListBoxDefaultItemHeight(box);
    }
    max_block_size = std::max(max_block_size, item_block_size);
  }
  return max_block_size;
}

LayoutUnit MenuListIntrinsicInlineSize(const HTMLSelectElement& select,
                                       const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  float max_option_width = 0;
  if (!box.ShouldApplySizeContainment()) {
    for (auto* const option : select.GetOptionList()) {
      String text =
          style.ApplyTextTransform(option->TextIndentedToRespectGroupLabel());
      // We apply SELECT's style, not OPTION's style because max_option_width is
      // used to determine intrinsic width of the menulist box.
      max_option_width =
          std::max(max_option_width, ComputeTextWidth(text, style));
    }
  }

  LayoutTheme& theme = LayoutTheme::GetTheme();
  int paddings = theme.PopupInternalPaddingStart(style) +
                 theme.PopupInternalPaddingEnd(box.GetFrame(), style);
  return LayoutUnit(ceilf(max_option_width)) + LayoutUnit(paddings);
}

LayoutUnit MenuListIntrinsicBlockSize(const HTMLSelectElement& select,
                                      const LayoutBox& box) {
  if (!box.StyleRef().HasEffectiveAppearance())
    return kIndefiniteSize;
  const SimpleFontData* font_data = box.StyleRef().GetFont().PrimaryFont();
  DCHECK(font_data);
  const LayoutBox* inner_box = select.InnerElement().GetLayoutBox();
  return (font_data ? font_data->GetFontMetrics().Height() : 0) +
         (inner_box ? inner_box->BorderAndPaddingBlockSize() : LayoutUnit());
}

#if DCHECK_IS_ON()
void CheckDidAddFragment(const LayoutBox& box,
                         const PhysicalBoxFragment& new_fragment,
                         wtf_size_t new_fragment_index = kNotFound) {
  // If |HasFragmentItems|, |ChildrenInline()| should be true.
  // |HasFragmentItems| uses this condition to optimize .
  if (new_fragment.HasItems())
    DCHECK(box.ChildrenInline());

  wtf_size_t index = 0;
  for (const PhysicalBoxFragment& fragment : box.PhysicalFragments()) {
    DCHECK_EQ(fragment.IsFirstForNode(), index == 0);
    if (const FragmentItems* fragment_items = fragment.Items()) {
      fragment_items->CheckAllItemsAreValid();
    }
    // Don't check past the fragment just added. Those entries may be invalid at
    // this point.
    if (index == new_fragment_index)
      break;
    ++index;
  }
}
#else
inline void CheckDidAddFragment(const LayoutBox& box,
                                const PhysicalBoxFragment& fragment,
                                wtf_size_t new_fragment_index = kNotFound) {}
#endif

// Applies the overflow clip to |result|. For any axis that is clipped, |result|
// is reset to |no_overflow_rect|. If neither axis is clipped, nothing is
// changed.
void ApplyOverflowClip(OverflowClipAxes overflow_clip_axes,
                       const PhysicalRect& no_overflow_rect,
                       PhysicalRect& result) {
  if (overflow_clip_axes & kOverflowClipX) {
    result.SetX(no_overflow_rect.X());
    result.SetWidth(no_overflow_rect.Width());
  }
  if (overflow_clip_axes & kOverflowClipY) {
    result.SetY(no_overflow_rect.Y());
    result.SetHeight(no_overflow_rect.Height());
  }
}

int HypotheticalScrollbarThickness(const LayoutBox& box,
                                   ScrollbarOrientation scrollbar_orientation,
                                   bool should_include_overlay_thickness) {
  box.CheckIsNotDestroyed();

  if (PaintLayerScrollableArea* scrollable_area = box.GetScrollableArea()) {
    return scrollable_area->HypotheticalScrollbarThickness(
        scrollbar_orientation, should_include_overlay_thickness);
  } else {
    Page* page = box.GetFrame()->GetPage();
    ScrollbarTheme& theme = page->GetScrollbarTheme();

    if (theme.UsesOverlayScrollbars() && !should_include_overlay_thickness) {
      return 0;
    } else {
      ChromeClient& chrome_client = page->GetChromeClient();
      Document& document = box.GetDocument();
      float scale_from_dip =
          chrome_client.WindowToViewportScalar(document.GetFrame(), 1.0f);
      return theme.ScrollbarThickness(scale_from_dip,
                                      box.StyleRef().UsedScrollbarWidth());
    }
  }
}

void RecalcFragmentScrollableOverflow(RecalcScrollableOverflowResult& result,
                                      const PhysicalFragment& fragment) {
  for (const auto& child : fragment.PostLayoutChildren()) {
    if (child->GetLayoutObject()) {
      if (const auto* box = DynamicTo<PhysicalBoxFragment>(child.get())) {
        if (LayoutBox* owner_box = box->MutableOwnerLayoutBox())
          result.Unite(owner_box->RecalcScrollableOverflow());
      }
    } else {
      // We enter this branch when the |child| is a fragmentainer.
      RecalcFragmentScrollableOverflow(result, *child.get());
    }
  }
}

// Returns the logical offset in the LocationContainer() coordination system,
// and its WritingMode.
std::tuple<LogicalOffset, WritingMode> LogicalLocation(const LayoutBox& box) {
  LayoutBox* container = box.LocationContainer();
  WritingMode writing_mode = container->StyleRef().GetWritingMode();
  WritingModeConverter converter({writing_mode, TextDirection::kLtr},
                                 PhysicalSize(container->Size()));
  return {converter.ToLogical(box.PhysicalLocation(), PhysicalSize(box.Size())),
          writing_mode};
}

}  // namespace

LayoutBoxRareData::LayoutBoxRareData()
    : spanner_placeholder_(nullptr),
      // TODO(rego): We should store these based on physical direction.
      has_override_containing_block_content_logical_width_(false),
      has_previous_content_box_rect_(false) {}

void LayoutBoxRareData::Trace(Visitor* visitor) const {
  visitor->Trace(spanner_placeholder_);
  visitor->Trace(layout_child_);
}

LayoutBox::LayoutBox(ContainerNode* node) : LayoutBoxModelObject(node) {
  if (blink::IsA<HTMLLegendElement>(node))
    SetIsHTMLLegendElement();
}

void LayoutBox::Trace(Visitor* visitor) const {
  visitor->Trace(min_max_sizes_cache_);
  visitor->Trace(measure_cache_);
  visitor->Trace(layout_results_);
  visitor->Trace(overflow_);
  visitor->Trace(rare_data_);
  LayoutBoxModelObject::Trace(visitor);
}

LayoutBox::~LayoutBox() = default;

PaintLayerType LayoutBox::LayerTypeRequired() const {
  NOT_DESTROYED();
  if (IsStacked() || HasHiddenBackface() ||
      (StyleRef().SpecifiesColumns() && !IsLayoutNGObject()))
    return kNormalPaintLayer;

  if (HasNonVisibleOverflow() && !IsLayoutReplaced()) {
    return kOverflowClipPaintLayer;
  }

  return kNoPaintLayer;
}

void LayoutBox::WillBeDestroyed() {
  NOT_DESTROYED();
  ClearOverrideContainingBlockContentSize();

  ShapeOutsideInfo::RemoveInfo(*this);

  if (!DocumentBeingDestroyed()) {
    DisassociatePhysicalFragments();
  }

  if (Style() && StyleRef().HasOutOfFlowPosition()) {
    if (auto* display_locks = DisplayLocksAffectedByAnchors()) {
      NotifyContainingDisplayLocksForAnchorPositioning(display_locks, nullptr);
    }
  }

  LayoutBoxModelObject::WillBeDestroyed();
}

void LayoutBox::DisassociatePhysicalFragments() {
  if (FirstInlineFragmentItemIndex()) {
    FragmentItems::LayoutObjectWillBeDestroyed(*this);
    ClearFirstInlineFragmentItemIndex();
  }
  if (measure_cache_) {
    measure_cache_->LayoutObjectWillBeDestroyed();
  }
  for (auto result : layout_results_)
    result->GetPhysicalFragment().LayoutObjectWillBeDestroyed();
}

void LayoutBox::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutBoxModelObject::InsertedIntoTree();
  AddCustomLayoutChildIfNeeded();
}

void LayoutBox::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  ClearCustomLayoutChild();
  LayoutBoxModelObject::WillBeRemovedFromTree();
}

void LayoutBox::StyleWillChange(StyleDifference diff,
                                const ComputedStyle& new_style) {
  NOT_DESTROYED();
  const ComputedStyle* old_style = Style();
  if (old_style) {
    if (IsDocumentElement() || IsBody()) {
      // The background of the root element or the body element could propagate
      // up to the canvas. Just dirty the entire canvas when our style changes
      // substantially.
      if (diff.NeedsNormalPaintInvalidation() || diff.NeedsLayout()) {
        View()->SetShouldDoFullPaintInvalidation();
      }
    }

    // When a layout hint happens and an object's position style changes, we
    // have to do a layout to dirty the layout tree using the old position
    // value now.
    if (diff.NeedsFullLayout() && Parent()) {
      bool will_move_out_of_ifc = false;
      if (old_style->GetPosition() != new_style.GetPosition()) {
        if (!old_style->HasOutOfFlowPosition() &&
            new_style.HasOutOfFlowPosition()) {
          // We're about to go out of flow. Before that takes place, we need to
          // mark the current containing block chain for preferred widths
          // recalculation.
          SetNeedsLayoutAndIntrinsicWidthsRecalc(
              layout_invalidation_reason::kStyleChange);

          // Grid placement is different for out-of-flow elements, so if the
          // containing block is a grid, dirty the grid's placement. The
          // converse (going from out of flow to in flow) is handled in
          // LayoutBox::UpdateGridPositionAfterStyleChange.
          LayoutBlock* containing_block = ContainingBlock();
          if (containing_block && containing_block->IsLayoutGrid()) {
            containing_block->SetGridPlacementDirty(true);
          }

          // Out of flow are not part of |FragmentItems|, and that further
          // changes including destruction cannot be tracked. We need to mark it
          // is moved out from this IFC.
          will_move_out_of_ifc = true;
        } else {
          MarkContainerChainForLayout();
        }

        if (old_style->GetPosition() == EPosition::kStatic) {
          SetShouldDoFullPaintInvalidation();
        } else if (new_style.HasOutOfFlowPosition()) {
          Parent()->SetChildNeedsLayout();
        }
      }

      bool will_become_inflow = false;
      if ((old_style->IsFloating() || old_style->HasOutOfFlowPosition()) &&
          !new_style.IsFloating() && !new_style.HasOutOfFlowPosition()) {
        // As a float or OOF, this object may have been part of an inline
        // formatting context, but that's definitely no longer the case.
        will_become_inflow = true;
        will_move_out_of_ifc = true;
      }

      if (will_move_out_of_ifc && FirstInlineFragmentItemIndex()) {
        FragmentItems::LayoutObjectWillBeMoved(*this);
        ClearFirstInlineFragmentItemIndex();
      }
      if (will_become_inflow)
        SetIsInLayoutNGInlineFormattingContext(false);
    }
    // FIXME: This branch runs when !oldStyle, which means that layout was never
    // called so what's the point in invalidating the whole view that we never
    // painted?
  } else if (IsBody()) {
    View()->SetShouldDoFullPaintInvalidation();
  }

  LayoutBoxModelObject::StyleWillChange(diff, new_style);
}

void LayoutBox::StyleDidChange(StyleDifference diff,
                               const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBoxModelObject::StyleDidChange(diff, old_style);

  // Reflection works through PaintLayer. Some child classes e.g. LayoutSVGBlock
  // don't create layers and ignore reflections.
  if (HasReflection() && !HasLayer())
    SetHasReflection(false);

  if (auto* parent_flow_block = DynamicTo<LayoutBlockFlow>(Parent())) {
    if (IsFloatingOrOutOfFlowPositioned() && old_style &&
        !old_style->IsFloating() && !old_style->HasOutOfFlowPosition()) {
      // Note that |parent_flow_block| may have been destroyed after this call.
      parent_flow_block->ChildBecameFloatingOrOutOfFlow(this);
    }
  }

  SetOverflowClipAxes(ComputeOverflowClipAxes());

  // If our zoom factor changes and we have a defined scrollLeft/Top, we need to
  // adjust that value into the new zoomed coordinate space.  Note that the new
  // scroll offset may be outside the normal min/max range of the scrollable
  // area, which is weird but OK, because the scrollable area will update its
  // min/max in updateAfterLayout().
  const ComputedStyle& new_style = StyleRef();
  if (IsScrollContainer() && old_style &&
      old_style->EffectiveZoom() != new_style.EffectiveZoom()) {
    PaintLayerScrollableArea* scrollable_area = GetScrollableArea();
    DCHECK(scrollable_area);
    // We use GetScrollOffset() rather than ScrollPosition(), because scroll
    // offset is the distance from the beginning of flow for the box, which is
    // the dimension we want to preserve.
    ScrollOffset offset = scrollable_area->GetScrollOffset();
    if (!offset.IsZero()) {
      offset.Scale(new_style.EffectiveZoom() / old_style->EffectiveZoom());
      scrollable_area->SetScrollOffsetUnconditionally(offset);
    }
  }

  if (old_style && old_style->IsScrollContainer() != IsScrollContainer()) {
    if (auto* layer = EnclosingLayer())
      layer->ScrollContainerStatusChanged();
  }

  UpdateShapeOutsideInfoAfterStyleChange(*Style(), old_style);
  UpdateGridPositionAfterStyleChange(old_style);

  if (old_style) {
    // Regular column content (i.e. non-spanners) have a hook into the flow
    // thread machinery before (StyleWillChange()) and after (here in
    // StyleDidChange()) the style has changed. Column spanners, on the other
    // hand, only have a hook here. The LayoutMultiColumnSpannerPlaceholder code
    // will do all the necessary things, including removing it as a spanner, if
    // it should no longer be one. Therefore, make sure that we skip
    // FlowThreadDescendantStyleDidChange() in such cases, as that might trigger
    // a duplicate flow thread insertion notification, if the spanner no longer
    // is a spanner.
    if (LayoutMultiColumnSpannerPlaceholder* placeholder =
            SpannerPlaceholder()) {
      placeholder->LayoutObjectInFlowThreadStyleDidChange(old_style);
    }

    UpdateScrollSnapMappingAfterStyleChange(*old_style);

    if (ShouldClipOverflowAlongEitherAxis()) {
      // The overflow clip paint property depends on border sizes through
      // overflowClipRect(), and border radii, so we update properties on
      // border size or radii change.
      //
      // For some controls, it depends on paddings.
      if (!old_style->BorderSizeEquals(new_style) ||
          diff.BorderRadiusChanged() ||
          (HasControlClip() && !old_style->PaddingEqual(new_style))) {
        SetNeedsPaintPropertyUpdate();
      }
    }

    if (old_style->OverscrollBehaviorX() != new_style.OverscrollBehaviorX() ||
        old_style->OverscrollBehaviorY() != new_style.OverscrollBehaviorY()) {
      SetNeedsPaintPropertyUpdate();
    }

    if (old_style->OverflowX() != new_style.OverflowX() ||
        old_style->OverflowY() != new_style.OverflowY()) {
      SetNeedsPaintPropertyUpdate();
    }

    if (old_style->OverflowClipMargin() != new_style.OverflowClipMargin())
      SetNeedsPaintPropertyUpdate();

    if (IsInLayoutNGInlineFormattingContext() && IsAtomicInlineLevel() &&
        old_style->Direction() != new_style.Direction()) {
      SetNeedsCollectInlines();
    }

    if (IsBackgroundAttachmentFixedObject() &&
        new_style.BackgroundLayers().Clip() !=
            old_style->BackgroundLayers().Clip()) {
      SetNeedsPaintPropertyUpdate();
    }
  }

  // Update the script style map, from the new computed style.
  if (IsCustomItem())
    GetCustomLayoutChild()->styleMap()->UpdateStyle(GetDocument(), StyleRef());

  // Non-atomic inlines should be LayoutInline or LayoutText, not LayoutBox.
  DCHECK(!IsInline() || IsAtomicInlineLevel());
}

void LayoutBox::UpdateShapeOutsideInfoAfterStyleChange(
    const ComputedStyle& style,
    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  const ShapeValue* shape_outside = style.ShapeOutside();
  const ShapeValue* old_shape_outside =
      old_style ? old_style->ShapeOutside()
                : ComputedStyleInitialValues::InitialShapeOutside();

  const Length& shape_margin = style.ShapeMargin();
  const Length& old_shape_margin =
      old_style ? old_style->ShapeMargin()
                : ComputedStyleInitialValues::InitialShapeMargin();

  float shape_image_threshold = style.ShapeImageThreshold();
  float old_shape_image_threshold =
      old_style ? old_style->ShapeImageThreshold()
                : ComputedStyleInitialValues::InitialShapeImageThreshold();

  // FIXME: A future optimization would do a deep comparison for equality. (bug
  // 100811)
  if (shape_outside == old_shape_outside && shape_margin == old_shape_margin &&
      shape_image_threshold == old_shape_image_threshold)
    return;

  if (!shape_outside)
    ShapeOutsideInfo::RemoveInfo(*this);
  else
    ShapeOutsideInfo::EnsureInfo(*this).MarkShapeAsDirty();

  if (!IsFloating()) {
    return;
  }

  if (shape_outside || shape_outside != old_shape_outside) {
    if (auto* containing_block = ContainingBlock()) {
      containing_block->SetChildNeedsLayout();
    }
  }
}

namespace {

bool GridStyleChanged(const ComputedStyle* old_style,
                      const ComputedStyle& current_style) {
  return old_style->GridColumnStart() != current_style.GridColumnStart() ||
         old_style->GridColumnEnd() != current_style.GridColumnEnd() ||
         old_style->GridRowStart() != current_style.GridRowStart() ||
         old_style->GridRowEnd() != current_style.GridRowEnd() ||
         old_style->Order() != current_style.Order() ||
         old_style->HasOutOfFlowPosition() !=
             current_style.HasOutOfFlowPosition();
}

bool AlignmentChanged(const ComputedStyle* old_style,
                      const ComputedStyle& current_style) {
  return old_style->AlignSelf() != current_style.AlignSelf() ||
         old_style->JustifySelf() != current_style.JustifySelf();
}

}  // namespace

void LayoutBox::UpdateGridPositionAfterStyleChange(
    const ComputedStyle* old_style) {
  NOT_DESTROYED();

  if (!old_style)
    return;

  LayoutObject* parent = Parent();
  const bool was_out_of_flow = old_style->HasOutOfFlowPosition();
  const bool is_out_of_flow = StyleRef().HasOutOfFlowPosition();

  LayoutBlock* containing_block = ContainingBlock();
  if ((containing_block && containing_block->IsLayoutGrid()) &&
      GridStyleChanged(old_style, StyleRef())) {
    // Out-of-flow items do not impact grid placement.
    // TODO(kschmi): Scope this so that it only dirties the grid when track
    // sizing depends on grid item sizes.
    if (!was_out_of_flow || !is_out_of_flow)
      containing_block->SetGridPlacementDirty(true);

    // For out-of-flow elements with grid container as containing block, we need
    // to run the entire algorithm to place and size them correctly. As a
    // result, we trigger a full layout for GridNG.
    if (is_out_of_flow) {
      containing_block->SetNeedsLayout(layout_invalidation_reason::kGridChanged,
                                       kMarkContainerChain);
    }
  }

  // GridNG computes static positions for out-of-flow elements at layout time,
  // with alignment offsets baked in. So if alignment changes, we need to
  // schedule a layout.
  if (is_out_of_flow && AlignmentChanged(old_style, StyleRef())) {
    LayoutObject* grid_ng_ancestor = nullptr;
    if (containing_block && containing_block->IsLayoutGrid()) {
      grid_ng_ancestor = containing_block;
    } else if (parent && parent->IsLayoutGrid()) {
      grid_ng_ancestor = parent;
    }

    if (grid_ng_ancestor) {
      grid_ng_ancestor->SetNeedsLayout(layout_invalidation_reason::kGridChanged,
                                       kMarkContainerChain);
    }
  }
}

void LayoutBox::UpdateScrollSnapMappingAfterStyleChange(
    const ComputedStyle& old_style) {
  NOT_DESTROYED();
  DCHECK(Style());
  // scroll-snap-type and scroll-padding invalidate the snap container.
  if (old_style.GetScrollSnapType() != StyleRef().GetScrollSnapType() ||
      old_style.ScrollPaddingBottom() != StyleRef().ScrollPaddingBottom() ||
      old_style.ScrollPaddingLeft() != StyleRef().ScrollPaddingLeft() ||
      old_style.ScrollPaddingTop() != StyleRef().ScrollPaddingTop() ||
      old_style.ScrollPaddingRight() != StyleRef().ScrollPaddingRight()) {
    if (!NeedsLayout() && IsScrollContainer()) {
      GetScrollableArea()->EnqueueForSnapUpdateIfNeeded();
    }
  }

  // scroll-snap-align invalidates layout as we need to propagate the
  // snap-areas up the fragment-tree.
  if (old_style.GetScrollSnapAlign() != StyleRef().GetScrollSnapAlign()) {
    if (auto* containing_block = ContainingBlock()) {
      containing_block->SetNeedsLayout(layout_invalidation_reason::kStyleChange,
                                       kMarkContainerChain);
    }
  }

  auto SnapAreaDidChange = [&]() {
    auto* snap_container = ContainingScrollContainer();
    if (snap_container && !snap_container->NeedsLayout()) {
      snap_container->GetScrollableArea()->EnqueueForSnapUpdateIfNeeded();
    }
  };

  // scroll-snap-stop and scroll-margin invalidate the snap area.
  if (old_style.ScrollSnapStop() != StyleRef().ScrollSnapStop() ||
      old_style.ScrollMarginBottom() != StyleRef().ScrollMarginBottom() ||
      old_style.ScrollMarginLeft() != StyleRef().ScrollMarginLeft() ||
      old_style.ScrollMarginTop() != StyleRef().ScrollMarginTop() ||
      old_style.ScrollMarginRight() != StyleRef().ScrollMarginRight()) {
    SnapAreaDidChange();
  }

  // Transform invalidates the snap area.
  if (old_style.Transform() != StyleRef().Transform())
    SnapAreaDidChange();
}

void LayoutBox::UpdateFromStyle() {
  NOT_DESTROYED();
  LayoutBoxModelObject::UpdateFromStyle();

  const ComputedStyle& style_to_use = StyleRef();
  SetFloating(style_to_use.IsFloating() && !IsOutOfFlowPositioned() &&
              !style_to_use.IsInsideDisplayIgnoringFloatingChildren());
  SetHasTransformRelatedProperty(
      IsSVGChild() ? style_to_use.HasTransformRelatedPropertyForSVG()
                   : style_to_use.HasTransformRelatedProperty());
  SetHasReflection(style_to_use.BoxReflect());

  bool should_clip_overflow = (!StyleRef().IsOverflowVisibleAlongBothAxes() ||
                               ShouldApplyPaintContainment()) &&
                              RespectsCSSOverflow();
  if (should_clip_overflow != HasNonVisibleOverflow()) {
    // The overflow clip paint property depends on whether overflow clip is
    // present so we need to update paint properties if this changes.
    SetNeedsPaintPropertyUpdate();
    if (Layer())
      Layer()->SetNeedsCompositingInputsUpdate();
  }
  SetHasNonVisibleOverflow(should_clip_overflow);
}

void LayoutBox::LayoutSubtreeRoot() {
  NOT_DESTROYED();

  // Our own style may have changed which would disqualify us as a layout root
  // (e.g. our containment/writing-mode/formatting-context status/etc changed).
  // Skip subtree layout, and ensure our container chain needs layout.
  if (SelfNeedsFullLayout()) {
    MarkContainerChainForLayout();
    return;
  }

  const auto* previous_result = GetSingleCachedLayoutResult();
  DCHECK(previous_result);
  auto space = previous_result->GetConstraintSpaceForCaching();
  DCHECK_EQ(space.GetWritingMode(), StyleRef().GetWritingMode());
  const LayoutResult* result = BlockNode(this).Layout(space);
  GetDocument().GetFrame()->GetInputMethodController().DidLayoutSubtree(*this);

  if (IsOutOfFlowPositioned()) {
    result->CopyMutableOutOfFlowData(*previous_result);
  }

  // Even if we are a subtree layout root we need to mark our containing-block
  // for layout if:
  //  - Our baselines have shifted.
  //  - We've propagated any layout-objects (which affect our container chain).
  //
  // NOTE: We could weaken the constraints in ObjectIsRelayoutBoundary, and use
  // this technique to detect size-changes, etc if we wanted to expand this
  // optimization.
  const auto& previous_fragment =
      To<PhysicalBoxFragment>(previous_result->GetPhysicalFragment());
  const auto& fragment = To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  if (previous_fragment.FirstBaseline() != fragment.FirstBaseline() ||
      previous_fragment.LastBaseline() != fragment.LastBaseline() ||
      fragment.HasPropagatedLayoutObjects()) {
    if (auto* containing_block = ContainingBlock()) {
      containing_block->SetNeedsLayout(
          layout_invalidation_reason::kChildChanged, kMarkContainerChain);
    }
  }
}

// ClientWidth and ClientHeight represent the interior of an object excluding
// border and scrollbar.
DISABLE_CFI_PERF
LayoutUnit LayoutBox::ClientWidth() const {
  NOT_DESTROYED();
  // We need to clamp negative values. This function may be called during layout
  // before frame_size_ gets the final proper value. Another reason: While
  // border side values are currently limited to 2^20px (a recent change in the
  // code), if this limit is raised again in the future, we'd have ill effects
  // of saturated arithmetic otherwise.
  LayoutUnit width = Size().width;
  if (CanSkipComputeScrollbars()) {
    return (width - BorderLeft() - BorderRight()).ClampNegativeToZero();
  } else {
    return (width - BorderLeft() - BorderRight() -
            ComputeScrollbarsInternal(kClampToContentBox).HorizontalSum())
        .ClampNegativeToZero();
  }
}

DISABLE_CFI_PERF
LayoutUnit LayoutBox::ClientHeight() const {
  NOT_DESTROYED();
  // We need to clamp negative values. This function can be called during layout
  // before frame_size_ gets the final proper value. The scrollbar may be wider
  // than the padding box. Another reason: While border side values are
  // currently limited to 2^20px (a recent change in the code), if this limit is
  // raised again in the future, we'd have ill effects of saturated arithmetic
  // otherwise.
  LayoutUnit height = Size().height;
  if (CanSkipComputeScrollbars()) {
    return (height - BorderTop() - BorderBottom()).ClampNegativeToZero();
  } else {
    return (height - BorderTop() - BorderBottom() -
            ComputeScrollbarsInternal(kClampToContentBox).VerticalSum())
        .ClampNegativeToZero();
  }
}

LayoutUnit LayoutBox::ClientWidthFrom(LayoutUnit width) const {
  NOT_DESTROYED();
  if (CanSkipComputeScrollbars()) {
    return (width - BorderLeft() - BorderRight()).ClampNegativeToZero();
  } else {
    return (width - BorderLeft() - BorderRight() -
            ComputeScrollbarsInternal(kClampToContentBox).HorizontalSum())
        .ClampNegativeToZero();
  }
}

LayoutUnit LayoutBox::ClientHeightFrom(LayoutUnit height) const {
  NOT_DESTROYED();
  if (CanSkipComputeScrollbars()) {
    return (height - BorderTop() - BorderBottom()).ClampNegativeToZero();
  } else {
    return (height - BorderTop() - BorderBottom() -
            ComputeScrollbarsInternal(kClampToContentBox).VerticalSum())
        .ClampNegativeToZero();
  }
}

LayoutUnit LayoutBox::ClientWidthWithTableSpecialBehavior() const {
  NOT_DESTROYED();
  // clientWidth/Height is the visual portion of the box content, not including
  // borders or scroll bars, but includes padding. And per
  // https://www.w3.org/TR/CSS2/tables.html#model,
  // table wrapper box is a principal block box that contains the table box
  // itself and any caption boxes, and table grid box is a block-level box that
  // contains the table's internal table boxes. When table's border is specified
  // in CSS, the border is added to table grid box, not table wrapper box.
  // Currently, Blink doesn't have table wrapper box, and we are supposed to
  // retrieve clientWidth/Height from table wrapper box, not table grid box. So
  // when we retrieve clientWidth/Height, it includes table's border size.
  if (IsTable())
    return ClientWidth() + BorderLeft() + BorderRight();
  return ClientWidth();
}

LayoutUnit LayoutBox::ClientHeightWithTableSpecialBehavior() const {
  NOT_DESTROYED();
  // clientWidth/Height is the visual portion of the box content, not including
  // borders or scroll bars, but includes padding. And per
  // https://www.w3.org/TR/CSS2/tables.html#model,
  // table wrapper box is a principal block box that contains the table box
  // itself and any caption boxes, and table grid box is a block-level box that
  // contains the table's internal table boxes. When table's border is specified
  // in CSS, the border is added to table grid box, not table wrapper box.
  // Currently, Blink doesn't have table wrapper box, and we are supposed to
  // retrieve clientWidth/Height from table wrapper box, not table grid box. So
  // when we retrieve clientWidth/Height, it includes table's border size.
  if (IsTable())
    return ClientHeight() + BorderTop() + BorderBottom();
  return ClientHeight();
}

bool LayoutBox::UsesOverlayScrollbars() const {
  NOT_DESTROYED();
  if (StyleRef().HasCustomScrollbarStyle(DynamicTo<Element>(GetNode()))) {
    return false;
  }
  if (GetFrame()->GetPage()->GetScrollbarTheme().UsesOverlayScrollbars())
    return true;
  return false;
}

LayoutUnit LayoutBox::ScrollWidth() const {
  NOT_DESTROYED();
  if (IsScrollContainer())
    return GetScrollableArea()->ScrollWidth();
  if (StyleRef().IsScrollbarGutterStable() &&
      StyleRef().OverflowBlockDirection() == EOverflow::kHidden) {
    if (auto* scrollable_area = GetScrollableArea())
      return scrollable_area->ScrollWidth();
    else
      return ScrollableOverflowRect().Width();
  }
  // For objects with scrollable overflow, this matches IE.
  PhysicalRect overflow_rect = ScrollableOverflowRect();
  if (!StyleRef().GetWritingDirection().IsFlippedX()) {
    return std::max(ClientWidth(), overflow_rect.Right() - BorderLeft());
  }
  return ClientWidth() -
         std::min(LayoutUnit(), overflow_rect.X() - BorderLeft());
}

LayoutUnit LayoutBox::ScrollHeight() const {
  NOT_DESTROYED();
  if (IsScrollContainer())
    return GetScrollableArea()->ScrollHeight();
  if (StyleRef().IsScrollbarGutterStable() &&
      StyleRef().OverflowBlockDirection() == EOverflow::kHidden) {
    if (auto* scrollable_area = GetScrollableArea())
      return scrollable_area->ScrollHeight();
    else
      return ScrollableOverflowRect().Height();
  }
  // For objects with visible overflow, this matches IE.
  // FIXME: Need to work right with writing modes.
  return std::max(ClientHeight(),
                  ScrollableOverflowRect().Bottom() - BorderTop());
}

PhysicalBoxStrut LayoutBox::MarginBoxOutsets() const {
  NOT_DESTROYED();
  if (PhysicalFragmentCount()) {
    // We get margin data from the first physical fragment. Margins are
    // per-LayoutBox data, and we don't need to take care of block
    // fragmentation.
    return GetPhysicalFragment(0)->Margins();
  }
  return PhysicalBoxStrut();
}

void LayoutBox::QuadsInAncestorInternal(Vector<gfx::QuadF>& quads,
                                        const LayoutBoxModelObject* ancestor,
                                        MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock()) {
    flow_thread->QuadsInAncestorForDescendant(*this, quads, ancestor, mode);
    return;
  }
  quads.push_back(
      LocalRectToAncestorQuad(PhysicalBorderBoxRect(), ancestor, mode));
}

gfx::RectF LayoutBox::LocalBoundingBoxRectForAccessibility() const {
  NOT_DESTROYED();
  PhysicalSize size = Size();
  return gfx::RectF(0, 0, size.width.ToFloat(), size.height.ToFloat());
}

void LayoutBox::UpdateAfterLayout() {
  NOT_DESTROYED();
  // Transform-origin depends on box size, so we need to update the layer
  // transform after layout.
  if (HasLayer()) {
    Layer()->UpdateTransform();
    Layer()->UpdateScrollingAfterLayout();
  }

  GetFrame()->GetInputMethodController().DidUpdateLayout(*this);
  if (IsPositioned())
    GetFrame()->GetInputMethodController().DidLayoutSubtree(*this);
}

LayoutUnit LayoutBox::OverrideIntrinsicContentInlineSize() const {
  NOT_DESTROYED();

  // We only override a size contained dimension.
  if (!ShouldApplyInlineSizeContainment()) {
    return kIndefiniteSize;
  }

  const auto& style = StyleRef();
  const StyleIntrinsicLength& intrinsic_length =
      style.ContainIntrinsicInlineSize();

  if (intrinsic_length.HasAuto()) {
    const auto* context = GetDisplayLockContext();
    if (context && context->IsLocked()) {
      if (const auto* elem = DynamicTo<Element>(GetNode())) {
        if (const auto inline_size = elem->LastRememberedInlineSize()) {
          // ResizeObserverSize is adjusted to be in CSS space, we need to
          // adjust it back to Layout space by applying the effective zoom.
          return LayoutUnit::FromFloatRound(*inline_size *
                                            style.EffectiveZoom());
        }
      }
    }
  }

  if (const auto& length = intrinsic_length.GetLength()) {
    DCHECK(length->IsFixed());
    return LayoutUnit(length->Value());
  }

  return kIndefiniteSize;
}

LayoutUnit LayoutBox::OverrideIntrinsicContentBlockSize() const {
  NOT_DESTROYED();

  // We only override a size contained dimension.
  if (!ShouldApplyBlockSizeContainment()) {
    return kIndefiniteSize;
  }

  const auto& style = StyleRef();
  const StyleIntrinsicLength& intrinsic_length =
      style.ContainIntrinsicBlockSize();

  if (intrinsic_length.HasAuto()) {
    const auto* context = GetDisplayLockContext();
    if (context && context->IsLocked()) {
      if (const auto* elem = DynamicTo<Element>(GetNode())) {
        if (const auto inline_size = elem->LastRememberedBlockSize()) {
          // ResizeObserverSize is adjusted to be in CSS space, we need to
          // adjust it back to Layout space by applying the effective zoom.
          return LayoutUnit::FromFloatRound(*inline_size *
                                            style.EffectiveZoom());
        }
      }
    }
  }

  if (const auto& length = intrinsic_length.GetLength()) {
    DCHECK(length->IsFixed());
    return LayoutUnit(length->Value());
  }

  return kIndefiniteSize;
}

LayoutUnit LayoutBox::DefaultIntrinsicContentInlineSize() const {
  NOT_DESTROYED();

  if (!IsA<Element>(GetNode()))
    return kIndefiniteSize;
  const Element& element = *To<Element>(GetNode());

  const bool apply_fixed_size = StyleRef().ApplyControlFixedSize(&element);
  const auto* select = DynamicTo<HTMLSelectElement>(element);
  if (select && select->UsesMenuList() && !select->IsAppearanceBaseButton())
      [[unlikely]] {
    return apply_fixed_size ? MenuListIntrinsicInlineSize(*select, *this)
                            : kIndefiniteSize;
  }
  const auto* input = DynamicTo<HTMLInputElement>(element);
  if (input) [[unlikely]] {
    if (input->IsTextField() && apply_fixed_size) {
      return TextFieldIntrinsicInlineSize(*input, *this);
    }
    FormControlType type = input->FormControlType();
    if (type == FormControlType::kInputFile && apply_fixed_size) {
      return FileUploadControlIntrinsicInlineSize(*input, *this);
    }
    if (type == FormControlType::kInputRange) {
      return SliderIntrinsicInlineSize(*this);
    }
    auto effective_appearance = StyleRef().EffectiveAppearance();
    if (effective_appearance == kCheckboxPart) {
      return ThemePartIntrinsicSize(*this, WebThemeEngine::kPartCheckbox)
          .inline_size;
    }
    if (effective_appearance == kRadioPart) {
      return ThemePartIntrinsicSize(*this, WebThemeEngine::kPartRadio)
          .inline_size;
    }
    return kIndefiniteSize;
  }
  const auto* textarea = DynamicTo<HTMLTextAreaElement>(element);
  if (textarea && apply_fixed_size) [[unlikely]] {
    return TextAreaIntrinsicInlineSize(*textarea, *this);
  }
  if (IsSliderContainer(element))
    return SliderIntrinsicInlineSize(*this);

  return kIndefiniteSize;
}

LayoutUnit LayoutBox::DefaultIntrinsicContentBlockSize() const {
  NOT_DESTROYED();

  auto effective_appearance = StyleRef().EffectiveAppearance();
  if (effective_appearance == kCheckboxPart) {
    return ThemePartIntrinsicSize(*this, WebThemeEngine::kPartCheckbox)
        .block_size;
  }
  if (effective_appearance == kRadioPart) {
    return ThemePartIntrinsicSize(*this, WebThemeEngine::kPartRadio).block_size;
  }

  if (!StyleRef().ApplyControlFixedSize(GetNode())) {
    return kIndefiniteSize;
  }
  if (const auto* select = DynamicTo<HTMLSelectElement>(GetNode())) {
    if (!select->IsAppearanceBaseButton()) {
      if (select->UsesMenuList()) {
        return MenuListIntrinsicBlockSize(*select, *this);
      }
      return ListBoxItemBlockSize(*select, *this) * select->ListBoxSize() -
             ComputeLogicalScrollbars().BlockSum();
    }
  }
  if (IsTextField()) {
    return TextFieldIntrinsicBlockSize(*To<HTMLInputElement>(GetNode()), *this);
  }
  if (IsTextArea()) {
    return TextAreaIntrinsicBlockSize(*To<HTMLTextAreaElement>(GetNode()),
                                      *this);
  }

  return kIndefiniteSize;
}

LayoutUnit LayoutBox::LogicalLeft() const {
  NOT_DESTROYED();
  auto [offset, container_writing_mode] = LogicalLocation(*this);
  return IsParallelWritingMode(container_writing_mode,
                               StyleRef().GetWritingMode())
             ? offset.inline_offset
             : offset.block_offset;
}

LayoutUnit LayoutBox::LogicalTop() const {
  NOT_DESTROYED();
  auto [offset, container_writing_mode] = LogicalLocation(*this);
  return IsParallelWritingMode(container_writing_mode,
                               StyleRef().GetWritingMode())
             ? offset.block_offset
             : offset.inline_offset;
}

gfx::QuadF LayoutBox::AbsoluteContentQuad(MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  PhysicalRect rect = PhysicalContentBoxRect();
  return LocalRectToAbsoluteQuad(rect, flags);
}

PhysicalRect LayoutBox::PhysicalBackgroundRect(
    BackgroundRectType rect_type) const {
  NOT_DESTROYED();
  // If the background transfers to view, the used background of this object
  // is transparent.
  if (rect_type == kBackgroundKnownOpaqueRect && BackgroundTransfersToView())
    return PhysicalRect();

  std::optional<EFillBox> background_box;
  Color background_color = ResolveColor(GetCSSPropertyBackgroundColor());
  // Find the largest background rect of the given opaqueness.
  for (const FillLayer* cur = &(StyleRef().BackgroundLayers()); cur;
       cur = cur->Next()) {
    EFillBox current_clip = cur->Clip();
    if (rect_type == kBackgroundKnownOpaqueRect) {
      if (current_clip == EFillBox::kText)
        continue;

      if (cur->GetBlendMode() != BlendMode::kNormal ||
          cur->Composite() != kCompositeSourceOver)
        continue;

      bool layer_known_opaque = false;
      // Check if the image is opaque and fills the clip.
      if (const StyleImage* image = cur->GetImage()) {
        if ((cur->Repeat().x == EFillRepeat::kRepeatFill ||
             cur->Repeat().x == EFillRepeat::kRoundFill) &&
            (cur->Repeat().y == EFillRepeat::kRepeatFill ||
             cur->Repeat().y == EFillRepeat::kRoundFill) &&
            image->KnownToBeOpaque(GetDocument(), StyleRef())) {
          layer_known_opaque = true;
        }
      }

      // The background color is painted into the last layer.
      if (!cur->Next() && background_color.IsOpaque()) {
        layer_known_opaque = true;
      }

      // If neither the image nor the color are opaque then skip this layer.
      if (!layer_known_opaque)
        continue;
    } else {
      // Ignore invisible background layers for kBackgroundPaintedExtent.
      DCHECK_EQ(rect_type, kBackgroundPaintedExtent);
      if (!cur->GetImage() &&
          (cur->Next() || background_color.IsFullyTransparent())) {
        continue;
      }
      // A content-box clipped fill layer can be scrolled into the padding box
      // of the overflow container.
      if (current_clip == EFillBox::kContent &&
          cur->Attachment() == EFillAttachment::kLocal) {
        current_clip = EFillBox::kPadding;
      }
    }

    // Restrict clip if attachment is local.
    if (current_clip == EFillBox::kBorder &&
        cur->Attachment() == EFillAttachment::kLocal)
      current_clip = EFillBox::kPadding;

    background_box = background_box
                         ? EnclosingFillBox(*background_box, current_clip)
                         : current_clip;
  }

  if (!background_box)
    return PhysicalRect();

  if (*background_box == EFillBox::kText) {
    DCHECK_NE(rect_type, kBackgroundKnownOpaqueRect);
    *background_box = EFillBox::kBorder;
  }

  if (rect_type == kBackgroundPaintedExtent &&
      *background_box == EFillBox::kBorder &&
      BackgroundClipBorderBoxIsEquivalentToPaddingBox()) {
    *background_box = EFillBox::kPadding;
  }

  switch (*background_box) {
    case EFillBox::kBorder:
      return PhysicalBorderBoxRect();
    case EFillBox::kPadding:
      return PhysicalPaddingBoxRect();
    case EFillBox::kContent:
      return PhysicalContentBoxRect();
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return PhysicalRect();
}

void LayoutBox::AddOutlineRects(OutlineRectCollector& collector,
                                OutlineInfo* info,
                                const PhysicalOffset& additional_offset,
                                OutlineType) const {
  NOT_DESTROYED();
  collector.AddRect(PhysicalRect(additional_offset, Size()));
  if (info)
    *info = OutlineInfo::GetFromStyle(StyleRef());
}

bool LayoutBox::CanResize() const {
  NOT_DESTROYED();
  // We need a special case for <iframe> because they never have
  // hasOverflowClip(). However, they do "implicitly" clip their contents, so
  // we want to allow resizing them also.
  return (IsScrollContainer() || IsLayoutIFrame()) && StyleRef().HasResize();
}

bool LayoutBox::HasScrollbarGutters(ScrollbarOrientation orientation) const {
  NOT_DESTROYED();
  if (StyleRef().IsScrollbarGutterAuto())
    return false;

  DCHECK(StyleRef().IsScrollbarGutterStable());

  // Scrollbar-gutter propagates to the viewport
  // (see:|StyleResolver::PropagateStyleToViewport|).
  if (orientation == kVerticalScrollbar) {
    EOverflow overflow = StyleRef().OverflowY();
    return StyleRef().IsHorizontalWritingMode() &&
           (overflow == EOverflow::kAuto || overflow == EOverflow::kScroll ||
            overflow == EOverflow::kHidden) &&
           !UsesOverlayScrollbars() &&
           GetNode() != GetDocument().ViewportDefiningElement();
  } else {
    EOverflow overflow = StyleRef().OverflowX();
    return !StyleRef().IsHorizontalWritingMode() &&
           (overflow == EOverflow::kAuto || overflow == EOverflow::kScroll ||
            overflow == EOverflow::kHidden) &&
           !UsesOverlayScrollbars() &&
           GetNode() != GetDocument().ViewportDefiningElement();
  }
}

PhysicalBoxStrut LayoutBox::ComputeScrollbarsInternal(
    ShouldClampToContentBox clamp_to_content_box,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior,
    ShouldIncludeScrollbarGutter include_scrollbar_gutter) const {
  NOT_DESTROYED();
  PhysicalBoxStrut scrollbars;
  PaintLayerScrollableArea* scrollable_area = GetScrollableArea();

  if (include_scrollbar_gutter == kIncludeScrollbarGutter &&
      HasScrollbarGutters(kVerticalScrollbar)) {
    LayoutUnit gutter_size = LayoutUnit(HypotheticalScrollbarThickness(
        *this, kVerticalScrollbar, /* include_overlay_thickness */ true));
    if (ShouldPlaceVerticalScrollbarOnLeft()) {
      scrollbars.left = gutter_size;
      if (StyleRef().IsScrollbarGutterBothEdges())
        scrollbars.right = gutter_size;
    } else {
      scrollbars.right = gutter_size;
      if (StyleRef().IsScrollbarGutterBothEdges())
        scrollbars.left = gutter_size;
    }
  } else if (scrollable_area) {
    if (ShouldPlaceVerticalScrollbarOnLeft()) {
      scrollbars.left = LayoutUnit(scrollable_area->VerticalScrollbarWidth(
          overlay_scrollbar_clip_behavior));
    } else {
      scrollbars.right = LayoutUnit(scrollable_area->VerticalScrollbarWidth(
          overlay_scrollbar_clip_behavior));
    }
  }

  if (include_scrollbar_gutter == kIncludeScrollbarGutter &&
      HasScrollbarGutters(kHorizontalScrollbar)) {
    LayoutUnit gutter_size = LayoutUnit(
        HypotheticalScrollbarThickness(*this, kHorizontalScrollbar,
                                       /* include_overlay_thickness */ true));
    scrollbars.bottom = gutter_size;
    if (StyleRef().IsScrollbarGutterBothEdges())
      scrollbars.top = gutter_size;
  } else if (scrollable_area) {
    scrollbars.bottom = LayoutUnit(scrollable_area->HorizontalScrollbarHeight(
        overlay_scrollbar_clip_behavior));
  }

  // Use the width of the vertical scrollbar, unless it's larger than the
  // logical width of the content box, in which case we'll use that instead.
  // Scrollbar handling is quite bad in such situations, and this code here
  // is just to make sure that left-hand scrollbars don't mess up
  // scrollWidth. For the full story, visit http://crbug.com/724255.
  if (scrollbars.left > 0 && clamp_to_content_box == kClampToContentBox) {
    LayoutUnit max_width = Size().width - BorderAndPaddingWidth();
    scrollbars.left =
        std::min(scrollbars.left, max_width.ClampNegativeToZero());
  }

  return scrollbars;
}

void LayoutBox::Autoscroll(const PhysicalOffset& position_in_root_frame) {
  NOT_DESTROYED();
  LocalFrame* frame = GetFrame();
  if (!frame)
    return;

  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return;

  PhysicalOffset absolute_position =
      frame_view->ConvertFromRootFrame(position_in_root_frame);
  mojom::blink::ScrollIntoViewParamsPtr params =
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::ToEdgeIfNeeded(), ScrollAlignment::ToEdgeIfNeeded(),
          mojom::blink::ScrollType::kUser);
  scroll_into_view_util::ScrollRectToVisible(
      *this,
      PhysicalRect(absolute_position,
                   PhysicalSize(LayoutUnit(1), LayoutUnit(1))),
      std::move(params));
}

// If specified point is outside the border-belt-excluded box (the border box
// inset by the autoscroll activation threshold), returned offset denotes
// direction of scrolling.
PhysicalOffset LayoutBox::CalculateAutoscrollDirection(
    const gfx::PointF& point_in_root_frame) const {
  NOT_DESTROYED();
  if (!GetFrame())
    return PhysicalOffset();

  LocalFrameView* frame_view = GetFrame()->View();
  if (!frame_view)
    return PhysicalOffset();

  PhysicalRect absolute_scrolling_box(AbsoluteBoundingBoxRect());

  // Exclude scrollbars so the border belt (activation area) starts from the
  // scrollbar-content edge rather than the window edge.
  ExcludeScrollbars(absolute_scrolling_box,
                    kExcludeOverlayScrollbarSizeForHitTesting);

  PhysicalRect belt_box =
      View()->GetFrameView()->ConvertToRootFrame(absolute_scrolling_box);
  belt_box.Inflate(LayoutUnit(-kAutoscrollBeltSize));
  gfx::PointF point = point_in_root_frame;

  if (point.x() < belt_box.X())
    point.Offset(-kAutoscrollBeltSize, 0);
  else if (point.x() > belt_box.Right())
    point.Offset(kAutoscrollBeltSize, 0);

  if (point.y() < belt_box.Y())
    point.Offset(0, -kAutoscrollBeltSize);
  else if (point.y() > belt_box.Bottom())
    point.Offset(0, kAutoscrollBeltSize);

  return PhysicalOffset::FromVector2dFRound(point - point_in_root_frame);
}

LayoutBox* LayoutBox::FindAutoscrollable(LayoutObject* layout_object,
                                         bool is_middle_click_autoscroll) {
  while (layout_object && !(layout_object->IsBox() &&
                            To<LayoutBox>(layout_object)->IsUserScrollable())) {
    // Do not start selection-based autoscroll when the node is inside a
    // fixed-position element.
    if (!is_middle_click_autoscroll && layout_object->IsBox() &&
        To<LayoutBox>(layout_object)->IsFixedToView()) {
      return nullptr;
    }

    if (!layout_object->Parent() &&
        layout_object->GetNode() == layout_object->GetDocument() &&
        layout_object->GetDocument().LocalOwner()) {
      layout_object =
          layout_object->GetDocument().LocalOwner()->GetLayoutObject();
    } else {
      layout_object = layout_object->Parent();
    }
  }

  return DynamicTo<LayoutBox>(layout_object);
}

bool LayoutBox::HasHorizontallyScrollableAncestor(LayoutObject* layout_object) {
  while (layout_object) {
    if (layout_object->IsBox() &&
        To<LayoutBox>(layout_object)->HasScrollableOverflowX())
      return true;

    // Scroll is not propagating.
    if (layout_object->StyleRef().OverscrollBehaviorX() !=
        EOverscrollBehavior::kAuto)
      break;

    if (!layout_object->Parent() &&
        layout_object->GetNode() == layout_object->GetDocument() &&
        layout_object->GetDocument().LocalOwner()) {
      layout_object =
          layout_object->GetDocument().LocalOwner()->GetLayoutObject();
    } else {
      layout_object = layout_object->Parent();
    }
  }

  return false;
}

gfx::Vector2d LayoutBox::OriginAdjustmentForScrollbars() const {
  NOT_DESTROYED();
  if (CanSkipComputeScrollbars())
    return gfx::Vector2d();

  PhysicalBoxStrut scrollbars = ComputeScrollbarsInternal(kClampToContentBox);
  return gfx::Vector2d(scrollbars.left.ToInt(), scrollbars.top.ToInt());
}

gfx::Point LayoutBox::ScrollOrigin() const {
  NOT_DESTROYED();
  return GetScrollableArea() ? GetScrollableArea()->ScrollOrigin()
                             : gfx::Point();
}

PhysicalOffset LayoutBox::ScrolledContentOffset() const {
  NOT_DESTROYED();
  DCHECK(IsScrollContainer());
  DCHECK(GetScrollableArea());
  return PhysicalOffset::FromVector2dFFloor(
      GetScrollableArea()->GetScrollOffset());
}

gfx::Vector2d LayoutBox::PixelSnappedScrolledContentOffset() const {
  NOT_DESTROYED();
  DCHECK(IsScrollContainer());
  DCHECK(GetScrollableArea());
  return GetScrollableArea()->ScrollOffsetInt();
}

PhysicalRect LayoutBox::ClippingRect(const PhysicalOffset& location) const {
  NOT_DESTROYED();
  PhysicalRect result(InfiniteIntRect());
  if (ShouldClipOverflowAlongEitherAxis())
    result = OverflowClipRect(location);

  if (HasClip())
    result.Intersect(ClipRect(location));

  return result;
}

gfx::PointF LayoutBox::PerspectiveOrigin(const PhysicalSize* size) const {
  if (!HasTransformRelatedProperty())
    return gfx::PointF();

  // Use the |size| parameter instead of |Size()| if present.
  gfx::SizeF float_size = size ? gfx::SizeF(*size) : gfx::SizeF(Size());

  return PointForLengthPoint(StyleRef().PerspectiveOrigin(), float_size);
}

bool LayoutBox::MapVisualRectToContainer(
    const LayoutObject* container_object,
    const PhysicalOffset& container_offset,
    const LayoutObject* ancestor,
    VisualRectFlags visual_rect_flags,
    TransformState& transform_state) const {
  NOT_DESTROYED();
  bool container_preserve_3d = container_object->StyleRef().Preserves3D() &&
                               container_object == NearestAncestorForElement();

  TransformState::TransformAccumulation accumulation =
      container_preserve_3d ? TransformState::kAccumulateTransform
                            : TransformState::kFlattenTransform;

  // If there is no transform on this box, adjust for container offset and
  // container scrolling, then apply container clip.
  if (!ShouldUseTransformFromContainer(container_object)) {
    transform_state.Move(container_offset, accumulation);
    if (container_object->IsBox() && container_object != ancestor &&
        !To<LayoutBox>(container_object)
             ->MapContentsRectToBoxSpace(transform_state, accumulation, *this,
                                         visual_rect_flags)) {
      return false;
    }
    return true;
  }

  // Otherwise, do the following:
  // 1. Expand for pixel snapping.
  // 2. Generate transformation matrix combining, in this order
  //    a) transform,
  //    b) container offset,
  //    c) container scroll offset,
  //    d) perspective applied by container.
  // 3. Apply transform Transform+flattening.
  // 4. Apply container clip.

  // 1. Expand for pixel snapping.
  // Use EnclosingBoundingBox because we cannot properly compute pixel
  // snapping for painted elements within the transform since we don't know
  // the desired subpixel accumulation at this point, and the transform may
  // include a scale. This only makes sense for non-preserve3D.
  //
  // TODO(dbaron): Does the flattening here need to be done for the
  // early return case above as well?
  // (Why is this flattening needed in addition to the flattening done by
  // using TransformState::kAccumulateTransform?)
  if (!StyleRef().Preserves3D()) {
    transform_state.Flatten();
    transform_state.SetQuad(gfx::QuadF(gfx::RectF(
        gfx::ToEnclosingRect(transform_state.LastPlanarQuad().BoundingBox()))));
  }

  // 2. Generate transformation matrix.
  // a) Transform.
  gfx::Transform transform;
  if (Layer() && Layer()->Transform())
    transform.PreConcat(Layer()->CurrentTransform());

  // b) Container offset.
  transform.PostTranslate(container_offset.left.ToFloat(),
                          container_offset.top.ToFloat());

  // c) Container scroll offset.
  if (container_object->IsBox() && container_object != ancestor &&
      To<LayoutBox>(container_object)->ContainedContentsScroll(*this)) {
    PhysicalOffset offset(
        -To<LayoutBox>(container_object)->ScrolledContentOffset());
    transform.PostTranslate(offset.left, offset.top);
  }

  bool has_perspective = container_object && container_object->HasLayer() &&
                         container_object->StyleRef().HasPerspective();
  if (has_perspective && container_object != NearestAncestorForElement()) {
    has_perspective = false;

    if (StyleRef().Preserves3D() || transform.Creates3d()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kDifferentPerspectiveCBOrParent);
    }
  }

  // d) Perspective applied by container.
  if (has_perspective) {
    // Perspective on the container affects us, so we have to factor it in here.
    DCHECK(container_object->HasLayer());
    gfx::PointF perspective_origin;
    if (const auto* container_box = DynamicTo<LayoutBox>(container_object))
      perspective_origin = container_box->PerspectiveOrigin();

    gfx::Transform perspective_matrix;
    perspective_matrix.ApplyPerspectiveDepth(
        container_object->StyleRef().UsedPerspective());
    perspective_matrix.ApplyTransformOrigin(perspective_origin.x(),
                                            perspective_origin.y(), 0);

    transform = perspective_matrix * transform;
  }

  // 3. Apply transform and flatten.
  transform_state.ApplyTransform(transform, accumulation);
  if (!container_preserve_3d)
    transform_state.Flatten();

  // 4. Apply container clip.
  if (container_object->IsBox() && container_object != ancestor &&
      container_object->HasClipRelatedProperty()) {
    return To<LayoutBox>(container_object)
        ->ApplyBoxClips(transform_state, accumulation, visual_rect_flags);
  }

  return true;
}

bool LayoutBox::MapContentsRectToBoxSpace(
    TransformState& transform_state,
    TransformState::TransformAccumulation accumulation,
    const LayoutObject& contents,
    VisualRectFlags visual_rect_flags) const {
  NOT_DESTROYED();
  if (!HasClipRelatedProperty())
    return true;

  if (ContainedContentsScroll(contents))
    transform_state.Move(-ScrolledContentOffset());

  return ApplyBoxClips(transform_state, accumulation, visual_rect_flags);
}

bool LayoutBox::ContainedContentsScroll(const LayoutObject& contents) const {
  NOT_DESTROYED();
  if (IsA<LayoutView>(this) &&
      contents.StyleRef().GetPosition() == EPosition::kFixed) {
    return false;
  }
  return IsScrollContainer();
}

bool LayoutBox::ApplyBoxClips(
    TransformState& transform_state,
    TransformState::TransformAccumulation accumulation,
    VisualRectFlags visual_rect_flags) const {
  NOT_DESTROYED();
  // This won't work fully correctly for fixed-position elements, who should
  // receive CSS clip but for whom the current object is not in the containing
  // block chain.
  PhysicalRect clip_rect = ClippingRect(PhysicalOffset());

  transform_state.Flatten();
  PhysicalRect rect(
      gfx::ToEnclosingRect(transform_state.LastPlanarQuad().BoundingBox()));
  bool does_intersect;
  if (visual_rect_flags & kEdgeInclusive) {
    does_intersect = rect.InclusiveIntersect(clip_rect);
  } else {
    rect.Intersect(clip_rect);
    does_intersect = !rect.IsEmpty();
  }
  transform_state.SetQuad(gfx::QuadF(gfx::RectF(rect)));

  return does_intersect;
}

// TODO (lajava) Shouldn't we implement these functions based on physical
// direction ?.
LayoutUnit LayoutBox::OverrideContainingBlockContentLogicalWidth() const {
  NOT_DESTROYED();
  DCHECK(HasOverrideContainingBlockContentLogicalWidth());
  return rare_data_->override_containing_block_content_logical_width_;
}

// TODO (lajava) Shouldn't we implement these functions based on physical
// direction ?.
bool LayoutBox::HasOverrideContainingBlockContentLogicalWidth() const {
  NOT_DESTROYED();
  return rare_data_ &&
         rare_data_->has_override_containing_block_content_logical_width_;
}

// TODO (lajava) Shouldn't we implement these functions based on physical
// direction ?.
void LayoutBox::SetOverrideContainingBlockContentLogicalWidth(
    LayoutUnit logical_width) {
  NOT_DESTROYED();
  DCHECK_GE(logical_width, LayoutUnit(-1));
  EnsureRareData().override_containing_block_content_logical_width_ =
      logical_width;
  EnsureRareData().has_override_containing_block_content_logical_width_ = true;
}

// TODO (lajava) Shouldn't we implement these functions based on physical
// direction ?.
void LayoutBox::ClearOverrideContainingBlockContentSize() {
  NOT_DESTROYED();
  if (!rare_data_)
    return;
  EnsureRareData().has_override_containing_block_content_logical_width_ = false;
}

bool LayoutBox::HitTestAllPhases(HitTestResult& result,
                                 const HitTestLocation& hit_test_location,
                                 const PhysicalOffset& accumulated_offset) {
  NOT_DESTROYED();
  if (!MayIntersect(result, hit_test_location, accumulated_offset))
    return false;
  return LayoutObject::HitTestAllPhases(result, hit_test_location,
                                        accumulated_offset);
}

bool LayoutBox::HitTestOverflowControl(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& adjusted_location) const {
  NOT_DESTROYED();

  auto* scrollable_area = GetScrollableArea();
  if (!scrollable_area)
    return false;

  if (!VisibleToHitTestRequest(result.GetHitTestRequest()))
    return false;

  PhysicalOffset local_point = hit_test_location.Point() - adjusted_location;
  if (!scrollable_area->HitTestOverflowControls(result,
                                                ToRoundedPoint(local_point)))
    return false;

  UpdateHitTestResult(result, local_point);
  return result.AddNodeToListBasedTestResult(
             NodeForHitTest(), hit_test_location) == kStopHitTesting;
}

bool LayoutBox::NodeAtPoint(HitTestResult& result,
                            const HitTestLocation& hit_test_location,
                            const PhysicalOffset& accumulated_offset,
                            HitTestPhase phase) {
  NOT_DESTROYED();
  if (!MayIntersect(result, hit_test_location, accumulated_offset))
    return false;

  if (phase == HitTestPhase::kForeground && !HasSelfPaintingLayer() &&
      HitTestOverflowControl(result, hit_test_location, accumulated_offset))
    return true;

  bool skip_children = (result.GetHitTestRequest().GetStopNode() == this) ||
                       ChildPaintBlockedByDisplayLock();
  if (!skip_children && ShouldClipOverflowAlongEitherAxis()) {
    // PaintLayer::HitTestFragmentsWithPhase() checked the fragments'
    // foreground rect for intersection if a layer is self painting,
    // so only do the overflow clip check here for non-self-painting layers.
    if (!HasSelfPaintingLayer() &&
        !hit_test_location.Intersects(OverflowClipRect(
            accumulated_offset, kExcludeOverlayScrollbarSizeForHitTesting))) {
      skip_children = true;
    }
    if (!skip_children && StyleRef().HasBorderRadius()) {
      PhysicalRect bounds_rect(accumulated_offset, Size());
      skip_children = !hit_test_location.Intersects(
          RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(StyleRef(),
                                                                bounds_rect));
    }
  }

  if (!skip_children &&
      HitTestChildren(result, hit_test_location, accumulated_offset, phase)) {
    return true;
  }

  if (StyleRef().HasBorderRadius() &&
      HitTestClippedOutByBorder(hit_test_location, accumulated_offset))
    return false;

  // Now hit test ourselves.
  if (IsInSelfHitTestingPhase(phase) &&
      VisibleToHitTestRequest(result.GetHitTestRequest())) {
    PhysicalRect bounds_rect;
    if (result.GetHitTestRequest().IsHitTestVisualOverflow()) [[unlikely]] {
      bounds_rect = VisualOverflowRectIncludingFilters();
    } else {
      bounds_rect = PhysicalBorderBoxRect();
    }
    bounds_rect.Move(accumulated_offset);
    if (hit_test_location.Intersects(bounds_rect)) {
      UpdateHitTestResult(result,
                          hit_test_location.Point() - accumulated_offset);
      if (result.AddNodeToListBasedTestResult(NodeForHitTest(),
                                              hit_test_location,
                                              bounds_rect) == kStopHitTesting)
        return true;
    }
  }

  return false;
}

bool LayoutBox::HitTestChildren(HitTestResult& result,
                                const HitTestLocation& hit_test_location,
                                const PhysicalOffset& accumulated_offset,
                                HitTestPhase phase) {
  NOT_DESTROYED();
  for (LayoutObject* child = SlowLastChild(); child;
       child = child->PreviousSibling()) {
    if (child->HasLayer() &&
        To<LayoutBoxModelObject>(child)->Layer()->IsSelfPaintingLayer())
      continue;

    PhysicalOffset child_accumulated_offset = accumulated_offset;
    if (auto* box = DynamicTo<LayoutBox>(child))
      child_accumulated_offset += box->PhysicalLocation(this);

    if (child->NodeAtPoint(result, hit_test_location, child_accumulated_offset,
                           phase))
      return true;
  }

  return false;
}

bool LayoutBox::HitTestClippedOutByBorder(
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& border_box_location) const {
  NOT_DESTROYED();
  PhysicalRect border_rect = PhysicalBorderBoxRect();
  border_rect.Move(border_box_location);
  return !hit_test_location.Intersects(
      RoundedBorderGeometry::PixelSnappedRoundedBorder(StyleRef(),
                                                       border_rect));
}

void LayoutBox::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  NOTREACHED();
}

PhysicalRect LayoutBox::BackgroundPaintedExtent() const {
  NOT_DESTROYED();
  return PhysicalBackgroundRect(kBackgroundPaintedExtent);
}

bool LayoutBox::BackgroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect) const {
  NOT_DESTROYED();
  // If the element has appearance, it might be painted by theme.
  // We cannot be sure if theme paints the background opaque.
  // In this case it is safe to not assume opaqueness.
  // FIXME: May be ask theme if it paints opaque.
  if (StyleRef().HasEffectiveAppearance())
    return false;
  // FIXME: Check the opaqueness of background images.

  // FIXME: Use rounded rect if border radius is present.
  if (StyleRef().HasBorderRadius())
    return false;
  if (HasClipPath())
    return false;
  if (StyleRef().HasBlendMode())
    return false;
  return PhysicalBackgroundRect(kBackgroundKnownOpaqueRect)
      .Contains(local_rect);
}

// Note that callers are responsible for checking
// ChildPaintBlockedByDisplayLock(), since that is a property of the parent
// rather than of the child.
static bool IsCandidateForOpaquenessTest(const LayoutBox& child_box) {
  // Skip all layers to simplify ForegroundIsKnownToBeOpaqueInRect(). This
  // covers cases of clipped, transformed, translucent, composited, etc.
  if (child_box.HasLayer())
    return false;
  const ComputedStyle& child_style = child_box.StyleRef();
  if (child_style.UsedVisibility() != EVisibility::kVisible ||
      child_style.ShapeOutside()) {
    return false;
  }
  if (child_box.Size().IsZero())
    return false;
  // A replaced element with border-radius always clips the content.
  if (child_box.IsLayoutReplaced() && child_style.HasBorderRadius())
    return false;
  return true;
}

bool LayoutBox::ForegroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect,
    unsigned max_depth_to_test) const {
  NOT_DESTROYED();
  if (!max_depth_to_test)
    return false;
  if (ChildPaintBlockedByDisplayLock())
    return false;
  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    // We do not bother checking descendants of |LayoutInline|, including
    // block-in-inline, because the cost of checking them overweights the
    // benefits.
    if (!child->IsBox())
      continue;
    auto* child_box = To<LayoutBox>(child);
    if (!IsCandidateForOpaquenessTest(*child_box))
      continue;
    DCHECK(!child_box->IsPositioned());
    PhysicalRect child_local_rect = local_rect;
    child_local_rect.Move(-child_box->PhysicalLocation());
    if (child_local_rect.Y() < 0 || child_local_rect.X() < 0) {
      // If there is unobscured area above/left of a static positioned box then
      // the rect is probably not covered. This can cause false-negative in
      // non-horizontal-tb writing mode but is allowed.
      return false;
    }
    if (child_local_rect.Bottom() > child_box->Size().height ||
        child_local_rect.Right() > child_box->Size().width) {
      continue;
    }
    if (RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled() &&
        child->Style()->HasCurrentBackgroundColorAnimation()) {
      return false;
    }
    if (child_box->BackgroundIsKnownToBeOpaqueInRect(child_local_rect))
      return true;
    if (child_box->ForegroundIsKnownToBeOpaqueInRect(child_local_rect,
                                                     max_depth_to_test - 1))
      return true;
  }
  return false;
}

DISABLE_CFI_PERF
bool LayoutBox::ComputeBackgroundIsKnownToBeObscured() const {
  NOT_DESTROYED();
  if (ScrollsOverflow())
    return false;
  // Test to see if the children trivially obscure the background.
  if (!StyleRef().HasBackground())
    return false;
  // Root background painting is special.
  if (IsA<LayoutView>(this))
    return false;
  if (StyleRef().BoxShadow())
    return false;
  return ForegroundIsKnownToBeOpaqueInRect(BackgroundPaintedExtent(),
                                           kBackgroundObscurationTestMaxDepth);
}

void LayoutBox::ImageChanged(WrappedImagePtr image,
                             CanDeferInvalidation defer) {
  NOT_DESTROYED();
  bool is_box_reflect_image =
      (StyleRef().BoxReflect() && StyleRef().BoxReflect()->Mask().GetImage() &&
       StyleRef().BoxReflect()->Mask().GetImage()->Data() == image);

  if (is_box_reflect_image && HasLayer()) {
    Layer()->SetFilterOnEffectNodeDirty();
    SetNeedsPaintPropertyUpdate();
  }

  // TODO(chrishtr): support delayed paint invalidation for animated border
  // images.
  if ((StyleRef().BorderImage().GetImage() &&
       StyleRef().BorderImage().GetImage()->Data() == image) ||
      (StyleRef().MaskBoxImage().GetImage() &&
       StyleRef().MaskBoxImage().GetImage()->Data() == image) ||
      is_box_reflect_image) {
    SetShouldDoFullPaintInvalidationWithoutLayoutChange(
        PaintInvalidationReason::kImage);
  } else {
    for (const FillLayer* layer = &StyleRef().MaskLayers(); layer;
         layer = layer->Next()) {
      if (layer->GetImage() && image == layer->GetImage()->Data()) {
        SetShouldDoFullPaintInvalidationWithoutLayoutChange(
            PaintInvalidationReason::kImage);
        if (layer->GetImage()->IsMaskSource() && IsSVGChild()) {
          // Since an invalid <mask> reference does not yield a paint property
          // on SVG content (see CSSMaskPainter), we need to update paint
          // properties when such a reference changes.
          SetNeedsPaintPropertyUpdate();
        }
        break;
      }
    }
  }

  if (!BackgroundTransfersToView()) {
    for (const FillLayer* layer = &StyleRef().BackgroundLayers(); layer;
         layer = layer->Next()) {
      if (layer->GetImage() && image == layer->GetImage()->Data()) {
        bool maybe_animated =
            layer->GetImage()->CachedImage() &&
            layer->GetImage()->CachedImage()->GetImage() &&
            layer->GetImage()->CachedImage()->GetImage()->MaybeAnimated();
        if (defer == CanDeferInvalidation::kYes && maybe_animated)
          SetMayNeedPaintInvalidationAnimatedBackgroundImage();
        else
          SetBackgroundNeedsFullPaintInvalidation();
        break;
      }
    }
  }

  ShapeValue* shape_outside_value = StyleRef().ShapeOutside();
  if (!GetFrameView()->IsInPerformLayout() && IsFloating() &&
      shape_outside_value && shape_outside_value->GetImage() &&
      shape_outside_value->GetImage()->Data() == image) {
    ShapeOutsideInfo& info = ShapeOutsideInfo::EnsureInfo(*this);
    if (!info.IsComputingShape()) {
      info.MarkShapeAsDirty();
      if (auto* containing_block = ContainingBlock()) {
        containing_block->SetChildNeedsLayout();
      }
    }
  }
}

ResourcePriority LayoutBox::ComputeResourcePriority() const {
  NOT_DESTROYED();
  PhysicalRect view_bounds = ViewRect();
  PhysicalRect object_bounds = PhysicalContentBoxRect();
  // TODO(japhet): Is this IgnoreTransforms correct? Would it be better to use
  // the visual rect (which has ancestor clips and transforms applied)? Should
  // we map to the top-level viewport instead of the current (sub) frame?
  object_bounds.Move(LocalToAbsolutePoint(PhysicalOffset(), kIgnoreTransforms));

  // The object bounds might be empty right now, so intersects will fail since
  // it doesn't deal with empty rects. Use PhysicalRect::Contains in that case.
  bool is_visible;
  if (!object_bounds.IsEmpty())
    is_visible = view_bounds.Intersects(object_bounds);
  else
    is_visible = view_bounds.Contains(object_bounds);

  PhysicalRect screen_rect;
  if (!object_bounds.IsEmpty()) {
    screen_rect = view_bounds;
    screen_rect.Intersect(object_bounds);
  }

  int screen_area = 0;
  if (!screen_rect.IsEmpty() && is_visible)
    screen_area = (screen_rect.Width() * screen_rect.Height()).ToInt();
  return ResourcePriority(
      is_visible ? ResourcePriority::kVisible : ResourcePriority::kNotVisible,
      screen_area);
}

void LayoutBox::LocationChanged() {
  NOT_DESTROYED();
  // The location may change because of layout of other objects. Should check
  // this object for paint invalidation.
  if (!NeedsLayout())
    SetShouldCheckForPaintInvalidation();
}

void LayoutBox::SizeChanged() {
  NOT_DESTROYED();
  SetScrollableAreaSizeChanged(true);
  // The size may change because of layout of other objects. Should check this
  // object for paint invalidation.
  if (!NeedsLayout())
    SetShouldCheckForPaintInvalidation();
  // In flipped blocks writing mode, our children can change physical location,
  // but their flipped location remains the same.
  if (HasFlippedBlocksWritingMode()) {
    if (ChildrenInline())
      SetSubtreeShouldDoFullPaintInvalidation();
    else
      SetSubtreeShouldCheckForPaintInvalidation();
  }
}

bool LayoutBox::IntersectsVisibleViewport() const {
  NOT_DESTROYED();
  LayoutView* layout_view = View();
  while (auto* owner = layout_view->GetFrame()->OwnerLayoutObject()) {
    layout_view = owner->View();
  }
  // If this is the outermost LayoutView then it will always intersect. (`rect`
  // will be the viewport in that case.)
  if (this == layout_view) {
    return true;
  }
  PhysicalRect rect = VisualOverflowRect();
  MapToVisualRectInAncestorSpace(layout_view, rect);
  return rect.Intersects(PhysicalRect(
      layout_view->GetFrameView()->GetScrollableArea()->VisibleContentRect()));
}

void LayoutBox::EnsureIsReadyForPaintInvalidation() {
  NOT_DESTROYED();
  LayoutBoxModelObject::EnsureIsReadyForPaintInvalidation();

  bool new_obscured = ComputeBackgroundIsKnownToBeObscured();
  if (BackgroundIsKnownToBeObscured() != new_obscured) {
    SetBackgroundIsKnownToBeObscured(new_obscured);
    SetBackgroundNeedsFullPaintInvalidation();
  }

  if (MayNeedPaintInvalidationAnimatedBackgroundImage() &&
      !BackgroundIsKnownToBeObscured()) {
    SetBackgroundNeedsFullPaintInvalidation();
    SetShouldDelayFullPaintInvalidation();
  }

  if (ShouldDelayFullPaintInvalidation() && IntersectsVisibleViewport()) {
    // Do regular full paint invalidation if the object with delayed paint
    // invalidation is on screen.
    ClearShouldDelayFullPaintInvalidation();
    DCHECK(ShouldDoFullPaintInvalidation());
  }
}

void LayoutBox::InvalidatePaint(const PaintInvalidatorContext& context) const {
  NOT_DESTROYED();
  BoxPaintInvalidator(*this, context).InvalidatePaint();
}

void LayoutBox::ClearPaintFlags() {
  NOT_DESTROYED();
  LayoutObject::ClearPaintFlags();

  if (auto* scrollable_area = GetScrollableArea()) {
    if (auto* scrollbar =
            DynamicTo<CustomScrollbar>(scrollable_area->HorizontalScrollbar()))
      scrollbar->ClearPaintFlags();
    if (auto* scrollbar =
            DynamicTo<CustomScrollbar>(scrollable_area->VerticalScrollbar()))
      scrollbar->ClearPaintFlags();
  }
}

PhysicalRect LayoutBox::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  NOT_DESTROYED();
  PhysicalRect clip_rect;

  if (IsEffectiveRootScroller()) {
    // If this box is the effective root scroller, use the viewport clipping
    // rect since it will account for the URL bar correctly which the border
    // box does not. We can do this because the effective root scroller is
    // restricted such that it exactly fills the viewport. See
    // RootScrollerController::IsValidRootScroller()
    clip_rect = PhysicalRect(location, View()->ViewRect().size);
  } else {
    clip_rect = PhysicalBorderBoxRect();
    clip_rect.Contract(BorderOutsets());
    clip_rect.Move(location);

    // Videos need to be pre-snapped so that they line up with the
    // display_rect and can enable hardware overlays.
    // Embedded objects are always sized to fit the content rect, but they
    // could overflow by 1px due to pre-snapping. Adjust clip rect to
    // match pre-snapped box as a special case.
    if (IsVideo() || IsLayoutEmbeddedContent())
      clip_rect = LayoutReplaced::PreSnappedRectForPersistentSizing(clip_rect);

    if (HasNonVisibleOverflow()) {
      const auto overflow_clip = GetOverflowClipAxes();
      if (overflow_clip != kOverflowClipBothAxis) {
        ApplyVisibleOverflowToClipRect(overflow_clip, clip_rect);
      } else if (ShouldApplyOverflowClipMargin()) {
        switch (StyleRef().OverflowClipMargin()->GetReferenceBox()) {
          case StyleOverflowClipMargin::ReferenceBox::kBorderBox:
            clip_rect.Expand(BorderOutsets());
            break;
          case StyleOverflowClipMargin::ReferenceBox::kPaddingBox:
            break;
          case StyleOverflowClipMargin::ReferenceBox::kContentBox:
            clip_rect.Contract(PaddingOutsets());
            break;
        }
        clip_rect.Inflate(StyleRef().OverflowClipMargin()->GetMargin());
      }
    }
  }

  if (IsScrollContainer()) {
    // The additional gutters created by scrollbar-gutter don't occlude the
    // content underneath, so they should not be clipped out here.
    // See https://crbug.com/710214
    ExcludeScrollbars(clip_rect, overlay_scrollbar_clip_behavior,
                      kExcludeScrollbarGutter);
  }

  if (IsA<HTMLInputElement>(GetNode())) [[unlikely]] {
    // We only apply a clip to <input> buttons, and not regular <button>s.
    if (IsTextField() || IsInputButton()) {
      DCHECK(HasControlClip());
      PhysicalRect control_clip = PhysicalPaddingBoxRect();
      control_clip.Move(location);
      clip_rect.Intersect(control_clip);
    }
  } else if (IsMenuList()) [[unlikely]] {
    DCHECK(HasControlClip());
    PhysicalRect control_clip = PhysicalContentBoxRect();
    control_clip.Move(location);
    clip_rect.Intersect(control_clip);
  } else {
    DCHECK(!HasControlClip());
  }

  return clip_rect;
}

bool LayoutBox::HasControlClip() const {
  NOT_DESTROYED();
  if (IsTextField() || IsMenuList() || IsInputButton()) [[unlikely]] {
    return true;
  }
  return false;
}

void LayoutBox::ExcludeScrollbars(
    PhysicalRect& rect,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior,
    ShouldIncludeScrollbarGutter include_scrollbar_gutter) const {
  NOT_DESTROYED();
  if (CanSkipComputeScrollbars())
    return;

  PhysicalBoxStrut scrollbars = ComputeScrollbarsInternal(
      kDoNotClampToContentBox, overlay_scrollbar_clip_behavior,
      include_scrollbar_gutter);
  rect.offset.top += scrollbars.top;
  rect.offset.left += scrollbars.left;
  rect.size.width -= scrollbars.HorizontalSum();
  rect.size.height -= scrollbars.VerticalSum();
  rect.size.ClampNegativeToZero();
}

PhysicalRect LayoutBox::ClipRect(const PhysicalOffset& location) const {
  NOT_DESTROYED();
  PhysicalRect clip_rect(location, Size());
  LayoutUnit width = Size().width;
  LayoutUnit height = Size().height;

  if (!StyleRef().ClipLeft().IsAuto()) {
    LayoutUnit c = ValueForLength(StyleRef().ClipLeft(), width);
    clip_rect.offset.left += c;
    clip_rect.size.width -= c;
  }

  if (!StyleRef().ClipRight().IsAuto()) {
    clip_rect.size.width -=
        width - ValueForLength(StyleRef().ClipRight(), width);
  }

  if (!StyleRef().ClipTop().IsAuto()) {
    LayoutUnit c = ValueForLength(StyleRef().ClipTop(), height);
    clip_rect.offset.top += c;
    clip_rect.size.height -= c;
  }

  if (!StyleRef().ClipBottom().IsAuto()) {
    clip_rect.size.height -=
        height - ValueForLength(StyleRef().ClipBottom(), height);
  }

  return clip_rect;
}

LayoutUnit LayoutBox::ContainingBlockLogicalHeightForRelPositioned() const {
  NOT_DESTROYED();
  DCHECK(IsRelPositioned());

  // TODO(ikilpatrick): This is resolving percentages against incorrectly if
  // the container is an inline.
  auto* cb = To<LayoutBoxModelObject>(Container());
  return ContainingBlockLogicalHeightForPositioned(cb) -
         cb->PaddingLogicalHeight();
}

LayoutUnit LayoutBox::ContainingBlockLogicalWidthForContent() const {
  NOT_DESTROYED();
  if (HasOverrideContainingBlockContentLogicalWidth())
    return OverrideContainingBlockContentLogicalWidth();

  LayoutBlock* cb = ContainingBlock();
  if (IsOutOfFlowPositioned())
    return cb->ClientLogicalWidth();
  return cb->AvailableLogicalWidth();
}

PhysicalOffset LayoutBox::OffsetFromContainerInternal(
    const LayoutObject* o,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  DCHECK_EQ(o, Container());

  PhysicalOffset offset = PhysicalLocation();

  if (IsStickyPositioned() && !(mode & kIgnoreStickyOffset)) {
    offset += StickyPositionOffset();
  }

  if (o->IsScrollContainer())
    offset += OffsetFromScrollableContainer(o, mode & kIgnoreScrollOffset);

  if (NeedsAnchorPositionScrollAdjustment()) {
    offset += AnchorPositionScrollTranslationOffset();
  }

  return offset;
}

bool LayoutBox::HasInlineFragments() const {
  NOT_DESTROYED();
  return first_fragment_item_index_;
}

void LayoutBox::ClearFirstInlineFragmentItemIndex() {
  NOT_DESTROYED();
  CHECK(IsInLayoutNGInlineFormattingContext()) << *this;
  first_fragment_item_index_ = 0u;
}

void LayoutBox::SetFirstInlineFragmentItemIndex(wtf_size_t index) {
  NOT_DESTROYED();
  CHECK(IsInLayoutNGInlineFormattingContext()) << *this;
  DCHECK_NE(index, 0u);
  first_fragment_item_index_ = index;
}

void LayoutBox::InLayoutNGInlineFormattingContextWillChange(bool new_value) {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext())
    ClearFirstInlineFragmentItemIndex();
}

bool LayoutBox::PhysicalFragmentList::MayHaveFragmentItems() const {
  return !IsEmpty() && front().IsInlineFormattingContext();
}

bool LayoutBox::PhysicalFragmentList::SlowHasFragmentItems() const {
  for (const PhysicalBoxFragment& fragment : *this) {
    if (fragment.HasItems())
      return true;
  }
  return false;
}

wtf_size_t LayoutBox::PhysicalFragmentList::IndexOf(
    const PhysicalBoxFragment& fragment) const {
  wtf_size_t index = 0;
  for (const auto& result : layout_results_) {
    if (&result->GetPhysicalFragment() == &fragment) {
      return index;
    }
    ++index;
  }
  return kNotFound;
}

bool LayoutBox::PhysicalFragmentList::Contains(
    const PhysicalBoxFragment& fragment) const {
  return IndexOf(fragment) != kNotFound;
}

void LayoutBox::AddMeasureLayoutResult(const LayoutResult* result) {
  // Ensure the given result is valid for the measure cache.
  if (result->Status() != LayoutResult::kSuccess) {
    return;
  }
  if (result->GetConstraintSpaceForCaching().CacheSlot() !=
      LayoutResultCacheSlot::kMeasure) {
    return;
  }
  DCHECK(
      To<PhysicalBoxFragment>(result->GetPhysicalFragment()).IsOnlyForNode());

  if (!measure_cache_) {
    measure_cache_ = MakeGarbageCollected<MeasureCache>();
  }
  // Clear out old measure results if we need non-simplifed layout.
  if (NeedsLayout() && !NeedsSimplifiedLayoutOnly()) {
    measure_cache_->Clear();
  }
  measure_cache_->Add(result);
}

void LayoutBox::SetCachedLayoutResult(const LayoutResult* result,
                                      wtf_size_t index) {
  NOT_DESTROYED();
  if (result->GetConstraintSpaceForCaching().CacheSlot() ==
      LayoutResultCacheSlot::kMeasure) {
    DCHECK(!result->GetPhysicalFragment().GetBreakToken());
    DCHECK(
        To<PhysicalBoxFragment>(result->GetPhysicalFragment()).IsOnlyForNode());
    DCHECK_EQ(index, 0u);
    // We don't early return here, when setting the "measure" result we also
    // set the "layout" result.
    if (measure_cache_) {
      measure_cache_->InvalidateItems();
    }
    AddMeasureLayoutResult(result);
    if (IsTableCell()) {
      To<LayoutTableCell>(this)->InvalidateLayoutResultCacheAfterMeasure();
    }
  } else {
    // We have a "layout" result, and we may need to clear the old "measure"
    // result if we needed non-simplified layout.
    if (NeedsLayout() && !NeedsSimplifiedLayoutOnly()) {
      if (measure_cache_) {
        measure_cache_->Clear();
      }
    }
  }

  // If we're about to cache a layout result that is different than the measure
  // result, mark the measure result's fragment as no longer having valid
  // children. It can still be used to query information about this box's
  // fragment from the measure pass, but children might be out of sync with the
  // latest version of the tree.
  if (measure_cache_) {
    measure_cache_->SetFragmentChildrenInvalid(result);
  }

  SetLayoutResult(result, index);
}

void LayoutBox::SetLayoutResult(const LayoutResult* result, wtf_size_t index) {
  NOT_DESTROYED();
  DCHECK_EQ(result->Status(), LayoutResult::kSuccess);
  const auto& box_fragment =
      To<PhysicalBoxFragment>(result->GetPhysicalFragment());

  if (index != WTF::kNotFound && layout_results_.size() > index) {
    if (layout_results_.size() > index + 1) {
      // If we have reached the end, remove surplus results from previous
      // layout.
      //
      // Note: When an OOF is fragmented, we wait to lay it out at the
      // fragmentation context root. If the OOF lives above a column spanner,
      // though, we may lay it out early to make sure the OOF contributes to the
      // correct column block-size. Thus, if an item broke as a result of a
      // spanner, remove subsequent sibling items so that OOFs don't try to
      // access old fragments.
      //
      // Additionally, if an outer multicol has a spanner break, we may try
      // to access old fragments of the inner multicol if it hasn't completed
      // layout yet. Remove subsequent multicol fragments to avoid OOFs from
      // trying to access old fragments.
      //
      // TODO(layout-dev): Other solutions to handling interactions between OOFs
      // and spanner breaks may need to be considered.
      if (!box_fragment.GetBreakToken() ||
          box_fragment.GetBreakToken()->IsCausedByColumnSpanner() ||
          box_fragment.IsFragmentationContextRoot()) {
        // Before forgetting any old fragments and their items, we need to clear
        // associations.
        if (box_fragment.IsInlineFormattingContext())
          FragmentItems::ClearAssociatedFragments(this);
        ShrinkLayoutResults(index + 1);
      }
    }
    ReplaceLayoutResult(std::move(result), index);
    return;
  }

  DCHECK(index == layout_results_.size() || index == kNotFound);
  AppendLayoutResult(result);

  if (!box_fragment.GetBreakToken()) {
    FinalizeLayoutResults();
  }
}

void LayoutBox::AppendLayoutResult(const LayoutResult* result) {
  const auto& fragment = To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  // |layout_results_| is particularly critical when side effects are disabled.
  DCHECK(!DisableLayoutSideEffectsScope::IsDisabled());
  layout_results_.push_back(std::move(result));
  InvalidateCachedGeometry();
  CheckDidAddFragment(*this, fragment);
}

void LayoutBox::ReplaceLayoutResult(const LayoutResult* result,
                                    wtf_size_t index) {
  NOT_DESTROYED();
  DCHECK_LE(index, layout_results_.size());
  const LayoutResult* old_result = layout_results_[index];
  if (old_result == result)
    return;
  const auto& fragment = To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  const auto& old_fragment = old_result->GetPhysicalFragment();
  bool got_new_fragment = &old_fragment != &fragment;
  if (got_new_fragment) {
    if (HasFragmentItems()) {
      if (!index)
        InvalidateItems(*old_result);
      FragmentItems::ClearAssociatedFragments(this);
    }
    // We are about to replace a fragment, and the size may have changed. The
    // inline-size and total stitched block-size may still remain unchanged,
    // though, and pre-paint can only detect changes in the total stitched
    // size. So this is our last chance to detect any size changes at the
    // fragment itself. Only do this if we're fragmented, though. Otherwise
    // leave it to pre-paint to figure out if invalidation is really required,
    // since it's fine to just check the stitched sizes when not fragmented.
    // Unconditionally requiring full paint invalidation at size changes may be
    // unnecessary and expensive.
    if (layout_results_.size() > 1 && fragment.Size() != old_fragment.Size()) {
      SetShouldDoFullPaintInvalidation();
    }
  }
  // |layout_results_| is particularly critical when side effects are disabled.
  DCHECK(!DisableLayoutSideEffectsScope::IsDisabled());
  layout_results_[index] = std::move(result);
  InvalidateCachedGeometry();
  CheckDidAddFragment(*this, fragment, index);

  if (got_new_fragment && !fragment.GetBreakToken()) {
    // If this is the last result, the results vector better agree on that.
    DCHECK_EQ(index, layout_results_.size() - 1);

    FinalizeLayoutResults();
  }
}

void LayoutBox::FinalizeLayoutResults() {
  DCHECK(!layout_results_.empty());
  DCHECK(!layout_results_.back()->GetPhysicalFragment().GetBreakToken());
#if EXPENSIVE_DCHECKS_ARE_ON()
  CheckMayHaveFragmentItems();
#endif
  // If we've added all the results we were going to, and the node establishes
  // an inline formatting context, we have some finalization to do.
  if (HasFragmentItems()) {
    FragmentItems::FinalizeAfterLayout(layout_results_,
                                       *To<LayoutBlockFlow>(this));
  }
}

void LayoutBox::RebuildFragmentTreeSpine() {
  DCHECK(PhysicalFragmentCount());
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES(
      "Blink.Layout.RebuildFragmentTreeSpine");
  // If this box has an associated layout-result, rebuild the spine of the
  // fragment-tree to ensure consistency.
  LayoutBox* container = this;
  while (container && container->PhysicalFragmentCount() &&
         !container->NeedsLayout()) {
    for (auto& result : container->layout_results_)
      result = LayoutResult::CloneWithPostLayoutFragments(*result);
    container = container->ContainingNGBox();
  }

  if (container && container->NeedsLayout()) {
    // We stopped walking upwards because this container needs layout. This
    // typically means that updating the associated layout results is waste of
    // time, since we're probably going to lay it out anyway. However, in some
    // cases the container is going to hit the cache and therefore not perform
    // actual layout. If this happens, we need to update the layout results at
    // that point.
    container->SetHasBrokenSpine();
  }
}

void LayoutBox::ShrinkLayoutResults(wtf_size_t results_to_keep) {
  NOT_DESTROYED();
  DCHECK_GE(layout_results_.size(), results_to_keep);
  // Invalidate if inline |DisplayItemClient|s will be destroyed.
  for (wtf_size_t i = results_to_keep; i < layout_results_.size(); i++)
    InvalidateItems(*layout_results_[i]);
  // |layout_results_| is particularly critical when side effects are disabled.
  DCHECK(!DisableLayoutSideEffectsScope::IsDisabled());
  layout_results_.Shrink(results_to_keep);
  InvalidateCachedGeometry();
}

#if EXPENSIVE_DCHECKS_ARE_ON()
void LayoutBox::CheckMayHaveFragmentItems() const {
  NOT_DESTROYED();
  if (!MayHaveFragmentItems()) {
    DCHECK(!PhysicalFragments().SlowHasFragmentItems());
  }
}
#endif

void LayoutBox::InvalidateCachedGeometry() {
  NOT_DESTROYED();
  SetHasValidCachedGeometry(false);
  if (auto* block_flow = DynamicTo<LayoutBlockFlow>(this)) {
    if (auto* flow_thread = block_flow->MultiColumnFlowThread()) {
      flow_thread->SetHasValidCachedGeometry(false);
      for (auto* sibling = flow_thread->NextSiblingBox(); sibling;
           sibling = sibling->NextSiblingBox()) {
        sibling->SetHasValidCachedGeometry(false);
      }
    }
  }
}

// static
void LayoutBox::InvalidateItems(const LayoutResult& result) {
  // Invalidate if inline |DisplayItemClient|s will be destroyed.
  const auto& box_fragment =
      To<PhysicalBoxFragment>(result.GetPhysicalFragment());
  if (!box_fragment.HasItems())
    return;
  ObjectPaintInvalidator(*box_fragment.GetLayoutObject())
      .SlowSetPaintingLayerNeedsRepaint();
}

const LayoutResult* LayoutBox::GetCachedLayoutResult(
    const BlockBreakToken* break_token) const {
  NOT_DESTROYED();
  wtf_size_t index = FragmentIndex(break_token);
  if (index >= layout_results_.size())
    return nullptr;
  const LayoutResult* result = layout_results_[index];
  DCHECK(!result->GetPhysicalFragment().IsLayoutObjectDestroyedOrMoved() ||
         BeingDestroyed());
  return result;
}

const LayoutResult* LayoutBox::GetCachedMeasureResult(
    const ConstraintSpace& space,
    std::optional<FragmentGeometry>* fragment_geometry) const {
  NOT_DESTROYED();
  if (!measure_cache_) {
    return nullptr;
  }

  // If we've already had an actual layout pass, and the node fragmented, we
  // cannot reliably re-use the measure result. What we want to avoid here is
  // simplified layout inside a measure-result, as that would descend into a
  // fragment subtree generated by actual (fragmented) layout, which is
  // invalid. But it seems safer to stop such attempts here, so that we don't
  // hand out results that may cause problems if we end up with simplified
  // layout inside.
  if (!layout_results_.empty()) {
    const PhysicalBoxFragment* first_fragment = GetPhysicalFragment(0);
    if (first_fragment->GetBreakToken()) {
      return nullptr;
    }
  }

  return measure_cache_
             ? measure_cache_->Find(BlockNode(const_cast<LayoutBox*>(this)),
                                    space, fragment_geometry)
             : nullptr;
}

const LayoutResult* LayoutBox::GetSingleCachedLayoutResult() const {
  DCHECK_LE(layout_results_.size(), 1u);
  return GetCachedLayoutResult(nullptr);
}

const LayoutResult* LayoutBox::GetSingleCachedMeasureResultForTesting() const {
  return measure_cache_ ? measure_cache_->GetLastForTesting() : nullptr;
}

const LayoutResult* LayoutBox::GetLayoutResult(wtf_size_t i) const {
  NOT_DESTROYED();
  return layout_results_[i].Get();
}

const PhysicalBoxFragment&
LayoutBox::PhysicalFragmentList::Iterator::operator*() const {
  return To<PhysicalBoxFragment>((*iterator_)->GetPhysicalFragment());
}

const PhysicalBoxFragment& LayoutBox::PhysicalFragmentList::front() const {
  return To<PhysicalBoxFragment>(
      layout_results_.front()->GetPhysicalFragment());
}

const PhysicalBoxFragment& LayoutBox::PhysicalFragmentList::back() const {
  return To<PhysicalBoxFragment>(layout_results_.back()->GetPhysicalFragment());
}

const FragmentData* LayoutBox::FragmentDataFromPhysicalFragment(
    const PhysicalBoxFragment& physical_fragment) const {
  NOT_DESTROYED();
  return &FragmentList().at(BoxFragmentIndex(physical_fragment));
}

void LayoutBox::SetSpannerPlaceholder(
    LayoutMultiColumnSpannerPlaceholder& placeholder) {
  NOT_DESTROYED();
  // Not expected to change directly from one spanner to another.
  CHECK(!rare_data_ || !rare_data_->spanner_placeholder_);
  EnsureRareData().spanner_placeholder_ = &placeholder;
}

void LayoutBox::ClearSpannerPlaceholder() {
  NOT_DESTROYED();
  if (!rare_data_)
    return;
  rare_data_->spanner_placeholder_ = nullptr;
}

void LayoutBox::InflateVisualRectForFilterUnderContainer(
    TransformState& transform_state,
    const LayoutObject& container,
    const LayoutBoxModelObject* ancestor_to_stop_at) const {
  NOT_DESTROYED();
  transform_state.Flatten();
  // Apply visual overflow caused by reflections and filters defined on objects
  // between this object and container (not included) or ancestorToStopAt
  // (included).
  PhysicalOffset offset_from_container = OffsetFromContainer(&container);
  transform_state.Move(offset_from_container);
  for (LayoutObject* parent = Parent(); parent && parent != container;
       parent = parent->Parent()) {
    if (parent->IsBox()) {
      // Convert rect into coordinate space of parent to apply parent's
      // reflection and filter.
      PhysicalOffset parent_offset = parent->OffsetFromAncestor(&container);
      transform_state.Move(-parent_offset);
      To<LayoutBox>(parent)->InflateVisualRectForFilter(transform_state);
      transform_state.Move(parent_offset);
    }
    if (parent == ancestor_to_stop_at)
      break;
  }
  transform_state.Move(-offset_from_container);
}

bool LayoutBox::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags visual_rect_flags) const {
  NOT_DESTROYED();

  if (ancestor == this)
    return true;

  if (!(visual_rect_flags & kIgnoreFilters)) {
    InflateVisualRectForFilter(transform_state);
  }

  AncestorSkipInfo skip_info(ancestor, true);
  LayoutObject* container = Container(&skip_info);
  if (!container)
    return true;

  PhysicalOffset container_offset;
  if (auto* box = DynamicTo<LayoutBox>(container)) {
    container_offset += PhysicalLocation(box);
  } else {
    container_offset += PhysicalLocation();
  }

  if (IsStickyPositioned()) {
    container_offset += StickyPositionOffset();
  } else if (NeedsAnchorPositionScrollAdjustment()) [[unlikely]] {
    container_offset += AnchorPositionScrollTranslationOffset();
  }

  if (skip_info.FilterSkipped() && !(visual_rect_flags & kIgnoreFilters)) {
    InflateVisualRectForFilterUnderContainer(transform_state, *container,
                                             ancestor);
  }

  if (!MapVisualRectToContainer(container, container_offset, ancestor,
                                visual_rect_flags, transform_state))
    return false;

  if (skip_info.AncestorSkipped()) {
    bool preserve3D = container->StyleRef().Preserves3D();
    TransformState::TransformAccumulation accumulation =
        preserve3D ? TransformState::kAccumulateTransform
                   : TransformState::kFlattenTransform;

    // If the ancestor is below the container, then we need to map the rect into
    // ancestor's coordinates.
    PhysicalOffset ancestor_container_offset =
        ancestor->OffsetFromAncestor(container);
    transform_state.Move(-ancestor_container_offset, accumulation);
    return true;
  }

  if (IsFixedPositioned() && container == ancestor && container->IsLayoutView())
    transform_state.Move(To<LayoutView>(container)->OffsetForFixedPosition());

  return container->MapToVisualRectInAncestorSpaceInternal(
      ancestor, transform_state, visual_rect_flags);
}

void LayoutBox::InflateVisualRectForFilter(
    TransformState& transform_state) const {
  NOT_DESTROYED();
  if (!Layer() || !Layer()->PaintsWithFilters())
    return;

  transform_state.Flatten();
  PhysicalRect rect = PhysicalRect::EnclosingRect(
      transform_state.LastPlanarQuad().BoundingBox());
  transform_state.SetQuad(
      gfx::QuadF(gfx::RectF(Layer()->MapRectForFilter(rect))));
}

bool LayoutBox::SkipContainingBlockForPercentHeightCalculation(
    const LayoutBox* containing_block) {
  const bool in_quirks_mode = containing_block->GetDocument().InQuirksMode();
  // Anonymous blocks should not impede percentage resolution on a child.
  // Examples of such anonymous blocks are blocks wrapped around inlines that
  // have block siblings (from the CSS spec) and multicol flow threads (an
  // implementation detail). Another implementation detail, ruby columns, create
  // anonymous inline-blocks, so skip those too. All other types of anonymous
  // objects, such as table-cells, will be treated just as if they were
  // non-anonymous.
  if (containing_block->IsAnonymous()) {
    if (!in_quirks_mode && containing_block->Parent() &&
        containing_block->Parent()->IsFieldset()) {
      return false;
    }
    EDisplay display = containing_block->StyleRef().Display();
    return display == EDisplay::kBlock || display == EDisplay::kInlineBlock ||
           display == EDisplay::kFlowRoot;
  }

  // For quirks mode, we skip most auto-height containing blocks when computing
  // percentages.
  if (!in_quirks_mode ||
      !containing_block->StyleRef().LogicalHeight().HasAuto()) {
    return false;
  }

  const Node* node = containing_block->GetNode();
  if (node->IsInUserAgentShadowRoot()) [[unlikely]] {
    const Element* host = node->OwnerShadowHost();
    if (const auto* input = DynamicTo<HTMLInputElement>(host)) {
      // In web_tests/fast/forms/range/range-thumb-height-percentage.html, a
      // percent height for the slider thumb element should refer to the height
      // of the INPUT box.
      if (input->FormControlType() == FormControlType::kInputRange) {
        return true;
      }
    }
  }

  return !containing_block->IsLayoutReplaced() &&
         !containing_block->IsTableCell() &&
         !containing_block->IsOutOfFlowPositioned() &&
         !containing_block->IsLayoutGrid() &&
         !containing_block->IsFlexibleBox() &&
         !containing_block->IsLayoutCustom();
}

LayoutUnit LayoutBox::ContainingBlockLogicalHeightForPositioned(
    const LayoutBoxModelObject* containing_block) const {
  NOT_DESTROYED();

  // Use viewport as container for top-level fixed-position elements.
  const auto* view = DynamicTo<LayoutView>(containing_block);
  if (StyleRef().GetPosition() == EPosition::kFixed && view &&
      !GetDocument().Printing()) {
    if (LocalFrameView* frame_view = view->GetFrameView()) {
      // Don't use visibleContentRect since the PaintLayer's size has not been
      // set yet.
      gfx::Size viewport_size =
          frame_view->LayoutViewport()->ExcludeScrollbars(frame_view->Size());
      return LayoutUnit(containing_block->IsHorizontalWritingMode()
                            ? viewport_size.height()
                            : viewport_size.width());
    }
  }

  if (containing_block->IsBox())
    return To<LayoutBox>(containing_block)->ClientLogicalHeight();

  DCHECK(containing_block->IsLayoutInline());
  DCHECK(containing_block->CanContainOutOfFlowPositionedElement(
      StyleRef().GetPosition()));

  const auto* flow = To<LayoutInline>(containing_block);
  // If the containing block is empty, return a height of 0.
  if (!flow->HasInlineFragments())
    return LayoutUnit();

  LayoutUnit height_result;
  auto bounding_box_size = flow->PhysicalLinesBoundingBox().size;
  if (containing_block->IsHorizontalWritingMode())
    height_result = bounding_box_size.height;
  else
    height_result = bounding_box_size.width;
  height_result -= (containing_block->BorderBlockStart() +
                    containing_block->BorderBlockEnd());
  return height_result;
}

PhysicalRect LayoutBox::LocalCaretRect(int caret_offset) const {
  NOT_DESTROYED();
  // VisiblePositions at offsets inside containers either a) refer to the
  // positions before/after those containers (tables and select elements) or
  // b) refer to the position inside an empty block.
  // They never refer to children.
  // FIXME: Paint the carets inside empty blocks differently than the carets
  // before/after elements.
  LayoutUnit caret_width = GetFrameView()->CaretWidth();
  LogicalSize size(LogicalWidth(), LogicalHeight());

  LayoutUnit caret_block_size = size.block_size;
  // If height of box is smaller than font height, use the latter one,
  // otherwise the caret might become invisible.
  //
  // Also, if the box is not an atomic inline-level element, always use the font
  // height. This prevents the "big caret" bug described in:
  // <rdar://problem/3777804> Deleting all content in a document can result in
  // giant tall-as-window insertion point
  //
  // FIXME: ignoring :first-line, missing good reason to take care of
  const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
  LayoutUnit font_height =
      LayoutUnit(font_data ? font_data->GetFontMetrics().Height() : 0);
  if (font_height > size.block_size || (!IsAtomicInlineLevel() && !IsTable())) {
    caret_block_size = font_height;
  }

  // FIXME: Border/padding should be added for all elements but this workaround
  // is needed because we use offsets inside an "atomic" element to represent
  // positions before and after the element in deprecated editing offsets.
  bool apply_border_padding =
      GetNode() &&
      !(EditingIgnoresContent(*GetNode()) || IsDisplayInsideTable(GetNode()));

  if (RuntimeEnabledFeatures::SidewaysWritingModesEnabled()) {
    WritingDirectionMode writing_direction = Style()->GetWritingDirection();
    LogicalOffset offset;
    LayoutUnit content_inline_size = size.inline_size;
    if (apply_border_padding) {
      BoxStrut border_padding = (BorderOutsets() + PaddingOutsets())
                                    .ConvertToLogical(writing_direction);
      offset.inline_offset = border_padding.inline_start;
      offset.block_offset = border_padding.block_start;
      content_inline_size -= border_padding.InlineSum();
    }
    if (caret_offset) {
      offset.inline_offset += content_inline_size - caret_width;
    }

    LogicalRect rect(offset, LogicalSize(caret_width, caret_block_size));
    return WritingModeConverter(writing_direction, Size()).ToPhysical(rect);
  }
  const bool is_horizontal = IsHorizontalWritingMode();
  PhysicalOffset offset = PhysicalLocation();
  PhysicalRect rect(offset, is_horizontal
                                ? PhysicalSize(caret_width, caret_block_size)
                                : PhysicalSize(caret_block_size, caret_width));
  bool ltr = StyleRef().IsLeftToRightDirection();

  if ((!caret_offset) ^ ltr) {
    rect.Move(
        is_horizontal
            ? PhysicalOffset(size.inline_size - caret_width, LayoutUnit())
            : PhysicalOffset(LayoutUnit(), size.inline_size - caret_width));
  }

  // Move to local coords
  rect.Move(-offset);

  if (apply_border_padding) {
    rect.SetX(rect.X() + BorderLeft() + PaddingLeft());
    rect.SetY(rect.Y() + PaddingTop() + BorderTop());
  }

  return rect;
}

PositionWithAffinity LayoutBox::PositionForPointInFragments(
    const PhysicalOffset& target) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  DCHECK_GT(PhysicalFragmentCount(), 0u);

  if (PhysicalFragmentCount() == 1) {
    const PhysicalBoxFragment* fragment = GetPhysicalFragment(0);
    return fragment->PositionForPoint(target);
  }

  // When |this| is block fragmented, find the closest fragment.
  const PhysicalBoxFragment* closest_fragment = nullptr;
  PhysicalOffset closest_fragment_offset;
  LayoutUnit shortest_square_distance = LayoutUnit::Max();
  for (const PhysicalBoxFragment& fragment : PhysicalFragments()) {
    // If |fragment| contains |target|, call its |PositionForPoint|.
    const PhysicalOffset fragment_offset = fragment.OffsetFromOwnerLayoutBox();
    const PhysicalSize distance =
        PhysicalRect(fragment_offset, fragment.Size()).DistanceAsSize(target);
    if (distance.IsZero())
      return fragment.PositionForPoint(target - fragment_offset);

    // Otherwise find the closest fragment.
    const LayoutUnit square_distance =
        distance.width * distance.width + distance.height * distance.height;
    if (square_distance < shortest_square_distance || !closest_fragment) {
      shortest_square_distance = square_distance;
      closest_fragment = &fragment;
      closest_fragment_offset = fragment_offset;
    }
  }
  DCHECK(closest_fragment);
  return closest_fragment->PositionForPoint(target - closest_fragment_offset);
}

DISABLE_CFI_PERF
bool LayoutBox::ShouldBeConsideredAsReplaced() const {
  NOT_DESTROYED();
  if (IsAtomicInlineLevel())
    return true;
  // We need to detect all types of objects that should be treated as replaced.
  // Callers of this method will use the result for various things, such as
  // determining how to size the object, or whether it needs to avoid adjacent
  // floats, just like objects that establish a new formatting context.
  // IsAtomicInlineLevel() will not catch all the cases. Objects may be
  // block-level and still replaced, and we cannot deduce this from the
  // LayoutObject type. Checkboxes and radio buttons are such examples. We need
  // to check the Element type. This also applies to images, since we may have
  // created a block-flow LayoutObject for the ALT text (which still counts as
  // replaced).
  auto* element = DynamicTo<Element>(GetNode());
  if (!element)
    return false;
  if (element->IsFormControlElement()) {
    // Form control elements are generally replaced objects. Fieldsets are not,
    // though. A fieldset is (almost) a regular block container, and should be
    // treated as such.
    return !IsA<HTMLFieldSetElement>(element);
  }
  return IsA<HTMLImageElement>(element);
}

// Children of LayoutCustom object's are only considered "items" when it has a
// loaded algorithm.
bool LayoutBox::IsCustomItem() const {
  NOT_DESTROYED();
  auto* parent_layout_box = DynamicTo<LayoutCustom>(Parent());
  return parent_layout_box && parent_layout_box->IsLoaded();
}

PhysicalBoxStrut LayoutBox::ComputeVisualEffectOverflowOutsets() {
  NOT_DESTROYED();
  const ComputedStyle& style = StyleRef();
  DCHECK(style.HasVisualOverflowingEffect());

  PhysicalBoxStrut outsets = style.BoxDecorationOutsets();

  if (style.HasOutline()) {
    OutlineInfo info;
    Vector<PhysicalRect> outline_rects =
        OutlineRects(&info, PhysicalOffset(),
                     style.OutlineRectsShouldIncludeBlockInkOverflow());
    PhysicalRect rect = UnionRect(outline_rects);
    bool outline_affected = rect.size != Size();
    SetOutlineMayBeAffectedByDescendants(outline_affected);
    rect.Inflate(LayoutUnit(OutlinePainter::OutlineOutsetExtent(style, info)));
    outsets.Unite(PhysicalBoxStrut(-rect.Y(), rect.Right() - Size().width,
                                   rect.Bottom() - Size().height, -rect.X()));
  }

  return outsets;
}

bool LayoutBox::HasTopOverflow() const {
  NOT_DESTROYED();
  // Early-return for the major case.
  if (IsHorizontalWritingMode()) {
    return false;
  }
  switch (StyleRef().GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      return false;
    case WritingMode::kSidewaysLr:
      return StyleRef().IsLeftToRightDirection();
    case WritingMode::kVerticalLr:
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      return !StyleRef().IsLeftToRightDirection();
  }
}

bool LayoutBox::HasLeftOverflow() const {
  NOT_DESTROYED();
  // Early-return for the major case.
  if (IsHorizontalWritingMode()) {
    return !StyleRef().IsLeftToRightDirection();
  }
  switch (StyleRef().GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      return !StyleRef().IsLeftToRightDirection();
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysLr:
      return false;
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      return true;
  }
}

void LayoutBox::SetScrollableOverflowFromLayoutResults() {
  NOT_DESTROYED();
  ClearSelfNeedsScrollableOverflowRecalc();
  ClearChildNeedsScrollableOverflowRecalc();
  if (overflow_) {
    overflow_->scrollable_overflow.reset();
  }

  if (IsLayoutReplaced()) {
    return;
  }

  const WritingMode writing_mode = StyleRef().GetWritingMode();
  std::optional<PhysicalRect> scrollable_overflow;
  LayoutUnit consumed_block_size;
  LayoutUnit fragment_width_sum;

  // Iterate over all the fragments and unite their individual
  // scrollable-overflow to determine the final scrollable-overflow.
  for (const auto& layout_result : layout_results_) {
    const auto& fragment =
        To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment());

    // In order to correctly unite the overflow, we need to shift an individual
    // fragment's scrollable-overflow by previously consumed block-size so far.
    PhysicalOffset offset_adjust;
    switch (writing_mode) {
      case WritingMode::kHorizontalTb:
        offset_adjust = {LayoutUnit(), consumed_block_size};
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
        // For flipped-blocks writing-modes, we build the total overflow rect
        // from right-to-left (adding with negative offsets). At the end we
        // need to make the origin relative to the LHS, so we add the total
        // fragment width.
        fragment_width_sum += fragment.Size().width;
        offset_adjust = {-fragment.Size().width - consumed_block_size,
                         LayoutUnit()};
        break;
      case WritingMode::kVerticalLr:
      case WritingMode::kSidewaysLr:
        offset_adjust = {consumed_block_size, LayoutUnit()};
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    PhysicalRect fragment_scrollable_overflow = fragment.ScrollableOverflow();
    fragment_scrollable_overflow.offset += offset_adjust;

    // If we are the first fragment just set the scrollable-overflow.
    if (!scrollable_overflow) {
      scrollable_overflow = fragment_scrollable_overflow;
    } else {
      scrollable_overflow->UniteEvenIfEmpty(fragment_scrollable_overflow);
    }

    if (const auto* break_token = fragment.GetBreakToken()) {
      // The legacy engine doesn't understand our concept of repeated
      // fragments. Stop now. The overflow rectangle will represent the
      // fragment(s) generated under the first repeated root.
      if (break_token->IsRepeated())
        break;
      consumed_block_size = break_token->ConsumedBlockSize();
    }
  }

  if (!scrollable_overflow) {
    return;
  }

  if (IsFlippedBlocksWritingMode(writing_mode)) {
    scrollable_overflow->offset.left += fragment_width_sum;
  }

  if (scrollable_overflow->IsEmpty() ||
      PhysicalPaddingBoxRect().Contains(*scrollable_overflow)) {
    return;
  }

  DCHECK(!ScrollableOverflowIsSet());
  if (!overflow_)
    overflow_ = MakeGarbageCollected<BoxOverflowModel>();
  overflow_->scrollable_overflow.emplace(*scrollable_overflow);
}

RecalcScrollableOverflowResult LayoutBox::RecalcScrollableOverflowNG() {
  NOT_DESTROYED();

  RecalcScrollableOverflowResult child_result;
  // Don't attempt to rebuild the fragment tree or recalculate
  // scrollable-overflow, layout will do this for us.
  if (NeedsLayout())
    return RecalcScrollableOverflowResult();

  if (ChildNeedsScrollableOverflowRecalc()) {
    child_result = RecalcChildScrollableOverflowNG();
  }

  bool should_recalculate_scrollable_overflow =
      SelfNeedsScrollableOverflowRecalc() ||
      child_result.scrollable_overflow_changed;
  bool rebuild_fragment_tree = child_result.rebuild_fragment_tree;
  bool scrollable_overflow_changed = false;

  if (rebuild_fragment_tree || should_recalculate_scrollable_overflow) {
    for (auto& layout_result : layout_results_) {
      const auto& fragment =
          To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment());
      std::optional<PhysicalRect> scrollable_overflow;

      // Recalculate our scrollable-overflow if a child had its
      // scrollable-overflow changed, or if we are marked as dirty.
      if (should_recalculate_scrollable_overflow) {
        const PhysicalRect old_scrollable_overflow =
            fragment.ScrollableOverflow();
        const bool has_block_fragmentation =
            layout_result->GetConstraintSpaceForCaching()
                .HasBlockFragmentation();
#if DCHECK_IS_ON()
        PhysicalBoxFragment::AllowPostLayoutScope allow_post_layout_scope;
#endif
        const PhysicalRect new_scrollable_overflow =
            ScrollableOverflowCalculator::
                RecalculateScrollableOverflowForFragment(
                    fragment, has_block_fragmentation);

        // Set the appropriate flags if the scrollable-overflow changed.
        if (old_scrollable_overflow != new_scrollable_overflow) {
          scrollable_overflow = new_scrollable_overflow;
          scrollable_overflow_changed = true;
          rebuild_fragment_tree = true;
        }
      }

      if (scrollable_overflow) {
        fragment.GetMutableForStyleRecalc().SetScrollableOverflow(
            *scrollable_overflow);
      }
    }
    SetScrollableOverflowFromLayoutResults();
  }

  if (scrollable_overflow_changed && IsScrollContainer()) {
    Layer()->GetScrollableArea()->UpdateAfterOverflowRecalc();
  }

  // Only indicate to our parent that our scrollable overflow changed if we
  // have:
  //  - No layout containment applied.
  //  - No clipping (in both axes).
  scrollable_overflow_changed = scrollable_overflow_changed &&
                                !ShouldApplyLayoutContainment() &&
                                !ShouldClipOverflowAlongBothAxis();

  return {scrollable_overflow_changed, rebuild_fragment_tree};
}

RecalcScrollableOverflowResult LayoutBox::RecalcChildScrollableOverflowNG() {
  NOT_DESTROYED();
  DCHECK(ChildNeedsScrollableOverflowRecalc());
  ClearChildNeedsScrollableOverflowRecalc();

#if DCHECK_IS_ON()
  // We use PostLayout methods to navigate the fragment tree and reach the
  // corresponding LayoutObjects, so we need to use AllowPostLayoutScope here.
  PhysicalBoxFragment::AllowPostLayoutScope allow_post_layout_scope;
#endif
  RecalcScrollableOverflowResult result;
  for (auto& layout_result : layout_results_) {
    const auto& fragment =
        To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment());
    if (fragment.HasItems()) {
      for (InlineCursor cursor(fragment); cursor; cursor.MoveToNext()) {
        const PhysicalBoxFragment* child =
            cursor.Current()->PostLayoutBoxFragment();
        if (!child || !child->GetLayoutObject()->IsBox())
          continue;
        result.Unite(
            child->MutableOwnerLayoutBox()->RecalcScrollableOverflow());
      }
    }

    RecalcFragmentScrollableOverflow(result, fragment);
  }

  return result;
}

void LayoutBox::AddSelfVisualOverflow(const PhysicalRect& rect) {
  NOT_DESTROYED();
  if (rect.IsEmpty())
    return;

  PhysicalRect border_box = PhysicalBorderBoxRect();
  if (border_box.Contains(rect))
    return;

  if (!VisualOverflowIsSet()) {
    if (!overflow_)
      overflow_ = MakeGarbageCollected<BoxOverflowModel>();

    overflow_->visual_overflow.emplace(border_box);
  }

  overflow_->visual_overflow->AddSelfVisualOverflow(rect);
}

void LayoutBox::AddContentsVisualOverflow(const PhysicalRect& rect) {
  NOT_DESTROYED();
  if (rect.IsEmpty())
    return;

  // If hasOverflowClip() we always save contents visual overflow because we
  // need it
  // e.g. to determine whether to apply rounded corner clip on contents.
  // Otherwise we save contents visual overflow only if it overflows the border
  // box.
  PhysicalRect border_box = PhysicalBorderBoxRect();
  if (!HasNonVisibleOverflow() && border_box.Contains(rect))
    return;

  if (!VisualOverflowIsSet()) {
    if (!overflow_)
      overflow_ = MakeGarbageCollected<BoxOverflowModel>();

    overflow_->visual_overflow.emplace(border_box);
  }
  overflow_->visual_overflow->AddContentsVisualOverflow(rect);
}

void LayoutBox::UpdateHasSubpixelVisualEffectOutsets(
    const PhysicalBoxStrut& outsets) {
  if (!VisualOverflowIsSet()) {
    return;
  }
  overflow_->visual_overflow->SetHasSubpixelVisualEffectOutsets(
      !outsets.top.IsInteger() || !outsets.right.IsInteger() ||
      !outsets.bottom.IsInteger() || !outsets.left.IsInteger());
}

void LayoutBox::SetVisualOverflow(const PhysicalRect& self,
                                  const PhysicalRect& contents) {
  ClearVisualOverflow();
  AddSelfVisualOverflow(self);
  AddContentsVisualOverflow(contents);
  if (!VisualOverflowIsSet())
    return;

  const PhysicalRect overflow_rect =
      overflow_->visual_overflow->SelfVisualOverflowRect();
  const PhysicalSize box_size = Size();
  const PhysicalBoxStrut outsets(
      -overflow_rect.Y(), overflow_rect.Right() - box_size.width,
      overflow_rect.Bottom() - box_size.height, -overflow_rect.X());
  UpdateHasSubpixelVisualEffectOutsets(outsets);

  // |OutlineMayBeAffectedByDescendants| is set whenever outline style
  // changes. Update to the actual value here.
  const ComputedStyle& style = StyleRef();
  if (style.HasOutline()) {
    const LayoutUnit outline_extent(OutlinePainter::OutlineOutsetExtent(
        style, OutlineInfo::GetFromStyle(style)));
    SetOutlineMayBeAffectedByDescendants(
        outsets.top != outline_extent || outsets.right != outline_extent ||
        outsets.bottom != outline_extent || outsets.left != outline_extent);
  }
}

void LayoutBox::ClearVisualOverflow() {
  NOT_DESTROYED();
  if (overflow_)
    overflow_->visual_overflow.reset();
  // overflow_ will be reset by MutableForPainting::ClearPreviousOverflowData()
  // if we don't need it to store previous overflow data.
}

bool LayoutBox::CanUseFragmentsForVisualOverflow() const {
  NOT_DESTROYED();
  // TODO(crbug.com/1144203): Legacy, or no-fragments-objects such as
  // table-column. What to do with them is TBD.
  if (!PhysicalFragmentCount())
    return false;
  const PhysicalBoxFragment& fragment = *GetPhysicalFragment(0);
  if (!fragment.CanUseFragmentsForInkOverflow())
    return false;
  return true;
}

// Copy visual overflow from |PhysicalFragments()|.
void LayoutBox::CopyVisualOverflowFromFragments() {
  NOT_DESTROYED();
  DCHECK(CanUseFragmentsForVisualOverflow());
  const PhysicalRect previous_visual_overflow =
      VisualOverflowRectAllowingUnset();
  CopyVisualOverflowFromFragmentsWithoutInvalidations();
  const PhysicalRect visual_overflow = VisualOverflowRect();
  if (visual_overflow == previous_visual_overflow)
    return;
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    DeprecatedInvalidateIntersectionObserverCachedRects();
  }
  SetShouldCheckForPaintInvalidation();
}

void LayoutBox::CopyVisualOverflowFromFragmentsWithoutInvalidations() {
  NOT_DESTROYED();
  DCHECK(CanUseFragmentsForVisualOverflow());
  if (!PhysicalFragmentCount()) [[unlikely]] {
    DCHECK(IsLayoutTableCol());
    ClearVisualOverflow();
    return;
  }

  if (PhysicalFragmentCount() == 1) {
    const PhysicalBoxFragment& fragment = *GetPhysicalFragment(0);
    DCHECK(fragment.CanUseFragmentsForInkOverflow());
    if (!fragment.HasInkOverflow()) {
      ClearVisualOverflow();
      return;
    }
    SetVisualOverflow(fragment.SelfInkOverflowRect(),
                      fragment.ContentsInkOverflowRect());
    return;
  }

  // When block-fragmented, stitch visual overflows from all fragments.
  const LayoutBlock* cb = ContainingBlock();
  DCHECK(cb);
  const WritingMode writing_mode = cb->StyleRef().GetWritingMode();
  bool has_overflow = false;
  PhysicalRect self_rect;
  PhysicalRect contents_rect;
  const PhysicalBoxFragment* last_fragment = nullptr;
  for (const PhysicalBoxFragment& fragment : PhysicalFragments()) {
    DCHECK(fragment.CanUseFragmentsForInkOverflow());
    if (!fragment.HasInkOverflow()) {
      last_fragment = &fragment;
      continue;
    }
    has_overflow = true;

    PhysicalRect fragment_self_rect = fragment.SelfInkOverflowRect();
    PhysicalRect fragment_contents_rect = fragment.ContentsInkOverflowRect();

    // Stitch this fragment to the bottom of the last one in horizontal
    // writing mode, or to the right in vertical. Flipped blocks is handled
    // later, after the loop.
    if (last_fragment) {
      const BlockBreakToken* break_token = last_fragment->GetBreakToken();
      DCHECK(break_token);
      const LayoutUnit block_offset = break_token->ConsumedBlockSize();
      if (blink::IsHorizontalWritingMode(writing_mode)) {
        fragment_self_rect.offset.top += block_offset;
        fragment_contents_rect.offset.top += block_offset;
      } else {
        fragment_self_rect.offset.left += block_offset;
        fragment_contents_rect.offset.left += block_offset;
      }
    }
    last_fragment = &fragment;

    self_rect.Unite(fragment_self_rect);
    contents_rect.Unite(fragment_contents_rect);

    // The legacy engine doesn't understand our concept of repeated
    // fragments. Stop now. The overflow rectangle will represent the
    // fragment(s) generated under the first repeated root.
    if (fragment.GetBreakToken() && fragment.GetBreakToken()->IsRepeated()) {
      break;
    }
  }

  if (!has_overflow) {
    ClearVisualOverflow();
    return;
  }
  SetVisualOverflow(self_rect, contents_rect);
}

DISABLE_CFI_PERF
bool LayoutBox::HasUnsplittableScrollingOverflow() const {
  NOT_DESTROYED();
  // Fragmenting scrollbars is only problematic in interactive media, e.g.
  // multicol on a screen. If we're printing, which is non-interactive media, we
  // should allow objects with non-visible overflow to be paginated as normally.
  if (GetDocument().Printing())
    return false;

  // Treat any scrollable container as monolithic.
  return IsScrollContainer();
}

bool LayoutBox::IsMonolithic() const {
  NOT_DESTROYED();
  // TODO(almaher): Don't consider a writing mode root monolitic if
  // IsFlexibleBox(). The breakability should be handled at the item
  // level. (Likely same for Table and Grid).
  if (ShouldBeConsideredAsReplaced() || HasUnsplittableScrollingOverflow() ||
      (Parent() && IsWritingModeRoot()) ||
      (IsFixedPositioned() && GetDocument().Printing() &&
       IsA<LayoutView>(Container())) ||
      ShouldApplySizeContainment() || IsFrameSet() ||
      StyleRef().HasLineClamp() || IsScrollMarkerGroup()) {
    return true;
  }

  return false;
}

LayoutUnit LayoutBox::FirstLineHeight() const {
  if (IsAtomicInlineLevel()) {
    return FirstLineStyle()->IsHorizontalWritingMode()
               ? MarginHeight() + Size().height
               : MarginWidth() + Size().width;
  }
  return LayoutUnit();
}

PhysicalBoxStrut LayoutBox::BorderOutsetsForClipping() const {
  auto padding_box = -BorderOutsets();
  if (!ShouldApplyOverflowClipMargin())
    return padding_box;

  PhysicalBoxStrut overflow_clip_margin;
  switch (StyleRef().OverflowClipMargin()->GetReferenceBox()) {
    case StyleOverflowClipMargin::ReferenceBox::kBorderBox:
      break;
    case StyleOverflowClipMargin::ReferenceBox::kPaddingBox:
      overflow_clip_margin = padding_box;
      break;
    case StyleOverflowClipMargin::ReferenceBox::kContentBox:
      overflow_clip_margin = padding_box - PaddingOutsets();
      break;
  }

  return overflow_clip_margin.Inflate(
      StyleRef().OverflowClipMargin()->GetMargin());
}

PhysicalRect LayoutBox::VisualOverflowRect() const {
  NOT_DESTROYED();
  DCHECK(!IsLayoutMultiColumnSet());
  if (!VisualOverflowIsSet())
    return PhysicalBorderBoxRect();

  const PhysicalRect& self_visual_overflow_rect =
      overflow_->visual_overflow->SelfVisualOverflowRect();
  if (HasMask()) {
    return self_visual_overflow_rect;
  }

  const OverflowClipAxes overflow_clip_axes = GetOverflowClipAxes();
  if (ShouldApplyOverflowClipMargin()) {
    // We should apply overflow clip margin only if we clip overflow on both
    // axis.
    DCHECK_EQ(overflow_clip_axes, kOverflowClipBothAxis);
    const PhysicalRect& contents_visual_overflow_rect =
        overflow_->visual_overflow->ContentsVisualOverflowRect();
    if (!contents_visual_overflow_rect.IsEmpty()) {
      PhysicalRect result = PhysicalBorderBoxRect();
      PhysicalBoxStrut outsets = BorderOutsetsForClipping();
      result.ExpandEdges(outsets.top, outsets.right, outsets.bottom,
                         outsets.left);
      result.Intersect(contents_visual_overflow_rect);
      result.Unite(self_visual_overflow_rect);
      return result;
    }
  }

  if (overflow_clip_axes == kOverflowClipBothAxis)
    return self_visual_overflow_rect;

  PhysicalRect result =
      overflow_->visual_overflow->ContentsVisualOverflowRect();
  result.Unite(self_visual_overflow_rect);
  ApplyOverflowClip(overflow_clip_axes, self_visual_overflow_rect, result);
  return result;
}

#if DCHECK_IS_ON()
PhysicalRect LayoutBox::VisualOverflowRectAllowingUnset() const {
  NOT_DESTROYED();
  InkOverflow::ReadUnsetAsNoneScope read_unset_as_none;
  return VisualOverflowRect();
}

void LayoutBox::CheckIsVisualOverflowComputed() const {
  // TODO(crbug.com/1205708): There are still too many failures. Disable the
  // the check for now. Need to investigate the reason.
  return;
  /*
  if (InkOverflow::ReadUnsetAsNoneScope::IsActive())
    return;
  if (!CanUseFragmentsForVisualOverflow())
    return;
  // TODO(crbug.com/1203402): MathML needs some more work.
  if (IsMathML())
    return;
  for (const PhysicalBoxFragment& fragment : PhysicalFragments())
    DCHECK(fragment.IsInkOverflowComputed());
  */
}
#endif

PhysicalOffset LayoutBox::OffsetPoint(const Element* parent) const {
  NOT_DESTROYED();
  return AdjustedPositionRelativeTo(PhysicalLocation(), parent);
}

LayoutUnit LayoutBox::OffsetLeft(const Element* parent) const {
  NOT_DESTROYED();
  return OffsetPoint(parent).left;
}

LayoutUnit LayoutBox::OffsetTop(const Element* parent) const {
  NOT_DESTROYED();
  return OffsetPoint(parent).top;
}

PhysicalSize LayoutBox::Size() const {
  NOT_DESTROYED();
  if (!HasValidCachedGeometry()) {
    // const_cast in order to update the cached value.
    const_cast<LayoutBox*>(this)->SetHasValidCachedGeometry(true);
    const_cast<LayoutBox*>(this)->frame_size_ = ComputeSize();
  }
  return frame_size_;
}

PhysicalSize LayoutBox::ComputeSize() const {
  NOT_DESTROYED();
  const auto& results = GetLayoutResults();
  if (results.size() == 0) {
    return PhysicalSize();
  }
  const auto& first_fragment = results[0]->GetPhysicalFragment();
  if (results.size() == 1u) {
    return first_fragment.Size();
  }
  WritingModeConverter converter(first_fragment.Style().GetWritingDirection());
  const BlockBreakToken* previous_break_token = nullptr;
  LogicalSize size;
  for (const auto& result : results) {
    const auto& physical_fragment =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment());
    LogicalSize fragment_logical_size =
        converter.ToLogical(physical_fragment.Size());
    if (physical_fragment.IsFirstForNode()) {
      // Inline-size will only be set at the first fragment. Subsequent
      // fragments may have different inline-size (either because fragmentainer
      // inline-size is variable, or e.g. because available inline-size is
      // affected by floats). The legacy engine doesn't handle variable
      // inline-size (since it doesn't really understand fragmentation).  This
      // means that things like offsetWidth won't work correctly (since that's
      // still being handled by the legacy engine), but at least layout,
      // painting and hit-testing will be correct.
      size = fragment_logical_size;
    } else {
      DCHECK(previous_break_token);
      size.block_size = fragment_logical_size.block_size +
                        previous_break_token->ConsumedBlockSizeForLegacy();
    }
    previous_break_token = physical_fragment.GetBreakToken();
    // Continue in order to update logical height, unless this fragment is
    // past the block-end of the generating node (happens with overflow) or
    // is a repeated one.
    if (!previous_break_token || previous_break_token->IsRepeated() ||
        previous_break_token->IsAtBlockEnd()) {
      break;
    }
  }
  return converter.ToPhysical(size);
}

LayoutBox* LayoutBox::LocationContainer() const {
  NOT_DESTROYED();
  // Location of a non-root SVG object derived from LayoutBox should not be
  // affected by writing-mode of the containing box (SVGRoot).
  if (IsSVGChild())
    return nullptr;

  // Normally the box's location is relative to its containing box.
  LayoutObject* container = Container();
  while (container && !container->IsBox())
    container = container->Container();
  return To<LayoutBox>(container);
}

ShapeOutsideInfo* LayoutBox::GetShapeOutsideInfo() const {
  NOT_DESTROYED();
  return ShapeOutsideInfo::Info(*this);
}

CustomLayoutChild* LayoutBox::GetCustomLayoutChild() const {
  NOT_DESTROYED();
  DCHECK(rare_data_);
  DCHECK(rare_data_->layout_child_);
  return rare_data_->layout_child_.Get();
}

void LayoutBox::AddCustomLayoutChildIfNeeded() {
  NOT_DESTROYED();
  if (!IsCustomItem())
    return;

  const AtomicString& name = Parent()->StyleRef().DisplayLayoutCustomName();
  LayoutWorklet* worklet = LayoutWorklet::From(*GetDocument().domWindow());
  const CSSLayoutDefinition* definition =
      worklet->Proxy()->FindDefinition(name);

  // If there isn't a definition yet, the web developer defined layout isn't
  // loaded yet (or is invalid). The layout tree will get re-attached when
  // loaded, so don't bother creating a script representation of this node yet.
  if (!definition)
    return;

  EnsureRareData().layout_child_ =
      MakeGarbageCollected<CustomLayoutChild>(*definition, BlockNode(this));
}

void LayoutBox::ClearCustomLayoutChild() {
  NOT_DESTROYED();
  if (!rare_data_)
    return;

  if (rare_data_->layout_child_)
    rare_data_->layout_child_->ClearLayoutNode();

  rare_data_->layout_child_ = nullptr;
}

PhysicalRect LayoutBox::DebugRect() const {
  NOT_DESTROYED();
  return PhysicalRect(PhysicalLocation(), Size());
}

OverflowClipAxes LayoutBox::ComputeOverflowClipAxes() const {
  NOT_DESTROYED();
  if (ShouldApplyPaintContainment() || HasControlClip())
    return kOverflowClipBothAxis;

  if (!RespectsCSSOverflow() || !HasNonVisibleOverflow())
    return kNoOverflowClip;

  if (IsScrollContainer())
    return kOverflowClipBothAxis;
  return (StyleRef().OverflowX() == EOverflow::kVisible ? kNoOverflowClip
                                                        : kOverflowClipX) |
         (StyleRef().OverflowY() == EOverflow::kVisible ? kNoOverflowClip
                                                        : kOverflowClipY);
}

void LayoutBox::MutableForPainting::SavePreviousOverflowData() {
  if (!GetLayoutBox().overflow_)
    GetLayoutBox().overflow_ = MakeGarbageCollected<BoxOverflowModel>();
  auto& previous_overflow = GetLayoutBox().overflow_->previous_overflow_data;
  if (!previous_overflow)
    previous_overflow.emplace();
  previous_overflow->previous_scrollable_overflow_rect =
      GetLayoutBox().ScrollableOverflowRect();
  previous_overflow->previous_visual_overflow_rect =
      GetLayoutBox().VisualOverflowRect();
  previous_overflow->previous_self_visual_overflow_rect =
      GetLayoutBox().SelfVisualOverflowRect();
}

void LayoutBox::MutableForPainting::SetPreviousGeometryForLayoutShiftTracking(
    const PhysicalOffset& paint_offset,
    const PhysicalSize& size,
    const PhysicalRect& visual_overflow_rect) {
  FirstFragment().SetPaintOffset(paint_offset);
  GetLayoutBox().previous_size_ = size;
  if (PhysicalRect(PhysicalOffset(), size).Contains(visual_overflow_rect))
    return;

  if (!GetLayoutBox().overflow_)
    GetLayoutBox().overflow_ = MakeGarbageCollected<BoxOverflowModel>();
  auto& previous_overflow = GetLayoutBox().overflow_->previous_overflow_data;
  if (!previous_overflow)
    previous_overflow.emplace();
  previous_overflow->previous_visual_overflow_rect = visual_overflow_rect;
  // Other previous rects don't matter because they are used for paint
  // invalidation and we always do full paint invalidation on reattachment.
}

void LayoutBox::MutableForPainting::UpdateBackgroundPaintLocation() {
  GetLayoutBox().SetBackgroundPaintLocation(
      GetLayoutBox().ComputeBackgroundPaintLocation());
}

RasterEffectOutset LayoutBox::VisualRectOutsetForRasterEffects() const {
  NOT_DESTROYED();
  // If the box has subpixel visual effect outsets, as the visual effect may be
  // painted along the pixel-snapped border box, the pixels on the anti-aliased
  // edge of the effect may overflow the calculated visual rect. Expand visual
  // rect by one pixel in the case.
  return VisualOverflowIsSet() &&
                 overflow_->visual_overflow->HasSubpixelVisualEffectOutsets()
             ? RasterEffectOutset::kWholePixel
             : RasterEffectOutset::kNone;
}

TextDirection LayoutBox::ResolvedDirection() const {
  NOT_DESTROYED();
  if (IsInline() && IsAtomicInlineLevel() &&
      IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    cursor.MoveTo(*this);
    if (cursor) {
      return cursor.Current().ResolvedDirection();
    }
  }
  return StyleRef().Direction();
}

void LayoutBox::OverrideTickmarks(Vector<gfx::Rect> tickmarks) {
  NOT_DESTROYED();
  GetScrollableArea()->SetTickmarksOverride(std::move(tickmarks));
  InvalidatePaintForTickmarks();
}

void LayoutBox::InvalidatePaintForTickmarks() {
  NOT_DESTROYED();
  ScrollableArea* scrollable_area = GetScrollableArea();
  if (!scrollable_area)
    return;
  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  if (!scrollbar)
    return;
  scrollbar->SetNeedsPaintInvalidation(static_cast<ScrollbarPart>(~kThumbPart));
}

static bool HasInsetBoxShadow(const ComputedStyle& style) {
  if (!style.BoxShadow())
    return false;
  for (const ShadowData& shadow : style.BoxShadow()->Shadows()) {
    if (shadow.Style() == ShadowStyle::kInset)
      return true;
  }
  return false;
}

// If all borders and scrollbars are opaque, then background-clip: border-box
// is equivalent to background-clip: padding-box.
bool LayoutBox::BackgroundClipBorderBoxIsEquivalentToPaddingBox() const {
  const auto* scrollable_area = GetScrollableArea();
  if (scrollable_area) {
    if (auto* scrollbar = scrollable_area->HorizontalScrollbar()) {
      if (!scrollbar->IsOverlayScrollbar() && !scrollbar->IsOpaque()) {
        return false;
      }
    }
    if (auto* scrollbar = scrollable_area->VerticalScrollbar()) {
      if (!scrollbar->IsOverlayScrollbar() && !scrollbar->IsOpaque()) {
        return false;
      }
    }
  }

  if (StyleRef().BorderTopWidth() &&
      (!ResolveColor(GetCSSPropertyBorderTopColor()).IsOpaque() ||
       StyleRef().BorderTopStyle() != EBorderStyle::kSolid)) {
    return false;
  }
  if (StyleRef().BorderRightWidth() &&
      (!ResolveColor(GetCSSPropertyBorderRightColor()).IsOpaque() ||
       StyleRef().BorderRightStyle() != EBorderStyle::kSolid)) {
    return false;
  }
  if (StyleRef().BorderBottomWidth() &&
      (!ResolveColor(GetCSSPropertyBorderBottomColor()).IsOpaque() ||
       StyleRef().BorderBottomStyle() != EBorderStyle::kSolid)) {
    return false;
  }
  if (StyleRef().BorderLeftWidth() &&
      (!ResolveColor(GetCSSPropertyBorderLeftColor()).IsOpaque() ||
       StyleRef().BorderLeftStyle() != EBorderStyle::kSolid)) {
    return false;
  }

  if (!StyleRef().IsScrollbarGutterAuto()) {
    return false;
  }

  return true;
}

BackgroundPaintLocation LayoutBox::ComputeBackgroundPaintLocation() const {
  NOT_DESTROYED();
  bool may_have_scrolling_layers_without_scrolling = IsA<LayoutView>(this);
  const auto* scrollable_area = GetScrollableArea();
  bool scrolls_overflow = scrollable_area && scrollable_area->ScrollsOverflow();
  if (!scrolls_overflow && !may_have_scrolling_layers_without_scrolling)
    return kBackgroundPaintInBorderBoxSpace;

  // If we care about LCD text, paint root backgrounds into scrolling contents
  // layer even if style suggests otherwise. (For non-root scrollers, we just
  // avoid compositing - see PLSA::ComputeNeedsCompositedScrolling.)
  if (IsA<LayoutView>(this) &&
      GetDocument().GetSettings()->GetLCDTextPreference() ==
          LCDTextPreference::kStronglyPreferred) {
    return kBackgroundPaintInContentsSpace;
  }

  // Inset box shadow is painted in the scrolling area above the background, and
  // it doesn't scroll, so the background can only be painted in the main layer.
  if (HasInsetBoxShadow(StyleRef()))
    return kBackgroundPaintInBorderBoxSpace;

  // For simplicity, assume any border image can have inset, like the above.
  if (StyleRef().BorderImage().GetImage()) {
    return kBackgroundPaintInBorderBoxSpace;
  }

  // Assume optimistically that the background can be painted in the scrolling
  // contents until we find otherwise.
  BackgroundPaintLocation paint_location = kBackgroundPaintInContentsSpace;

  Color background_color = ResolveColor(GetCSSPropertyBackgroundColor());
  const FillLayer* layer = &(StyleRef().BackgroundLayers());
  for (; layer; layer = layer->Next()) {
    if (layer->Attachment() == EFillAttachment::kLocal)
      continue;

    // The background color is either the only background or it's the
    // bottommost value from the background property (see final-bg-layer in
    // https://drafts.csswg.org/css-backgrounds/#the-background).
    if (!layer->GetImage() && !layer->Next() &&
        !background_color.IsFullyTransparent() &&
        StyleRef().IsScrollbarGutterAuto()) {
      // Solid color layers with an effective background clip of the padding box
      // can be treated as local.
      EFillBox clip = layer->Clip();
      if (clip == EFillBox::kPadding)
        continue;
      // A border box can be treated as a padding box if the border is opaque or
      // there is no border and we don't have custom scrollbars.
      if (clip == EFillBox::kBorder) {
        if (BackgroundClipBorderBoxIsEquivalentToPaddingBox())
          continue;
        // If we have an opaque background color, we can safely paint it into
        // both the scrolling contents layer and the graphics layer to preserve
        // LCD text. The background color is either the only background or
        // behind background-attachment:local images (ensured by previous
        // iterations of the loop). For the latter case, the first paint of the
        // images doesn't matter because it will be covered by the second paint
        // of the opaque color.
        if (background_color.IsOpaque()) {
          paint_location = kBackgroundPaintInBothSpaces;
          continue;
        }
      } else if (clip == EFillBox::kContent &&
                 StyleRef().PaddingTop().IsZero() &&
                 StyleRef().PaddingLeft().IsZero() &&
                 StyleRef().PaddingRight().IsZero() &&
                 StyleRef().PaddingBottom().IsZero()) {
        // A content fill box can be treated as a padding fill box if there is
        // no padding.
        continue;
      }
    }
    return kBackgroundPaintInBorderBoxSpace;
  }

  // It can't paint in the scrolling contents because it has different 3d
  // context than the scrolling contents.
  if (!StyleRef().Preserves3D() && Parent() &&
      Parent()->StyleRef().Preserves3D()) {
    return kBackgroundPaintInBorderBoxSpace;
  }

  return paint_location;
}

bool LayoutBox::ComputeCanCompositeBackgroundAttachmentFixed() const {
  NOT_DESTROYED();
  DCHECK(IsBackgroundAttachmentFixedObject());
  if (GetDocument().GetSettings()->GetLCDTextPreference() ==
      LCDTextPreference::kStronglyPreferred) {
    return false;
  }
  // The fixed attachment background must be the only background layer.
  if (StyleRef().BackgroundLayers().Next() ||
      StyleRef().BackgroundLayers().Clip() == EFillBox::kText) {
    return false;
  }
  // To support box shadow, we'll need to paint the outset and inset box
  // shadows in separate display items in case there are outset box shadow,
  // background, inset box shadow and border in paint order.
  if (StyleRef().BoxShadow()) {
    return false;
  }
  // The theme may paint the background differently for an appearance.
  if (StyleRef().HasEffectiveAppearance()) {
    return false;
  }
  // For now the BackgroundClip paint property node doesn't support rounded
  // corners. If we want to support this, we need to ensure
  // - there is no obvious bleeding issues, and
  // - both the fast path and the slow path of composited rounded clip work.
  if (StyleRef().HasBorderRadius()) {
    return false;
  }
  return true;
}

bool LayoutBox::IsFixedToView(
    const LayoutObject* container_for_fixed_position) const {
  if (!IsFixedPositioned())
    return false;

  const auto* container = container_for_fixed_position;
  if (!container)
    container = Container();
  else
    DCHECK_EQ(container, Container());
  return container->IsLayoutView();
}

PhysicalRect LayoutBox::ComputeStickyConstrainingRect() const {
  NOT_DESTROYED();
  DCHECK(IsScrollContainer());
  PhysicalRect constraining_rect(OverflowClipRect(PhysicalOffset()));
  constraining_rect.Move(PhysicalOffset(-BorderLeft() + PaddingLeft(),
                                        -BorderTop() + PaddingTop()));
  constraining_rect.ContractEdges(LayoutUnit(), PaddingLeft() + PaddingRight(),
                                  PaddingTop() + PaddingBottom(), LayoutUnit());
  return constraining_rect;
}

AnchorPositionScrollData* LayoutBox::GetAnchorPositionScrollData() const {
  if (Element* element = DynamicTo<Element>(GetNode())) {
    return element->GetAnchorPositionScrollData();
  }
  return nullptr;
}

bool LayoutBox::NeedsAnchorPositionScrollAdjustment() const {
  if (auto* data = GetAnchorPositionScrollData()) {
    return data->NeedsScrollAdjustment();
  }
  return false;
}

bool LayoutBox::AnchorPositionScrollAdjustmentAfectedByViewportScrolling()
    const {
  if (auto* data = GetAnchorPositionScrollData()) {
    return data->NeedsScrollAdjustment() &&
           data->IsAffectedByViewportScrolling();
  }
  return false;
}

PhysicalOffset LayoutBox::AnchorPositionScrollTranslationOffset() const {
  if (auto* data = GetAnchorPositionScrollData()) {
    return data->TranslationAsPhysicalOffset();
  }
  return PhysicalOffset();
}

namespace {

template <typename Function>
void ForEachAnchorQueryOnContainer(const LayoutBox& box, Function func) {
  const LayoutObject* container = box.Container();
  if (container->IsLayoutBlock()) {
    for (const PhysicalBoxFragment& fragment :
         To<LayoutBlock>(container)->PhysicalFragments()) {
      if (const PhysicalAnchorQuery* anchor_query = fragment.AnchorQuery()) {
        func(*anchor_query);
      }
    }
    return;
  }

  // Now the container is an inline box that's also an abspos containing block.
  CHECK(container->IsLayoutInline());
  const LayoutInline* inline_container = To<LayoutInline>(container);
  if (!inline_container->HasInlineFragments()) {
    return;
  }
  InlineCursor cursor;
  cursor.MoveTo(*container);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    if (const PhysicalBoxFragment* fragment = cursor.Current().BoxFragment()) {
      if (const PhysicalAnchorQuery* anchor_query = fragment->AnchorQuery()) {
        func(*anchor_query);
      }
    }
  }
}

#if EXPENSIVE_DCHECKS_ARE_ON()
template <typename Function>
void AssertSameDataOnLayoutResults(
    const LayoutBox::LayoutResultList& layout_results,
    Function func) {
  // When an out-of-flow box is fragmented, the position fallback results on all
  // fragments should be the same.
  for (wtf_size_t i = 1; i < layout_results.size(); ++i) {
    DCHECK(func(layout_results[i]) == func(layout_results[i - 1]));
  }
}

#endif

}  // namespace

const LayoutObject* LayoutBox::FindTargetAnchor(
    const ScopedCSSName& anchor_name) const {
  if (!IsOutOfFlowPositioned()) {
    return nullptr;
  }

  // Go through the already built PhysicalAnchorQuery to avoid tree traversal.
  const LayoutObject* anchor = nullptr;
  auto search_for_anchor = [&](const PhysicalAnchorQuery& anchor_query) {
    if (const LayoutObject* current =
            anchor_query.AnchorLayoutObject(*this, &anchor_name)) {
      if (!anchor ||
          (anchor != current && anchor->IsBeforeInPreOrder(*current))) {
        anchor = current;
      }
    }
  };
  ForEachAnchorQueryOnContainer(*this, search_for_anchor);
  return anchor;
}

const LayoutObject* LayoutBox::AcceptableImplicitAnchor() const {
  if (!IsOutOfFlowPositioned()) {
    return nullptr;
  }
  Element* element = DynamicTo<Element>(GetNode());
  Element* anchor_element =
      element ? element->ImplicitAnchorElement() : nullptr;
  LayoutObject* anchor_layout_object =
      anchor_element ? anchor_element->GetLayoutObject() : nullptr;
  if (!anchor_layout_object) {
    return nullptr;
  }
  // Go through the already built PhysicalAnchorQuery to avoid tree traversal.
  bool is_acceptable_anchor = false;
  auto validate_anchor = [&](const PhysicalAnchorQuery& anchor_query) {
    if (anchor_query.AnchorLayoutObject(*this, anchor_layout_object)) {
      is_acceptable_anchor = true;
    }
  };
  ForEachAnchorQueryOnContainer(*this, validate_anchor);
  return is_acceptable_anchor ? anchor_layout_object : nullptr;
}

const HeapVector<NonOverflowingScrollRange>*
LayoutBox::NonOverflowingScrollRanges() const {
  const auto& layout_results = GetLayoutResults();
  if (layout_results.empty()) {
    return nullptr;
  }
  // We only need to check the first fragment, because when the box is
  // fragmented, position fallback results are duplicated on all fragments.
#if EXPENSIVE_DCHECKS_ARE_ON()
  for (wtf_size_t i = 1; i < layout_results.size(); ++i) {
    DCHECK(base::ValuesEquivalent(
        layout_results[i]->NonOverflowingScrollRanges(),
        layout_results[i - 1]->NonOverflowingScrollRanges()));
  }
#endif
  return layout_results.front()->NonOverflowingScrollRanges();
}

const BoxStrut& LayoutBox::OutOfFlowInsetsForGetComputedStyle() const {
  const auto& layout_results = GetLayoutResults();
  // We should call this function only after the node is laid out.
  CHECK(layout_results.size());
  // We only need to check the first fragment, because when the box is
  // fragmented, insets are duplicated on all fragments.
#if EXPENSIVE_DCHECKS_ARE_ON()
  AssertSameDataOnLayoutResults(layout_results, [](const auto& result) {
    return result->OutOfFlowInsetsForGetComputedStyle();
  });
#endif
  return GetLayoutResults().front()->OutOfFlowInsetsForGetComputedStyle();
}

Element* LayoutBox::AccessibilityAnchor() const {
  const auto& layout_results = GetLayoutResults();
  if (layout_results.empty()) {
    return nullptr;
  }
  return layout_results.front()->AccessibilityAnchor();
}

const HeapHashSet<Member<Element>>* LayoutBox::DisplayLocksAffectedByAnchors()
    const {
  const auto& layout_results = GetLayoutResults();
  if (layout_results.empty()) {
    return nullptr;
  }
  return layout_results.front()->DisplayLocksAffectedByAnchors();
}

void LayoutBox::NotifyContainingDisplayLocksForAnchorPositioning(
    const HeapHashSet<Member<Element>>* past_display_locks_affected_by_anchors,
    const HeapHashSet<Member<Element>>* display_locks_affected_by_anchors)
    const {
  auto notify_display_locks =
      [](const HeapHashSet<Member<Element>>* display_locks) {
        if (!display_locks) {
          return;
        }
        for (auto& display_lock_element : *display_locks) {
          display_lock_element->GetDisplayLockContext()
              ->SetAnchorPositioningRenderStateMayHaveChanged();
        }
      };

  notify_display_locks(past_display_locks_affected_by_anchors);
  notify_display_locks(display_locks_affected_by_anchors);
}

bool LayoutBox::NeedsAnchorPositionScrollAdjustmentInX() const {
  const auto& layout_results = GetLayoutResults();
  if (layout_results.empty()) {
    return false;
  }
  // We only need to check the first fragment, because when the box is
  // fragmented, position fallback results are duplicated on all fragments.
#if EXPENSIVE_DCHECKS_ARE_ON()
  AssertSameDataOnLayoutResults(layout_results, [](const auto& result) {
    return result->NeedsAnchorPositionScrollAdjustmentInX();
  });
#endif
  return layout_results.front()->NeedsAnchorPositionScrollAdjustmentInX();
}

bool LayoutBox::NeedsAnchorPositionScrollAdjustmentInY() const {
  const auto& layout_results = GetLayoutResults();
  if (layout_results.empty()) {
    return false;
  }
  // We only need to check the first fragment, because when the box is
  // fragmented, position fallback results are duplicated on all fragments.
#if EXPENSIVE_DCHECKS_ARE_ON()
  AssertSameDataOnLayoutResults(layout_results, [](const auto& result) {
    return result->NeedsAnchorPositionScrollAdjustmentInY();
  });
#endif
  return layout_results.front()->NeedsAnchorPositionScrollAdjustmentInY();
}

WritingModeConverter LayoutBox::CreateWritingModeConverter() const {
  return WritingModeConverter({Style()->GetWritingMode(), TextDirection::kLtr},
                              Size());
}

bool LayoutBox::IsReadingFlowContainer() const {
  if (!RuntimeEnabledFeatures::CSSReadingFlowEnabled()) {
    return false;
  }
  const ComputedStyle& style = StyleRef();
  switch (style.ReadingFlow()) {
    case EReadingFlow::kNormal:
      return false;
    case EReadingFlow::kFlexVisual:
    case EReadingFlow::kFlexFlow:
      return IsFlexibleBox();
    case EReadingFlow::kGridRows:
    case EReadingFlow::kGridColumns:
    case EReadingFlow::kGridOrder:
      return IsLayoutGrid();
  }
  return false;
}

const HeapVector<Member<Element>>& LayoutBox::ReadingFlowElements() const {
  if (const auto* elements = GetPhysicalFragment(0)->ReadingFlowElements()) {
    return *elements;
  }
  DEFINE_STATIC_LOCAL(Persistent<HeapVector<Member<Element>>>, empty_vector,
                      (MakeGarbageCollected<HeapVector<Member<Element>>>()));
  return *empty_vector;
}

}  // namespace blink
