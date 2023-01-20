/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
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

#include "third_party/blink/renderer/core/layout/layout_object.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/css_toggle_map.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/layout_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_summary_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"
#include "third_party/blink/renderer/core/layout/layout_deprecated_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_grid.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource_style_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_object_inl.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/style/cursor_data.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state_storage.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

template <typename Predicate>
LayoutObject* FindAncestorByPredicate(const LayoutObject* descendant,
                                      LayoutObject::AncestorSkipInfo* skip_info,
                                      Predicate predicate) {
  for (auto* object = descendant->Parent(); object; object = object->Parent()) {
    if (predicate(object))
      return object;
    if (skip_info)
      skip_info->Update(*object);

    if (UNLIKELY(object->IsColumnSpanAll())) {
      // The containing block chain goes directly from the column spanner to the
      // multi-column container.
      const auto* multicol_container =
          object->SpannerPlaceholder()->MultiColumnBlockFlow();
      if (multicol_container->IsLayoutNGObject()) {
        while (object->Parent() != multicol_container) {
          object = object->Parent();
          if (skip_info)
            skip_info->Update(*object);
        }
      }
    }
  }
  return nullptr;
}

inline bool MightTraversePhysicalFragments(const LayoutObject& obj) {
  if (!obj.IsLayoutNGObject()) {
    // Non-NG objects should be painted, hit-tested, etc. by legacy.
    if (obj.IsBox())
      return false;
    // Non-LayoutBox objects (such as LayoutInline) don't necessarily create NG
    // LayoutObjects. If they are laid out by an NG container, though, we may be
    // allowed to traverse their fragments. We can't check that at this point
    // (potentially before initial layout), though. Unless there are other
    // reasons that prevent us from allowing fragment traversal, we'll
    // optimistically return true now, and check later.
  }
  // The NG paint system currently doesn't support replaced content.
  if (obj.IsLayoutReplaced())
    return false;
  // The NG paint system currently doesn't support table-cells.
  if (obj.IsTableCellLegacy())
    return false;
  // Text controls have some logic in the layout objects that will be missed if
  // we traverse the fragment tree when hit-testing.
  if (obj.IsTextControlIncludingNG())
    return false;
  return true;
}

bool HasNativeBackgroundPainter(Node* node) {
  if (!RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled())
    return false;

  Element* element = DynamicTo<Element>(node);
  if (!element)
    return false;

  ElementAnimations* element_animations = element->GetElementAnimations();
  if (!element_animations)
    return false;

  return element_animations->CompositedBackgroundColorStatus() ==
         ElementAnimations::CompositedPaintStatus::kComposited;
}

bool HasClipPathPaintWorklet(Node* node) {
  if (!RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled())
    return false;

  Element* element = DynamicTo<Element>(node);
  if (!element)
    return false;

  ElementAnimations* element_animations = element->GetElementAnimations();
  if (!element_animations)
    return false;

  return element_animations->CompositedClipPathStatus() ==
         ElementAnimations::CompositedPaintStatus::kComposited;
}

StyleDifference AdjustForCompositableAnimationPaint(
    scoped_refptr<const ComputedStyle> old_style,
    scoped_refptr<const ComputedStyle> new_style,
    Node* node,
    StyleDifference diff) {
  DCHECK(new_style);

  // Background color changes that are triggered by animations on the compositor
  // thread can skip paint invalidation.
  bool had_background_color_animation =
      old_style ? old_style->HasCurrentBackgroundColorAnimation() : false;

  bool has_background_color_animation =
      new_style->HasCurrentBackgroundColorAnimation();
  // If animation status changed, we need a paint invalidation regardless of
  // whether the background color changed.
  if (RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled() &&
      (had_background_color_animation != has_background_color_animation))
    diff.SetNeedsNormalPaintInvalidation();

  bool skip_background_color_paint_invalidation =
      !diff.BackgroundColorChanged() || HasNativeBackgroundPainter(node);
  if (!skip_background_color_paint_invalidation)
    diff.SetNeedsNormalPaintInvalidation();

  bool had_clip_path_animation =
      old_style ? old_style->HasCurrentClipPathAnimation() : false;

  bool has_clip_path_animation = new_style->HasCurrentClipPathAnimation();
  // If animation status changed, we need a paint invalidation regardless of
  // whether the background color changed.
  if (RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled() &&
      (had_clip_path_animation != has_clip_path_animation))
    diff.SetNeedsNormalPaintInvalidation();

  bool skip_clip_path_paint_invalidation =
      !diff.ClipPathChanged() || HasClipPathPaintWorklet(node);
  if (!skip_clip_path_paint_invalidation)
    diff.SetNeedsNormalPaintInvalidation();

  return diff;
}

}  // namespace

static int g_allow_destroying_layout_object_in_finalizer = 0;

void ApplyVisibleOverflowToClipRect(OverflowClipAxes overflow_clip,
                                    PhysicalRect& clip_rect) {
  DCHECK_NE(overflow_clip, kOverflowClipBothAxis);
  const LayoutRect infinite_rect(LayoutRect::InfiniteIntRect());
  if ((overflow_clip & kOverflowClipX) == kNoOverflowClip) {
    clip_rect.offset.left = infinite_rect.X();
    clip_rect.size.width = infinite_rect.Width();
  }
  if ((overflow_clip & kOverflowClipY) == kNoOverflowClip) {
    clip_rect.offset.top = infinite_rect.Y();
    clip_rect.size.height = infinite_rect.Height();
  }
}

AllowDestroyingLayoutObjectInFinalizerScope::
    AllowDestroyingLayoutObjectInFinalizerScope() {
  ++g_allow_destroying_layout_object_in_finalizer;
}
AllowDestroyingLayoutObjectInFinalizerScope::
    ~AllowDestroyingLayoutObjectInFinalizerScope() {
  CHECK_GT(g_allow_destroying_layout_object_in_finalizer, 0);
  --g_allow_destroying_layout_object_in_finalizer;
}

#if DCHECK_IS_ON()

LayoutObject::SetLayoutNeededForbiddenScope::SetLayoutNeededForbiddenScope(
    LayoutObject& layout_object)
    : layout_object_(layout_object),
      preexisting_forbidden_(layout_object_.IsSetNeedsLayoutForbidden()) {
  layout_object_.SetNeedsLayoutIsForbidden(true);
}

LayoutObject::SetLayoutNeededForbiddenScope::~SetLayoutNeededForbiddenScope() {
  layout_object_.SetNeedsLayoutIsForbidden(preexisting_forbidden_);
}
#endif

struct SameSizeAsLayoutObject : public GarbageCollected<SameSizeAsLayoutObject>,
                                ImageResourceObserver,
                                DisplayItemClient {
  // Normally these additional bitfields can use the gap between
  // DisplayItemClient and bitfields_.
  uint8_t additional_bitfields_;
  uint16_t additional_bitfields2_;
#if DCHECK_IS_ON()
  unsigned debug_bitfields_;
#endif
  unsigned bitfields_;
  unsigned bitfields2_;
  unsigned bitfields3_;
  void* pointers[1];
  Member<void*> members[5];
#if DCHECK_IS_ON()
  bool is_destroyed_;
#endif
};

ASSERT_SIZE(LayoutObject, SameSizeAsLayoutObject);

bool LayoutObject::affects_parent_block_ = false;

LayoutObject* LayoutObject::CreateObject(Element* element,
                                         const ComputedStyle& style,
                                         LegacyLayout legacy) {
  DCHECK(IsAllowedToModifyLayoutTreeStructure(element->GetDocument()));

  // Minimal support for content properties replacing an entire element.
  // Works only if we have exactly one piece of content and it's a URL, with
  // some optional alternative text. Otherwise acts as if we didn't support this
  // feature.
  const ContentData* content_data = style.GetContentData();
  if (!element->IsPseudoElement() &&
      ShouldUseContentDataForElement(content_data)) {
    LayoutImage* image = MakeGarbageCollected<LayoutImage>(element);
    // LayoutImageResourceStyleImage requires a style being present on the
    // image but we don't want to trigger a style change now as the node is
    // not fully attached. Moving this code to style change doesn't make sense
    // as it should be run once at layoutObject creation.
    image->SetStyleInternal(const_cast<ComputedStyle*>(&style));
    if (const StyleImage* style_image =
            To<ImageContentData>(content_data)->GetImage()) {
      image->SetImageResource(
          MakeGarbageCollected<LayoutImageResourceStyleImage>(
              const_cast<StyleImage*>(style_image)));
      image->SetIsGeneratedContent();
    } else {
      image->SetImageResource(MakeGarbageCollected<LayoutImageResource>());
    }
    image->SetStyleInternal(nullptr);
    return image;
  } else if (element->GetPseudoId() == kPseudoIdMarker) {
    return LayoutObjectFactory::CreateListMarker(*element, style, legacy);
  }

  switch (style.Display()) {
    case EDisplay::kNone:
    case EDisplay::kContents:
      return nullptr;
    case EDisplay::kInline:
      return MakeGarbageCollected<LayoutInline>(element);
    case EDisplay::kBlock:
    case EDisplay::kFlowRoot:
    case EDisplay::kInlineBlock:
    case EDisplay::kListItem:
      return LayoutObjectFactory::CreateBlockFlow(*element, style, legacy);
    case EDisplay::kTable:
    case EDisplay::kInlineTable:
      return LayoutObjectFactory::CreateTable(*element, style, legacy);
    case EDisplay::kTableRowGroup:
    case EDisplay::kTableHeaderGroup:
    case EDisplay::kTableFooterGroup:
      return LayoutObjectFactory::CreateTableSection(*element, style, legacy);
    case EDisplay::kTableRow:
      return LayoutObjectFactory::CreateTableRow(*element, style, legacy);
    case EDisplay::kTableColumnGroup:
    case EDisplay::kTableColumn:
      return LayoutObjectFactory::CreateTableColumn(*element, style, legacy);
    case EDisplay::kTableCell:
      return LayoutObjectFactory::CreateTableCell(*element, style, legacy);
    case EDisplay::kTableCaption:
      return LayoutObjectFactory::CreateTableCaption(*element, style, legacy);
    case EDisplay::kWebkitBox:
    case EDisplay::kWebkitInlineBox:
      if (style.IsDeprecatedWebkitBoxWithVerticalLineClamp()) {
        return LayoutObjectFactory::CreateBlockForLineClamp(*element, style,
                                                            legacy);
      }
      UseCounter::Count(element->GetDocument(),
                        WebFeature::kWebkitBoxWithoutWebkitLineClamp);
      return LayoutObjectFactory::CreateFlexibleBox(*element, style, legacy);
    case EDisplay::kFlex:
    case EDisplay::kInlineFlex:
      UseCounter::Count(element->GetDocument(), WebFeature::kCSSFlexibleBox);
      return LayoutObjectFactory::CreateFlexibleBox(*element, style, legacy);
    case EDisplay::kGrid:
    case EDisplay::kInlineGrid:
      UseCounter::Count(element->GetDocument(), WebFeature::kCSSGridLayout);
      return LayoutObjectFactory::CreateGrid(*element, style, legacy);
    case EDisplay::kMath:
    case EDisplay::kBlockMath:
      return LayoutObjectFactory::CreateMath(*element, style, legacy);
    case EDisplay::kLayoutCustom:
    case EDisplay::kInlineLayoutCustom:
      return LayoutObjectFactory::CreateCustom(*element, style, legacy);
  }

  NOTREACHED();
  return nullptr;
}

LayoutObject::LayoutObject(Node* node)
    : full_paint_invalidation_reason_(
          static_cast<unsigned>(PaintInvalidationReason::kNone)),
      positioned_state_(kIsStaticallyPositioned),
      selection_state_(static_cast<unsigned>(SelectionState::kNone)),
      selection_state_for_paint_(static_cast<unsigned>(SelectionState::kNone)),
      subtree_paint_property_update_reasons_(
          static_cast<unsigned>(SubtreePaintPropertyUpdateReason::kNone)),
      background_paint_location_(kBackgroundPaintInBorderBoxSpace),
      overflow_clip_axes_(kNoOverflowClip),
#if DCHECK_IS_ON()
      has_ax_object_(false),
      set_needs_layout_forbidden_(false),
      as_image_observer_count_(0),
#endif
      bitfields_(node),
      style_(nullptr),
      node_(node),
      parent_(nullptr),
      previous_(nullptr),
      next_(nullptr),
      fragment_(MakeGarbageCollected<FragmentData>()) {
  InstanceCounters::IncrementCounter(InstanceCounters::kLayoutObjectCounter);
  if (node_)
    GetFrameView()->IncrementLayoutObjectCount();
}

LayoutObject::~LayoutObject() {
  DCHECK(bitfields_.BeingDestroyed());
#if DCHECK_IS_ON()
  DCHECK(is_destroyed_);
#endif
  InstanceCounters::DecrementCounter(InstanceCounters::kLayoutObjectCounter);
}

bool LayoutObject::IsDescendantOf(const LayoutObject* obj) const {
  NOT_DESTROYED();
  for (const LayoutObject* r = this; r; r = r->parent_) {
    if (r == obj)
      return true;
  }
  return false;
}

bool LayoutObject::IsHR() const {
  NOT_DESTROYED();
  return IsA<HTMLHRElement>(GetNode());
}

bool LayoutObject::IsStyleGenerated() const {
  NOT_DESTROYED();
  if (const auto* layout_text_fragment = DynamicTo<LayoutTextFragment>(this))
    return !layout_text_fragment->AssociatedTextNode();

  const Node* node = GetNode();
  return !node || node->IsPseudoElement();
}

void LayoutObject::MarkMayHaveAnchorQuery() {
  for (LayoutObject* runner = this; runner && !runner->MayHaveAnchorQuery();
       runner = runner->Parent()) {
    runner->SetSelfMayHaveAnchorQuery();
  }
}

void LayoutObject::SetIsInsideFlowThreadIncludingDescendants(
    bool inside_flow_thread) {
  NOT_DESTROYED();
  LayoutObject* next;
  for (LayoutObject* object = this; object; object = next) {
    // If object is a fragmentation context it already updated the descendants
    // flag accordingly.
    if (object->IsLayoutFlowThread()) {
      next = object->NextInPreOrderAfterChildren(this);
      continue;
    }
    next = object->NextInPreOrder(this);
    DCHECK_NE(inside_flow_thread, object->IsInsideFlowThread());
    object->SetIsInsideFlowThread(inside_flow_thread);
  }
}

bool LayoutObject::RequiresAnonymousTableWrappers(
    const LayoutObject* new_child) const {
  NOT_DESTROYED();
  // Check should agree with:
  // CSS 2.1 Tables: 17.2.1 Anonymous table objects
  // http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes
  if (new_child->IsLayoutTableCol()) {
    bool is_column_in_column_group =
        new_child->StyleRef().Display() == EDisplay::kTableColumn &&
        IsLayoutTableCol();
    return !IsTable() && !is_column_in_column_group;
  }
  if (new_child->IsTableCaption())
    return !IsTable();
  if (new_child->IsTableSection())
    return !IsTable();
  if (new_child->IsTableRow())
    return !IsTableSection();
  if (new_child->IsTableCell())
    return !IsTableRow();
  return false;
}

#if DCHECK_IS_ON()

void LayoutObject::AssertFragmentTree(bool display_locked) const {
  NOT_DESTROYED();
  for (const LayoutObject* layout_object = this; layout_object;) {
    // |LayoutNGMixin::UpdateInFlowBlockLayout| may |SetNeedsLayout| to its
    // containing block. Don't check if it will be re-laid out.
    if (layout_object->NeedsLayout()) {
      layout_object = layout_object->NextInPreOrderAfterChildren(this);
      continue;
    }

    // If display-locked, fragments may not be removed from the tree even after
    // the |LayoutObject| was destroyed, but still they should be consistent.
    if (!display_locked && layout_object->ChildLayoutBlockedByDisplayLock()) {
      layout_object->AssertFragmentTree(
          /* display_locked */ true);
      layout_object = layout_object->NextInPreOrderAfterChildren(this);
      continue;
    }

    // Check the direct children of the fragment. Grand-children and further
    // descendants will be checked by descendant LayoutObjects.
    if (const auto* box = DynamicTo<LayoutBox>(layout_object)) {
      for (const NGPhysicalBoxFragment& fragment : box->PhysicalFragments()) {
        DCHECK_EQ(box, fragment.OwnerLayoutBox());
        fragment.AssertFragmentTreeChildren(
            /* allow_destroyed_or_moved */ display_locked);
      }
    }
    layout_object = layout_object->NextInPreOrder(this);
  }
}

void LayoutObject::AssertClearedPaintInvalidationFlags() const {
  NOT_DESTROYED();
  if (ChildPrePaintBlockedByDisplayLock())
    return;

  if (PaintInvalidationStateIsDirty()) {
    ShowLayoutTreeForThis();
    NOTREACHED();
  }

  // Assert that the number of FragmentData and NGPhysicalBoxFragment objects
  // are identical. This was added as part of investigating crbug.com/1244130

  // Only LayoutBox has fragments. Bail if it's not a box, or if fragment
  // traversal isn't supported here.
  if (!IsBox() || !CanTraversePhysicalFragments())
    return;

  // Make an exception for table columns (unless they establish a layer, which
  // would be dangerous (but hopefully also impossible)), since they don't
  // produce fragments.
  if (IsLayoutTableCol() && !HasLayer())
    return;

  // Make an exception for <frameset> children, which don't produce fragments
  // if the number of children is larger than <rows count> * <cols count>.
  if (Parent() && Parent()->IsFrameSetIncludingNG())
    return;

  // Sometimes we just have a Layout(NG)View with no children, and the view is
  // not marked for layout, even if it has never been laid out. It seems that we
  // don't actually paint under such circumstances, which means that it doesn't
  // matter whether we have fragments or not. See crbug.com/1288742
  if (IsLayoutView() && !EverHadLayout() && !SlowFirstChild())
    return;

  wtf_size_t fragment_count = 0;
  for (const FragmentData* walker = &FirstFragment(); walker;
       walker = walker->NextFragment())
    fragment_count++;
  if (fragment_count != To<LayoutBox>(this)->PhysicalFragmentCount()) {
    ShowLayoutTreeForThis();
    DCHECK_EQ(fragment_count, To<LayoutBox>(this)->PhysicalFragmentCount());
  }
}

#endif  // DCHECK_IS_ON()

DISABLE_CFI_PERF
void LayoutObject::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  NOT_DESTROYED();
  DCHECK(IsAllowedToModifyLayoutTreeStructure(GetDocument()) ||
         IsLayoutNGObjectForFormattedText());

  LayoutObjectChildList* children = VirtualChildren();
  DCHECK(children);
  if (!children)
    return;

  if (RequiresAnonymousTableWrappers(new_child)) {
    // Generate an anonymous table or reuse existing one from previous child
    // Per: 17.2.1 Anonymous table objects 3. Generate missing parents
    // http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes
    LayoutObject* table;
    LayoutObject* after_child =
        before_child ? before_child->PreviousSibling() : children->LastChild();
    if (after_child && after_child->IsAnonymous() && after_child->IsTable() &&
        !after_child->IsBeforeContent()) {
      table = after_child;
    } else {
      table = LayoutObjectFactory::CreateAnonymousTableWithParent(
          *this, !new_child->IsLayoutNGObject());
      children->InsertChildNode(this, table, before_child);
    }
    table->AddChild(new_child);
  } else if (LIKELY(new_child->IsHorizontalWritingMode()) ||
             !new_child->IsText()) {
    children->InsertChildNode(this, new_child, before_child);
  } else if (IsA<LayoutNGTextCombine>(*this)) {
    DCHECK(LayoutNGTextCombine::ShouldBeParentOf(*new_child)) << new_child;
    new_child->SetStyle(Style());
    children->InsertChildNode(this, new_child, before_child);
  } else if (!IsHorizontalWritingMode() &&
             LayoutNGTextCombine::ShouldBeParentOf(*new_child)) {
    if (before_child) {
      if (IsA<LayoutNGTextCombine>(before_child)) {
        DCHECK(!DynamicTo<LayoutNGTextCombine>(before_child->PreviousSibling()))
            << before_child->PreviousSibling();
        before_child->AddChild(new_child, before_child->SlowFirstChild());
      } else if (auto* const previous_sibling = DynamicTo<LayoutNGTextCombine>(
                     before_child->PreviousSibling())) {
        previous_sibling->AddChild(new_child);
      } else {
        children->InsertChildNode(
            this,
            LayoutNGTextCombine::CreateAnonymous(To<LayoutText>(new_child)),
            before_child);
      }
    } else if (auto* const last_child =
                   DynamicTo<LayoutNGTextCombine>(SlowLastChild())) {
      last_child->AddChild(new_child);
    } else {
      children->AppendChildNode(this, LayoutNGTextCombine::CreateAnonymous(
                                          To<LayoutText>(new_child)));
    }
  } else {
    // In case of append/insert <br style="writing-mode:vertical-rl">
    // See http://crbug.com/1222121 and http://crbug.com/1258331
    DCHECK(!new_child->IsHorizontalWritingMode()) << new_child;
    DCHECK(new_child->IsText()) << new_child;
    children->InsertChildNode(this, new_child, before_child);
  }

  if (new_child->IsText() &&
      new_child->StyleRef().TextTransform() == ETextTransform::kCapitalize)
    To<LayoutText>(new_child)->TransformText();
}

void LayoutObject::RemoveChild(LayoutObject* old_child) {
  NOT_DESTROYED();
  DCHECK(IsAllowedToModifyLayoutTreeStructure(GetDocument()) ||
         IsLayoutNGObjectForFormattedText());

  LayoutObjectChildList* children = VirtualChildren();
  DCHECK(children);
  if (!children)
    return;

  children->RemoveChildNode(this, old_child);
}

void LayoutObject::NotifyPriorityScrollAnchorStatusChanged() {
  NOT_DESTROYED();
  if (!Parent())
    return;
  for (auto* layer = Parent()->EnclosingLayer(); layer;
       layer = layer->Parent()) {
    if (PaintLayerScrollableArea* scrollable_area =
            layer->GetScrollableArea()) {
      DCHECK(scrollable_area->GetScrollAnchor());
      scrollable_area->GetScrollAnchor()->ClearSelf();
    }
  }
}

void LayoutObject::RegisterSubtreeChangeListenerOnDescendants(bool value) {
  NOT_DESTROYED();
  // If we're set to the same value then we're done as that means it's
  // set down the tree that way already.
  if (bitfields_.SubtreeChangeListenerRegistered() == value)
    return;

  bitfields_.SetSubtreeChangeListenerRegistered(value);

  for (LayoutObject* curr = SlowFirstChild(); curr; curr = curr->NextSibling())
    curr->RegisterSubtreeChangeListenerOnDescendants(value);
}

bool LayoutObject::NotifyOfSubtreeChange() {
  NOT_DESTROYED();
  if (!bitfields_.SubtreeChangeListenerRegistered() ||
      bitfields_.NotifiedOfSubtreeChange()) {
    return false;
  }
  bitfields_.SetNotifiedOfSubtreeChange(true);
  return true;
}

void LayoutObject::HandleSubtreeModifications() {
  NOT_DESTROYED();
  if (ConsumesSubtreeChangeNotification())
    SubtreeDidChange();
  bitfields_.SetNotifiedOfSubtreeChange(false);
}

LayoutObject* LayoutObject::NextInPreOrder() const {
  NOT_DESTROYED();
  if (LayoutObject* o = SlowFirstChild())
    return o;

  return NextInPreOrderAfterChildren();
}

bool LayoutObject::IsForElement() const {
  if (!IsAnonymous()) {
    return true;
  }

  // When a block is inside of an inline, the part of the inline that
  // wraps the block is represented in the layout tree by a block that
  // is marked as anonymous, but has a continuation that's not
  // anonymous.

  if (!IsBox()) {
    return false;
  }

  auto* continuation = To<LayoutBox>(this)->Continuation();
  if (!continuation || continuation->IsAnonymous()) {
    return false;
  }

  DCHECK(continuation->IsInline());
  DCHECK(IsLayoutBlockFlow());
  return true;
}

bool LayoutObject::HasClipRelatedProperty() const {
  NOT_DESTROYED();
  // This function detects a bunch of properties that can potentially affect
  // clip inheritance chain. However such generalization is practically useless
  // because these properties change clip inheritance in different way that
  // needs to be handled explicitly.
  // CSS clip applies clip to the current element and all descendants.
  // CSS overflow clip applies only to containing-block descendants.
  // CSS contain:paint applies to all descendants by making itself a containing
  // block for all descendants.
  // CSS clip-path/mask/filter induces a stacking context and applies inherited
  // clip to that stacking context, while resetting clip for descendants. This
  // special behavior is already handled elsewhere.
  if (HasClip() || ShouldClipOverflowAlongEitherAxis())
    return true;
  // Paint containment establishes isolation which creates clip isolation nodes.
  // Style & Layout containment also establish isolation (see
  // |NeedsIsolationNodes| in PaintPropertyTreeBuilder).
  if (ShouldApplyPaintContainment() ||
      (ShouldApplyStyleContainment() && ShouldApplyLayoutContainment())) {
    return true;
  }
  if (IsBox() && To<LayoutBox>(this)->HasControlClip())
    return true;
  return false;
}

bool LayoutObject::IsRenderedLegendInternal() const {
  NOT_DESTROYED();
  DCHECK(IsBox());
  DCHECK(IsRenderedLegendCandidate());

  const auto* parent = Parent();
  // We may not be inserted into the tree yet.
  if (!parent)
    return false;

  const auto* parent_layout_block = DynamicTo<LayoutBlock>(parent);
  return parent_layout_block && IsA<HTMLFieldSetElement>(parent->GetNode()) &&
         LayoutFieldset::FindInFlowLegend(*parent_layout_block) == this;
}

bool LayoutObject::IsListMarkerForSummary() const {
  if (!IsListMarkerIncludingAll())
    return false;
  if (const auto* summary =
          DynamicTo<HTMLSummaryElement>(Parent()->GetNode())) {
    if (!summary->IsMainSummary())
      return false;
    if (ListMarker::GetListStyleCategory(GetDocument(), StyleRef()) !=
        ListMarker::ListStyleCategory::kSymbol)
      return false;
    const AtomicString& name =
        StyleRef().ListStyleType()->GetCounterStyleName();
    return name == "disclosure-open" || name == "disclosure-closed";
  }
  return false;
}

bool LayoutObject::IsInListMarker() const {
  // List markers are either leaf nodes (legacy LayoutListMarker), or have
  // exactly one leaf child. So there's no need to traverse ancestors.
  return Parent() && Parent()->IsListMarkerIncludingAll();
}

LayoutObject* LayoutObject::NonCulledParent() const {
  LayoutObject* parent = Parent();
  for (; parent; parent = parent->Parent()) {
    if (const auto* parent_inline_box = DynamicTo<LayoutInline>(parent)) {
      if (!parent_inline_box->AlwaysCreateLineBoxesForLayoutInline())
        continue;
    }
    return parent;
  }
  return nullptr;
}

LayoutObject* LayoutObject::NextInPreOrderAfterChildren() const {
  NOT_DESTROYED();
  LayoutObject* o = NextSibling();
  if (!o) {
    o = Parent();
    while (o && !o->NextSibling())
      o = o->Parent();
    if (o)
      o = o->NextSibling();
  }

  return o;
}

LayoutObject* LayoutObject::NextInPreOrder(
    const LayoutObject* stay_within) const {
  NOT_DESTROYED();
  if (LayoutObject* o = SlowFirstChild())
    return o;

  return NextInPreOrderAfterChildren(stay_within);
}

LayoutObject* LayoutObject::PreviousInPostOrder(
    const LayoutObject* stay_within) const {
  NOT_DESTROYED();
  if (LayoutObject* o = SlowLastChild())
    return o;

  return PreviousInPostOrderBeforeChildren(stay_within);
}

LayoutObject* LayoutObject::NextInPreOrderAfterChildren(
    const LayoutObject* stay_within) const {
  NOT_DESTROYED();
  if (this == stay_within)
    return nullptr;

  const LayoutObject* current = this;
  LayoutObject* next = current->NextSibling();
  for (; !next; next = current->NextSibling()) {
    current = current->Parent();
    if (!current || current == stay_within)
      return nullptr;
  }
  return next;
}

LayoutObject* LayoutObject::PreviousInPostOrderBeforeChildren(
    const LayoutObject* stay_within) const {
  NOT_DESTROYED();
  if (this == stay_within)
    return nullptr;

  const LayoutObject* current = this;
  LayoutObject* previous = current->PreviousSibling();
  for (; !previous; previous = current->PreviousSibling()) {
    current = current->Parent();
    if (!current || current == stay_within)
      return nullptr;
  }
  return previous;
}

LayoutObject* LayoutObject::PreviousInPreOrder() const {
  NOT_DESTROYED();
  if (LayoutObject* o = PreviousSibling()) {
    while (LayoutObject* last_child = o->SlowLastChild())
      o = last_child;
    return o;
  }

  return Parent();
}

LayoutObject* LayoutObject::PreviousInPreOrder(
    const LayoutObject* stay_within) const {
  NOT_DESTROYED();
  if (this == stay_within)
    return nullptr;

  return PreviousInPreOrder();
}

wtf_size_t LayoutObject::Depth() const {
  wtf_size_t depth = 0;
  for (const LayoutObject* object = this; object; object = object->Parent())
    ++depth;
  return depth;
}

LayoutObject* LayoutObject::CommonAncestor(const LayoutObject& other,
                                           CommonAncestorData* data) const {
  if (this == &other)
    return const_cast<LayoutObject*>(this);

  const wtf_size_t depth = Depth();
  const wtf_size_t other_depth = other.Depth();
  const LayoutObject* iterator = this;
  const LayoutObject* other_iterator = &other;
  const LayoutObject* last = nullptr;
  const LayoutObject* other_last = nullptr;
  if (depth > other_depth) {
    for (wtf_size_t i = depth - other_depth; i; --i) {
      last = iterator;
      iterator = iterator->Parent();
    }
  } else if (other_depth > depth) {
    for (wtf_size_t i = other_depth - depth; i; --i) {
      other_last = other_iterator;
      other_iterator = other_iterator->Parent();
    }
  }
  while (iterator) {
    DCHECK(other_iterator);
    if (iterator == other_iterator) {
      if (data) {
        data->last = const_cast<LayoutObject*>(last);
        data->other_last = const_cast<LayoutObject*>(other_last);
      }
      return const_cast<LayoutObject*>(iterator);
    }
    last = iterator;
    iterator = iterator->Parent();
    other_last = other_iterator;
    other_iterator = other_iterator->Parent();
  }
  DCHECK(!other_iterator);
  return nullptr;
}

bool LayoutObject::IsBeforeInPreOrder(const LayoutObject& other) const {
  DCHECK_NE(this, &other);
  CommonAncestorData data;
  const LayoutObject* common_ancestor = CommonAncestor(other, &data);
  DCHECK(common_ancestor);
  DCHECK(data.last || data.other_last);
  if (!data.last)
    return true;  // |this| is the ancestor of |other|.
  if (!data.other_last)
    return false;  // |other| is the ancestor of |this|.
  for (const LayoutObject* child = common_ancestor->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child == data.last)
      return true;
    if (child == data.other_last)
      return false;
  }
  NOTREACHED();
  return false;
}

LayoutObject* LayoutObject::LastLeafChild() const {
  NOT_DESTROYED();
  LayoutObject* r = SlowLastChild();
  while (r) {
    LayoutObject* n = nullptr;
    n = r->SlowLastChild();
    if (!n)
      break;
    r = n;
  }
  return r;
}

static void AddLayers(LayoutObject* obj,
                      PaintLayer* parent_layer,
                      LayoutObject*& new_object,
                      PaintLayer*& before_child) {
  if (obj->HasLayer()) {
    if (!before_child && new_object) {
      // We need to figure out the layer that follows newObject. We only do
      // this the first time we find a child layer, and then we update the
      // pointer values for newObject and beforeChild used by everyone else.
      before_child =
          new_object->Parent()->FindNextLayer(parent_layer, new_object);
      new_object = nullptr;
    }
    parent_layer->AddChild(To<LayoutBoxModelObject>(obj)->Layer(),
                           before_child);
    return;
  }

  for (LayoutObject* curr = obj->SlowFirstChild(); curr;
       curr = curr->NextSibling())
    AddLayers(curr, parent_layer, new_object, before_child);
}

void LayoutObject::AddLayers(PaintLayer* parent_layer) {
  NOT_DESTROYED();
  if (!parent_layer)
    return;

  LayoutObject* object = this;
  PaintLayer* before_child = nullptr;
  blink::AddLayers(this, parent_layer, object, before_child);
}

void LayoutObject::RemoveLayers(PaintLayer* parent_layer) {
  NOT_DESTROYED();
  if (!parent_layer)
    return;

  if (HasLayer()) {
    parent_layer->RemoveChild(To<LayoutBoxModelObject>(this)->Layer());
    return;
  }

  for (LayoutObject* curr = SlowFirstChild(); curr; curr = curr->NextSibling())
    curr->RemoveLayers(parent_layer);
}

void LayoutObject::MoveLayers(PaintLayer* old_parent, PaintLayer* new_parent) {
  NOT_DESTROYED();
  if (!new_parent)
    return;

  if (HasLayer()) {
    PaintLayer* layer = To<LayoutBoxModelObject>(this)->Layer();
    DCHECK_EQ(old_parent, layer->Parent());
    if (old_parent)
      old_parent->RemoveChild(layer);
    new_parent->AddChild(layer);
    return;
  }

  for (LayoutObject* curr = SlowFirstChild(); curr; curr = curr->NextSibling())
    curr->MoveLayers(old_parent, new_parent);
}

PaintLayer* LayoutObject::FindNextLayer(PaintLayer* parent_layer,
                                        LayoutObject* start_point,
                                        bool check_parent) {
  NOT_DESTROYED();
  // Error check the parent layer passed in. If it's null, we can't find
  // anything.
  if (!parent_layer)
    return nullptr;

  // Step 1: If our layer is a child of the desired parent, then return our
  // layer.
  PaintLayer* our_layer =
      HasLayer() ? To<LayoutBoxModelObject>(this)->Layer() : nullptr;
  if (our_layer && our_layer->Parent() == parent_layer)
    return our_layer;

  // Step 2: If we don't have a layer, or our layer is the desired parent, then
  // descend into our siblings trying to find the next layer whose parent is the
  // desired parent.
  if (!our_layer || our_layer == parent_layer) {
    for (LayoutObject* curr = start_point ? start_point->NextSibling()
                                          : SlowFirstChild();
         curr; curr = curr->NextSibling()) {
      PaintLayer* next_layer =
          curr->FindNextLayer(parent_layer, nullptr, false);
      if (next_layer)
        return next_layer;
    }
  }

  // Step 3: If our layer is the desired parent layer, then we're finished. We
  // didn't find anything.
  if (parent_layer == our_layer)
    return nullptr;

  // Step 4: If |checkParent| is set, climb up to our parent and check its
  // siblings that follow us to see if we can locate a layer.
  if (check_parent && Parent())
    return Parent()->FindNextLayer(parent_layer, this, true);

  return nullptr;
}

PaintLayer* LayoutObject::EnclosingLayer() const {
  NOT_DESTROYED();
  for (const LayoutObject* current = this; current;
       current = current->Parent()) {
    if (current->HasLayer())
      return To<LayoutBoxModelObject>(current)->Layer();
  }
  // TODO(crbug.com/365897): we should get rid of detached layout subtrees, at
  // which point this code should not be reached.
  return nullptr;
}

PaintLayer* LayoutObject::PaintingLayer(int max_depth) const {
  NOT_DESTROYED();
  auto FindContainer = [](const LayoutObject& object) -> const LayoutObject* {
    // Column spanners paint through their multicolumn containers which can
    // be accessed through the associated out-of-flow placeholder's parent.
    if (object.IsColumnSpanAll())
      return object.SpannerPlaceholder();
    // Use ContainingBlock() instead of Parent() for floating objects to omit
    // any self-painting layers of inline objects that don't paint the floating
    // object. This is only needed for inline-level floats not managed by
    // LayoutNG. LayoutNG floats are painted by the correct painting layer.
    if (object.IsFloating() && !object.IsInLayoutNGInlineFormattingContext())
      return object.ContainingBlock();
    if (IsA<LayoutView>(object))
      return object.GetFrame()->OwnerLayoutObject();
    return object.Parent();
  };
  int depth = 0;
  for (const LayoutObject* current = this; current;
       current = FindContainer(*current)) {
    if (max_depth != -1 && ++depth > max_depth) {
      break;
    }
    if (current->HasLayer() &&
        To<LayoutBoxModelObject>(current)->Layer()->IsSelfPaintingLayer())
      return To<LayoutBoxModelObject>(current)->Layer();
  }
  // TODO(crbug.com/365897): we should get rid of detached layout subtrees, at
  // which point this code should not be reached.
  return nullptr;
}

bool LayoutObject::IsFixedPositionObjectInPagedMedia() const {
  NOT_DESTROYED();
  if (StyleRef().GetPosition() != EPosition::kFixed)
    return false;
  LayoutView* view = View();
  return Container() == view && view->PageLogicalHeight() &&
         // TODO(crbug.com/619094): Figure out the correct behaviour for fixed
         // position objects in paged media with vertical writing modes.
         view->IsHorizontalWritingMode();
}

LayoutBox* LayoutObject::EnclosingBox() const {
  NOT_DESTROYED();
  LayoutObject* curr = const_cast<LayoutObject*>(this);
  while (curr) {
    if (curr->IsBox())
      return To<LayoutBox>(curr);
    curr = curr->Parent();
  }

  NOTREACHED();
  return nullptr;
}

LayoutBlockFlow* LayoutObject::FragmentItemsContainer() const {
  NOT_DESTROYED();
  DCHECK(!IsOutOfFlowPositioned());
  auto* block_flow = DynamicTo<LayoutBlockFlow>(ContainingNGBox());
  if (!block_flow || !block_flow->IsLayoutNGObject())
    return nullptr;
#if EXPENSIVE_DCHECKS_ARE_ON()
  // Make sure that we don't skip blocks in the ancestry chain (which might
  // happen if there are out-of-flow positioned objects, for instance). In this
  // method we don't want to escape the enclosing inline formatting context.
  for (const LayoutObject* walker = Parent(); walker != block_flow;
       walker = walker->Parent())
    DCHECK(!walker->IsLayoutBlock());
#endif
  return block_flow;
}

LayoutBox* LayoutObject::ContainingNGBox() const {
  NOT_DESTROYED();
  if (RuntimeEnabledFeatures::LayoutMediaNGContainerEnabled() && Parent() &&
      Parent()->IsMedia())
    return To<LayoutBox>(Parent());
  LayoutBlock* containing_block = ContainingBlock();
  if (!containing_block)
    return nullptr;
  // Flow threads should be invisible to LayoutNG, so skip to the multicol
  // container.
  if (UNLIKELY(containing_block->IsLayoutFlowThread()))
    containing_block = To<LayoutBlockFlow>(containing_block->Parent());
  if (!containing_block->IsLayoutNGObject())
    return nullptr;
  return containing_block;
}

LayoutBlock* LayoutObject::ContainingFragmentationContextRoot() const {
  NOT_DESTROYED();
  if (!MightBeInsideFragmentationContext())
    return nullptr;
  bool found_column_spanner = IsColumnSpanAll();
  for (LayoutBlock* ancestor = ContainingBlock(); ancestor;
       ancestor = ancestor->ContainingBlock()) {
    if (ancestor->IsFragmentationContextRoot()) {
      // Column spanners do not participate in the fragmentation context
      // of their nearest fragmentation context, but rather the next above,
      // if there is one.
      if (found_column_spanner)
        return ancestor->ContainingFragmentationContextRoot();
      return ancestor;
    }
    if (ancestor->IsColumnSpanAll())
      found_column_spanner = true;
  }
  return nullptr;
}

bool LayoutObject::IsFirstInlineFragmentSafe() const {
  NOT_DESTROYED();
  DCHECK(IsInline());
  LayoutBlockFlow* block_flow = FragmentItemsContainer();
  return block_flow && !block_flow->NeedsLayout();
}

LayoutFlowThread* LayoutObject::LocateFlowThreadContainingBlock() const {
  NOT_DESTROYED();
  DCHECK(IsInsideFlowThread());

  // See if we have the thread cached because we're in the middle of layout.
  if (LayoutView* view = View()) {
    if (LayoutState* layout_state = view->GetLayoutState()) {
      if (LayoutFlowThread* flow_thread = layout_state->FlowThread())
        return flow_thread;
    }
  }

  // Not in the middle of layout so have to find the thread the slow way.
  return LayoutFlowThread::LocateFlowThreadContainingBlockOf(
      *this, LayoutFlowThread::kAnyAncestor);
}

static inline bool ObjectIsRelayoutBoundary(const LayoutObject* object) {
  // FIXME: In future it may be possible to broaden these conditions in order to
  // improve performance.

  // Positioned objects always have self-painting layers and are safe to use as
  // relayout boundaries.
  bool is_svg_root = object->IsSVGRoot();
  bool has_self_painting_layer =
      object->HasLayer() &&
      To<LayoutBoxModelObject>(object)->HasSelfPaintingLayer();
  if (!has_self_painting_layer && !is_svg_root)
    return false;

  // LayoutInline can't be relayout roots since LayoutBlockFlow is responsible
  // for layouting them.
  if (object->IsLayoutInline())
    return false;

  // Table parts can't be relayout roots since the table is responsible for
  // layouting all the parts.
  if (object->IsTablePart())
    return false;

  // OOF-positioned objects which rely on their static-position for placement
  // cannot be relayout boundaries (their final position would be incorrect).
  const ComputedStyle* style = object->Style();
  if (object->IsOutOfFlowPositioned() &&
      (style->HasAutoLeftAndRight() || style->HasAutoTopAndBottom()))
    return false;

  if (const auto* layout_box = DynamicTo<LayoutBox>(object)) {
    // In general we can't relayout a flex item independently of its container;
    // not only is the result incorrect due to the override size that's set, it
    // also messes with the cached main size on the flexbox.
    if (layout_box->IsFlexItemIncludingNG())
      return false;

    // Similarly to flex items, we can't relayout a grid item independently of
    // its container. This also applies to out of flow items of the grid, as we
    // need the cached information of the grid to recompute the out of flow
    // item's containing block rect.
    if (layout_box->ContainingBlock()->IsLayoutGridIncludingNG())
      return false;

    if (const NGLayoutResult* layout_result =
            layout_box->GetCachedLayoutResult(nullptr)) {
      const NGPhysicalFragment& fragment = layout_result->PhysicalFragment();

      // Fragmented nodes cannot be relayout roots.
      if (fragment.BreakToken())
        return false;

      // In LayoutNG, if box has any OOF descendants, they are propagated to
      // parent. Therefore, we must mark parent chain for layout.
      if (fragment.HasOutOfFlowPositionedDescendants())
        return false;

      // Anchor queries should be propagated across the layout boundaries, even
      // when `contain: strict` is explicitly set.
      if (fragment.HasAnchorQuery())
        return false;
    }

    // A box which doesn't establish a new formating context can pass a whole
    // bunch of state (floats, margins) to an arbitrary sibling, causing that
    // sibling to position/size differently.
    if (!layout_box->CreatesNewFormattingContext())
      return false;
  }

  // MathML subtrees can't be relayout roots because of the embellished operator
  // and space-like logic.
  if (object->IsMathML() && !object->IsMathMLRoot())
    return false;

  if (object->ShouldApplyLayoutContainment() &&
      object->ShouldApplySizeContainment()) {
    return true;
  }

  // SVG roots are sufficiently self-contained to be a relayout boundary, even
  // if their size is non-fixed.
  if (is_svg_root)
    return true;

  // If either dimension is percent-based, intrinsic, or anything but fixed,
  // this object cannot form a re-layout boundary. A non-fixed computed logical
  // height will allow the object to grow and shrink based on the content
  // inside. The same goes for for logical width, if this objects is inside a
  // shrink-to-fit container, for instance.
  if (!style->Width().IsFixed() || !style->Height().IsFixed())
    return false;

  if (object->IsTextControlIncludingNG())
    return true;

  if (!object->ShouldClipOverflowAlongBothAxis())
    return false;

  // Scrollbar parts can be removed during layout. Avoid the complexity of
  // having to deal with that.
  if (object->IsLayoutCustomScrollbarPart())
    return false;

  // Inside multicol it's generally problematic to allow relayout roots. The
  // multicol container itself may be scheduled for relayout as well (due to
  // other changes that may have happened since the previous layout pass),
  // which might affect the column heights, which may affect how this object
  // breaks across columns). Spanners may also have been added or removed since
  // the previous layout pass, which is just another way of affecting the column
  // heights (and the number of rows). Instead of identifying cases where it's
  // safe to allow relayout roots, just disallow them inside multicol.
  if (object->IsInsideFlowThread())
    return false;

  return true;
}

// Mark this object needing to re-run |CollectInlines()|.
//
// The flag is propagated to its container so that NGInlineNode that contains
// |this| is marked too. When |this| is a container, the propagation stops at
// |this|. When invalidating on inline blocks, floats, or OOF, caller need to
// pay attention whether it should mark its inner context or outer.
void LayoutObject::SetNeedsCollectInlines() {
  NOT_DESTROYED();
  if (NeedsCollectInlines())
    return;

  if (UNLIKELY(IsSVGChild() && !IsNGSVGText() && !IsSVGInline() &&
               !IsSVGInlineText() && !IsNGSVGForeignObject()))
    return;

  // Don't mark |LayoutFlowThread| because |CollectInlines()| skips them.
  if (!IsLayoutFlowThread())
    SetNeedsCollectInlines(true);

  if (LayoutObject* parent = Parent())
    parent->SetChildNeedsCollectInlines();
}

void LayoutObject::SetChildNeedsCollectInlines() {
  NOT_DESTROYED();
  LayoutObject* object = this;
  do {
    // Should not stop at |LayoutFlowThread| as |CollectInlines()| skips them.
    if (UNLIKELY(object->IsLayoutFlowThread())) {
      object = object->Parent();
      continue;
    }
    if (object->NeedsCollectInlines())
      break;
    object->SetNeedsCollectInlines(true);

    // Stop marking at the inline formatting context root. This is usually a
    // |LayoutBlockFlow|, but some other classes can have children; e.g.,
    // |LayoutButton| or |LayoutSVGRoot|. |LayoutInline| is the only class we
    // collect recursively (see |CollectInlines|). Use the same condition here.
    if (!object->IsLayoutInline())
      break;

    object = object->Parent();
  } while (object);
}

void LayoutObject::MarkContainerChainForLayout(bool schedule_relayout,
                                               SubtreeLayoutScope* layouter) {
  NOT_DESTROYED();
#if DCHECK_IS_ON()
  DCHECK(!IsSetNeedsLayoutForbidden());
  DCHECK(!GetDocument().InPostLifecycleSteps());
#endif
  DCHECK(!layouter || this != layouter->Root());
  // When we're in layout, we're marking a descendant as needing layout with
  // the intention of visiting it during this layout. We shouldn't be
  // scheduling it to be laid out later. Also, scheduleRelayout() must not be
  // called while iterating LocalFrameView::layout_subtree_root_list_.
  schedule_relayout &= !GetFrameView()->IsInPerformLayout();

  LayoutObject* object = Container();
  LayoutObject* last = this;

  bool simplified_normal_flow_layout = NeedsSimplifiedNormalFlowLayout() &&
                                       !SelfNeedsLayout() &&
                                       !NormalChildNeedsLayout();

  while (object) {
    if (object->SelfNeedsLayout())
      return;

    // Note that if the last element we processed was blocked by a display lock,
    // and the reason we're propagating a change is that a subtree needed layout
    // (ie |last| doesn't need either self layout or positioned movement
    // layout), then we can return and stop the dirty bit propagation. Note that
    // it's not enough to check |object|, since the element that is actually
    // locked needs its child bits set properly, we need to go one more
    // iteration after that.
    if (!last->SelfNeedsLayout() && !last->NeedsPositionedMovementLayout() &&
        last->ChildLayoutBlockedByDisplayLock()) {
      return;
    }

    // Don't mark the outermost object of an unrooted subtree. That object will
    // be marked when the subtree is added to the document.
    LayoutObject* container = object->Container();
    if (!container && !IsA<LayoutView>(object))
      return;
    if (!last->IsTextOrSVGChild() && last->StyleRef().HasOutOfFlowPosition()) {
      object = last->ContainingBlock();
      if (object->PosChildNeedsLayout())
        return;
      container = object->Container();
      object->SetPosChildNeedsLayout(true);
      simplified_normal_flow_layout = true;
    } else if (simplified_normal_flow_layout) {
      if (object->NeedsSimplifiedNormalFlowLayout())
        return;
      object->SetNeedsSimplifiedNormalFlowLayout(true);
    } else {
      if (object->NormalChildNeedsLayout())
        return;
      object->SetNormalChildNeedsLayout(true);
    }
#if DCHECK_IS_ON()
    DCHECK(!object->IsSetNeedsLayoutForbidden());
#endif

    object->MarkSelfPaintingLayerForVisualOverflowRecalc();

    if (layouter) {
      layouter->RecordObjectMarkedForLayout(object);

      if (object == layouter->Root()) {
        if (auto* painting_layer = PaintingLayer())
          painting_layer->SetNeedsVisualOverflowRecalc();
        return;
      }
    }

    last = object;
    if (schedule_relayout && ObjectIsRelayoutBoundary(last))
      break;
    object = container;
  }

  if (schedule_relayout)
    last->ScheduleRelayout();
}

// LayoutNG has different OOF-positioned handling compared to the existing
// layout system. To correctly determine the static-position of the object,
// LayoutNG "bubbles" up the static-position inside the NGLayoutResult.
// See: |NGLayoutResult::OutOfFlowPositionedDescendants()|.
//
// Column spanners also have a bubbling mechanism, and therefore also need to
// mark ancestors between the element itself and the containing block (the
// multicol container).
//
// Whenever an OOF-positioned object is added/removed we need to invalidate
// layout for all the layout objects which may have stored a NGLayoutResult
// with this object contained in that list.
//
// In the future it may be possible to optimize this, e.g.
//  - For the removal case, add a pass which modifies the layout result to
//    remove the OOF-positioned descendant.
//  - For the adding case, if the OOF-positioned doesn't require a
//    static-position, simply insert the object up the NGLayoutResult chain with
//    an invalid static-position.
void LayoutObject::MarkParentForSpannerOrOutOfFlowPositionedChange() {
  NOT_DESTROYED();
#if DCHECK_IS_ON()
  DCHECK(!IsSetNeedsLayoutForbidden());
  DCHECK(!GetDocument().InPostLifecycleSteps());
#endif

  LayoutObject* object = Parent();
  if (!object)
    return;

  // As OOF-positioned objects are represented as an object replacement
  // character in the inline items list. We need to ensure we collect the
  // inline items again to either collect or drop the OOF-positioned object.
  //
  // Note that this isn't necessary if we're dealing with a column spanner here,
  // but in order to keep things simple, we'll make no difference.
  object->SetNeedsCollectInlines();

  const LayoutBlock* containing_block = ContainingBlock();
  while (object != containing_block) {
    object->SetChildNeedsLayout(kMarkOnlyThis);
    object = object->Parent();
  }
  // Finally mark the parent block for layout. This will mark everything which
  // has an OOF-positioned object or column spanner in a NGLayoutResult as
  // needing layout.
  if (object)
    object->SetChildNeedsLayout();
}

#if DCHECK_IS_ON()
void LayoutObject::CheckBlockPositionedObjectsNeedLayout() {
  NOT_DESTROYED();
  if (ChildLayoutBlockedByDisplayLock())
    return;
  DCHECK(!NeedsLayout());

  auto* layout_block = DynamicTo<LayoutBlock>(this);
  if (layout_block)
    layout_block->CheckPositionedObjectsNeedLayout();
}
#endif

void LayoutObject::SetIntrinsicLogicalWidthsDirty(
    MarkingBehavior mark_parents) {
  NOT_DESTROYED();
  bitfields_.SetIntrinsicLogicalWidthsDirty(true);
  bitfields_.SetIntrinsicLogicalWidthsDependsOnBlockConstraints(true);
  bitfields_.SetIntrinsicLogicalWidthsChildDependsOnBlockConstraints(true);
  if (mark_parents == kMarkContainerChain &&
      (IsText() || !StyleRef().HasOutOfFlowPosition()))
    InvalidateContainerIntrinsicLogicalWidths();
}

void LayoutObject::ClearIntrinsicLogicalWidthsDirty() {
  NOT_DESTROYED();
  bitfields_.SetIntrinsicLogicalWidthsDirty(false);
}

bool LayoutObject::IsFontFallbackValid() const {
  NOT_DESTROYED();
  return StyleRef().GetFont().IsFallbackValid() &&
         FirstLineStyle()->GetFont().IsFallbackValid();
}

void LayoutObject::InvalidateSubtreeLayoutForFontUpdates() {
  NOT_DESTROYED();
  if (!IsFontFallbackValid()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kFontsChanged);
  }
  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    child->InvalidateSubtreeLayoutForFontUpdates();
  }
}

void LayoutObject::InvalidateIntersectionObserverCachedRects() {
  NOT_DESTROYED();
  if (GetNode() && GetNode()->IsElementNode()) {
    if (auto* data = To<Element>(GetNode())->IntersectionObserverData()) {
      data->InvalidateCachedRects();
    }
  }
}

static inline bool NGKeepInvalidatingBeyond(LayoutObject* o) {
  // Because LayoutNG does not work on individual inline objects, we can't
  // use a dirty width on an inline as a signal that it is safe to stop --
  // inlines never get marked as clean. Instead, we need to keep going to the
  // next block container.
  // Atomic inlines do not have this problem as they are treated like blocks
  // in this context.
  // There's a similar issue for flow thread objects, as they are invisible to
  // LayoutNG.
  if (o->IsLayoutInline() || o->IsText() || o->IsLayoutFlowThread())
    return true;
  return false;
}

inline void LayoutObject::InvalidateContainerIntrinsicLogicalWidths() {
  NOT_DESTROYED();
  // In order to avoid pathological behavior when inlines are deeply nested, we
  // do include them in the chain that we mark dirty (even though they're kind
  // of irrelevant).
  auto IntrinsicContainer = [](const LayoutObject* current) -> LayoutObject* {
    // Table cell intrinsic logical-widths are queried directly from a <table>
    // rather than from their parents (sections or rows). Skip these when
    // invalidating.
    if (current->IsTableCell()) {
      if (current->IsTableCellLegacy())
        return current->ContainingBlock();
      else
        return To<LayoutNGTableCell>(current)->Table();
    }
    return current->Container();
  };

  LayoutObject* o = IntrinsicContainer(this);
  while (o &&
         (!o->IntrinsicLogicalWidthsDirty() || NGKeepInvalidatingBeyond(o))) {
    // Don't invalidate the outermost object of an unrooted subtree. That object
    // will be invalidated when the subtree is added to the document.
    LayoutObject* container = IntrinsicContainer(o);
    if (!container && !IsA<LayoutView>(o))
      break;

    o->bitfields_.SetIntrinsicLogicalWidthsDirty(true);
    // A positioned object has no effect on the min/max width of its containing
    // block ever. We can optimize this case and not go up any further.
    if (o->StyleRef().HasOutOfFlowPosition())
      break;
    o = container;
  }
}

LayoutObject* LayoutObject::ContainerForAbsolutePosition(
    AncestorSkipInfo* skip_info) const {
  NOT_DESTROYED();
  return FindAncestorByPredicate(this, skip_info, [](LayoutObject* candidate) {
    return candidate->CanContainAbsolutePositionObjects();
  });
}

LayoutObject* LayoutObject::ContainerForFixedPosition(
    AncestorSkipInfo* skip_info) const {
  NOT_DESTROYED();
  DCHECK(!IsText());
  return FindAncestorByPredicate(this, skip_info, [](LayoutObject* candidate) {
    return candidate->CanContainFixedPositionObjects();
  });
}

LayoutBlock* LayoutObject::ContainingBlockForAbsolutePosition(
    AncestorSkipInfo* skip_info) const {
  NOT_DESTROYED();
  auto* container = ContainerForAbsolutePosition(skip_info);
  return FindNonAnonymousContainingBlock(container, skip_info);
}

LayoutBlock* LayoutObject::ContainingBlockForFixedPosition(
    AncestorSkipInfo* skip_info) const {
  NOT_DESTROYED();
  auto* container = ContainerForFixedPosition(skip_info);
  return FindNonAnonymousContainingBlock(container, skip_info);
}

const LayoutBlock* LayoutObject::InclusiveContainingBlock() const {
  NOT_DESTROYED();
  auto* layout_block = DynamicTo<LayoutBlock>(this);
  return layout_block ? layout_block : ContainingBlock();
}

const LayoutBox* LayoutObject::ContainingScrollContainer() const {
  NOT_DESTROYED();
  if (auto* layer = EnclosingLayer()) {
    if (auto* box = layer->GetLayoutBox()) {
      if (box != this && box->IsScrollContainer())
        return box;
    }
    if (auto* scroll_container_layer = layer->ContainingScrollContainerLayer())
      return scroll_container_layer->GetLayoutBox();
  }
  return nullptr;
}

LayoutObject* LayoutObject::NonAnonymousAncestor() const {
  NOT_DESTROYED();
  LayoutObject* ancestor = Parent();
  while (ancestor && ancestor->IsAnonymous())
    ancestor = ancestor->Parent();
  return ancestor;
}

LayoutObject* LayoutObject::NearestAncestorForElement() const {
  NOT_DESTROYED();
  LayoutObject* ancestor = Parent();
  while (ancestor && !ancestor->IsForElement())
    ancestor = ancestor->Parent();
  return ancestor;
}

bool LayoutObject::IsAnonymousNGMulticolInlineWrapper() const {
  NOT_DESTROYED();
  if (!IsLayoutNGBlockFlow() || !IsAnonymousBlock())
    return false;

  const LayoutBox* container = ContainingNGBox();
  if (!container)
    return false;

  return container->IsFragmentationContextRoot();
}

LayoutBlock* LayoutObject::FindNonAnonymousContainingBlock(
    LayoutObject* container,
    AncestorSkipInfo* skip_info) {
  // For inlines, we return the nearest non-anonymous enclosing
  // block. We don't try to return the inline itself. This allows us to avoid
  // having a positioned objects list in all LayoutInlines and lets us return a
  // strongly-typed LayoutBlock* result from this method. The
  // LayoutObject::Container() method can actually be used to obtain the inline
  // directly.
  if (container && !container->IsLayoutBlock())
    container = container->ContainingBlock(skip_info);

  // Allow an NG anonymous wrapper of an inline to be the containing block if it
  // is the direct child of a multicol. This avoids the multicol from
  // incorrectly becoming the containing block in the case of an inline
  // container.
  while (container && container->IsAnonymousBlock() &&
         !container->IsAnonymousNGMulticolInlineWrapper())
    container = container->ContainingBlock(skip_info);

  return DynamicTo<LayoutBlock>(container);
}

bool LayoutObject::ComputeIsFixedContainer(const ComputedStyle* style) const {
  NOT_DESTROYED();
  if (!style)
    return false;
  bool is_document_element = IsDocumentElement();
  // https://www.w3.org/TR/filter-effects-1/#FilterProperty
  if (!is_document_element && style->HasNonInitialFilter())
    return true;
  // Backdrop-filter creates a containing block for fixed and absolute
  // positioned elements:
  // https://drafts.fxtf.org/filter-effects-2/#backdrop-filter-operation
  if (!is_document_element && style->HasNonInitialBackdropFilter())
    return true;
  // The LayoutView is always a container of fixed positioned descendants. In
  // addition, SVG foreignObjects become such containers, so that descendants
  // of a foreignObject cannot escape it. Similarly, text controls let authors
  // select elements inside that are created by user agent shadow DOM, and we
  // have (C++) code that assumes that the elements are indeed contained by the
  // text control. So just make sure this is the case.
  if (IsA<LayoutView>(this) || IsSVGForeignObjectIncludingNG() ||
      IsTextControlIncludingNG())
    return true;

  // crbug.com/1153042: If <fieldset> is a fixed container, its anonymous
  // content box should be a fixed container.
  if (IsAnonymous() && Parent() && Parent()->IsLayoutNGFieldset() &&
      Parent()->CanContainFixedPositionObjects()) {
    return true;
  }

  // https://www.w3.org/TR/css-transforms-1/#containing-block-for-all-descendants

  // For transform-style specifically, we want to consider the computed
  // value rather than the used value.
  if (style->HasTransformRelatedProperty() ||
      style->TransformStyle3D() == ETransformStyle3D::kPreserve3d) {
    if (!IsInline() || IsAtomicInlineLevel())
      return true;
  }
  // https://www.w3.org/TR/css-contain-1/#containment-layout
  if (IsEligibleForPaintOrLayoutContainment() &&
      (ShouldApplyPaintContainment(*style) ||
       ShouldApplyLayoutContainment(*style) ||
       style->WillChangeProperties().Contains(CSSPropertyID::kContain)))
    return true;

  return false;
}

bool LayoutObject::ComputeIsAbsoluteContainer(
    const ComputedStyle* style) const {
  NOT_DESTROYED();
  if (!style)
    return false;
  return style->CanContainAbsolutePositionObjects() ||
         ComputeIsFixedContainer(style) ||
         // crbug.com/1153042: If <fieldset> is an absolute container, its
         // anonymous content box should be an absolute container.
         (IsAnonymous() && Parent() && Parent()->IsLayoutNGFieldset() &&
          Parent()->StyleRef().CanContainAbsolutePositionObjects());
}

gfx::RectF LayoutObject::AbsoluteBoundingBoxRectF(
    MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  DCHECK(!(flags & kIgnoreTransforms));
  Vector<gfx::QuadF> quads;
  AbsoluteQuads(quads, flags);

  wtf_size_t n = quads.size();
  if (n == 0)
    return gfx::RectF();

  gfx::RectF result = quads[0].BoundingBox();
  for (wtf_size_t i = 1; i < n; ++i)
    result.Union(quads[i].BoundingBox());
  return result;
}

gfx::Rect LayoutObject::AbsoluteBoundingBoxRect(
    MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  DCHECK(!(flags & kIgnoreTransforms));
  Vector<gfx::QuadF> quads;
  AbsoluteQuads(quads, flags);

  wtf_size_t n = quads.size();
  if (!n)
    return gfx::Rect();

  gfx::RectF result;
  for (auto& quad : quads)
    result.Union(quad.BoundingBox());
  return gfx::ToEnclosingRect(result);
}

PhysicalRect LayoutObject::AbsoluteBoundingBoxRectHandlingEmptyInline(
    MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  return PhysicalRect::EnclosingRect(AbsoluteBoundingBoxRectF(flags));
}

PhysicalRect LayoutObject::AbsoluteBoundingBoxRectForScrollIntoView() const {
  NOT_DESTROYED();
  // Ignore sticky position offsets for the purposes of scrolling elements into
  // view. See https://www.w3.org/TR/css-position-3/#stickypos-scroll for
  // details

  const MapCoordinatesFlags flag =
      (RuntimeEnabledFeatures::CSSPositionStickyStaticScrollPositionEnabled())
          ? kIgnoreStickyOffset
          : 0;

  PhysicalRect rect = AbsoluteBoundingBoxRectHandlingEmptyInline(flag);
  const auto& style = StyleRef();
  rect.ExpandEdges(LayoutUnit(style.ScrollMarginTop()),
                   LayoutUnit(style.ScrollMarginRight()),
                   LayoutUnit(style.ScrollMarginBottom()),
                   LayoutUnit(style.ScrollMarginLeft()));
  return rect;
}

void LayoutObject::AddAbsoluteRectForLayer(gfx::Rect& result) {
  NOT_DESTROYED();
  if (HasLayer())
    result.Union(AbsoluteBoundingBoxRect());
  for (LayoutObject* current = SlowFirstChild(); current;
       current = current->NextSibling())
    current->AddAbsoluteRectForLayer(result);
}

gfx::Rect LayoutObject::AbsoluteBoundingBoxRectIncludingDescendants() const {
  NOT_DESTROYED();
  gfx::Rect result = AbsoluteBoundingBoxRect();
  for (LayoutObject* current = SlowFirstChild(); current;
       current = current->NextSibling())
    current->AddAbsoluteRectForLayer(result);
  return result;
}

void LayoutObject::Paint(const PaintInfo&) const {
  NOT_DESTROYED();
}

RecalcLayoutOverflowResult LayoutObject::RecalcLayoutOverflow() {
  NOT_DESTROYED();
  ClearSelfNeedsLayoutOverflowRecalc();
  if (!ChildNeedsLayoutOverflowRecalc())
    return RecalcLayoutOverflowResult();

  ClearChildNeedsLayoutOverflowRecalc();
  bool children_layout_overflow_changed = false;
  for (LayoutObject* current = SlowFirstChild(); current;
       current = current->NextSibling()) {
    children_layout_overflow_changed |=
        current->RecalcLayoutOverflow().layout_overflow_changed;
  }
  return {children_layout_overflow_changed, /* rebuild_fragment_tree */ false};
}

void LayoutObject::RecalcVisualOverflow() {
  NOT_DESTROYED();
  for (LayoutObject* current = SlowFirstChild(); current;
       current = current->NextSibling()) {
    if (current->HasLayer() &&
        To<LayoutBoxModelObject>(current)->HasSelfPaintingLayer())
      continue;
    current->RecalcVisualOverflow();
  }
}

void LayoutObject::RecalcNormalFlowChildVisualOverflowIfNeeded() {
  NOT_DESTROYED();
  if (IsOutOfFlowPositioned() ||
      (HasLayer() && To<LayoutBoxModelObject>(this)->HasSelfPaintingLayer()))
    return;
  RecalcVisualOverflow();
}

#if DCHECK_IS_ON()
void LayoutObject::InvalidateVisualOverflow() {
  if (auto* box = DynamicTo<LayoutBox>(this)) {
    for (const NGPhysicalBoxFragment& fragment : box->PhysicalFragments())
      fragment.GetMutableForPainting().InvalidateInkOverflow();
  }
  // For now, we can only check |LayoutBox| laid out by NG.
}
#endif

bool LayoutObject::HasDistortingVisualEffects() const {
  NOT_DESTROYED();
  // TODO(szager): Check occlusion information propagated from out-of-process
  // parent frame.

  auto& first_fragment = EnclosingLayer()->GetLayoutObject().FirstFragment();
  // This can happen for an iframe element which is outside the viewport and has
  // therefore never been painted. In that case, we do the safe thing -- report
  // it as having distorting visual effects.
  if (!first_fragment.HasLocalBorderBoxProperties())
    return true;
  auto paint_properties = first_fragment.LocalBorderBoxProperties();

  // No filters, no blends, no opacity < 100%.
  for (const auto* effect = &paint_properties.Effect().Unalias(); effect;
       effect = effect->UnaliasedParent()) {
    if (effect->HasRealEffects())
      return true;
  }

  auto& local_frame_root = GetDocument().GetFrame()->LocalFrameRoot();
  auto& root_fragment = local_frame_root.ContentLayoutObject()->FirstFragment();
  CHECK(root_fragment.HasLocalBorderBoxProperties());
  const auto& root_properties = root_fragment.LocalBorderBoxProperties();

  // The only allowed transforms are 2D translation and proportional up-scaling.
  gfx::Transform projection = GeometryMapper::SourceToDestinationProjection(
      paint_properties.Transform(), root_properties.Transform());
  if (!projection.Is2dProportionalUpscaleAndOr2dTranslation())
    return true;

  return false;
}

bool LayoutObject::HasNonZeroEffectiveOpacity() const {
  NOT_DESTROYED();
  const FragmentData& fragment =
      EnclosingLayer()->GetLayoutObject().FirstFragment();

  // This can happen for an iframe element which is outside the viewport and has
  // therefore never been painted. In that case, we do the safe thing -- report
  // it as having non-zero opacity -- since this method is used by
  // IntersectionObserver to detect occlusion.
  if (!fragment.HasLocalBorderBoxProperties())
    return true;

  const auto& paint_properties = fragment.LocalBorderBoxProperties();

  for (const auto* effect = &paint_properties.Effect().Unalias(); effect;
       effect = effect->UnaliasedParent()) {
    if (effect->Opacity() == 0.0)
      return false;
  }
  return true;
}

String LayoutObject::DecoratedName() const {
  NOT_DESTROYED();
  StringBuilder name;
  name.Append(GetName());

  if (IsAnonymous())
    name.Append(" (anonymous)");
  // FIXME: Remove the special case for LayoutView here (requires rebaseline of
  // all tests).
  if (IsOutOfFlowPositioned() && !IsA<LayoutView>(this))
    name.Append(" (positioned)");
  if (IsRelPositioned())
    name.Append(" (relative positioned)");
  if (IsStickyPositioned())
    name.Append(" (sticky positioned)");
  if (IsFloating())
    name.Append(" (floating)");
  if (SpannerPlaceholder())
    name.Append(" (column spanner)");

  return name.ToString();
}

String LayoutObject::DebugName() const {
  NOT_DESTROYED();
  StringBuilder name;
  name.Append(DecoratedName());

  if (const Node* node = GetNode()) {
    name.Append(' ');
    name.Append(node->DebugName());
  }
  return name.ToString();
}

DOMNodeId LayoutObject::OwnerNodeId() const {
  NOT_DESTROYED();
  return GetNode() ? DOMNodeIds::IdForNode(GetNode()) : kInvalidDOMNodeId;
}

void LayoutObject::InvalidateDisplayItemClients(
    PaintInvalidationReason reason) const {
  NOT_DESTROYED();
  // This default implementation invalidates only the object itself as a
  // DisplayItemClient.
  DCHECK(!GetSelectionDisplayItemClient());
  ObjectPaintInvalidator(*this).InvalidateDisplayItemClient(*this, reason);
}

PhysicalRect LayoutObject::AbsoluteSelectionRect() const {
  NOT_DESTROYED();
  PhysicalRect selection_rect = LocalSelectionVisualRect();
  if (!selection_rect.IsEmpty())
    MapToVisualRectInAncestorSpace(View(), selection_rect);

  if (LocalFrameView* frame_view = GetFrameView())
    return frame_view->DocumentToFrame(selection_rect);

  return selection_rect;
}

DISABLE_CFI_PERF
void LayoutObject::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  NOT_DESTROYED();
  ObjectPaintInvalidatorWithContext(*this, context).InvalidatePaint();
}

PhysicalRect LayoutObject::VisualRectInDocument(VisualRectFlags flags) const {
  NOT_DESTROYED();
  PhysicalRect rect = LocalVisualRect();
  MapToVisualRectInAncestorSpace(View(), rect, flags);
  return rect;
}

PhysicalRect LayoutObject::LocalVisualRectIgnoringVisibility() const {
  NOT_DESTROYED();
  NOTREACHED();
  return PhysicalRect();
}

bool LayoutObject::MapToVisualRectInAncestorSpaceInternalFastPath(
    const LayoutBoxModelObject* ancestor,
    PhysicalRect& rect,
    VisualRectFlags visual_rect_flags,
    bool& intersects) const {
  NOT_DESTROYED();
  intersects = true;
  if (!(visual_rect_flags & kUseGeometryMapper) || !ancestor ||
      !ancestor->FirstFragment().HasLocalBorderBoxProperties())
    return false;

  if (ancestor == this)
    return true;

  AncestorSkipInfo skip_info(ancestor);
  PropertyTreeState container_properties = PropertyTreeState::Uninitialized();
  const LayoutObject* property_container =
      GetPropertyContainer(&skip_info, &container_properties);
  if (!property_container)
    return false;

  // This works because it's not possible to have any intervening clips,
  // effects, transforms between |this| and |property_container|, and therefore
  // FirstFragment().PaintOffset() is relative to the transform space defined by
  // FirstFragment().LocalBorderBoxProperties() (if this == property_container)
  // or property_container->FirstFragment().ContentsProperties().
  rect.Move(FirstFragment().PaintOffset());
  if (property_container != ancestor) {
    FloatClipRect clip_rect((gfx::RectF(rect)));
    intersects = GeometryMapper::LocalToAncestorVisualRect(
        container_properties, ancestor->FirstFragment().ContentsProperties(),
        clip_rect, kIgnoreOverlayScrollbarSize,
        (visual_rect_flags & kEdgeInclusive) ? kInclusiveIntersect
                                             : kNonInclusiveIntersect);
    rect = PhysicalRect::EnclosingRect(clip_rect.Rect());
  }
  rect.offset -= ancestor->FirstFragment().PaintOffset();

  return true;
}

bool LayoutObject::MapToVisualRectInAncestorSpace(
    const LayoutBoxModelObject* ancestor,
    PhysicalRect& rect,
    VisualRectFlags visual_rect_flags) const {
  NOT_DESTROYED();
  bool intersects = true;
  if (MapToVisualRectInAncestorSpaceInternalFastPath(
          ancestor, rect, visual_rect_flags, intersects))
    return intersects;

  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 gfx::QuadF(gfx::RectF(rect)));
  intersects = MapToVisualRectInAncestorSpaceInternal(ancestor, transform_state,
                                                      visual_rect_flags);
  transform_state.Flatten();
  rect = PhysicalRect::EnclosingRect(
      transform_state.LastPlanarQuad().BoundingBox());
  return intersects;
}

bool LayoutObject::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags visual_rect_flags) const {
  NOT_DESTROYED();
  // For any layout object that doesn't override this method (the main example
  // is LayoutText), the rect is assumed to be in the parent's coordinate space,
  // except for container flip.

  if (ancestor == this)
    return true;

  if (LayoutObject* parent = Parent()) {
    if (parent->IsBox()) {
      bool preserve3d = parent->StyleRef().Preserves3D() && !parent->IsText();
      TransformState::TransformAccumulation accumulation =
          preserve3d ? TransformState::kAccumulateTransform
                     : TransformState::kFlattenTransform;

      if (parent != ancestor &&
          !To<LayoutBox>(parent)->MapContentsRectToBoxSpace(
              transform_state, accumulation, *this, visual_rect_flags))
        return false;
    }
    return parent->MapToVisualRectInAncestorSpaceInternal(
        ancestor, transform_state, visual_rect_flags);
  }
  return true;
}

const LayoutObject* LayoutObject::GetPropertyContainer(
    AncestorSkipInfo* skip_info,
    PropertyTreeStateOrAlias* container_properties) const {
  NOT_DESTROYED();
  const LayoutObject* property_container = this;
  while (!property_container->FirstFragment().HasLocalBorderBoxProperties()) {
    property_container = property_container->Container(skip_info);
    if (!property_container || (skip_info && skip_info->AncestorSkipped()) ||
        property_container->FirstFragment().NextFragment())
      return nullptr;
  }
  if (container_properties) {
    if (property_container == this) {
      *container_properties = FirstFragment().LocalBorderBoxProperties();
    } else {
      *container_properties =
          property_container->FirstFragment().ContentsProperties();
    }
  }
  return property_container;
}

HitTestResult LayoutObject::HitTestForOcclusion(
    const PhysicalRect& hit_rect) const {
  NOT_DESTROYED();
  LocalFrame* frame = GetDocument().GetFrame();
  DCHECK(!frame->View()->NeedsLayout());
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kIgnorePointerEventsNone | HitTestRequest::kReadOnly |
      HitTestRequest::kIgnoreClipping |
      HitTestRequest::kIgnoreZeroOpacityObjects |
      HitTestRequest::kHitTestVisualOverflow;
  HitTestLocation location(hit_rect);
  return frame->GetEventHandler().HitTestResultAtLocation(location, hit_type,
                                                          this, true);
}

void LayoutObject::DirtyLinesFromChangedChild(LayoutObject*, MarkingBehavior) {
  NOT_DESTROYED();
}

std::ostream& operator<<(std::ostream& out, const LayoutObject& object) {
  String info;
#if DCHECK_IS_ON()
  StringBuilder string_builder;
  object.DumpLayoutObject(string_builder, false, 0);
  info = string_builder.ToString();
#else
  info = object.DebugName();
#endif
  return out << static_cast<const void*>(&object) << ":" << info.Utf8();
}

std::ostream& operator<<(std::ostream& out, const LayoutObject* object) {
  if (!object)
    return out << "<null>";
  return out << *object;
}

#if DCHECK_IS_ON()

void LayoutObject::ShowTreeForThis() const {
  NOT_DESTROYED();
  if (GetNode())
    ::ShowTree(GetNode());
}

void LayoutObject::ShowLayoutTreeForThis() const {
  NOT_DESTROYED();
  ShowLayoutTree(this, nullptr);
}

void LayoutObject::ShowLineTreeForThis() const {
  NOT_DESTROYED();
  if (const LayoutBlock* cb = InclusiveContainingBlock()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(cb);
    if (child_block_flow) {
      child_block_flow->ShowLineTreeAndMark(nullptr, nullptr, nullptr, nullptr,
                                            this);
    }
  }
}

void LayoutObject::ShowLayoutObject() const {
  NOT_DESTROYED();

  if (getenv("RUNNING_UNDER_RR")) {
    // Printing timestamps requires an IPC to get the local time, which
    // does not work in an rr replay session. Just disable timestamp printing
    // globally, since we don't need them. Affecting global state isn't a
    // problem because invoking this from a rr session creates a temporary
    // program environment that will be destroyed as soon as the invocation
    // completes.
    logging::SetLogItems(true, true, false, false);
  }

  StringBuilder string_builder;
  DumpLayoutObject(string_builder, true, kShowTreeCharacterOffset);
  DLOG(INFO) << "\n" << string_builder.ToString().Utf8();
}

void LayoutObject::DumpLayoutObject(StringBuilder& string_builder,
                                    bool dump_address,
                                    unsigned show_tree_character_offset) const {
  NOT_DESTROYED();
  string_builder.Append(DecoratedName());

  if (dump_address)
    string_builder.AppendFormat(" %p", this);

  if (IsText() && To<LayoutText>(this)->IsTextFragment()) {
    string_builder.AppendFormat(
        " \"%s\" ", To<LayoutText>(this)->GetText().Ascii().c_str());
  }

  if (VirtualContinuation())
    string_builder.AppendFormat(" continuation=%p", VirtualContinuation());

  if (GetNode()) {
    while (string_builder.length() < show_tree_character_offset)
      string_builder.Append(' ');
    string_builder.Append('\t');
    string_builder.Append(GetNode()->ToString());
  }
  if (ChildLayoutBlockedByDisplayLock())
    string_builder.Append(" (display-locked)");
}

void LayoutObject::DumpLayoutTreeAndMark(StringBuilder& string_builder,
                                         const LayoutObject* marked_object1,
                                         const char* marked_label1,
                                         const LayoutObject* marked_object2,
                                         const char* marked_label2,
                                         unsigned depth) const {
  NOT_DESTROYED();
  StringBuilder object_info;
  if (marked_object1 == this && marked_label1)
    object_info.Append(marked_label1);
  if (marked_object2 == this && marked_label2)
    object_info.Append(marked_label2);
  while (object_info.length() < depth * 2)
    object_info.Append(' ');

  DumpLayoutObject(object_info, true, kShowTreeCharacterOffset);
  string_builder.Append(object_info);

  if (!ChildLayoutBlockedByDisplayLock()) {
    for (const LayoutObject* child = SlowFirstChild(); child;
         child = child->NextSibling()) {
      string_builder.Append('\n');
      child->DumpLayoutTreeAndMark(string_builder, marked_object1,
                                   marked_label1, marked_object2, marked_label2,
                                   depth + 1);
    }
  }
}

#endif  // DCHECK_IS_ON()

bool LayoutObject::IsSelected() const {
  NOT_DESTROYED();
  // Keep this fast and small, used in very hot functions to skip computing
  // selection when this is not selected. This function may be inlined in
  // link-optimized builds, but keeping fast and small helps running perf
  // tests.
  return GetSelectionState() != SelectionState::kNone ||
         // TODO(kojii): Can't we set SelectionState() properly to
         // LayoutTextFragment too?
         (IsA<LayoutTextFragment>(*this) && LayoutSelection::IsSelected(*this));
}

bool LayoutObject::IsSelectable() const {
  NOT_DESTROYED();
  return StyleRef().IsSelectable();
}

const ComputedStyle& LayoutObject::SlowEffectiveStyle(
    NGStyleVariant style_variant) const {
  NOT_DESTROYED();
  switch (style_variant) {
    case NGStyleVariant::kStandard:
      return StyleRef();
    case NGStyleVariant::kFirstLine:
      if (IsInline() && IsAtomicInlineLevel())
        return StyleRef();
      return FirstLineStyleRef();
    case NGStyleVariant::kEllipsis:
      // The ellipsis is styled according to the line style.
      // https://www.w3.org/TR/css-overflow-3/#ellipsing-details
      // Use first-line style if exists since most cases it is the first line.
      DCHECK(IsInline());
      if (LayoutObject* block = ContainingBlock())
        return block->FirstLineStyleRef();
      return FirstLineStyleRef();
  }
  NOTREACHED();
  return StyleRef();
}

const ComputedStyle* LayoutObject::SlowStyleForContinuationOutline() const {
  NOT_DESTROYED();
  // Fail fast using bitfields is done in |StyleForContinuationOutline|.
  DCHECK(IsAnonymous() && !IsInline());
  const auto* block_flow = DynamicTo<LayoutBlockFlow>(this);
  if (!block_flow)
    return nullptr;

  // Check ancestors of the continuation in case nested inline boxes; e.g.
  // <span style="outline: auto">
  //   <span>
  //     <div>block</div>
  //   </span>
  // </span>
  for (const LayoutObject* continuation = block_flow->Continuation();
       UNLIKELY(continuation && continuation->IsLayoutInline());
       continuation = continuation->Parent()) {
    const ComputedStyle& style = continuation->StyleRef();
    if (style.OutlineStyleIsAuto() &&
        NGOutlineUtils::HasPaintedOutline(style, continuation->GetNode()))
      return &style;
  }
  return nullptr;
}

// Called when an object that was floating or positioned becomes a normal flow
// object again. We have to make sure the layout tree updates as needed to
// accommodate the new normal flow object.
static inline void HandleDynamicFloatPositionChange(LayoutObject* object) {
  // We have gone from not affecting the inline status of the parent flow to
  // suddenly having an impact.  See if there is a mismatch between the parent
  // flow's childrenInline() state and our state.
  object->SetInline(object->StyleRef().IsDisplayInlineType());
  if (object->IsInline() != object->Parent()->ChildrenInline()) {
    if (!object->IsInline()) {
      To<LayoutBoxModelObject>(object->Parent())->ChildBecameNonInline(object);
    } else {
      // An anonymous block must be made to wrap this inline.
      LayoutBlock* block =
          To<LayoutBlock>(object->Parent())->CreateAnonymousBlock();
      LayoutObjectChildList* childlist = object->Parent()->VirtualChildren();
      childlist->InsertChildNode(object->Parent(), block, object);
      block->Children()->AppendChildNode(
          block, childlist->RemoveChildNode(object->Parent(), object));
    }
  }
}

StyleDifference LayoutObject::AdjustStyleDifference(
    StyleDifference diff) const {
  NOT_DESTROYED();
  if (diff.TransformChanged() && IsSVG()) {
    // Skip a full layout for transforms at the html/svg boundary which do not
    // affect sizes inside SVG.
    if (!IsSVGRoot())
      diff.SetNeedsFullLayout();
  }

  // Optimization: for decoration/color property changes, invalidation is only
  // needed if we have style or text affected by these properties.
  if (diff.TextDecorationOrColorChanged() &&
      !diff.NeedsNormalPaintInvalidation() &&
      !diff.NeedsSimplePaintInvalidation()) {
    if (StyleRef().HasOutlineWithCurrentColor() ||
        StyleRef().HasBackgroundRelatedColorReferencingCurrentColor() ||
        // Skip any text nodes that do not contain text boxes. Whitespace cannot
        // be skipped or we will miss invalidating decorations (e.g.,
        // underlines). MathML elements are not skipped either as some of them
        // do special painting (e.g. fraction bar).
        (IsText() && !IsBR() && To<LayoutText>(this)->HasInlineFragments()) ||
        (IsSVG() && StyleRef().IsFillColorCurrentColor()) ||
        (IsSVG() && StyleRef().IsStrokeColorCurrentColor()) ||
        IsListMarkerForNormalContent() || IsMathML()) {
      diff.SetNeedsSimplePaintInvalidation();
    }
  }

  // TODO(1088373): Pixel_WebGLHighToLowPower fails without this. This isn't the
  // right way to ensure GPU switching. Investigate and do it in the right way.
  if (!diff.NeedsNormalPaintInvalidation() && IsLayoutView() && Style() &&
      !Style()->GetFont().IsFallbackValid()) {
    diff.SetNeedsNormalPaintInvalidation();
  }

  // The answer to layerTypeRequired() for plugins, iframes, and canvas can
  // change without the actual style changing, since it depends on whether we
  // decide to composite these elements. When the/ layer status of one of these
  // elements changes, we need to force a layout.
  if (!diff.NeedsFullLayout() && Style() && IsBoxModelObject()) {
    bool requires_layer =
        To<LayoutBoxModelObject>(this)->LayerTypeRequired() != kNoPaintLayer;
    if (HasLayer() != requires_layer)
      diff.SetNeedsFullLayout();
  }

  return diff;
}

void LayoutObject::SetPseudoElementStyle(
    scoped_refptr<const ComputedStyle> pseudo_style,
    bool match_parent_size) {
  NOT_DESTROYED();
  DCHECK(pseudo_style->StyleType() == kPseudoIdBefore ||
         pseudo_style->StyleType() == kPseudoIdAfter ||
         pseudo_style->StyleType() == kPseudoIdMarker ||
         pseudo_style->StyleType() == kPseudoIdFirstLetter);

  // FIXME: We should consider just making all pseudo items use an inherited
  // style.

  // Images are special and must inherit the pseudoStyle so the width and height
  // of the pseudo element doesn't change the size of the image. In all other
  // cases we can just share the style.
  //
  // Quotes are also LayoutInline, so we need to create an inherited style to
  // avoid getting an inline with positioning or an invalid display.
  //
  if (IsImage() || IsQuote()) {
    ComputedStyleBuilder builder =
        GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
    builder.InheritFrom(*pseudo_style);
    if (match_parent_size) {
      DCHECK(IsImage());
      builder.SetWidth(Length::Percent(100));
      builder.SetHeight(Length::Percent(100));
    }
    SetStyle(builder.TakeStyle());
    return;
  }

  if (IsText() && Parent() && UNLIKELY(Parent()->IsInitialLetterBox())) {
    // Note: `Parent()` can be null for text for generated contents.
    // See "accessibility/css-generated-content.html"
    scoped_refptr<const ComputedStyle> initial_letter_text_style =
        GetDocument().GetStyleResolver().StyleForInitialLetterText(
            *pseudo_style, Parent()->ContainingBlock()->StyleRef());
    SetStyle(std::move(initial_letter_text_style));
    return;
  }

  if (IsText() && UNLIKELY(IsA<LayoutNGTextCombine>(Parent()))) {
    // See http://crbug.com/1222640
    ComputedStyleBuilder combined_text_style_builder =
        GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
    combined_text_style_builder.InheritFrom(*pseudo_style);
    StyleAdjuster::AdjustStyleForCombinedText(combined_text_style_builder);
    SetStyle(combined_text_style_builder.TakeStyle());
    return;
  }

  SetStyle(std::move(pseudo_style));
}

DISABLE_CFI_PERF
void LayoutObject::SetStyle(scoped_refptr<const ComputedStyle> style,
                            ApplyStyleChanges apply_changes) {
  NOT_DESTROYED();
  if (style_ == style)
    return;

  if (apply_changes == ApplyStyleChanges::kNo) {
    SetStyleInternal(std::move(style));
    return;
  }

  DCHECK(style);

  StyleDifference diff;
  if (style_) {
    diff = style_->VisualInvalidationDiff(GetDocument(), *style);
    if (const auto* cached_inherited_first_line_style =
            style_->GetCachedPseudoElementStyle(kPseudoIdFirstLineInherited)) {
      // Merge the difference to the first line style because even if the new
      // style is the same as the old style, the new style may have some higher
      // priority properties overriding first line style.
      // See external/wpt/css/css-pseudo/first-line-change-inline-color*.html.
      diff.Merge(cached_inherited_first_line_style->VisualInvalidationDiff(
          GetDocument(), *style));
    }

    auto HighlightPseudoUpdateDiff = [this, style,
                                      &diff](const PseudoId pseudo) {
      DCHECK(pseudo == kPseudoIdTargetText ||
             pseudo == kPseudoIdSpellingError ||
             pseudo == kPseudoIdGrammarError);

      if (style_->HasPseudoElementStyle(pseudo) ||
          style->HasPseudoElementStyle(pseudo)) {
        const ComputedStyle* pseudo_old_style = nullptr;
        const ComputedStyle* pseudo_new_style = nullptr;

        // TODO(rego): Refactor this code so we can call something like
        // HighlightData()->PseudoStyle(pseudo) and avoid the switch (we could
        // also avoid the switch in
        // HighlightPaintingUtils::HighlightPseudoStyle().
        switch (pseudo) {
          case kPseudoIdTargetText:
            pseudo_old_style = style_->HighlightData()
                                   ? style_->HighlightData()->TargetText()
                                   : nullptr;
            pseudo_new_style = style->HighlightData()
                                   ? style->HighlightData()->TargetText()
                                   : nullptr;
            break;
          case kPseudoIdSpellingError:
            pseudo_old_style = style_->HighlightData()
                                   ? style_->HighlightData()->SpellingError()
                                   : nullptr;
            pseudo_new_style = style->HighlightData()
                                   ? style->HighlightData()->SpellingError()
                                   : nullptr;
            break;
          case kPseudoIdGrammarError:
            pseudo_old_style = style_->HighlightData()
                                   ? style_->HighlightData()->GrammarError()
                                   : nullptr;
            pseudo_new_style = style->HighlightData()
                                   ? style->HighlightData()->GrammarError()
                                   : nullptr;
            break;
          default:
            NOTREACHED();
        }

        if (pseudo_old_style && pseudo_new_style) {
          diff.Merge(pseudo_old_style->VisualInvalidationDiff(
              GetDocument(), *pseudo_new_style));
        } else {
          diff.SetNeedsNormalPaintInvalidation();
        }
      }
    };

    if (RuntimeEnabledFeatures::HighlightInheritanceEnabled()) {
      // TODO(rego): We don't do anything regarding ::selection, as ::selection
      // uses its own mechanism for this (see
      // LayoutObject::InvalidateSelectedChildrenOnStyleChange()). Maybe in the
      // future we could detect changes here for ::selection too.
      HighlightPseudoUpdateDiff(kPseudoIdTargetText);
      if (RuntimeEnabledFeatures::CSSSpellingGrammarErrorsEnabled()) {
        HighlightPseudoUpdateDiff(kPseudoIdSpellingError);
        HighlightPseudoUpdateDiff(kPseudoIdGrammarError);
      }
    }
  }

  diff = AdjustStyleDifference(diff);

  // A change to a property that can be animated on the compositor or an
  // animation affecting that property may require paint invalidation.
  diff = AdjustForCompositableAnimationPaint(style_, style, GetNode(), diff);

  StyleWillChange(diff, *style);

  scoped_refptr<const ComputedStyle> old_style = std::move(style_);
  SetStyleInternal(std::move(style));

  if (!IsText())
    UpdateImageObservers(old_style.get(), style_.get());

  CheckCounterChanges(old_style.get(), style_.get());

  bool does_not_need_layout_or_paint_invalidation = !parent_;

  StyleDidChange(diff, old_style.get());

  // FIXME: |this| might be destroyed here. This can currently happen for a
  // LayoutTextFragment when its first-letter block gets an update in
  // LayoutTextFragment::styleDidChange. For LayoutTextFragment(s),
  // we will safely bail out with the doesNotNeedLayoutOrPaintInvalidation flag.
  // We might want to broaden this condition in the future as we move
  // layoutObject changes out of layout and into style changes.
  if (does_not_need_layout_or_paint_invalidation)
    return;

  // Now that the layer (if any) has been updated, we need to adjust the diff
  // again, check whether we should layout now, and decide if we need to
  // invalidate paints.
  StyleDifference updated_diff = AdjustStyleDifference(diff);

  if (updated_diff.NeedsSimplePaintInvalidation()) {
    DCHECK(!diff.NeedsNormalPaintInvalidation());
    constexpr int kMaxDepth = 5;
    if (auto* painting_layer = PaintingLayer(kMaxDepth)) {
      painting_layer->SetNeedsRepaint();
      InvalidateDisplayItemClients(PaintInvalidationReason::kStyle);
      GetFrameView()->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
    } else {
      updated_diff.SetNeedsNormalPaintInvalidation();
    }
  }

  if (!diff.NeedsFullLayout()) {
    if (updated_diff.NeedsFullLayout()) {
      SetNeedsLayoutAndIntrinsicWidthsRecalc(
          layout_invalidation_reason::kStyleChange);
    } else if (updated_diff.NeedsPositionedMovementLayout()) {
      SetNeedsPositionedMovementLayout();
    }
  }

  // TODO(cbiesinger): Shouldn't this check container->NeedsLayout, since that's
  // the one we'll mark for NeedsOverflowRecalc()?
  if (diff.TransformChanged() && !NeedsLayout()) {
    if (LayoutBlock* container = ContainingBlock())
      container->SetNeedsOverflowRecalc();
  }

  if (diff.NeedsRecomputeVisualOverflow()) {
    if (!IsInLayoutNGInlineFormattingContext() && !IsLayoutNGObject() &&
        !IsLayoutBlock() && !NeedsLayout()) {
      // TODO(crbug.com/1128199): This is still needed because
      // RecalcVisualOverflow() does not actually compute the visual overflow
      // for inline elements (legacy layout). However in LayoutNG
      // RecalcInlineChildrenInkOverflow() is called and visual overflow is
      // recomputed properly so we don't need this (see crbug.com/1043927).
      SetNeedsLayoutAndIntrinsicWidthsRecalc(
          layout_invalidation_reason::kStyleChange);
    } else {
      if (IsInLayoutNGInlineFormattingContext() && !NeedsLayout()) {
        if (auto* text = DynamicTo<LayoutText>(this))
          text->InvalidateVisualOverflow();
      }
      PaintingLayer()->SetNeedsVisualOverflowRecalc();
      // TODO(crbug.com/1385848): This looks like an over-invalidation.
      // visual overflow change should not require checking for layout change.
      SetShouldCheckForPaintInvalidation();
    }
#if DCHECK_IS_ON()
    InvalidateVisualOverflow();
#endif
  }

  if (diff.NeedsNormalPaintInvalidation() ||
      updated_diff.NeedsNormalPaintInvalidation()) {
    if (IsSVGRoot()) {
      // LayoutSVGRoot::LocalVisualRect() depends on some styles.
      SetShouldDoFullPaintInvalidation();
    } else {
      // We'll set needing geometry change later if the style change does cause
      // possible layout change or visual overflow change.
      SetShouldDoFullPaintInvalidationWithoutLayoutChange(
          PaintInvalidationReason::kStyle);
    }
  }

  if (diff.NeedsNormalPaintInvalidation() && old_style &&
      !old_style->ClipPathDataEquivalent(*style_)) {
    SetNeedsPaintPropertyUpdate();
    PaintingLayer()->SetNeedsCompositingInputsUpdate();
  }

  if (!IsLayoutNGObject() && old_style &&
      old_style->Visibility() != style_->Visibility()) {
    SetShouldDoFullPaintInvalidation();
  }

  // Text nodes share style with their parents but the paint properties don't
  // apply to them, hence the !isText() check. If property nodes are added or
  // removed as a result of these style changes, PaintPropertyTreeBuilder will
  // call SetNeedsRepaint to cause re-generation of PaintChunks.
  // This is skipped if no layer is present because |PaintLayer::StyleDidChange|
  // will handle this invalidation.
  if (!IsText() && !HasLayer() &&
      (diff.TransformChanged() || diff.OpacityChanged() ||
       diff.ZIndexChanged() || diff.FilterChanged() || diff.CssClipChanged() ||
       diff.BlendModeChanged() || diff.MaskChanged() ||
       diff.CompositingReasonsChanged())) {
    SetNeedsPaintPropertyUpdate();
  }

  if (!IsText() && diff.CompositablePaintEffectChanged()) {
    SetShouldDoFullPaintInvalidationWithoutLayoutChange(
        PaintInvalidationReason::kStyle);
  }
}

void LayoutObject::UpdateFirstLineImageObservers(
    const ComputedStyle* new_style) {
  NOT_DESTROYED();
  bool has_new_first_line_style =
      new_style && new_style->HasPseudoElementStyle(kPseudoIdFirstLine) &&
      BehavesLikeBlockContainer();
  DCHECK(!has_new_first_line_style || new_style == Style());

  if (!bitfields_.RegisteredAsFirstLineImageObserver() &&
      !has_new_first_line_style)
    return;

  using FirstLineStyleMap = HeapHashMap<WeakMember<const LayoutObject>,
                                        scoped_refptr<const ComputedStyle>>;
  DEFINE_STATIC_LOCAL(Persistent<FirstLineStyleMap>, first_line_style_map,
                      (MakeGarbageCollected<FirstLineStyleMap>()));
  DCHECK_EQ(bitfields_.RegisteredAsFirstLineImageObserver(),
            first_line_style_map->Contains(this));
  const auto* old_first_line_style =
      bitfields_.RegisteredAsFirstLineImageObserver()
          ? first_line_style_map->at(this)
          : nullptr;

  // UpdateFillImages() may indirectly call LayoutBlock::ImageChanged() which
  // will invalidate the first line style cache and remove a reference to
  // new_first_line_style, so hold a reference here.
  scoped_refptr<const ComputedStyle> new_first_line_style =
      has_new_first_line_style ? FirstLineStyleWithoutFallback() : nullptr;

  if (new_first_line_style && !new_first_line_style->HasBackgroundImage())
    new_first_line_style = nullptr;

  if (old_first_line_style || new_first_line_style) {
    UpdateFillImages(
        old_first_line_style ? &old_first_line_style->BackgroundLayers()
                             : nullptr,
        new_first_line_style ? &new_first_line_style->BackgroundLayers()
                             : nullptr);
    if (new_first_line_style) {
      // The cached first line style may have been invalidated during
      // UpdateFillImages, so get it again. However, the new cached first line
      // style should be the same as the previous new_first_line_style.
      DCHECK(FillLayer::ImagesIdentical(
          &new_first_line_style->BackgroundLayers(),
          &FirstLineStyleWithoutFallback()->BackgroundLayers()));
      new_first_line_style = FirstLineStyleWithoutFallback();
      bitfields_.SetRegisteredAsFirstLineImageObserver(true);
      first_line_style_map->Set(this, std::move(new_first_line_style));
    } else {
      bitfields_.SetRegisteredAsFirstLineImageObserver(false);
      first_line_style_map->erase(this);
    }
    DCHECK_EQ(bitfields_.RegisteredAsFirstLineImageObserver(),
              first_line_style_map->Contains(this));
  }
}

void LayoutObject::StyleWillChange(StyleDifference diff,
                                   const ComputedStyle& new_style) {
  NOT_DESTROYED();
  if (style_) {
    bool visibility_changed = style_->Visibility() != new_style.Visibility();
    // If our z-index changes value or our visibility changes,
    // we need to dirty our stacking context's z-order list.
    if (visibility_changed ||
        style_->EffectiveZIndex() != new_style.EffectiveZIndex() ||
        IsStackingContext(*style_) != IsStackingContext(new_style)) {
      GetDocument().SetAnnotatedRegionsDirty(true);
    }

    bool background_color_changed =
        ResolveColorFast(GetCSSPropertyBackgroundColor()) !=
        ResolveColorFast(new_style, GetCSSPropertyBackgroundColor());

    if (diff.TextDecorationOrColorChanged() || background_color_changed ||
        style_->GetFontDescription() != new_style.GetFontDescription() ||
        style_->GetWritingDirection() != new_style.GetWritingDirection() ||
        style_->InsideLink() != new_style.InsideLink() ||
        style_->VerticalAlign() != new_style.VerticalAlign() ||
        style_->GetTextAlign() != new_style.GetTextAlign() ||
        style_->TextIndent() != new_style.TextIndent()) {
      if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
        cache->StyleChanged(this);
    }

    if (diff.TransformChanged()) {
      if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
        cache->LocationChanged(this);
    }

    if (visibility_changed || style_->IsInert() != new_style.IsInert()) {
      if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
        cache->ChildrenChanged(Parent());
        cache->ChildrenChanged(this);
      }
    }

    // Keep layer hierarchy visibility bits up to date if visibility changes.
    if (visibility_changed) {
      // We might not have an enclosing layer yet because we might not be in the
      // tree.
      if (PaintLayer* layer = EnclosingLayer())
        layer->DirtyVisibleContentStatus();
      GetDocument().GetFrame()->GetInputMethodController().DidChangeVisibility(
          *this);
    }

    if (IsFloating() &&
        (style_->UnresolvedFloating() != new_style.UnresolvedFloating())) {
      // For changes in float styles, we need to conceivably remove ourselves
      // from the floating objects list.
      To<LayoutBox>(this)->RemoveFloatingOrPositionedChildFromBlockLists();
    } else if (IsOutOfFlowPositioned() &&
               (style_->GetPosition() != new_style.GetPosition())) {
      // For changes in positioning styles, we need to conceivably remove
      // ourselves from the positioned objects list.
      To<LayoutBox>(this)->RemoveFloatingOrPositionedChildFromBlockLists();
    }

    affects_parent_block_ =
        IsFloatingOrOutOfFlowPositioned() &&
        ((!new_style.IsFloating() ||
          new_style.IsInsideDisplayIgnoringFloatingChildren()) &&
         !new_style.HasOutOfFlowPosition()) &&
        Parent() &&
        (Parent()->IsLayoutBlockFlow() || Parent()->IsLayoutInline());

    // Clearing these bits is required to avoid leaving stale layoutObjects.
    // FIXME: We shouldn't need that hack if our logic was totally correct.
    if (diff.NeedsLayout()) {
      SetFloating(false);
      ClearPositionedState();
    }
  } else {
    affects_parent_block_ = false;
  }

  // Elements with non-auto touch-action will send a SetTouchAction message
  // on touchstart in EventHandler::handleTouchEvent, and so effectively have
  // a touchstart handler that must be reported.
  //
  // Since a CSS property cannot be applied directly to a text node, a
  // handler will have already been added for its parent so ignore it.
  //
  // Elements may inherit touch action from parent frame, so we need to report
  // touchstart handler if the root layout object has non-auto effective touch
  // action.
  TouchAction old_touch_action = TouchAction::kAuto;
  bool is_document_element = GetNode() && IsDocumentElement();
  if (style_)
    old_touch_action = style_->EffectiveTouchAction();
  TouchAction new_touch_action = new_style.EffectiveTouchAction();
  if (GetNode() && !GetNode()->IsTextNode() &&
      (old_touch_action == TouchAction::kAuto) !=
          (new_touch_action == TouchAction::kAuto)) {
    EventHandlerRegistry& registry =
        GetDocument().GetFrame()->GetEventHandlerRegistry();
    if (new_touch_action != TouchAction::kAuto) {
      registry.DidAddEventHandler(*GetNode(),
                                  EventHandlerRegistry::kTouchAction);
    } else {
      registry.DidRemoveEventHandler(*GetNode(),
                                     EventHandlerRegistry::kTouchAction);
    }
    MarkEffectiveAllowedTouchActionChanged();
  }
  if (is_document_element && style_ && style_->Opacity() == 0.0f &&
      new_style.Opacity() != 0.0f) {
    if (LocalFrameView* frame_view = GetFrameView())
      frame_view->GetPaintTimingDetector().ReportIgnoredContent();
  }
}

static bool AreNonIdenticalCursorListsEqual(const ComputedStyle* a,
                                            const ComputedStyle* b) {
  DCHECK_NE(a->Cursors(), b->Cursors());
  return a->Cursors() && b->Cursors() && *a->Cursors() == *b->Cursors();
}

static inline bool AreCursorsEqual(const ComputedStyle* a,
                                   const ComputedStyle* b) {
  return a->Cursor() == b->Cursor() && (a->Cursors() == b->Cursors() ||
                                        AreNonIdenticalCursorListsEqual(a, b));
}

void LayoutObject::SetScrollAnchorDisablingStyleChangedOnAncestor() {
  NOT_DESTROYED();
  // Walk up the parent chain and find the first scrolling block to disable
  // scroll anchoring on.
  LayoutObject* object = Parent();
  Element* viewport_defining_element = GetDocument().ViewportDefiningElement();
  while (object) {
    auto* block = DynamicTo<LayoutBlock>(object);
    if (block && (block->IsScrollContainer() ||
                  block->GetNode() == viewport_defining_element)) {
      block->SetScrollAnchorDisablingStyleChanged(true);
      return;
    }
    object = object->Parent();
  }
}

static void ClearAncestorScrollAnchors(LayoutObject* layout_object) {
  PaintLayer* layer = nullptr;
  if (LayoutObject* parent = layout_object->Parent())
    layer = parent->EnclosingLayer();

  while (layer) {
    if (PaintLayerScrollableArea* scrollable_area =
            layer->GetScrollableArea()) {
      ScrollAnchor* anchor = scrollable_area->GetScrollAnchor();
      DCHECK(anchor);
      anchor->Clear();
    }
    layer = layer->Parent();
  }
}

bool LayoutObject::BelongsToElementChangingOverflowBehaviour() const {
  auto* element = DynamicTo<Element>(GetNode());
  if (!element)
    return false;

  return IsA<HTMLVideoElement>(element) || IsA<HTMLCanvasElement>(element) ||
         IsA<HTMLImageElement>(element);
}

void LayoutObject::StyleDidChange(StyleDifference diff,
                                  const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (HasHiddenBackface()) {
    if (Parent() && Parent()->StyleRef().UsedTransformStyle3D() ==
                        ETransformStyle3D::kPreserve3d) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHiddenBackfaceWithPossible3D);
      UseCounter::Count(GetDocument(), WebFeature::kHiddenBackfaceWith3D);
      UseCounter::Count(GetDocument(),
                        WebFeature::kHiddenBackfaceWithPreserve3D);
    } else if (style_->HasTransform()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHiddenBackfaceWithPossible3D);
      // For consistency with existing code usage, this uses
      // Has3DTransformOperation rather than the slightly narrower
      // HasNonTrivial3DTransformOperation (which used to exist, and was only
      // web-exposed for compositing decisions on low-end devices).  However,
      // given the discussion in
      // https://github.com/w3c/csswg-drafts/issues/3305 it's possible we may
      // want to tie backface-visibility behavior to something closer to the
      // latter.
      if (style_->Has3DTransformOperation()) {
        UseCounter::Count(GetDocument(), WebFeature::kHiddenBackfaceWith3D);
      }
    }
  }

  if (ShouldApplyStrictContainment() && style_->IsContentVisibilityVisible()) {
    if (ShouldApplyStyleContainment()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kCSSContainAllWithoutContentVisibility);
    }
    UseCounter::Count(GetDocument(),
                      WebFeature::kCSSContainStrictWithoutContentVisibility);
  }

  // See the discussion at
  // https://github.com/w3c/csswg-drafts/issues/7144#issuecomment-1090933632
  // for more information.
  //
  // For a replaced element that isn't SVG or a embedded content, such as iframe
  // or object, we want to count the number of pages that have an explicit
  // overflow: visible (that remains visible after style adjuster). Separately,
  // we also want to count out of those cases how many have an object-fit none
  // or cover or non-default object-position, all of which may cause overflow.
  //
  // Note that SVG already supports overflow: visible, meaning we won't be
  // changing the behavior regardless of the counts. Likewise, embedded content
  // will remain clipped regardless of the overflow: visible behvaior change.
  // Note for this reason we exclude SVG and embedded content from the counts.
  if (BelongsToElementChangingOverflowBehaviour()) {
    if ((StyleRef().HasExplicitOverflowXVisible() &&
         StyleRef().OverflowX() == EOverflow::kVisible) ||
        (StyleRef().HasExplicitOverflowYVisible() &&
         StyleRef().OverflowY() == EOverflow::kVisible)) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kExplicitOverflowVisibleOnReplacedElement);

      Deprecation::CountDeprecation(
          GetDocument().GetExecutionContext(),
          WebFeature::kExplicitOverflowVisibleOnReplacedElement);
      if (!StyleRef().ObjectPropertiesPreventReplacedOverflow()) {
        UseCounter::Count(
            GetDocument(),
            WebFeature::
                kExplicitOverflowVisibleOnReplacedElementWithObjectProp);
      }
    }
  }

  // First assume the outline will be affected. It may be updated when we know
  // it's not affected.
  SetOutlineMayBeAffectedByDescendants(style_->HasOutline());

  if (affects_parent_block_)
    HandleDynamicFloatPositionChange(this);

  if (diff.NeedsFullLayout()) {
    // If the in-flow state of an element is changed, disable scroll
    // anchoring on the containing scroller.
    if (old_style->HasOutOfFlowPosition() != style_->HasOutOfFlowPosition()) {
      SetScrollAnchorDisablingStyleChangedOnAncestor();
      MarkParentForSpannerOrOutOfFlowPositionedChange();
    } else if (old_style->GetColumnSpan() != style_->GetColumnSpan()) {
      MarkParentForSpannerOrOutOfFlowPositionedChange();
    }

    // If the object already needs layout, then setNeedsLayout won't do
    // any work. But if the containing block has changed, then we may need
    // to mark the new containing blocks for layout. The change that can
    // directly affect the containing block of this object is a change to
    // the position style.
    if (NeedsLayout() && old_style->GetPosition() != style_->GetPosition()) {
      MarkContainerChainForLayout();
    }

    SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kStyleChange);
  } else if (diff.NeedsPositionedMovementLayout()) {
    SetNeedsPositionedMovementLayout();
  }

  if (diff.ScrollAnchorDisablingPropertyChanged())
    SetScrollAnchorDisablingStyleChanged(true);

  // Don't check for paint invalidation here; we need to wait until the layer
  // has been updated by subclasses before we know if we have to invalidate
  // paints (in setStyle()).

  if (old_style && !AreCursorsEqual(old_style, Style())) {
    if (LocalFrame* frame = GetFrame()) {
      // Cursor update scheduling is done by the local root, which is the main
      // frame if there are no RemoteFrame ancestors in the frame tree. Use of
      // localFrameRoot() is discouraged but will change when cursor update
      // scheduling is moved from EventHandler to PageEventHandler.
      frame->LocalFrameRoot().GetEventHandler().ScheduleCursorUpdate();
    }
  }

  if (diff.NeedsNormalPaintInvalidation() && old_style) {
    if (ResolveColor(*old_style, GetCSSPropertyBackgroundColor()) !=
            ResolveColor(GetCSSPropertyBackgroundColor()) ||
        old_style->BackgroundLayers() != StyleRef().BackgroundLayers())
      SetBackgroundNeedsFullPaintInvalidation();
  }

  ApplyPseudoElementStyleChanges(old_style);

  if (old_style &&
      old_style->UsedTransformStyle3D() != StyleRef().UsedTransformStyle3D()) {
    // Change of transform-style may affect descendant transform property nodes.
    AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kTransformStyleChanged);
  }

  if (old_style && old_style->OverflowAnchor() != StyleRef().OverflowAnchor()) {
    ClearAncestorScrollAnchors(this);
  }

  // Note: It's possible this will be moved to a particular later point within
  // the "update the rendering" steps, and thus not belong here.
  const auto* toggle_root = StyleRef().ToggleRoot();
  if (toggle_root && (!old_style || !old_style->ToggleRoot() ||
                      *toggle_root != *(old_style->ToggleRoot()))) {
    // This element has toggle specifiers; these specifiers require that we
    // create toggles.
    Element* element = DynamicTo<Element>(GetNode());
    DCHECK(element);
    if (element) {
      element->EnsureToggleMap().CreateToggles(toggle_root);
    }
  }

  if (StyleRef().AnchorName())
    MarkMayHaveAnchorQuery();
}

void LayoutObject::ApplyPseudoElementStyleChanges(
    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  ApplyFirstLineChanges(old_style);

  if ((old_style && old_style->HasPseudoElementStyle(kPseudoIdSelection)) ||
      StyleRef().HasPseudoElementStyle(kPseudoIdSelection))
    InvalidateSelectedChildrenOnStyleChange();
}

void LayoutObject::ApplyFirstLineChanges(const ComputedStyle* old_style) {
  NOT_DESTROYED();
  bool has_old_first_line_style =
      old_style && old_style->HasPseudoElementStyle(kPseudoIdFirstLine);
  bool has_new_first_line_style =
      StyleRef().HasPseudoElementStyle(kPseudoIdFirstLine);
  if (!has_old_first_line_style && !has_new_first_line_style)
    return;

  StyleDifference diff;
  bool has_diff = false;
  if (Parent() && has_old_first_line_style && has_new_first_line_style) {
    if (const auto* old_first_line_style =
            old_style->GetCachedPseudoElementStyle(kPseudoIdFirstLine)) {
      if (const auto* new_first_line_style = FirstLineStyleWithoutFallback()) {
        diff = old_first_line_style->VisualInvalidationDiff(
            GetDocument(), *new_first_line_style);
        diff = AdjustForCompositableAnimationPaint(
            old_first_line_style, new_first_line_style, GetNode(), diff);
        has_diff = true;
      }
    }
  }
  if (!has_diff) {
    diff.SetNeedsNormalPaintInvalidation();
    diff.SetNeedsFullLayout();
  }

  if (BehavesLikeBlockContainer() && (diff.NeedsNormalPaintInvalidation() ||
                                      diff.TextDecorationOrColorChanged())) {
    if (auto* first_line_container =
            To<LayoutBlock>(this)->NearestInnerBlockWithFirstLine())
      first_line_container->SetShouldDoFullPaintInvalidationForFirstLine();
  }

  if (diff.NeedsLayout()) {
    if (diff.NeedsFullLayout())
      SetNeedsCollectInlines();
    SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kStyleChange);
  }
}

void LayoutObject::AddAsImageObserver(StyleImage* image) {
  NOT_DESTROYED();
  if (!image)
    return;
#if DCHECK_IS_ON()
  ++as_image_observer_count_;
#endif
  image->AddClient(this);
}

void LayoutObject::RemoveAsImageObserver(StyleImage* image) {
  NOT_DESTROYED();
  if (!image)
    return;
#if DCHECK_IS_ON()
  SECURITY_DCHECK(as_image_observer_count_ > 0u);
  --as_image_observer_count_;
#endif
  image->RemoveClient(this);
}

void LayoutObject::UpdateFillImages(const FillLayer* old_layers,
                                    const FillLayer* new_layers) {
  NOT_DESTROYED();
  // Optimize the common case
  if (FillLayer::ImagesIdentical(old_layers, new_layers))
    return;

  // Go through the new layers and AddAsImageObserver() first, to avoid removing
  // all clients of an image.
  for (const FillLayer* curr_new = new_layers; curr_new;
       curr_new = curr_new->Next())
    AddAsImageObserver(curr_new->GetImage());

  for (const FillLayer* curr_old = old_layers; curr_old;
       curr_old = curr_old->Next())
    RemoveAsImageObserver(curr_old->GetImage());
}

void LayoutObject::UpdateCursorImages(const CursorList* old_cursors,
                                      const CursorList* new_cursors) {
  NOT_DESTROYED();
  if (old_cursors && new_cursors && *old_cursors == *new_cursors)
    return;

  if (new_cursors) {
    for (const auto& cursor : *new_cursors)
      AddAsImageObserver(cursor.GetImage());
  }
  if (old_cursors) {
    for (const auto& cursor : *old_cursors)
      RemoveAsImageObserver(cursor.GetImage());
  }
}

void LayoutObject::UpdateImage(StyleImage* old_image, StyleImage* new_image) {
  NOT_DESTROYED();
  if (old_image != new_image) {
    // AddAsImageObserver first, to avoid removing all clients of an image.
    AddAsImageObserver(new_image);
    RemoveAsImageObserver(old_image);
  }
}

void LayoutObject::UpdateShapeImage(const ShapeValue* old_shape_value,
                                    const ShapeValue* new_shape_value) {
  NOT_DESTROYED();
  if (old_shape_value || new_shape_value) {
    UpdateImage(old_shape_value ? old_shape_value->GetImage() : nullptr,
                new_shape_value ? new_shape_value->GetImage() : nullptr);
  }
}

void LayoutObject::CheckCounterChanges(const ComputedStyle* old_style,
                                       const ComputedStyle* new_style) {
  NOT_DESTROYED();
  DCHECK(new_style);
  if (old_style) {
    if (old_style->CounterDirectivesEqual(*new_style))
      return;
  } else {
    if (!new_style->GetCounterDirectives())
      return;
  }
  LayoutCounter::LayoutObjectStyleChanged(*this, old_style, *new_style);
  View()->SetNeedsMarkerOrCounterUpdate();
}

PhysicalRect LayoutObject::ViewRect() const {
  NOT_DESTROYED();
  return View()->ViewRect();
}

gfx::PointF LayoutObject::AncestorToLocalPoint(
    const LayoutBoxModelObject* ancestor,
    const gfx::PointF& container_point,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  TransformState transform_state(
      TransformState::kUnapplyInverseTransformDirection, container_point);
  MapAncestorToLocal(ancestor, transform_state, mode);
  transform_state.Flatten();

  return transform_state.LastPlanarPoint();
}

gfx::QuadF LayoutObject::AncestorToLocalQuad(
    const LayoutBoxModelObject* ancestor,
    const gfx::QuadF& quad,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  TransformState transform_state(
      TransformState::kUnapplyInverseTransformDirection,
      quad.BoundingBox().CenterPoint(), quad);
  MapAncestorToLocal(ancestor, transform_state, mode);
  transform_state.Flatten();
  return transform_state.LastPlanarQuad();
}

void LayoutObject::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                      TransformState& transform_state,
                                      MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  if (ancestor == this)
    return;

  AncestorSkipInfo skip_info(ancestor);
  const LayoutObject* container = Container(&skip_info);
  if (!container)
    return;

  PhysicalOffset container_offset =
      OffsetFromContainer(container, mode & kIgnoreScrollOffset);

  // TODO(smcgruer): This is inefficient. Instead we should avoid including
  // offsetForInFlowPosition in offsetFromContainer when ignoring sticky.
  if (mode & kIgnoreStickyOffset && IsStickyPositioned()) {
    container_offset -=
        To<LayoutBoxModelObject>(this)->OffsetForInFlowPosition();
  }

  if (IsLayoutFlowThread()) {
    // So far the point has been in flow thread coordinates (i.e. as if
    // everything in the fragmentation context lived in one tall single column).
    // Convert it to a visual point now, since we're about to escape the flow
    // thread.
    container_offset += PhysicalOffsetToBeNoop(
        ColumnOffset(transform_state.MappedPoint().ToLayoutPoint()));
  }

  // Text objects just copy their parent's computed style, so we need to ignore
  // them.
  bool use_transforms = !(mode & kIgnoreTransforms);

  const bool container_preserves_3d = container->StyleRef().Preserves3D();
  // Just because container and this have preserve-3d doesn't mean all
  // the DOM elements between them do.  (We know they don't have a
  // transform, though, since otherwise they'd be the container.)
  const bool path_preserves_3d = container == NearestAncestorForElement();
  const bool preserve3d = use_transforms && container_preserves_3d &&
                          !container->IsText() && path_preserves_3d;

  if (use_transforms && ShouldUseTransformFromContainer(container)) {
    gfx::Transform t;
    GetTransformFromContainer(container, container_offset, t);
    transform_state.ApplyTransform(t, preserve3d
                                          ? TransformState::kAccumulateTransform
                                          : TransformState::kFlattenTransform);
  } else {
    transform_state.Move(container_offset,
                         preserve3d ? TransformState::kAccumulateTransform
                                    : TransformState::kFlattenTransform);
  }

  if (skip_info.AncestorSkipped()) {
    // There can't be a transform between |ancestor| and |o|, because transforms
    // create containers, so it should be safe to just subtract the delta
    // between the ancestor and |o|.
    transform_state.Move(-ancestor->OffsetFromAncestor(container),
                         preserve3d ? TransformState::kAccumulateTransform
                                    : TransformState::kFlattenTransform);
    return;
  }

  container->MapLocalToAncestor(ancestor, transform_state, mode);
}

void LayoutObject::MapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                                      TransformState& transform_state,
                                      MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  if (this == ancestor)
    return;

  AncestorSkipInfo skip_info(ancestor);
  LayoutObject* container = Container(&skip_info);
  if (!container)
    return;

  if (!skip_info.AncestorSkipped())
    container->MapAncestorToLocal(ancestor, transform_state, mode);

  PhysicalOffset container_offset = OffsetFromContainer(container);
  bool use_transforms = !(mode & kIgnoreTransforms);

  // Just because container and this have preserve-3d doesn't mean all
  // the DOM elements between them do.  (We know they don't have a
  // transform, though, since otherwise they'd be the container.)
  if (container != NearestAncestorForElement()) {
    transform_state.Move(PhysicalOffset(), TransformState::kFlattenTransform);
  }

  const bool preserve3d = use_transforms && StyleRef().Preserves3D();
  if (use_transforms && ShouldUseTransformFromContainer(container)) {
    gfx::Transform t;
    GetTransformFromContainer(container, container_offset, t);
    transform_state.ApplyTransform(t, preserve3d
                                          ? TransformState::kAccumulateTransform
                                          : TransformState::kFlattenTransform);
  } else {
    transform_state.Move(container_offset,
                         preserve3d ? TransformState::kAccumulateTransform
                                    : TransformState::kFlattenTransform);
  }

  if (IsLayoutFlowThread()) {
    // Descending into a flow thread. Convert to the local coordinate space,
    // i.e. flow thread coordinates.
    PhysicalOffset visual_point = transform_state.MappedPoint();
    transform_state.Move(
        visual_point -
        PhysicalOffsetToBeNoop(
            To<LayoutFlowThread>(this)->VisualPointToFlowThreadPoint(
                visual_point.ToLayoutPoint())));
  }

  if (skip_info.AncestorSkipped()) {
    container_offset = ancestor->OffsetFromAncestor(container);
    transform_state.Move(-container_offset);
  }
}

bool LayoutObject::ShouldUseTransformFromContainer(
    const LayoutObject* container_object) const {
  NOT_DESTROYED();
  // hasTransform() indicates whether the object has transform, transform-style
  // or perspective. We just care about transform, so check the layer's
  // transform directly.
  return (HasLayer() && To<LayoutBoxModelObject>(this)->Layer()->Transform()) ||
         (container_object && container_object->StyleRef().HasPerspective());
}

void LayoutObject::GetTransformFromContainer(
    const LayoutObject* container_object,
    const PhysicalOffset& offset_in_container,
    gfx::Transform& transform,
    const PhysicalSize* size) const {
  NOT_DESTROYED();
  transform.MakeIdentity();
  PaintLayer* layer =
      HasLayer() ? To<LayoutBoxModelObject>(this)->Layer() : nullptr;
  if (layer && layer->Transform())
    transform.PreConcat(layer->CurrentTransform());

  transform.PostTranslate(offset_in_container.left.ToFloat(),
                          offset_in_container.top.ToFloat());

  bool has_perspective = container_object && container_object->HasLayer() &&
                         container_object->StyleRef().HasPerspective();
  if (has_perspective && container_object != NearestAncestorForElement()) {
    has_perspective = false;

    if (StyleRef().Preserves3D() || transform.Creates3d()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kDifferentPerspectiveCBOrParent);
    }
  }

  if (has_perspective) {
    // Perspective on the container affects us, so we have to factor it in here.
    DCHECK(container_object->HasLayer());
    gfx::PointF perspective_origin;
    if (const auto* container_box = DynamicTo<LayoutBox>(container_object))
      perspective_origin = container_box->PerspectiveOrigin(size);

    gfx::Transform perspective_matrix;
    perspective_matrix.ApplyPerspectiveDepth(
        container_object->StyleRef().UsedPerspective());
    perspective_matrix.ApplyTransformOrigin(perspective_origin.x(),
                                            perspective_origin.y(), 0);

    transform = perspective_matrix * transform;
  }
}

gfx::PointF LayoutObject::LocalToAncestorPoint(
    const gfx::PointF& local_point,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 local_point);
  MapLocalToAncestor(ancestor, transform_state, mode);
  transform_state.Flatten();

  return transform_state.LastPlanarPoint();
}

PhysicalRect LayoutObject::LocalToAncestorRect(
    const PhysicalRect& rect,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  return PhysicalRect::EnclosingRect(
      LocalToAncestorQuad(gfx::QuadF(gfx::RectF(rect)), ancestor, mode)
          .BoundingBox());
}

gfx::QuadF LayoutObject::LocalToAncestorQuad(
    const gfx::QuadF& local_quad,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  // Track the point at the center of the quad's bounding box. As
  // MapLocalToAncestor() calls OffsetFromContainer(), it will use that point
  // as the reference point to decide which column's transform to apply in
  // multiple-column blocks.
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 local_quad.BoundingBox().CenterPoint(),
                                 local_quad);
  MapLocalToAncestor(ancestor, transform_state, mode);
  transform_state.Flatten();

  return transform_state.LastPlanarQuad();
}

void LayoutObject::LocalToAncestorRects(
    Vector<PhysicalRect>& rects,
    const LayoutBoxModelObject* ancestor,
    const PhysicalOffset& pre_offset,
    const PhysicalOffset& post_offset) const {
  NOT_DESTROYED();
  for (wtf_size_t i = 0; i < rects.size(); ++i) {
    PhysicalRect& rect = rects[i];
    rect.Move(pre_offset);
    gfx::QuadF container_quad =
        LocalToAncestorQuad(gfx::QuadF(gfx::RectF(rect)), ancestor);
    PhysicalRect container_rect =
        PhysicalRect::EnclosingRect(container_quad.BoundingBox());
    if (container_rect.IsEmpty()) {
      rects.EraseAt(i--);
      continue;
    }
    container_rect.Move(post_offset);
    rects[i] = container_rect;
  }
}

gfx::Transform LayoutObject::LocalToAncestorTransform(
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  DCHECK(!(mode & kIgnoreTransforms));
  TransformState transform_state(TransformState::kApplyTransformDirection);
  MapLocalToAncestor(ancestor, transform_state, mode);
  return transform_state.AccumulatedTransform();
}

bool LayoutObject::OffsetForContainerDependsOnPoint(
    const LayoutObject* container) const {
  return IsLayoutFlowThread() ||
         (container->StyleRef().IsFlippedBlocksWritingMode() &&
          container->IsBox());
}

PhysicalOffset LayoutObject::OffsetFromContainer(
    const LayoutObject* o,
    bool ignore_scroll_offset) const {
  NOT_DESTROYED();
  return OffsetFromContainerInternal(o, ignore_scroll_offset);
}

PhysicalOffset LayoutObject::OffsetFromContainerInternal(
    const LayoutObject* o,
    bool ignore_scroll_offset) const {
  NOT_DESTROYED();
  DCHECK_EQ(o, Container());
  return o->IsScrollContainer()
             ? OffsetFromScrollableContainer(o, ignore_scroll_offset)
             : PhysicalOffset();
}

PhysicalOffset LayoutObject::OffsetFromScrollableContainer(
    const LayoutObject* container,
    bool ignore_scroll_offset) const {
  NOT_DESTROYED();
  DCHECK(container->IsScrollContainer());

  if (IsFixedPositioned() && container->IsLayoutView())
    return PhysicalOffset();

  const auto* box = To<LayoutBox>(container);
  if (!ignore_scroll_offset)
    return -box->ScrolledContentOffset();

  // ScrollOrigin accounts for other writing modes whose content's origin is not
  // at the top-left.
  return PhysicalOffset(box->GetScrollableArea()->ScrollOrigin());
}

PhysicalOffset LayoutObject::OffsetFromAncestor(
    const LayoutObject* ancestor_container) const {
  NOT_DESTROYED();
  if (ancestor_container == this)
    return PhysicalOffset();

  PhysicalOffset offset;
  PhysicalOffset reference_point;
  const LayoutObject* curr_container = this;
  AncestorSkipInfo skip_info(ancestor_container);
  do {
    const LayoutObject* next_container = curr_container->Container(&skip_info);

    // This means we reached the top without finding container.
    CHECK(next_container);
    if (!next_container)
      break;
    DCHECK(!curr_container->HasTransformRelatedProperty());
    PhysicalOffset current_offset =
        curr_container->OffsetFromContainer(next_container);
    offset += current_offset;
    reference_point += current_offset;
    curr_container = next_container;
  } while (curr_container != ancestor_container &&
           !skip_info.AncestorSkipped());
  if (skip_info.AncestorSkipped()) {
    DCHECK(curr_container);
    offset -= ancestor_container->OffsetFromAncestor(curr_container);
  }

  return offset;
}

LayoutRect LayoutObject::LocalCaretRect(
    const InlineBox*,
    int,
    LayoutUnit* extra_width_to_end_of_line) const {
  NOT_DESTROYED();
  if (extra_width_to_end_of_line)
    *extra_width_to_end_of_line = LayoutUnit();

  return LayoutRect();
}

bool LayoutObject::IsRooted() const {
  NOT_DESTROYED();
  const LayoutObject* object = this;
  while (object->Parent() && !object->HasLayer())
    object = object->Parent();
  if (object->HasLayer())
    return To<LayoutBoxModelObject>(object)->Layer()->Root()->IsRootLayer();
  return false;
}

RespectImageOrientationEnum LayoutObject::ShouldRespectImageOrientation(
    const LayoutObject* layout_object) {
  if (layout_object && layout_object->Style() &&
      layout_object->StyleRef().RespectImageOrientation() !=
          kRespectImageOrientation)
    return kDoNotRespectImageOrientation;

  return kRespectImageOrientation;
}

inline void LayoutObject::ClearLayoutRootIfNeeded() const {
  NOT_DESTROYED();
  if (LocalFrameView* view = GetFrameView()) {
    if (!DocumentBeingDestroyed())
      view->ClearLayoutSubtreeRoot(*this);
  }
}

void LayoutObject::WillBeDestroyed() {
  NOT_DESTROYED();
  // Destroy any leftover anonymous children.
  LayoutObjectChildList* children = VirtualChildren();
  if (children)
    children->DestroyLeftoverChildren();

  if (LocalFrame* frame = GetFrame()) {
    // If this layoutObject is being autoscrolled, stop the autoscrolling.
    if (frame->GetPage())
      frame->GetPage()->GetAutoscrollController().StopAutoscrollIfNeeded(this);
  }

  Remove();

  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->Remove(this);

  // If this layoutObject had a parent, remove should have destroyed any
  // counters attached to this layoutObject and marked the affected other
  // counters for reevaluation. This apparently redundant check is here for the
  // case when this layoutObject had no parent at the time remove() was called.

  if (HasCounterNodeMap())
    LayoutCounter::DestroyCounterNodes(*this);

  // Remove the handler if node had touch-action set. Handlers are not added
  // for text nodes so don't try removing for one too. Need to check if
  // m_style is null in cases of partial construction. Any handler we added
  // previously may have already been removed by the Document independently.
  if (GetNode() && !GetNode()->IsTextNode() && style_ &&
      style_->GetTouchAction() != TouchAction::kAuto) {
    EventHandlerRegistry& registry =
        GetDocument().GetFrame()->GetEventHandlerRegistry();
    if (registry.EventHandlerTargets(EventHandlerRegistry::kTouchAction)
            ->Contains(GetNode())) {
      registry.DidRemoveEventHandler(*GetNode(),
                                     EventHandlerRegistry::kTouchAction);
    }
  }

  SetAncestorLineBoxDirty(false);

  ClearLayoutRootIfNeeded();

  // Remove this object as ImageResourceObserver.
  if (style_ && !IsText())
    UpdateImageObservers(style_.get(), nullptr);

  // We must have removed all image observers.
  SECURITY_CHECK(!bitfields_.RegisteredAsFirstLineImageObserver());
#if DCHECK_IS_ON()
  SECURITY_DCHECK(as_image_observer_count_ == 0u);
#endif

  if (GetFrameView()) {
    GetFrameView()->RemovePendingTransformUpdate(*this);
    GetFrameView()->RemovePendingOpacityUpdate(*this);
    SetIsBackgroundAttachmentFixedObject(false);
  }
}

DISABLE_CFI_PERF
void LayoutObject::InsertedIntoTree() {
  NOT_DESTROYED();
  // FIXME: We should DCHECK(isRooted()) here but generated content makes some
  // out-of-order insertion.

  bitfields_.SetMightTraversePhysicalFragments(
      MightTraversePhysicalFragments(*this));

  // Keep our layer hierarchy updated. Optimize for the common case where we
  // don't have any children and don't have a layer attached to ourselves.
  PaintLayer* layer = nullptr;
  if (SlowFirstChild() || HasLayer()) {
    layer = Parent()->EnclosingLayer();
    AddLayers(layer);
  }

  // If |this| is visible but this object was not, tell the layer it has some
  // visible content that needs to be drawn and layer visibility optimization
  // can't be used
  if (Parent()->StyleRef().Visibility() != EVisibility::kVisible &&
      StyleRef().Visibility() == EVisibility::kVisible && !HasLayer()) {
    if (!layer)
      layer = Parent()->EnclosingLayer();
    if (layer)
      layer->DirtyVisibleContentStatus();
  }

  // |FirstInlineFragment()| should be cleared. |LayoutObjectChildList| does
  // this, just check here for all new objects in the tree.
  DCHECK(!HasInlineFragments());

  if (Parent()->ChildrenInline())
    Parent()->DirtyLinesFromChangedChild(this);

  if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock())
    flow_thread->FlowThreadDescendantWasInserted(this);

  if (const Element* element = DynamicTo<Element>(GetNode());
      element && element->HasAnchoredPopover()) {
    MarkMayHaveAnchorQuery();
  } else if (MayHaveAnchorQuery()) {
    Parent()->MarkMayHaveAnchorQuery();
  }
}

enum FindReferencingScrollAnchorsBehavior { kDontClear, kClear };

static bool FindReferencingScrollAnchors(
    LayoutObject* layout_object,
    FindReferencingScrollAnchorsBehavior behavior) {
  PaintLayer* layer = nullptr;
  if (LayoutObject* parent = layout_object->Parent())
    layer = parent->EnclosingLayer();
  bool found = false;

  // Walk up the layer tree to clear any scroll anchors that reference us.
  while (layer) {
    if (PaintLayerScrollableArea* scrollable_area =
            layer->GetScrollableArea()) {
      ScrollAnchor* anchor = scrollable_area->GetScrollAnchor();
      DCHECK(anchor);
      if (anchor->RefersTo(layout_object)) {
        found = true;
        if (behavior == kClear)
          anchor->NotifyRemoved(layout_object);
        else
          return true;
      }
    }
    layer = layer->Parent();
  }
  return found;
}

void LayoutObject::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  // FIXME: We should DCHECK(isRooted()) but we have some out-of-order removals
  // which would need to be fixed first.

  // If we remove a visible child from an invisible parent, we don't know the
  // layer visibility any more.
  PaintLayer* layer = nullptr;
  if (Parent()->StyleRef().Visibility() != EVisibility::kVisible &&
      StyleRef().Visibility() == EVisibility::kVisible && !HasLayer()) {
    layer = Parent()->EnclosingLayer();
    if (layer)
      layer->DirtyVisibleContentStatus();
  }

  // Keep our layer hierarchy updated.
  if (SlowFirstChild() || HasLayer()) {
    if (!layer)
      layer = Parent()->EnclosingLayer();
    RemoveLayers(layer);
  }

  if (IsOutOfFlowPositioned() && Parent()->ChildrenInline())
    Parent()->DirtyLinesFromChangedChild(this);

  RemoveFromLayoutFlowThread();

  // Update cached boundaries in SVG layoutObjects if a child is removed.
  if (Parent()->IsSVG())
    Parent()->SetNeedsBoundariesUpdate();

  if (bitfields_.IsScrollAnchorObject()) {
    // Clear the bit first so that anchor.clear() doesn't recurse into
    // findReferencingScrollAnchors.
    bitfields_.SetIsScrollAnchorObject(false);
    FindReferencingScrollAnchors(this, kClear);
  }

  if (LocalFrameView* frame_view = GetFrameView())
    frame_view->GetPaintTimingDetector().LayoutObjectWillBeDestroyed(*this);
}

void LayoutObject::SetNeedsPaintPropertyUpdate() {
  NOT_DESTROYED();
  SetNeedsPaintPropertyUpdatePreservingCachedRects();
  InvalidateIntersectionObserverCachedRects();
}

void LayoutObject::SetNeedsPaintPropertyUpdatePreservingCachedRects() {
  NOT_DESTROYED();
  DCHECK(!GetDocument().InPostLifecycleSteps());
  if (bitfields_.NeedsPaintPropertyUpdate())
    return;

  // Anytime a layout object needs a paint property update, we should also do
  // intersection observation.
  // TODO(vmpstr): Figure out if there's a cleaner way to do this outside of
  // this function, since this is potentially called many times for a single
  // frame view subtree.
  GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);

  bitfields_.SetNeedsPaintPropertyUpdate(true);
  if (Parent())
    Parent()->SetDescendantNeedsPaintPropertyUpdate();
}

void LayoutObject::SetDescendantNeedsPaintPropertyUpdate() {
  NOT_DESTROYED();
  for (auto* ancestor = this;
       ancestor && !ancestor->DescendantNeedsPaintPropertyUpdate();
       ancestor = ancestor->Parent()) {
    ancestor->bitfields_.SetDescendantNeedsPaintPropertyUpdate(true);
  }
}

void LayoutObject::ForceAllAncestorsNeedPaintPropertyUpdate() {
  NOT_DESTROYED();
  LayoutObject* ancestor = Parent();
  while (ancestor) {
    ancestor->SetNeedsPaintPropertyUpdate();
    ancestor = ancestor->Parent();
  }
}

void LayoutObject::MaybeClearIsScrollAnchorObject() {
  NOT_DESTROYED();
  if (!bitfields_.IsScrollAnchorObject())
    return;
  bitfields_.SetIsScrollAnchorObject(
      FindReferencingScrollAnchors(this, kDontClear));
}

void LayoutObject::RemoveFromLayoutFlowThread() {
  NOT_DESTROYED();
  if (!IsInsideFlowThread())
    return;

  // Sometimes we remove the element from the flow, but it's not destroyed at
  // that time.
  // It's only until later when we actually destroy it and remove all the
  // children from it.
  // Currently, that happens for firstLetter elements and list markers.
  // Pass in the flow thread so that we don't have to look it up for all the
  // children.
  // If we're a column spanner, we need to use our parent to find the flow
  // thread, since a spanner doesn't have the flow thread in its containing
  // block chain. We still need to notify the flow thread when the layoutObject
  // removed happens to be a spanner, so that we get rid of the spanner
  // placeholder, and column sets around the placeholder get merged.
  LayoutFlowThread* flow_thread = IsColumnSpanAll()
                                      ? Parent()->FlowThreadContainingBlock()
                                      : FlowThreadContainingBlock();
  RemoveFromLayoutFlowThreadRecursive(flow_thread);
}

void LayoutObject::RemoveFromLayoutFlowThreadRecursive(
    LayoutFlowThread* layout_flow_thread) {
  NOT_DESTROYED();
  if (const LayoutObjectChildList* children = VirtualChildren()) {
    for (LayoutObject* child = children->FirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsLayoutFlowThread())
        continue;  // Don't descend into inner fragmentation contexts.
      child->RemoveFromLayoutFlowThreadRecursive(
          child->IsLayoutFlowThread() ? To<LayoutFlowThread>(child)
                                      : layout_flow_thread);
    }
  }

  if (layout_flow_thread && layout_flow_thread != this)
    layout_flow_thread->FlowThreadDescendantWillBeRemoved(this);
  SetIsInsideFlowThread(false);
  CHECK(!SpannerPlaceholder());
}

void LayoutObject::DestroyAndCleanupAnonymousWrappers(
    bool performing_reattach) {
  NOT_DESTROYED();
  // If the tree is destroyed, there is no need for a clean-up phase.
  if (DocumentBeingDestroyed()) {
    Destroy();
    return;
  }

  LayoutObject* destroy_root = this;
  LayoutObject* destroy_root_parent = destroy_root->Parent();
  for (; destroy_root_parent && destroy_root_parent->IsAnonymous();
       destroy_root = destroy_root_parent,
       destroy_root_parent = destroy_root_parent->Parent()) {
    // Anonymous block continuations are tracked and destroyed elsewhere (see
    // the bottom of LayoutBlockFlow::RemoveChild)
    auto* destroy_root_parent_block =
        DynamicTo<LayoutBlockFlow>(destroy_root_parent);
    if (destroy_root_parent_block &&
        destroy_root_parent_block->IsAnonymousBlockContinuation())
      break;
    // A flow thread is tracked by its containing block. Whether its children
    // are removed or not is irrelevant.
    if (destroy_root_parent->IsLayoutFlowThread())
      break;
    // The anonymous fieldset contents wrapper should be kept.
    if (destroy_root_parent->Parent() &&
        destroy_root_parent->Parent()->IsLayoutNGFieldset())
      break;
    // RubyBase should be kept if RubyText exists
    if (destroy_root_parent->IsRubyBase()) {
      auto* ruby_run = DynamicTo<LayoutRubyRun>(destroy_root_parent->Parent());
      if (ruby_run && ruby_run->HasRubyText())
        break;
    }

    // We need to keep the anonymous parent, if it won't become empty by the
    // removal of this LayoutObject.
    if (destroy_root->PreviousSibling())
      break;
    if (const LayoutObject* sibling = destroy_root->NextSibling()) {
      if (destroy_root->GetNode()) {
        // When there are inline continuations, there may be multiple layout
        // objects generated from the same node, and those are special. They
        // will be removed as part of destroying |this|, in
        // LayoutInline::WillBeDestroyed(). So if that's all we have left, we
        // need to realize now that the anonymous containing block will become
        // empty. So we have to destroy it.
        while (sibling && sibling->GetNode() == destroy_root->GetNode())
          sibling = sibling->NextSibling();
      }
      if (sibling)
        break;
      DCHECK(destroy_root->IsLayoutInline());
      DCHECK(To<LayoutInline>(destroy_root)->Continuation());
    }
  }

  if (!performing_reattach && destroy_root_parent) {
    while (destroy_root_parent->IsAnonymous())
      destroy_root_parent = destroy_root_parent->Parent();
    GetDocument().GetStyleEngine().DetachedFromParent(destroy_root_parent);
  }

  destroy_root->Destroy();

  // WARNING: |this| is deleted here.
}

void LayoutObject::Destroy() {
  NOT_DESTROYED();
  DCHECK(
      g_allow_destroying_layout_object_in_finalizer ||
      !ThreadState::IsSweepingOnOwningThread(*ThreadStateStorage::Current()));

  // Mark as being destroyed to avoid trouble with merges in |RemoveChild()| and
  // other house keepings.
  bitfields_.SetBeingDestroyed(true);
  WillBeDestroyed();
#if DCHECK_IS_ON()
  DCHECK(!has_ax_object_);
  is_destroyed_ = true;
#endif
}

PositionWithAffinity LayoutObject::PositionForPoint(
    const PhysicalOffset&) const {
  NOT_DESTROYED();
  // NG codepath requires |kPrePaintClean|.
  // |SelectionModifier| calls this only in legacy codepath.
  DCHECK(!IsLayoutNGObject() || GetDocument().Lifecycle().GetState() >=
                                    DocumentLifecycle::kPrePaintClean);
  return CreatePositionWithAffinity(0);
}

bool LayoutObject::CanHaveAdditionalCompositingReasons() const {
  NOT_DESTROYED();
  return false;
}

CompositingReasons LayoutObject::AdditionalCompositingReasons() const {
  NOT_DESTROYED();
  return CompositingReason::kNone;
}

bool LayoutObject::HitTestAllPhases(HitTestResult& result,
                                    const HitTestLocation& hit_test_location,
                                    const PhysicalOffset& accumulated_offset) {
  NOT_DESTROYED();
  if (NodeAtPoint(result, hit_test_location, accumulated_offset,
                  HitTestPhase::kForeground)) {
    return true;
  }
  if (NodeAtPoint(result, hit_test_location, accumulated_offset,
                  HitTestPhase::kFloat)) {
    return true;
  }
  if (NodeAtPoint(result, hit_test_location, accumulated_offset,
                  HitTestPhase::kDescendantBlockBackgrounds)) {
    return true;
  }
  if (NodeAtPoint(result, hit_test_location, accumulated_offset,
                  HitTestPhase::kSelfBlockBackground)) {
    return true;
  }
  return false;
}

Node* LayoutObject::NodeForHitTest() const {
  NOT_DESTROYED();
  if (Node* node = GetNode())
    return node;

  // If we hit the anonymous layoutObjects inside generated content we should
  // actually hit the generated content so walk up to the PseudoElement.
  if (const LayoutObject* parent = Parent()) {
    if (parent->IsBeforeOrAfterContent() || parent->IsMarkerContent() ||
        parent->StyleRef().StyleType() == kPseudoIdFirstLetter) {
      for (; parent; parent = parent->Parent()) {
        if (Node* node = parent->GetNode())
          return node;
      }
    }
  }

  return nullptr;
}

void LayoutObject::UpdateHitTestResult(HitTestResult& result,
                                       const PhysicalOffset& point) const {
  NOT_DESTROYED();
  if (result.InnerNode())
    return;

  if (Node* n = NodeForHitTest())
    result.SetNodeAndPosition(n, point);
}

bool LayoutObject::NodeAtPoint(HitTestResult&,
                               const HitTestLocation&,
                               const PhysicalOffset&,
                               HitTestPhase) {
  NOT_DESTROYED();
  return false;
}

void LayoutObject::ScheduleRelayout() {
  NOT_DESTROYED();
  if (auto* layout_view = DynamicTo<LayoutView>(this)) {
    if (LocalFrameView* view = layout_view->GetFrameView())
      view->ScheduleRelayout();
  } else {
    if (IsRooted()) {
      layout_view = View();
      if (layout_view) {
        if (LocalFrameView* frame_view = layout_view->GetFrameView())
          frame_view->ScheduleRelayoutOfSubtree(this);
      }
    }
  }
}

void LayoutObject::ForceLayout() {
  NOT_DESTROYED();
  SetSelfNeedsLayoutForAvailableSpace(true);
  UpdateLayout();
}

const ComputedStyle* LayoutObject::FirstLineStyleWithoutFallback() const {
  NOT_DESTROYED();
  DCHECK(GetDocument().GetStyleEngine().UsesFirstLineRules());

  // Normal markers don't use ::first-line styles in Chromium, so be consistent
  // and return null for content markers. This may need to change depending on
  // https://github.com/w3c/csswg-drafts/issues/4506
  if (IsMarkerContent())
    return nullptr;
  if (IsText()) {
    if (!Parent())
      return nullptr;
    return Parent()->FirstLineStyleWithoutFallback();
  }

  if (BehavesLikeBlockContainer()) {
    if (const ComputedStyle* cached =
            StyleRef().GetCachedPseudoElementStyle(kPseudoIdFirstLine)) {
      // If the style is cached by getComputedStyle(element, "::first-line"), it
      // is marked with IsEnsuredInDisplayNone(). In that case we might not have
      // the correct ::first-line style for laying out the ::first-line. Ignore
      // the cached ComputedStyle and overwrite it using
      // ReplaceCachedPseudoElementStyle() below.
      if (!cached->IsEnsuredInDisplayNone())
        return cached;
    }

    if (Element* element = DynamicTo<Element>(GetNode())) {
      if (element->ShadowPseudoId() ==
          shadow_element_names::kPseudoInternalInputSuggested) {
        // Disable ::first-line style for autofill previews. See
        // crbug.com/1227170.
        return nullptr;
      }
    }

    for (const LayoutBlock* first_line_block = To<LayoutBlock>(this);
         first_line_block;
         first_line_block = first_line_block->FirstLineStyleParentBlock()) {
      const ComputedStyle& style = first_line_block->StyleRef();
      if (!style.HasPseudoElementStyle(kPseudoIdFirstLine))
        continue;
      if (first_line_block == this) {
        if (const ComputedStyle* cached =
                first_line_block->GetCachedPseudoElementStyle(
                    kPseudoIdFirstLine)) {
          return cached;
        }
        continue;
      }

      // We can't use first_line_block->GetCachedPseudoElementStyle() because
      // it's based on first_line_block's style. We need to get the uncached
      // first line style based on this object's style and cache the result in
      // it.
      if (scoped_refptr<ComputedStyle> first_line_style =
              first_line_block->GetUncachedPseudoElementStyle(
                  StyleRequest(kPseudoIdFirstLine, Style()))) {
        return StyleRef().ReplaceCachedPseudoElementStyle(
            std::move(first_line_style), kPseudoIdFirstLine, g_null_atom);
      }
    }
  } else if (!IsAnonymous() && IsLayoutInline() &&
             !GetNode()->IsFirstLetterPseudoElement()) {
    if (const ComputedStyle* cached =
            StyleRef().GetCachedPseudoElementStyle(kPseudoIdFirstLineInherited))
      return cached;

    if (const ComputedStyle* parent_first_line_style =
            Parent()->FirstLineStyleWithoutFallback()) {
      // A first-line style is in effect. Get uncached first line style based on
      // parent_first_line_style and cache the result in this object's style.
      if (scoped_refptr<ComputedStyle> first_line_style =
              GetUncachedPseudoElementStyle(StyleRequest(
                  kPseudoIdFirstLineInherited, parent_first_line_style))) {
        return StyleRef().AddCachedPseudoElementStyle(
            std::move(first_line_style), kPseudoIdFirstLineInherited,
            g_null_atom);
      }
    }
  }
  return nullptr;
}

const ComputedStyle* LayoutObject::GetCachedPseudoElementStyle(
    PseudoId pseudo) const {
  NOT_DESTROYED();
  DCHECK_NE(pseudo, kPseudoIdBefore);
  DCHECK_NE(pseudo, kPseudoIdAfter);
  if (!GetNode())
    return nullptr;

  Element* element = Traversal<Element>::FirstAncestorOrSelf(*GetNode());
  if (!element)
    return nullptr;

  return element->CachedStyleForPseudoElement(pseudo);
}

scoped_refptr<ComputedStyle> LayoutObject::GetUncachedPseudoElementStyle(
    const StyleRequest& request) const {
  NOT_DESTROYED();
  DCHECK_NE(request.pseudo_id, kPseudoIdBefore);
  DCHECK_NE(request.pseudo_id, kPseudoIdAfter);
  if (!GetNode())
    return nullptr;

  Element* element = Traversal<Element>::FirstAncestorOrSelf(*GetNode());
  if (!element)
    return nullptr;
  if (element->IsPseudoElement() &&
      request.pseudo_id != kPseudoIdFirstLineInherited)
    return nullptr;

  return element->UncachedStyleForPseudoElement(request);
}

const ComputedStyle* LayoutObject::GetSelectionStyle() const {
  if (UsesHighlightPseudoInheritance(kPseudoIdSelection)) {
    if (!StyleRef().HighlightData()) {
      return nullptr;
    }
    return StyleRef().HighlightData()->Selection();
  }
  return GetCachedPseudoElementStyle(kPseudoIdSelection);
}

void LayoutObject::AddAnnotatedRegions(Vector<AnnotatedRegionValue>& regions) {
  NOT_DESTROYED();
  // Convert the style regions to absolute coordinates.
  if (StyleRef().Visibility() != EVisibility::kVisible || !IsBox())
    return;

  if (StyleRef().DraggableRegionMode() == EDraggableRegionMode::kNone)
    return;

  auto* box = To<LayoutBox>(this);
  PhysicalRect local_bounds = box->PhysicalBorderBoxRect();
  PhysicalRect abs_bounds = LocalToAbsoluteRect(local_bounds);

  AnnotatedRegionValue region;
  region.draggable =
      StyleRef().DraggableRegionMode() == EDraggableRegionMode::kDrag;
  region.bounds = abs_bounds;
  regions.push_back(region);
}

bool LayoutObject::WillRenderImage() {
  NOT_DESTROYED();
  // Without visibility we won't render (and therefore don't care about
  // animation).
  if (StyleRef().Visibility() != EVisibility::kVisible)
    return false;

  // We will not render a new image when ExecutionContext is paused
  if (GetDocument().GetExecutionContext()->IsContextPaused())
    return false;

  // Suspend animations when the page is not visible.
  if (GetDocument().hidden())
    return false;

  // If we're not in a window (i.e., we're dormant from being in a background
  // tab) then we don't want to render either.
  return GetDocument().View()->IsVisible();
}

bool LayoutObject::GetImageAnimationPolicy(
    mojom::blink::ImageAnimationPolicy& policy) {
  NOT_DESTROYED();
  if (!GetDocument().GetSettings())
    return false;
  policy = GetDocument().GetSettings()->GetImageAnimationPolicy();
  return true;
}

bool LayoutObject::IsInsideListMarker() const {
  NOT_DESTROYED();
  return (IsListMarkerForNormalContent() &&
          To<LayoutListMarker>(this)->IsInside()) ||
         IsInsideListMarkerForCustomContent();
}

bool LayoutObject::IsOutsideListMarker() const {
  NOT_DESTROYED();
  return (IsListMarkerForNormalContent() &&
          !To<LayoutListMarker>(this)->IsInside()) ||
         IsOutsideListMarkerForCustomContent();
}

void LayoutObject::ImageChanged(ImageResourceContent* image,
                                CanDeferInvalidation defer) {
  NOT_DESTROYED();
  DCHECK(node_);

  // Image change notifications should not be received during paint because
  // the resulting invalidations will be cleared following paint. This can also
  // lead to modifying the tree out from under paint(), see: crbug.com/616700.
  DCHECK_NE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::LifecycleState::kInPaint);

  ImageChanged(static_cast<WrappedImagePtr>(image), defer);
}

void LayoutObject::ImageNotifyFinished(ImageResourceContent* image) {
  NOT_DESTROYED();
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ImageLoaded(this);

  if (LocalDOMWindow* window = GetDocument().domWindow())
    ImageElementTiming::From(*window).NotifyImageFinished(*this, image);
  if (LocalFrameView* frame_view = GetFrameView())
    frame_view->GetPaintTimingDetector().NotifyImageFinished(*this, image);
}

Element* LayoutObject::OffsetParent(const Element* base) const {
  NOT_DESTROYED();
  if (IsDocumentElement() || IsBody())
    return nullptr;

  if (IsFixedPositioned())
    return nullptr;

  HashSet<Member<TreeScope>> ancestor_tree_scopes;
  if (base)
    ancestor_tree_scopes = base->GetAncestorTreeScopes();

  float effective_zoom = StyleRef().EffectiveZoom();
  Node* node = nullptr;
  for (LayoutObject* ancestor = Parent(); ancestor;
       ancestor = ancestor->Parent()) {
    // Spec: http://www.w3.org/TR/cssom-view/#offset-attributes

    node = ancestor->GetNode();

    if (!node)
      continue;

    // If |base| was provided, then we should not return an Element which is
    // closed shadow hidden from |base|. If we keep going up the flat tree, then
    // we will eventually get to a node which is not closed shadow hidden from
    // |base|. https://github.com/w3c/csswg-drafts/issues/159
    // TODO(crbug.com/920069): Remove the feature check here when the feature
    // has gotten to stable without any issues.
    if (RuntimeEnabledFeatures::OffsetParentNewSpecBehaviorEnabled() && base &&
        !ancestor_tree_scopes.Contains(&node->GetTreeScope())) {
      // If 'position: fixed' node is found while traversing up, terminate the
      // loop and return null.
      if (ancestor->IsFixedPositioned())
        return nullptr;
      continue;
    }

    // TODO(kochi): If |base| or |node| is nested deep in shadow roots, this
    // loop may get expensive, as IsClosedShadowHiddenFrom() can take up to
    // O(N+M) time (N and M are depths).
    // TODO(crbug.com/920069): Remove this when the feature has gotten to stable
    // without any issues.
    if (!RuntimeEnabledFeatures::OffsetParentNewSpecBehaviorEnabled() && base &&
        (node->IsClosedShadowHiddenFrom(*base) ||
         (node->IsInShadowTree() &&
          node->ContainingShadowRoot()->IsUserAgent()))) {
      // If 'position: fixed' node is found while traversing up, terminate the
      // loop and return null.
      if (ancestor->IsFixedPositioned())
        return nullptr;
      continue;
    }

    if (ancestor->CanContainAbsolutePositionObjects())
      break;

    if (IsA<HTMLBodyElement>(*node))
      break;

    if (!IsPositioned() &&
        (IsA<HTMLTableElement>(*node) || IsA<HTMLTableCellElement>(*node)))
      break;

    // Webkit specific extension where offsetParent stops at zoom level changes.
    if (effective_zoom != ancestor->StyleRef().EffectiveZoom())
      break;
  }

  return DynamicTo<Element>(node);
}

void LayoutObject::NotifyImageFullyRemoved(ImageResourceContent* image) {
  NOT_DESTROYED();
  if (LocalDOMWindow* window = GetDocument().domWindow())
    ImageElementTiming::From(*window).NotifyImageRemoved(this, image);
  if (LocalFrameView* frame_view = GetFrameView())
    frame_view->GetPaintTimingDetector().NotifyImageRemoved(*this, image);
}

PositionWithAffinity LayoutObject::CreatePositionWithAffinity(
    int offset,
    TextAffinity affinity) const {
  NOT_DESTROYED();
  // If this is a non-anonymous layoutObject in an editable area, then it's
  // simple.
  Node* const node = NonPseudoNode();
  if (!node)
    return FindPosition();
  return AdjustForEditingBoundary(
      PositionWithAffinity(Position(node, offset), affinity));
}

PositionWithAffinity LayoutObject::FindPosition() const {
  NOT_DESTROYED();
  // We don't want to cross the boundary between editable and non-editable
  // regions of the document, but that is either impossible or at least
  // extremely unlikely in any normal case because we stop as soon as we
  // find a single non-anonymous layoutObject.

  // Find a nearby non-anonymous layoutObject.
  const LayoutObject* child = this;
  while (const LayoutObject* parent = child->Parent()) {
    // Find non-anonymous content after.
    for (const LayoutObject* layout_object = child->NextInPreOrder(parent);
         layout_object; layout_object = layout_object->NextInPreOrder(parent)) {
      if (const Node* node = layout_object->NonPseudoNode()) {
        return PositionWithAffinity(FirstPositionInOrBeforeNode(*node));
      }
    }

    // Find non-anonymous content before.
    for (const LayoutObject* layout_object = child->PreviousInPreOrder();
         layout_object; layout_object = layout_object->PreviousInPreOrder()) {
      if (layout_object == parent)
        break;
      if (const Node* node = layout_object->NonPseudoNode())
        return PositionWithAffinity(LastPositionInOrAfterNode(*node));
    }

    // Use the parent itself unless it too is anonymous.
    if (const Node* node = parent->NonPseudoNode())
      return PositionWithAffinity(FirstPositionInOrBeforeNode(*node));

    // Repeat at the next level up.
    child = parent;
  }

  // Everything was anonymous. Give up.
  return PositionWithAffinity();
}

PositionWithAffinity LayoutObject::FirstPositionInOrBeforeThis() const {
  NOT_DESTROYED();
  if (Node* node = NonPseudoNode())
    return AdjustForEditingBoundary(FirstPositionInOrBeforeNode(*node));
  return FindPosition();
}

PositionWithAffinity LayoutObject::LastPositionInOrAfterThis() const {
  NOT_DESTROYED();
  if (Node* node = NonPseudoNode())
    return AdjustForEditingBoundary(LastPositionInOrAfterNode(*node));
  return FindPosition();
}

PositionWithAffinity LayoutObject::PositionAfterThis() const {
  NOT_DESTROYED();
  if (Node* node = NonPseudoNode())
    return AdjustForEditingBoundary(Position::AfterNode(*node));
  return FindPosition();
}

PositionWithAffinity LayoutObject::PositionBeforeThis() const {
  NOT_DESTROYED();
  if (Node* node = NonPseudoNode())
    return AdjustForEditingBoundary(Position::BeforeNode(*node));
  return FindPosition();
}

PositionWithAffinity LayoutObject::CreatePositionWithAffinity(
    int offset) const {
  NOT_DESTROYED();
  return CreatePositionWithAffinity(offset, TextAffinity::kDownstream);
}

CursorDirective LayoutObject::GetCursor(const PhysicalOffset&,
                                        ui::Cursor&) const {
  NOT_DESTROYED();
  return kSetCursorBasedOnStyle;
}

bool LayoutObject::CanUpdateSelectionOnRootLineBoxes() const {
  NOT_DESTROYED();
  if (NeedsLayout())
    return false;

  const LayoutBlock* containing_block = ContainingBlock();
  return containing_block ? !containing_block->NeedsLayout() : false;
}

void LayoutObject::SetNeedsBoundariesUpdate() {
  NOT_DESTROYED();
  DCHECK(!GetDocument().InPostLifecycleSteps());
  if (IsSVGChild()) {
    // The boundaries affect mask clip and clip path mask/clip.
    if (StyleRef().MaskerResource() || StyleRef().HasClipPath())
      SetNeedsPaintPropertyUpdate();
  }
  if (LayoutObject* layout_object = Parent())
    layout_object->SetNeedsBoundariesUpdate();
}

gfx::RectF LayoutObject::ObjectBoundingBox() const {
  NOT_DESTROYED();
  NOTREACHED();
  return gfx::RectF();
}

gfx::RectF LayoutObject::StrokeBoundingBox() const {
  NOT_DESTROYED();
  NOTREACHED();
  return gfx::RectF();
}

gfx::RectF LayoutObject::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  NOTREACHED();
  return gfx::RectF();
}

AffineTransform LayoutObject::LocalSVGTransform() const {
  NOT_DESTROYED();
  return AffineTransform();
}

bool LayoutObject::IsRelayoutBoundary() const {
  NOT_DESTROYED();
  return ObjectIsRelayoutBoundary(this);
}

void LayoutObject::SetShouldInvalidateSelection() {
  NOT_DESTROYED();
  bitfields_.SetShouldInvalidateSelection(true);
  SetShouldCheckForPaintInvalidation();
}

void LayoutObject::SetShouldDoFullPaintInvalidation(
    PaintInvalidationReason reason) {
  NOT_DESTROYED();
  DCHECK(IsLayoutPaintInvalidationReason(reason));
  SetShouldCheckForPaintInvalidation();
  SetShouldDoFullPaintInvalidationWithoutLayoutChangeInternal(reason);
}

void LayoutObject::SetShouldDoFullPaintInvalidationWithoutLayoutChange(
    PaintInvalidationReason reason) {
  NOT_DESTROYED();
  DCHECK(IsNonLayoutFullPaintInvalidationReason(reason));
  // Use SetBackgroundNeedsFullPaintInvalidation() instead. See comment of the
  // function.
  DCHECK_NE(reason, PaintInvalidationReason::kBackground);
  SetShouldDoFullPaintInvalidationWithoutLayoutChangeInternal(reason);
}

void LayoutObject::SetShouldDoFullPaintInvalidationWithoutLayoutChangeInternal(
    PaintInvalidationReason reason) {
  NOT_DESTROYED();
  // Only full invalidation reasons are allowed.
  DCHECK(IsFullPaintInvalidationReason(reason));
  bitfields_.SetShouldDelayFullPaintInvalidation(false);
  if (reason > FullPaintInvalidationReason()) {
    SetShouldCheckForPaintInvalidationWithoutLayoutChange();
    full_paint_invalidation_reason_ = static_cast<unsigned>(reason);
    DCHECK_EQ(reason, FullPaintInvalidationReason());
  }
}

void LayoutObject::SetShouldCheckForPaintInvalidation() {
  NOT_DESTROYED();
  if (ShouldCheckLayoutForPaintInvalidation()) {
    DCHECK(ShouldCheckForPaintInvalidation());
    return;
  }
  GetFrameView()->ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  bitfields_.SetShouldCheckForPaintInvalidation(true);
  bitfields_.SetShouldCheckLayoutForPaintInvalidation(true);
  for (LayoutObject* ancestor = Parent();
       ancestor && !ancestor->DescendantShouldCheckLayoutForPaintInvalidation();
       ancestor = ancestor->Parent()) {
    ancestor->bitfields_.SetShouldCheckForPaintInvalidation(true);
    ancestor->bitfields_.SetDescendantShouldCheckLayoutForPaintInvalidation(
        true);
  }
}

void LayoutObject::SetShouldCheckForPaintInvalidationWithoutLayoutChange() {
  NOT_DESTROYED();
  if (ShouldCheckForPaintInvalidation()) {
    return;
  }
  GetFrameView()->ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  bitfields_.SetShouldCheckForPaintInvalidation(true);
  for (LayoutObject* ancestor = Parent();
       ancestor && !ancestor->ShouldCheckForPaintInvalidation();
       ancestor = ancestor->Parent()) {
    ancestor->bitfields_.SetShouldCheckForPaintInvalidation(true);
  }
}

void LayoutObject::SetSubtreeShouldCheckForPaintInvalidation() {
  NOT_DESTROYED();
  if (SubtreeShouldCheckForPaintInvalidation()) {
    DCHECK(ShouldCheckForPaintInvalidation());
    return;
  }
  SetShouldCheckForPaintInvalidation();
  bitfields_.SetSubtreeShouldCheckForPaintInvalidation(true);
}

void LayoutObject::SetMayNeedPaintInvalidationAnimatedBackgroundImage() {
  NOT_DESTROYED();
  if (MayNeedPaintInvalidationAnimatedBackgroundImage())
    return;
  bitfields_.SetMayNeedPaintInvalidationAnimatedBackgroundImage(true);
  SetShouldCheckForPaintInvalidationWithoutLayoutChange();
}

void LayoutObject::SetShouldDelayFullPaintInvalidation() {
  NOT_DESTROYED();
  // Should have already set a full paint invalidation reason.
  DCHECK(IsFullPaintInvalidationReason(FullPaintInvalidationReason()));
  // Subtree full paint invalidation can't be delayed.
  if (bitfields_.SubtreeShouldDoFullPaintInvalidation()) {
    return;
  }

  bitfields_.SetShouldDelayFullPaintInvalidation(true);
  if (!ShouldCheckForPaintInvalidation()) {
    // This will also schedule a visual update.
    SetShouldCheckForPaintInvalidationWithoutLayoutChange();
  } else {
    // Schedule visual update for the next document cycle in which we will
    // check if the delayed invalidation should be promoted to a real
    // invalidation.
    GetFrameView()->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  }
}

void LayoutObject::ClearShouldDelayFullPaintInvalidation() {
  // This will clear ShouldDelayFullPaintInvalidation() flag.
  SetShouldDoFullPaintInvalidationWithoutLayoutChangeInternal(
      FullPaintInvalidationReason());
}

void LayoutObject::ClearPaintInvalidationFlags() {
  NOT_DESTROYED();
// PaintInvalidationStateIsDirty should be kept in sync with the
// booleans that are cleared below.
#if DCHECK_IS_ON()
  DCHECK(!ShouldCheckForPaintInvalidation() || PaintInvalidationStateIsDirty());
#endif
  if (!ShouldDelayFullPaintInvalidation()) {
    full_paint_invalidation_reason_ =
        static_cast<unsigned>(PaintInvalidationReason::kNone);
    bitfields_.SetBackgroundNeedsFullPaintInvalidation(false);
  }
  bitfields_.SetShouldCheckForPaintInvalidation(false);
  bitfields_.SetSubtreeShouldCheckForPaintInvalidation(false);
  bitfields_.SetSubtreeShouldDoFullPaintInvalidation(false);
  bitfields_.SetMayNeedPaintInvalidationAnimatedBackgroundImage(false);
  bitfields_.SetShouldCheckLayoutForPaintInvalidation(false);
  bitfields_.SetDescendantShouldCheckLayoutForPaintInvalidation(false);
  bitfields_.SetShouldInvalidateSelection(false);
}

#if DCHECK_IS_ON()
bool LayoutObject::PaintInvalidationStateIsDirty() const {
  NOT_DESTROYED();
  return BackgroundNeedsFullPaintInvalidation() ||
         ShouldCheckForPaintInvalidation() || ShouldInvalidateSelection() ||
         ShouldCheckLayoutForPaintInvalidation() ||
         DescendantShouldCheckLayoutForPaintInvalidation() ||
         ShouldDoFullPaintInvalidation() ||
         SubtreeShouldDoFullPaintInvalidation() ||
         MayNeedPaintInvalidationAnimatedBackgroundImage();
}
#endif

void LayoutObject::EnsureIsReadyForPaintInvalidation() {
  NOT_DESTROYED();
  DCHECK(!NeedsLayout() || ChildLayoutBlockedByDisplayLock());

  // Force full paint invalidation if the outline may be affected by descendants
  // and this object is marked for checking paint invalidation for any reason.
  if (bitfields_.OutlineMayBeAffectedByDescendants() ||
      bitfields_.PreviousOutlineMayBeAffectedByDescendants()) {
    SetShouldDoFullPaintInvalidationWithoutLayoutChange(
        PaintInvalidationReason::kOutline);
  }
  bitfields_.SetPreviousOutlineMayBeAffectedByDescendants(
      bitfields_.OutlineMayBeAffectedByDescendants());
}

void LayoutObject::ClearPaintFlags() {
  NOT_DESTROYED();
  DCHECK_EQ(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
  ClearPaintInvalidationFlags();
  bitfields_.SetNeedsPaintPropertyUpdate(false);
  bitfields_.SetEffectiveAllowedTouchActionChanged(false);
  bitfields_.SetBlockingWheelEventHandlerChanged(false);

  if (!ChildPrePaintBlockedByDisplayLock()) {
    bitfields_.SetDescendantNeedsPaintPropertyUpdate(false);
    bitfields_.SetDescendantEffectiveAllowedTouchActionChanged(false);
    bitfields_.SetDescendantBlockingWheelEventHandlerChanged(false);
    subtree_paint_property_update_reasons_ =
        static_cast<unsigned>(SubtreePaintPropertyUpdateReason::kNone);
  }
}

bool LayoutObject::IsAllowedToModifyLayoutTreeStructure(Document& document) {
  return document.Lifecycle().StateAllowsLayoutTreeMutations() ||
         document.GetStyleEngine().InContainerQueryStyleRecalc();
}

void LayoutObject::SetSubtreeShouldDoFullPaintInvalidation(
    PaintInvalidationReason reason) {
  NOT_DESTROYED();
  SetShouldDoFullPaintInvalidation(reason);
  bitfields_.SetSubtreeShouldDoFullPaintInvalidation(true);
}

void LayoutObject::SetIsBackgroundAttachmentFixedObject(
    bool is_background_attachment_fixed_object) {
  NOT_DESTROYED();
  DCHECK(GetFrameView());
  if (bitfields_.IsBackgroundAttachmentFixedObject() ==
      is_background_attachment_fixed_object)
    return;
  bitfields_.SetIsBackgroundAttachmentFixedObject(
      is_background_attachment_fixed_object);
  if (is_background_attachment_fixed_object)
    GetFrameView()->AddBackgroundAttachmentFixedObject(this);
  else
    GetFrameView()->RemoveBackgroundAttachmentFixedObject(this);
}

PhysicalRect LayoutObject::DebugRect() const {
  NOT_DESTROYED();
  return PhysicalRect();
}

void LayoutObject::InvalidateSelectedChildrenOnStyleChange() {
  NOT_DESTROYED();
  // LayoutSelection::Commit() propagates the state up the containing node
  // chain to
  // tell if a block contains selected nodes or not. If this layout object is
  // not a block, we need to get the selection state from the containing block
  // to tell if we have any selected node children.
  LayoutBlock* block =
      IsLayoutBlock() ? To<LayoutBlock>(this) : ContainingBlock();
  if (!block)
    return;
  if (!block->IsSelected())
    return;

  // ::selection style only applies to direct selection leaf children of the
  // element on which the ::selection style is set. Thus, we only walk the
  // direct children here.
  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->CanBeSelectionLeaf())
      continue;
    if (!child->IsSelected())
      continue;
    child->SetShouldInvalidateSelection();
  }
}

void LayoutObject::MarkEffectiveAllowedTouchActionChanged() {
  NOT_DESTROYED();
  DCHECK(!GetDocument().InPostLifecycleSteps());
  bitfields_.SetEffectiveAllowedTouchActionChanged(true);
  // If we're locked, mark our descendants as needing this change. This is used
  // a signal to ensure we mark the element as needing effective allowed
  // touch action recalculation when the element becomes unlocked.
  if (ChildPrePaintBlockedByDisplayLock()) {
    bitfields_.SetDescendantEffectiveAllowedTouchActionChanged(true);
    return;
  }

  if (Parent())
    Parent()->MarkDescendantEffectiveAllowedTouchActionChanged();
}

void LayoutObject::MarkDescendantEffectiveAllowedTouchActionChanged() {
  DCHECK(!GetDocument().InPostLifecycleSteps());
  LayoutObject* obj = this;
  while (obj && !obj->DescendantEffectiveAllowedTouchActionChanged()) {
    obj->bitfields_.SetDescendantEffectiveAllowedTouchActionChanged(true);
    if (obj->ChildPrePaintBlockedByDisplayLock())
      break;

    obj = obj->Parent();
  }
}

void LayoutObject::MarkBlockingWheelEventHandlerChanged() {
  DCHECK(!GetDocument().InPostLifecycleSteps());
  bitfields_.SetBlockingWheelEventHandlerChanged(true);
  // If we're locked, mark our descendants as needing this change. This is used
  // as a signal to ensure we mark the element as needing wheel event handler
  // recalculation when the element becomes unlocked.
  if (ChildPrePaintBlockedByDisplayLock()) {
    bitfields_.SetDescendantBlockingWheelEventHandlerChanged(true);
    return;
  }

  if (Parent())
    Parent()->MarkDescendantBlockingWheelEventHandlerChanged();
}

void LayoutObject::MarkDescendantBlockingWheelEventHandlerChanged() {
  DCHECK(!GetDocument().InPostLifecycleSteps());
  LayoutObject* obj = this;
  while (obj && !obj->DescendantBlockingWheelEventHandlerChanged()) {
    obj->bitfields_.SetDescendantBlockingWheelEventHandlerChanged(true);
    if (obj->ChildPrePaintBlockedByDisplayLock())
      break;

    obj = obj->Parent();
  }
}

// Note about ::first-letter pseudo-element:
//   When an element has ::first-letter pseudo-element, first letter characters
//   are taken from |Text| node and first letter characters are considered
//   as content of <pseudo:first-letter>.
//   For following HTML,
//      <style>div::first-letter {color: red}</style>
//      <div>abc</div>
//   we have following layout tree:
//      LayoutBlockFlow {DIV} at (0,0) size 784x55
//        LayoutInline {<pseudo:first-letter>} at (0,0) size 22x53
//          LayoutTextFragment (anonymous) at (0,1) size 22x53
//            text run at (0,1) width 22: "a"
//        LayoutTextFragment {#text} at (21,30) size 16x17
//          text run at (21,30) width 16: "bc"
//  In this case, |Text::layoutObject()| for "abc" returns |LayoutTextFragment|
//  containing "bc", and it is called remaining part.
//
//  Even if |Text| node contains only first-letter characters, e.g. just "a",
//  remaining part of |LayoutTextFragment|, with |fragmentLength()| == 0, is
//  appeared in layout tree.
//
//  When |Text| node contains only first-letter characters and whitespaces, e.g.
//  "B\n", associated |LayoutTextFragment| is first-letter part instead of
//  remaining part.
//
//  Punctuation characters are considered as first-letter. For "(1)ab",
//  "(1)" are first-letter part and "ab" are remaining part.
const LayoutObject* AssociatedLayoutObjectOf(const Node& node,
                                             int offset_in_node,
                                             LayoutObjectSide object_side) {
  DCHECK_GE(offset_in_node, 0);
  LayoutObject* layout_object = node.GetLayoutObject();
  if (!node.IsTextNode() || !layout_object ||
      !To<LayoutText>(layout_object)->IsTextFragment())
    return layout_object;
  auto* layout_text_fragment = To<LayoutTextFragment>(layout_object);
  if (!layout_text_fragment->IsRemainingTextLayoutObject()) {
    DCHECK_LE(
        static_cast<unsigned>(offset_in_node),
        layout_text_fragment->Start() + layout_text_fragment->FragmentLength());
    return layout_text_fragment;
  }
  if (layout_text_fragment->FragmentLength()) {
    const unsigned threshold =
        object_side == LayoutObjectSide::kRemainingTextIfOnBoundary
            ? layout_text_fragment->Start()
            : layout_text_fragment->Start() + 1;
    if (static_cast<unsigned>(offset_in_node) >= threshold)
      return layout_object;
  }
  return layout_text_fragment->GetFirstLetterPart();
}

bool LayoutObject::CanBeSelectionLeaf() const {
  NOT_DESTROYED();
  if (SlowFirstChild() || StyleRef().Visibility() != EVisibility::kVisible ||
      DisplayLockUtilities::LockedAncestorPreventingPaint(*this)) {
    return false;
  }
  return CanBeSelectionLeafInternal();
}

Vector<PhysicalRect> LayoutObject::CollectOutlineRectsAndAdvance(
    NGOutlineType outline_type,
    FragmentDataIterator& iterator) const {
  NOT_DESTROYED();
  Vector<PhysicalRect> outline_rects;
  PhysicalOffset paint_offset = iterator.GetFragmentData()->PaintOffset();

  if (iterator.Cursor()) {
    wtf_size_t fragment_index = iterator.Cursor()->ContainerFragmentIndex();
    do {
      const NGFragmentItem* item = iterator.Cursor()->Current().Item();
      if (!item)
        continue;
      if (const NGPhysicalBoxFragment* box_fragment = item->BoxFragment()) {
        box_fragment->AddSelfOutlineRects(
            paint_offset + item->OffsetInContainerFragment(), outline_type,
            &outline_rects, nullptr);
      } else {
        PhysicalRect rect;
        rect = item->RectInContainerFragment();
        rect.Move(paint_offset);
        outline_rects.push_back(rect);
      }
      // Keep going as long as we're within the same container fragment. If
      // we're block-fragmented, there will be multiple container fragments,
      // each with their own FragmentData object.
    } while (iterator.Advance() &&
             iterator.Cursor()->ContainerFragmentIndex() == fragment_index);
  } else {
    if (const NGPhysicalBoxFragment* box_fragment =
            iterator.GetPhysicalBoxFragment()) {
      box_fragment->AddSelfOutlineRects(paint_offset, outline_type,
                                        &outline_rects, nullptr);
    } else {
      outline_rects = OutlineRects(nullptr, paint_offset, outline_type);
    }
    iterator.Advance();
  }

  return outline_rects;
}

Vector<PhysicalRect> LayoutObject::OutlineRects(
    OutlineInfo* info,
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type) const {
  NOT_DESTROYED();
  Vector<PhysicalRect> outline_rects;
  AddOutlineRects(outline_rects, info, additional_offset, outline_type);
  return outline_rects;
}

void LayoutObject::SetModifiedStyleOutsideStyleRecalc(
    scoped_refptr<const ComputedStyle> style,
    ApplyStyleChanges apply_changes) {
  NOT_DESTROYED();
  SetStyle(style, apply_changes);
  if (IsAnonymous() || !GetNode() || !GetNode()->IsElementNode())
    return;
  GetNode()->SetComputedStyle(std::move(style));
}

LayoutUnit LayoutObject::FlipForWritingModeInternal(
    LayoutUnit position,
    LayoutUnit width,
    const LayoutBox* box_for_flipping) const {
  NOT_DESTROYED();
  DCHECK(!IsBox());
  DCHECK(HasFlippedBlocksWritingMode());
  DCHECK(!box_for_flipping || box_for_flipping == ContainingBlock());
  // For now, block flipping doesn't apply for non-box SVG objects.
  if (IsSVG())
    return position;
  return (box_for_flipping ? box_for_flipping : ContainingBlock())
      ->FlipForWritingMode(position, width);
}

bool LayoutObject::SelfPaintingLayerNeedsVisualOverflowRecalc() const {
  NOT_DESTROYED();
  if (HasLayer()) {
    auto* box_model_object = To<LayoutBoxModelObject>(this);
    if (box_model_object->HasSelfPaintingLayer())
      return box_model_object->Layer()->NeedsVisualOverflowRecalc();
  }
  return false;
}

void LayoutObject::MarkSelfPaintingLayerForVisualOverflowRecalc() {
  NOT_DESTROYED();
  DCHECK(!GetDocument().InPostLifecycleSteps());
  if (HasLayer()) {
    auto* box_model_object = To<LayoutBoxModelObject>(this);
    if (box_model_object->HasSelfPaintingLayer())
      box_model_object->Layer()->SetNeedsVisualOverflowRecalc();
  }
#if DCHECK_IS_ON()
  InvalidateVisualOverflow();
#endif
}

bool LayoutObject::IsShapingDeferred() const {
  if (const auto* block_flow = DynamicTo<LayoutBlockFlow>(this)) {
    return block_flow->HasNGInlineNodeData() &&
           block_flow->GetNGInlineNodeData()->IsShapingDeferred();
  }
  return false;
}

bool LayoutObject::ForceLegacyLayoutForChildren() const {
  NOT_DESTROYED();
  if (bitfields_.ForceLegacyLayout())
    return true;

  // For container queries, we may end up marking an element for forcing legacy
  // layout without re-attaching the container itself because we are performing
  // layout for the container when this is detected. Descendants should still
  // have legacy forced.
  //
  // We skip over anonymous ancestors to do the check because anonymous children
  // may need to be kept in sync with its parent (For instance LayoutFlowThread
  // for multicol), in which case the ForceLegacyLayout flag matches the
  // size container LayoutObject flag and not the element flag.
  const LayoutObject* non_anonymous =
      IsAnonymous() ? NonAnonymousAncestor() : this;
  if (!non_anonymous)
    return false;
  if (Element* element = DynamicTo<Element>(non_anonymous->GetNode()))
    return element->ShouldForceLegacyLayout();
  return false;
}

void LayoutObject::SetSVGDescendantMayHaveTransformRelatedAnimation() {
  NOT_DESTROYED();
  auto* object = this;
  while (!object->IsSVGRoot()) {
    DCHECK(object->IsSVGChild());
    if (object->SVGDescendantMayHaveTransformRelatedAnimation())
      break;
    if (object->IsSVGHiddenContainer())
      return;
    object->bitfields_.SetSVGDescendantMayHaveTransformRelatedAnimation(true);
    object = object->Parent();
    if (!object)
      return;
  }
  // If we have set SetSVGDescendantMayHaveTransformRelatedAnimation() for
  // any object, set the enclosing layer needs repaint because some
  // LayoutSVGContainer may paint differently by ignoring the cull rect.
  // See SVGContainerPainter.
  if (object != this) {
    if (auto* layer = object->EnclosingLayer())
      layer->SetNeedsRepaint();
  }
}

void LayoutObject::InvalidateSubtreePositionFallback(bool mark_style_dirty) {
  NOT_DESTROYED();

  bool invalidate = StyleRef().PositionFallback() != nullptr;
  if (invalidate) {
    // Invalidate layout as @position-fallback styles are applied during layout.
    SetNeedsLayout(layout_invalidation_reason::kStyleChange);
  }

  if (mark_style_dirty) {
    if (Node* node = GetNode()) {
      if (node->GetStyleChangeType() == kSubtreeStyleChange) {
        // No need to further mark for style recalc inside this subtree.
        mark_style_dirty = false;
      }
      if (invalidate && mark_style_dirty) {
        // Need to invalidate style to avoid using stale cached position
        // fallback styles.
        node->SetNeedsStyleRecalc(kLocalStyleChange,
                                  StyleChangeReasonForTracing::Create(
                                      style_change_reason::kStyleSheetChange));
      }
    }
  }

  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    child->InvalidateSubtreePositionFallback(mark_style_dirty);
  }
}

bool IsMenuList(const LayoutObject* object) {
  if (!object)
    return false;
  auto* select = DynamicTo<HTMLSelectElement>(object->GetNode());
  return select && select->UsesMenuList();
}

bool IsListBox(const LayoutObject* object) {
  if (!object)
    return false;
  auto* select = DynamicTo<HTMLSelectElement>(object->GetNode());
  return select && !select->UsesMenuList();
}

}  // namespace blink

#if DCHECK_IS_ON()

void ShowTree(const blink::LayoutObject* object) {
  if (getenv("RUNNING_UNDER_RR")) {
    // Printing timestamps requires an IPC to get the local time, which
    // does not work in an rr replay session. Just disable timestamp printing
    // globally, since we don't need them. Affecting global state isn't a
    // problem because invoking this from a rr session creates a temporary
    // program environment that will be destroyed as soon as the invocation
    // completes.
    logging::SetLogItems(true, true, false, false);
  }

  if (object)
    object->ShowTreeForThis();
  else
    DLOG(INFO) << "Cannot showTree. Root is (nil)";
}

void ShowLineTree(const blink::LayoutObject* object) {
  if (getenv("RUNNING_UNDER_RR")) {
    // Printing timestamps requires an IPC to get the local time, which
    // does not work in an rr replay session. Just disable timestamp printing
    // globally, since we don't need them. Affecting global state isn't a
    // problem because invoking this from a rr session creates a temporary
    // program environment that will be destroyed as soon as the invocation
    // completes.
    logging::SetLogItems(true, true, false, false);
  }

  if (object)
    object->ShowLineTreeForThis();
  else
    DLOG(INFO) << "Cannot showLineTree. Root is (nil)";
}

void ShowLayoutTree(const blink::LayoutObject* object1) {
  ShowLayoutTree(object1, nullptr);
}

void ShowLayoutTree(const blink::LayoutObject* object1,
                    const blink::LayoutObject* object2) {
  if (getenv("RUNNING_UNDER_RR")) {
    // Printing timestamps requires an IPC to get the local time, which
    // does not work in an rr replay session. Just disable timestamp printing
    // globally, since we don't need them. Affecting global state isn't a
    // problem because invoking this from a rr session creates a temporary
    // program environment that will be destroyed as soon as the invocation
    // completes.
    logging::SetLogItems(true, true, false, false);
  }

  if (object1) {
    const blink::LayoutObject* root = object1;
    while (root->Parent())
      root = root->Parent();
    if (object1) {
      StringBuilder string_builder;
      root->DumpLayoutTreeAndMark(string_builder, object1, "*", object2, "-",
                                  0);
      DLOG(INFO) << "\n" << string_builder.ToString().Utf8();
    }
  } else {
    DLOG(INFO) << "Cannot showLayoutTree. Root is (nil)";
  }
}

#endif  // DCHECK_IS_ON()
