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
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/layout_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/deprecated_schedule_style_recalc_during_layout.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
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
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/custom/layout_ng_custom.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/style/cursor_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
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

// In order for an image to be rendered from the content property, there can be
// at most one piece of image content data, followed by some optional
// alternative text.
bool ShouldUseContentData(const ContentData* content_data) {
  if (!content_data)
    return false;
  if (!content_data->IsImage())
    return false;
  if (content_data->Next() && !content_data->Next()->IsAltText())
    return false;

  return true;
}

template <typename Predicate>
LayoutObject* FindAncestorByPredicate(const LayoutObject* descendant,
                                      LayoutObject::AncestorSkipInfo* skip_info,
                                      Predicate predicate) {
  for (auto* object = descendant->Parent(); object; object = object->Parent()) {
    if (predicate(object))
      return object;
    if (skip_info)
      skip_info->Update(*object);
  }
  return nullptr;
}

}  // namespace

static int g_allow_destroying_layout_object_in_finalizer = 0;

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

struct SameSizeAsLayoutObject : ImageResourceObserver, DisplayItemClient {
  // Normally this field uses the gap between DisplayItemClient and
  // LayoutObject's other fields.
  uint8_t paint_invalidation_reason_;
#if DCHECK_IS_ON()
  unsigned debug_bitfields_;
#endif
  unsigned bitfields_;
  unsigned bitfields2_;
  unsigned bitfields3_;
  void* pointers[4];
  Member<void*> members[1];
  // The following fields are in FragmentData.
  PhysicalOffset paint_offset_;
  std::unique_ptr<int> rare_data_;
};

ASSERT_SIZE(LayoutObject, SameSizeAsLayoutObject);

bool LayoutObject::affects_parent_block_ = false;

void* LayoutObject::operator new(size_t sz) {
  DCHECK(IsMainThread());
  return WTF::Partitions::LayoutPartition()->Alloc(
      sz, WTF_HEAP_PROFILER_TYPE_NAME(LayoutObject));
}

void LayoutObject::operator delete(void* ptr) {
  DCHECK(IsMainThread());
  WTF::Partitions::LayoutPartition()->Free(ptr);
}

LayoutObject* LayoutObject::CreateObject(Element* element,
                                         const ComputedStyle& style,
                                         LegacyLayout legacy) {
  DCHECK(IsAllowedToModifyLayoutTreeStructure(element->GetDocument()));

  // Minimal support for content properties replacing an entire element.
  // Works only if we have exactly one piece of content and it's a URL, with
  // some optional alternative text. Otherwise acts as if we didn't support this
  // feature.
  const ContentData* content_data = style.GetContentData();
  if (!element->IsPseudoElement() && ShouldUseContentData(content_data)) {
    LayoutImage* image = new LayoutImage(element);
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
      return new LayoutInline(element);
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
                        WebFeature::kLegacyLayoutByFlexBox);
      return new LayoutFlexibleBox(element);
    case EDisplay::kFlex:
    case EDisplay::kInlineFlex:
      UseCounter::Count(element->GetDocument(), WebFeature::kCSSFlexibleBox);
      return LayoutObjectFactory::CreateFlexibleBox(*element, style, legacy);
    case EDisplay::kGrid:
    case EDisplay::kInlineGrid:
      UseCounter::Count(element->GetDocument(), WebFeature::kCSSGridLayout);
      return LayoutObjectFactory::CreateGrid(*element, style, legacy);
    case EDisplay::kMath:
    case EDisplay::kInlineMath:
      return LayoutObjectFactory::CreateMath(*element, style, legacy);
    case EDisplay::kLayoutCustom:
    case EDisplay::kInlineLayoutCustom:
      DCHECK(RuntimeEnabledFeatures::LayoutNGEnabled());
      return new LayoutNGCustom(element);
  }

  NOTREACHED();
  return nullptr;
}

LayoutObject::LayoutObject(Node* node)
    : full_paint_invalidation_reason_(PaintInvalidationReason::kNone),
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
      next_(nullptr) {
  InstanceCounters::IncrementCounter(InstanceCounters::kLayoutObjectCounter);
  if (node_)
    GetFrameView()->IncrementLayoutObjectCount();

  if (const auto* element = DynamicTo<Element>(GetNode())) {
    if (element->ShouldForceLegacyLayout())
      SetForceLegacyLayout();
  }
}

LayoutObject::~LayoutObject() {
#if DCHECK_IS_ON()
  DCHECK(!has_ax_object_);
  DCHECK(BeingDestroyed());
#endif
  InstanceCounters::DecrementCounter(InstanceCounters::kLayoutObjectCounter);
}

bool LayoutObject::IsDescendantOf(const LayoutObject* obj) const {
  for (const LayoutObject* r = this; r; r = r->parent_) {
    if (r == obj)
      return true;
  }
  return false;
}

bool LayoutObject::IsHR() const {
  return IsA<HTMLHRElement>(GetNode());
}

bool LayoutObject::IsStyleGenerated() const {
  if (const auto* layout_text_fragment = ToLayoutTextFragmentOrNull(this))
    return !layout_text_fragment->AssociatedTextNode();

  const Node* node = GetNode();
  return !node || node->IsPseudoElement();
}

void LayoutObject::SetIsInsideFlowThreadIncludingDescendants(
    bool inside_flow_thread) {
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

void LayoutObject::AssertClearedPaintInvalidationFlags() const {
  if (!PaintInvalidationStateIsDirty() || ChildPrePaintBlockedByDisplayLock())
    return;
  // NG text objects are exempt, as pre-paint walking doesn't visit those with
  // no paint effects (only white-space, for instance).
  if ((IsText() && IsLayoutNGObject()) ||
      // and culled inline boxes too.
      (IsInLayoutNGInlineFormattingContext() && IsLayoutInline()))
    return;
  ShowLayoutTreeForThis();
  NOTREACHED();
}

#endif  // DCHECK_IS_ON()

DISABLE_CFI_PERF
void LayoutObject::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  DCHECK(IsAllowedToModifyLayoutTreeStructure(GetDocument()));

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
      table = LayoutObjectFactory::CreateAnonymousTableWithParent(*this);
      children->InsertChildNode(this, table, before_child);
    }
    table->AddChild(new_child);
  } else {
    children->InsertChildNode(this, new_child, before_child);
  }

  if (new_child->IsText() &&
      new_child->StyleRef().TextTransform() == ETextTransform::kCapitalize)
    ToLayoutText(new_child)->TransformText();
}

void LayoutObject::RemoveChild(LayoutObject* old_child) {
  DCHECK(IsAllowedToModifyLayoutTreeStructure(GetDocument()));

  LayoutObjectChildList* children = VirtualChildren();
  DCHECK(children);
  if (!children)
    return;

  children->RemoveChildNode(this, old_child);
}

void LayoutObject::NotifyPriorityScrollAnchorStatusChanged() {
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
  // If we're set to the same value then we're done as that means it's
  // set down the tree that way already.
  if (bitfields_.SubtreeChangeListenerRegistered() == value)
    return;

  bitfields_.SetSubtreeChangeListenerRegistered(value);

  for (LayoutObject* curr = SlowFirstChild(); curr; curr = curr->NextSibling())
    curr->RegisterSubtreeChangeListenerOnDescendants(value);
}

void LayoutObject::NotifyAncestorsOfSubtreeChange() {
  if (bitfields_.NotifiedOfSubtreeChange())
    return;

  bitfields_.SetNotifiedOfSubtreeChange(true);
  if (Parent())
    Parent()->NotifyAncestorsOfSubtreeChange();
}

void LayoutObject::NotifyOfSubtreeChange() {
  if (!bitfields_.SubtreeChangeListenerRegistered())
    return;
  if (bitfields_.NotifiedOfSubtreeChange())
    return;

  NotifyAncestorsOfSubtreeChange();

  // We can modify the layout tree during layout which means that we may
  // try to schedule this during performLayout. This should no longer
  // happen when crbug.com/370457 is fixed.
  DeprecatedScheduleStyleRecalcDuringLayout marker(GetDocument().Lifecycle());
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void LayoutObject::HandleSubtreeModifications() {
  DCHECK(WasNotifiedOfSubtreeChange());
  DCHECK(GetDocument().Lifecycle().StateAllowsLayoutTreeNotifications());

  if (ConsumesSubtreeChangeNotification())
    SubtreeDidChange();

  bitfields_.SetNotifiedOfSubtreeChange(false);

  for (LayoutObject* object = SlowFirstChild(); object;
       object = object->NextSibling()) {
    if (!object->WasNotifiedOfSubtreeChange())
      continue;
    object->HandleSubtreeModifications();
  }
}

LayoutObject* LayoutObject::NextInPreOrder() const {
  if (LayoutObject* o = SlowFirstChild())
    return o;

  return NextInPreOrderAfterChildren();
}

bool LayoutObject::HasClipRelatedProperty() const {
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
  if (HasClip() || HasNonVisibleOverflow())
    return true;
  // Paint containment establishes isolation which creates clip isolation nodes.
  // Style & Layout containment also establish isolation (see
  // |NeedsIsolationNodes| in PaintPropertyTreeBuilder).
  if (ShouldApplyPaintContainment() ||
      (ShouldApplyStyleContainment() && ShouldApplyLayoutContainment())) {
    return true;
  }
  if (IsBox() && ToLayoutBox(this)->HasControlClip())
    return true;
  return false;
}

bool LayoutObject::IsRenderedLegendInternal() const {
  DCHECK(IsBox());
  DCHECK(IsRenderedLegendCandidate());

  const auto* parent = Parent();
  // We may not be inserted into the tree yet.
  if (!parent)
    return false;
  if (RuntimeEnabledFeatures::LayoutNGFieldsetEnabled()) {
    // If there is a rendered legend, it will be found inside the anonymous
    // fieldset wrapper. If the anonymous fieldset wrapper is a multi-column,
    // the rendered legend will be found inside the multi-column flow thread.
    if (parent->IsLayoutFlowThread())
      parent = parent->Parent();
    if (parent->IsAnonymous() && parent->Parent()->IsLayoutNGFieldset())
      parent = parent->Parent();
  }
  const auto* parent_layout_block = DynamicTo<LayoutBlock>(parent);
  return parent_layout_block && IsA<HTMLFieldSetElement>(parent->GetNode()) &&
         LayoutFieldset::FindInFlowLegend(*parent_layout_block) == this;
}

LayoutObject* LayoutObject::NextInPreOrderAfterChildren() const {
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
  if (LayoutObject* o = SlowFirstChild())
    return o;

  return NextInPreOrderAfterChildren(stay_within);
}

LayoutObject* LayoutObject::PreviousInPostOrder(
    const LayoutObject* stay_within) const {
  if (LayoutObject* o = SlowLastChild())
    return o;

  return PreviousInPostOrderBeforeChildren(stay_within);
}

LayoutObject* LayoutObject::NextInPreOrderAfterChildren(
    const LayoutObject* stay_within) const {
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
  if (LayoutObject* o = PreviousSibling()) {
    while (LayoutObject* last_child = o->SlowLastChild())
      o = last_child;
    return o;
  }

  return Parent();
}

LayoutObject* LayoutObject::PreviousInPreOrder(
    const LayoutObject* stay_within) const {
  if (this == stay_within)
    return nullptr;

  return PreviousInPreOrder();
}

LayoutObject* LayoutObject::LastLeafChild() const {
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
    parent_layer->AddChild(ToLayoutBoxModelObject(obj)->Layer(), before_child);
    return;
  }

  for (LayoutObject* curr = obj->SlowFirstChild(); curr;
       curr = curr->NextSibling())
    AddLayers(curr, parent_layer, new_object, before_child);
}

void LayoutObject::AddLayers(PaintLayer* parent_layer) {
  if (!parent_layer)
    return;

  LayoutObject* object = this;
  PaintLayer* before_child = nullptr;
  blink::AddLayers(this, parent_layer, object, before_child);
}

void LayoutObject::RemoveLayers(PaintLayer* parent_layer) {
  if (!parent_layer)
    return;

  if (HasLayer()) {
    parent_layer->RemoveChild(ToLayoutBoxModelObject(this)->Layer());
    return;
  }

  for (LayoutObject* curr = SlowFirstChild(); curr; curr = curr->NextSibling())
    curr->RemoveLayers(parent_layer);
}

void LayoutObject::MoveLayers(PaintLayer* old_parent, PaintLayer* new_parent) {
  if (!new_parent)
    return;

  if (HasLayer()) {
    PaintLayer* layer = ToLayoutBoxModelObject(this)->Layer();
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
  // Error check the parent layer passed in. If it's null, we can't find
  // anything.
  if (!parent_layer)
    return nullptr;

  // Step 1: If our layer is a child of the desired parent, then return our
  // layer.
  PaintLayer* our_layer =
      HasLayer() ? ToLayoutBoxModelObject(this)->Layer() : nullptr;
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
  for (const LayoutObject* current = this; current;
       current = current->Parent()) {
    if (current->HasLayer())
      return ToLayoutBoxModelObject(current)->Layer();
  }
  // TODO(crbug.com/365897): we should get rid of detached layout subtrees, at
  // which point this code should not be reached.
  return nullptr;
}

PaintLayer* LayoutObject::PaintingLayer() const {
  auto FindContainer = [](const LayoutObject& object) -> const LayoutObject* {
    if (object.IsRenderedLegend())
      return LayoutFieldset::FindLegendContainingBlock(ToLayoutBox(object));
    // Use ContainingBlock() instead of ParentCrossingFrames() for floating
    // objects to omit any self-painting layers of inline objects that don't
    // paint the floating object. This is only needed for inline-level floats
    // not managed by LayoutNG. LayoutNG floats are painted by the correct
    // painting layer.
    if (object.IsFloating() && !object.IsInLayoutNGInlineFormattingContext())
      return object.ContainingBlock();
    return object.ParentCrossingFrames();
  };

  for (const LayoutObject* current = this; current;
       current = FindContainer(*current)) {
    if (current->HasLayer() &&
        ToLayoutBoxModelObject(current)->Layer()->IsSelfPaintingLayer()) {
      return ToLayoutBoxModelObject(current)->Layer();
    } else if (current->IsColumnSpanAll()) {
      // Column spanners paint through their multicolumn containers which can
      // be accessed through the associated out-of-flow placeholder's parent.
      current = current->SpannerPlaceholder();
    }
  }
  // TODO(crbug.com/365897): we should get rid of detached layout subtrees, at
  // which point this code should not be reached.
  return nullptr;
}

bool LayoutObject::IsFixedPositionObjectInPagedMedia() const {
  if (StyleRef().GetPosition() != EPosition::kFixed)
    return false;
  LayoutView* view = View();
  return Container() == view && view->PageLogicalHeight() &&
         // TODO(crbug.com/619094): Figure out the correct behaviour for fixed
         // position objects in paged media with vertical writing modes.
         view->IsHorizontalWritingMode();
}

PhysicalRect LayoutObject::ScrollRectToVisible(
    const PhysicalRect& rect,
    mojom::blink::ScrollIntoViewParamsPtr params) {
  LayoutBox* enclosing_box = EnclosingBox();
  if (!enclosing_box)
    return rect;

  GetDocument().GetFrame()->GetSmoothScrollSequencer().AbortAnimations();
  GetDocument().GetFrame()->GetSmoothScrollSequencer().SetScrollType(
      params->type);
  params->is_for_scroll_sequence |=
      params->type == mojom::blink::ScrollType::kProgrammatic;
  PhysicalRect new_location =
      enclosing_box->ScrollRectToVisibleRecursive(rect, std::move(params));
  GetDocument().GetFrame()->GetSmoothScrollSequencer().RunQueuedAnimations();

  return new_location;
}

LayoutBox* LayoutObject::EnclosingBox() const {
  LayoutObject* curr = const_cast<LayoutObject*>(this);
  while (curr) {
    if (curr->IsBox())
      return ToLayoutBox(curr);
    curr = curr->Parent();
  }

  NOTREACHED();
  return nullptr;
}

LayoutBlockFlow* LayoutObject::RootInlineFormattingContext() const {
  for (LayoutObject* parent = Parent(); parent; parent = parent->Parent()) {
    if (auto* block_flow = DynamicTo<LayoutBlockFlow>(parent)) {
      // Skip |LayoutFlowThread| because it is skipped when finding the first
      // child in |GetLayoutObjectForFirstChildNode|.
      if (UNLIKELY(block_flow->IsLayoutFlowThread()))
        return DynamicTo<LayoutBlockFlow>(block_flow->Parent());
      return block_flow;
    }
  }
  return nullptr;
}

LayoutBlockFlow* LayoutObject::FragmentItemsContainer() const {
  for (LayoutObject* parent = Parent(); parent; parent = parent->Parent()) {
    if (auto* block_flow = DynamicTo<LayoutBlockFlow>(parent))
      return block_flow;
  }
  return nullptr;
}

LayoutBlockFlow* LayoutObject::ContainingNGBlockFlow() const {
  DCHECK(IsInline());
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return nullptr;
  if (LayoutObject* parent = Parent()) {
    LayoutBox* box = parent->EnclosingBox();
    DCHECK(box);
    if (NGBlockNode::CanUseNewLayout(*box)) {
      DCHECK(box->IsLayoutBlockFlow());
      return To<LayoutBlockFlow>(box);
    }
  }
  return nullptr;
}

const NGPhysicalBoxFragment* LayoutObject::ContainingBlockFlowFragment() const {
  DCHECK(IsInline() || IsText());
  LayoutBlockFlow* const block_flow = ContainingNGBlockFlow();
  if (!block_flow || !block_flow->ChildrenInline())
    return nullptr;
  // TODO(kojii): CurrentFragment isn't always available after layout clean.
  // Investigate why.
  return block_flow->CurrentFragment();
}

bool LayoutObject::IsFirstInlineFragmentSafe() const {
  DCHECK(IsInline());
  LayoutBlockFlow* block_flow = ContainingNGBlockFlow();
  return block_flow && !block_flow->NeedsLayout();
}

LayoutBox* LayoutObject::EnclosingScrollableBox() const {
  for (LayoutObject* ancestor = Parent(); ancestor;
       ancestor = ancestor->Parent()) {
    if (!ancestor->IsBox())
      continue;

    LayoutBox* ancestor_box = ToLayoutBox(ancestor);
    if (ancestor_box->CanBeScrolledAndHasScrollableArea())
      return ancestor_box;
  }

  return nullptr;
}

LayoutFlowThread* LayoutObject::LocateFlowThreadContainingBlock() const {
  DCHECK(IsInsideFlowThread());

  // See if we have the thread cached because we're in the middle of layout.
  if (LayoutView* view = View()) {
    if (LayoutState* layout_state = view->GetLayoutState()) {
      // TODO(mstensho): We should really just return whatever
      // layoutState->flowThread() returns here, also if the value is nullptr.
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
      ToLayoutBoxModelObject(object)->HasSelfPaintingLayer();
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

  if (UNLIKELY(RuntimeEnabledFeatures::LayoutNGFragmentTraversalEnabled() &&
               object->IsLayoutNGObject())) {
    // We need to rebuild the entire NG fragment spine all the way from the root
    // (or at least the nearest self-painting paint layer), since we traverse
    // the fragments, and not objects. Fragment painting is initiated at
    // self-painting layers, but we cannot check if it's a self-painting layer
    // now, because it may cease to be one during layout (an object with clipped
    // overflow that no longer has content that requires it to clip).
    return false;
  }

  if (const LayoutBox* layout_box = ToLayoutBoxOrNull(object)) {
    // In general we can't relayout a flex item independently of its container;
    // not only is the result incorrect due to the override size that's set, it
    // also messes with the cached main size on the flexbox.
    if (layout_box->IsFlexItemIncludingNG())
      return false;

    // In LayoutNG, if box has any OOF descendants, they are propagated to
    // parent. Therefore, we must mark parent chain for layout.
    if (const NGLayoutResult* layouot_result =
            layout_box->GetCachedLayoutResult()) {
      if (layouot_result->PhysicalFragment()
              .HasOutOfFlowPositionedDescendants())
        return false;
    }
  }

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

  if (object->IsTextControl())
    return true;

  if (!object->HasNonVisibleOverflow())
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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  if (NeedsCollectInlines())
    return;

  if (UNLIKELY(IsSVGChild()))
    return;

  // Don't mark |LayoutFlowThread| because |CollectInlines()| skips them.
  if (!IsLayoutFlowThread())
    SetNeedsCollectInlines(true);

  if (LayoutObject* parent = Parent())
    parent->SetChildNeedsCollectInlines();
}

void LayoutObject::SetChildNeedsCollectInlines() {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
#if DCHECK_IS_ON()
  DCHECK(!IsSetNeedsLayoutForbidden());
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
    // (ie self doesn't need layout), then we can return and stop the dirty bit
    // propagation. Note that it's not enough to check |object|, since the
    // element that is actually locked needs its child bits set properly, we
    // need to go one more iteration after that.
    if (!last->SelfNeedsLayout() && last->ChildLayoutBlockedByDisplayLock()) {
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
void LayoutObject::MarkParentForOutOfFlowPositionedChange() {
#if DCHECK_IS_ON()
  DCHECK(!IsSetNeedsLayoutForbidden());
#endif

  LayoutObject* object = Parent();
  if (!object)
    return;

  // As OOF-positioned objects are represented as an object replacement
  // character in the inline items list. We need to ensure we collect the
  // inline items again to either collect or drop the OOF-positioned object.
  object->SetNeedsCollectInlines();

  const LayoutBlock* containing_block = ContainingBlock();
  while (object != containing_block) {
    object->SetChildNeedsLayout(kMarkOnlyThis);
    object = object->Parent();
  }
  // Finally mark the parent block for layout. This will mark everything which
  // has an OOF-positioned object in a NGLayoutResult as needing layout.
  if (object)
    object->SetChildNeedsLayout();
}

#if DCHECK_IS_ON()
void LayoutObject::CheckBlockPositionedObjectsNeedLayout() {
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
  bitfields_.SetIntrinsicLogicalWidthsDirty(true);
  bitfields_.SetIntrinsicLogicalWidthsDependsOnPercentageBlockSize(true);
  bitfields_.SetIntrinsicLogicalWidthsChildDependsOnPercentageBlockSize(true);
  if (mark_parents == kMarkContainerChain &&
      (IsText() || !StyleRef().HasOutOfFlowPosition()))
    InvalidateContainerIntrinsicLogicalWidths();
}

void LayoutObject::ClearIntrinsicLogicalWidthsDirty() {
  bitfields_.SetIntrinsicLogicalWidthsDirty(false);
}

bool LayoutObject::IsFontFallbackValid() const {
  return StyleRef().GetFont().IsFallbackValid() &&
         FirstLineStyle()->GetFont().IsFallbackValid();
}

void LayoutObject::InvalidateSubtreeLayoutForFontUpdates() {
  if (!RuntimeEnabledFeatures::
          CSSReducedFontLoadingLayoutInvalidationsEnabled() ||
      !IsFontFallbackValid()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kFontsChanged);
  }
  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    child->InvalidateSubtreeLayoutForFontUpdates();
  }
}

void LayoutObject::InvalidateIntersectionObserverCachedRects() {
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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return false;
  if (o->IsLayoutInline() || o->IsText() || o->IsLayoutFlowThread())
    return true;
  return false;
}

inline void LayoutObject::InvalidateContainerIntrinsicLogicalWidths() {
  // In order to avoid pathological behavior when inlines are deeply nested, we
  // do include them in the chain that we mark dirty (even though they're kind
  // of irrelevant).
  LayoutObject* o = IsTableCell() ? ContainingBlock() : Container();
  while (o &&
         (!o->IntrinsicLogicalWidthsDirty() || NGKeepInvalidatingBeyond(o))) {
    // Don't invalidate the outermost object of an unrooted subtree. That object
    // will be invalidated when the subtree is added to the document.
    LayoutObject* container =
        o->IsTableCell() ? o->ContainingBlock() : o->Container();
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
  return FindAncestorByPredicate(this, skip_info, [](LayoutObject* candidate) {
    return candidate->CanContainAbsolutePositionObjects();
  });
}

LayoutObject* LayoutObject::ContainerForFixedPosition(
    AncestorSkipInfo* skip_info) const {
  DCHECK(!IsText());
  return FindAncestorByPredicate(this, skip_info, [](LayoutObject* candidate) {
    return candidate->CanContainFixedPositionObjects();
  });
}

LayoutBlock* LayoutObject::ContainingBlockForAbsolutePosition(
    AncestorSkipInfo* skip_info) const {
  auto* container = ContainerForAbsolutePosition(skip_info);
  return FindNonAnonymousContainingBlock(container, skip_info);
}

LayoutBlock* LayoutObject::ContainingBlockForFixedPosition(
    AncestorSkipInfo* skip_info) const {
  auto* container = ContainerForFixedPosition(skip_info);
  return FindNonAnonymousContainingBlock(container, skip_info);
}

const LayoutBlock* LayoutObject::InclusiveContainingBlock() const {
  auto* layout_block = DynamicTo<LayoutBlock>(this);
  return layout_block ? layout_block : ContainingBlock();
}

LayoutBlock* LayoutObject::ContainingBlock(AncestorSkipInfo* skip_info) const {
  if (!IsTextOrSVGChild()) {
    if (style_->GetPosition() == EPosition::kFixed)
      return ContainingBlockForFixedPosition(skip_info);
    if (style_->GetPosition() == EPosition::kAbsolute)
      return ContainingBlockForAbsolutePosition(skip_info);
  }
  LayoutObject* object;
  if (IsColumnSpanAll()) {
    object = SpannerPlaceholder()->ContainingBlock();
  } else if (IsRenderedLegend()) {
    return LayoutFieldset::FindLegendContainingBlock(ToLayoutBox(*this),
                                                     skip_info);
  } else {
    object = Parent();
    if (!object && IsLayoutCustomScrollbarPart()) {
      object = ToLayoutCustomScrollbarPart(this)
                   ->GetScrollableArea()
                   ->GetLayoutBox();
    }
    while (object && ((object->IsInline() && !object->IsAtomicInlineLevel()) ||
                      !object->IsLayoutBlock())) {
      if (skip_info)
        skip_info->Update(*object);
      object = object->Parent();
    }
  }

  return DynamicTo<LayoutBlock>(object);
}

LayoutObject* LayoutObject::NonAnonymousAncestor() const {
  LayoutObject* ancestor = Parent();
  while (ancestor && ancestor->IsAnonymous())
    ancestor = ancestor->Parent();
  return ancestor;
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

  while (container && container->IsAnonymousBlock())
    container = container->ContainingBlock(skip_info);

  return DynamicTo<LayoutBlock>(container);
}

bool LayoutObject::ComputeIsFixedContainer(const ComputedStyle* style) const {
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
  if (IsA<LayoutView>(this) || IsSVGForeignObject() || IsTextControl())
    return true;
  // https://www.w3.org/TR/css-transforms-1/#containing-block-for-all-descendants

  if (RuntimeEnabledFeatures::TransformInteropEnabled() &&
      style->TransformStyle3D() == ETransformStyle3D::kPreserve3d)
    return true;

  if (style->HasTransformRelatedProperty()) {
    if (!IsInline() || IsAtomicInlineLevel())
      return true;
  }
  // https://www.w3.org/TR/css-contain-1/#containment-layout
  if (ShouldApplyPaintContainment(*style) ||
      ShouldApplyLayoutContainment(*style))
    return true;

  // We intend to change behavior to set containing block based on computed
  // rather than used style of transform-style. HasTransformRelatedProperty
  // above will return true if the *used* value of transform-style is
  // preserve-3d, so to estimate compat we need to count if the line below is
  // reached.
  if (style->TransformStyle3D() == ETransformStyle3D::kPreserve3d) {
    UseCounter::Count(
        GetDocument(),
        WebFeature::kTransformStyleContainingBlockComputedUsedMismatch);
  }

  return false;
}

bool LayoutObject::ComputeIsAbsoluteContainer(
    const ComputedStyle* style) const {
  if (!style)
    return false;
  return style->CanContainAbsolutePositionObjects() ||
         ComputeIsFixedContainer(style);
}

FloatRect LayoutObject::AbsoluteBoundingBoxFloatRect(
    MapCoordinatesFlags flags) const {
  DCHECK(!(flags & kIgnoreTransforms));
  Vector<FloatQuad> quads;
  AbsoluteQuads(quads, flags);

  wtf_size_t n = quads.size();
  if (n == 0)
    return FloatRect();

  FloatRect result = quads[0].BoundingBox();
  for (wtf_size_t i = 1; i < n; ++i)
    result.Unite(quads[i].BoundingBox());
  return result;
}

IntRect LayoutObject::AbsoluteBoundingBoxRect(MapCoordinatesFlags flags) const {
  DCHECK(!(flags & kIgnoreTransforms));
  Vector<FloatQuad> quads;
  AbsoluteQuads(quads, flags);

  wtf_size_t n = quads.size();
  if (!n)
    return IntRect();

  IntRect result = quads[0].EnclosingBoundingBox();
  for (wtf_size_t i = 1; i < n; ++i)
    result.Unite(quads[i].EnclosingBoundingBox());
  return result;
}

PhysicalRect LayoutObject::AbsoluteBoundingBoxRectHandlingEmptyInline() const {
  return PhysicalRect::EnclosingRect(AbsoluteBoundingBoxFloatRect());
}

PhysicalRect LayoutObject::AbsoluteBoundingBoxRectForScrollIntoView() const {
  PhysicalRect rect = AbsoluteBoundingBoxRectHandlingEmptyInline();
  const auto& style = StyleRef();
  rect.ExpandEdges(LayoutUnit(style.ScrollMarginTop()),
                   LayoutUnit(style.ScrollMarginRight()),
                   LayoutUnit(style.ScrollMarginBottom()),
                   LayoutUnit(style.ScrollMarginLeft()));
  return rect;
}

void LayoutObject::AddAbsoluteRectForLayer(IntRect& result) {
  if (HasLayer())
    result.Unite(AbsoluteBoundingBoxRect());
  for (LayoutObject* current = SlowFirstChild(); current;
       current = current->NextSibling())
    current->AddAbsoluteRectForLayer(result);
}

IntRect LayoutObject::AbsoluteBoundingBoxRectIncludingDescendants() const {
  IntRect result = AbsoluteBoundingBoxRect();
  for (LayoutObject* current = SlowFirstChild(); current;
       current = current->NextSibling())
    current->AddAbsoluteRectForLayer(result);
  return result;
}

void LayoutObject::Paint(const PaintInfo&) const {}

const LayoutBoxModelObject& LayoutObject::DirectlyCompositableContainer()
    const {
  CHECK(IsRooted());

  if (const LayoutBoxModelObject* container =
          EnclosingDirectlyCompositableContainer())
    return *container;

  // If the current frame is not composited, we send just return the main
  // frame's LayoutView so that we generate invalidations on the window.
  const LayoutView* layout_view = View();
  while (const LayoutObject* owner_object =
             layout_view->GetFrame()->OwnerLayoutObject())
    layout_view = owner_object->View();

  DCHECK(layout_view);
  return *layout_view;
}

const LayoutBoxModelObject*
LayoutObject::EnclosingDirectlyCompositableContainer() const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  LayoutBoxModelObject* container = nullptr;
  // FIXME: CompositingState is not necessarily up to date for many callers of
  // this function.
  DisableCompositingQueryAsserts disabler;

  if (PaintLayer* painting_layer = PaintingLayer()) {
    if (PaintLayer* compositing_layer =
            painting_layer
                ->EnclosingDirectlyCompositableLayerCrossingFrameBoundaries())
      container = &compositing_layer->GetLayoutObject();
  }
  return container;
}

bool LayoutObject::RecalcLayoutOverflow() {
  if (!ChildNeedsLayoutOverflowRecalc())
    return false;
  ClearChildNeedsLayoutOverflowRecalc();
  bool children_layout_overflow_changed = false;
  for (LayoutObject* current = SlowFirstChild(); current;
       current = current->NextSibling()) {
    if (current->RecalcLayoutOverflow())
      children_layout_overflow_changed = true;
  }
  return children_layout_overflow_changed;
}

void LayoutObject::RecalcVisualOverflow() {
  for (LayoutObject* current = SlowFirstChild(); current;
       current = current->NextSibling()) {
    if (current->HasLayer() &&
        ToLayoutBoxModelObject(current)->HasSelfPaintingLayer())
      continue;
    current->RecalcVisualOverflow();
  }
}

void LayoutObject::RecalcNormalFlowChildVisualOverflowIfNeeded() {
  if (IsOutOfFlowPositioned() ||
      (HasLayer() && ToLayoutBoxModelObject(this)->HasSelfPaintingLayer()))
    return;
  RecalcVisualOverflow();
}

bool LayoutObject::HasDistortingVisualEffects() const {
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
    if (!effect->Filter().IsEmpty() || !effect->BackdropFilter().IsEmpty() ||
        effect->GetColorFilter() != kColorFilterNone ||
        effect->BlendMode() != SkBlendMode::kSrcOver ||
        effect->Opacity() != 1.0) {
      return true;
    }
  }

  auto& local_frame_root = GetDocument().GetFrame()->LocalFrameRoot();
  auto& root_fragment = local_frame_root.ContentLayoutObject()->FirstFragment();
  CHECK(root_fragment.HasLocalBorderBoxProperties());
  const auto& root_properties = root_fragment.LocalBorderBoxProperties();

  // The only allowed transforms are 2D translation and proportional up-scaling.
  const auto& translation_2d_or_matrix =
      GeometryMapper::SourceToDestinationProjection(
          paint_properties.Transform(), root_properties.Transform());
  if (!translation_2d_or_matrix.IsIdentityOr2DTranslation() &&
      !translation_2d_or_matrix.Matrix()
           .Is2DProportionalUpscaleAndOr2DTranslation())
    return true;

  return false;
}

bool LayoutObject::HasNonZeroEffectiveOpacity() const {
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
  StringBuilder name;
  name.Append(DecoratedName());

  if (const Node* node = GetNode()) {
    name.Append(' ');
    name.Append(node->DebugName());
  }
  return name.ToString();
}

DOMNodeId LayoutObject::OwnerNodeId() const {
  return GetNode() ? DOMNodeIds::IdForNode(GetNode()) : kInvalidDOMNodeId;
}

bool LayoutObject::IsPaintInvalidationContainer() const {
  return HasLayer() &&
         ToLayoutBoxModelObject(this)->Layer()->IsPaintInvalidationContainer();
}

bool LayoutObject::CanBeCompositedForDirectReasons() const {
  return HasLayer() && ToLayoutBoxModelObject(this)
                           ->Layer()
                           ->CanBeCompositedForDirectReasons();
}

void LayoutObject::InvalidateDisplayItemClients(
    PaintInvalidationReason reason) const {
  // This default implementation invalidates only the object itself as a
  // DisplayItemClient.
  DCHECK(!GetSelectionDisplayItemClient());
  ObjectPaintInvalidator(*this).InvalidateDisplayItemClient(*this, reason);
}

bool LayoutObject::CompositedScrollsWithRespectTo(
    const LayoutBoxModelObject& paint_invalidation_container) const {
  return paint_invalidation_container.UsesCompositedScrolling() &&
         this != &paint_invalidation_container;
}

PhysicalRect LayoutObject::AbsoluteSelectionRect() const {
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
  ObjectPaintInvalidatorWithContext(*this, context).InvalidatePaint();
}

PhysicalRect LayoutObject::VisualRectInDocument(VisualRectFlags flags) const {
  PhysicalRect rect = LocalVisualRect();
  MapToVisualRectInAncestorSpace(View(), rect, flags);
  return rect;
}

PhysicalRect LayoutObject::LocalVisualRectIgnoringVisibility() const {
  NOTREACHED();
  return PhysicalRect();
}

bool LayoutObject::MapToVisualRectInAncestorSpaceInternalFastPath(
    const LayoutBoxModelObject* ancestor,
    PhysicalRect& rect,
    VisualRectFlags visual_rect_flags,
    bool& intersects) const {
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
    FloatClipRect clip_rect((FloatRect(rect)));
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
  bool intersects = true;
  if (MapToVisualRectInAncestorSpaceInternalFastPath(
          ancestor, rect, visual_rect_flags, intersects))
    return intersects;

  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 FloatQuad(FloatRect(rect)));
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
          !ToLayoutBox(parent)->MapContentsRectToBoxSpace(
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

void LayoutObject::DirtyLinesFromChangedChild(LayoutObject*, MarkingBehavior) {}

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
  if (GetNode())
    ::showTree(GetNode());
}

void LayoutObject::ShowLayoutTreeForThis() const {
  showLayoutTree(this, nullptr);
}

void LayoutObject::ShowLineTreeForThis() const {
  if (const LayoutBlock* cb = InclusiveContainingBlock()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(cb);
    if (child_block_flow) {
      child_block_flow->ShowLineTreeAndMark(nullptr, nullptr, nullptr, nullptr,
                                            this);
    }
  }
}

void LayoutObject::ShowLayoutObject() const {
  StringBuilder string_builder;
  DumpLayoutObject(string_builder, true, kShowTreeCharacterOffset);
  DLOG(INFO) << "\n" << string_builder.ToString().Utf8();
}

void LayoutObject::DumpLayoutObject(StringBuilder& string_builder,
                                    bool dump_address,
                                    unsigned show_tree_character_offset) const {
  string_builder.Append(DecoratedName());

  if (dump_address)
    string_builder.AppendFormat(" %p", this);

  if (IsText() && ToLayoutText(this)->IsTextFragment()) {
    string_builder.AppendFormat(" \"%s\" ",
                                ToLayoutText(this)->GetText().Ascii().c_str());
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
  // Keep this fast and small, used in very hot functions to skip computing
  // selection when this is not selected. This function may be inlined in
  // link-optimized builds, but keeping fast and small helps running perf
  // tests.
  return GetSelectionState() != SelectionState::kNone ||
         // TODO(kojii): Can't we set SelectionState() properly to
         // LayoutTextFragment too?
         (IsText() && ToLayoutText(*this).IsTextFragment() &&
          LayoutSelection::IsSelected(*this));
}

bool LayoutObject::IsSelectable() const {
  return !IsInert() && !(StyleRef().UserSelect() == EUserSelect::kNone &&
                         StyleRef().UserModify() == EUserModify::kReadOnly);
}

const ComputedStyle& LayoutObject::SlowEffectiveStyle(
    NGStyleVariant style_variant) const {
  switch (style_variant) {
    case NGStyleVariant::kStandard:
      return StyleRef();
    case NGStyleVariant::kFirstLine:
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
      ToLayoutBoxModelObject(object->Parent())->ChildBecameNonInline(object);
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
  if (diff.TransformChanged() && IsSVG()) {
    // Skip a full layout for transforms at the html/svg boundary which do not
    // affect sizes inside SVG.
    if (!IsSVGRoot())
      diff.SetNeedsFullLayout();
  }

  // Optimization: for decoration/color property changes, invalidation is only
  // needed if we have style or text affected by these properties.
  if (diff.TextDecorationOrColorChanged() && !diff.NeedsPaintInvalidation()) {
    if (StyleRef().HasBorderColorReferencingCurrentColor() ||
        StyleRef().HasOutlineWithCurrentColor() ||
        StyleRef().HasBackgroundRelatedColorReferencingCurrentColor() ||
        // Skip any text nodes that do not contain text boxes. Whitespace cannot
        // be skipped or we will miss invalidating decorations (e.g.,
        // underlines). MathML elements are not skipped either as some of them
        // do special painting (e.g. fraction bar).
        (IsText() && !IsBR() && ToLayoutText(this)->HasInlineFragments()) ||
        (IsSVG() && StyleRef().SvgStyle().IsFillColorCurrentColor()) ||
        (IsSVG() && StyleRef().SvgStyle().IsStrokeColorCurrentColor()) ||
        IsListMarkerForNormalContent() || IsDetailsMarker() || IsMathML())
      diff.SetNeedsPaintInvalidation();
  }

  // TODO(1088373): Pixel_WebGLHighToLowPower fails without this. This isn't the
  // right way to ensure GPU switching. Investigate and do it in the right way.
  if (RuntimeEnabledFeatures::CSSReducedFontLoadingInvalidationsEnabled() &&
      !diff.NeedsPaintInvalidation() && IsLayoutView() && Style() &&
      !Style()->GetFont().IsFallbackValid()) {
    diff.SetNeedsPaintInvalidation();
  }

  // The answer to layerTypeRequired() for plugins, iframes, and canvas can
  // change without the actual style changing, since it depends on whether we
  // decide to composite these elements. When the/ layer status of one of these
  // elements changes, we need to force a layout.
  if (!diff.NeedsFullLayout() && Style() && IsBoxModelObject()) {
    bool requires_layer =
        ToLayoutBoxModelObject(this)->LayerTypeRequired() != kNoPaintLayer;
    if (HasLayer() != requires_layer)
      diff.SetNeedsFullLayout();
  }

  return diff;
}

void LayoutObject::SetPseudoElementStyle(
    scoped_refptr<const ComputedStyle> pseudo_style) {
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
    scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
    style->InheritFrom(*pseudo_style);
    SetStyle(std::move(style));
    return;
  }

  SetStyle(std::move(pseudo_style));
}

void LayoutObject::MarkContainerChainForOverflowRecalcIfNeeded(
    bool mark_container_chain_layout_overflow_recalc) {
  LayoutObject* object = this;
  do {
    // Cell and row need to propagate the flag to their containing section and
    // row as their containing block is the table wrapper.
    // This enables us to only recompute overflow the modified sections / rows.
    object = object->IsTableCell() || object->IsTableRow()
                 ? object->Parent()
                 : object->Container();
    if (object) {
      bool already_needs_layout_overflow_recalc = false;
      if (mark_container_chain_layout_overflow_recalc) {
        already_needs_layout_overflow_recalc =
            object->ChildNeedsLayoutOverflowRecalc();
        if (!already_needs_layout_overflow_recalc)
          object->SetChildNeedsLayoutOverflowRecalc();
      }

      if (object->HasLayer()) {
        auto* box_model_object = ToLayoutBoxModelObject(object);
        if (box_model_object->HasSelfPaintingLayer()) {
          auto* layer = box_model_object->Layer();
          if (layer->NeedsVisualOverflowRecalc()) {
            if (already_needs_layout_overflow_recalc)
              return;
          } else {
            layer->SetNeedsVisualOverflowRecalc();
          }
        }
      }
    }

  } while (object);
}

void LayoutObject::SetNeedsOverflowRecalc(
    OverflowRecalcType overflow_recalc_type) {
  bool mark_container_chain_layout_overflow_recalc =
      !SelfNeedsLayoutOverflowRecalc();

  if (overflow_recalc_type ==
      OverflowRecalcType::kLayoutAndVisualOverflowRecalc) {
    SetSelfNeedsLayoutOverflowRecalc();
  }

  DCHECK(overflow_recalc_type ==
             OverflowRecalcType::kOnlyVisualOverflowRecalc ||
         overflow_recalc_type ==
             OverflowRecalcType::kLayoutAndVisualOverflowRecalc);
  SetShouldCheckForPaintInvalidation();
  MarkSelfPaintingLayerForVisualOverflowRecalc();

  if (mark_container_chain_layout_overflow_recalc) {
    MarkContainerChainForOverflowRecalcIfNeeded(
        overflow_recalc_type ==
        OverflowRecalcType::kLayoutAndVisualOverflowRecalc);
  }
}

DISABLE_CFI_PERF
void LayoutObject::SetStyle(scoped_refptr<const ComputedStyle> style,
                            ApplyStyleChanges apply_changes) {
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
  }

  diff = AdjustStyleDifference(diff);

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
    if (!IsLayoutNGObject() && !IsLayoutBlock() && !NeedsLayout()) {
      // TODO(rego): This is still needed because RecalcVisualOverflow() does
      // not actually compute the visual overflow for inline elements (legacy
      // layout). However in LayoutNG RecalcInlineChildrenInkOverflow() is
      // called and visual overflow is recomputed properly so we don't need this
      // (see crbug.com/1043927).
      SetNeedsLayoutAndIntrinsicWidthsRecalc(
          layout_invalidation_reason::kStyleChange);
    } else {
      PaintingLayer()->SetNeedsVisualOverflowRecalc();
      SetShouldCheckForPaintInvalidation();
    }
  }

  if (diff.NeedsPaintInvalidation() || updated_diff.NeedsPaintInvalidation()) {
    if (IsSVGRoot()) {
      // LayoutSVGRoot::LocalVisualRect() depends on some styles.
      SetShouldDoFullPaintInvalidation();
    } else {
      // We'll set needing geometry change later if the style change does cause
      // possible layout change or visual overflow change.
      SetShouldDoFullPaintInvalidationWithoutGeometryChange();
    }
  }

  if (diff.NeedsPaintInvalidation() && old_style &&
      !old_style->ClipPathDataEquivalent(*style_)) {
    InvalidateClipPathCache();
    PaintingLayer()->SetNeedsCompositingInputsUpdate();
  }

  if (diff.NeedsVisualRectUpdate())
    SetShouldCheckForPaintInvalidation();

  // Text nodes share style with their parents but the paint properties don't
  // apply to them, hence the !isText() check. If property nodes are added or
  // removed as a result of these style changes, PaintPropertyTreeBuilder will
  // call SetNeedsRepaint to cause re-generation of PaintChunks.
  if (!IsText() && !HasLayer() &&
      (diff.TransformChanged() || diff.OpacityChanged() ||
       diff.ZIndexChanged() || diff.FilterChanged() ||
       diff.BackdropFilterChanged() || diff.CssClipChanged() ||
       diff.BlendModeChanged() || diff.MaskChanged())) {
    SetNeedsPaintPropertyUpdate();
  }
}

void LayoutObject::UpdateImageObservers(const ComputedStyle* old_style,
                                        const ComputedStyle* new_style) {
  DCHECK(old_style || new_style);
  DCHECK(!IsText());

  UpdateFillImages(old_style ? &old_style->BackgroundLayers() : nullptr,
                   new_style ? &new_style->BackgroundLayers() : nullptr);
  UpdateFillImages(old_style ? &old_style->MaskLayers() : nullptr,
                   new_style ? &new_style->MaskLayers() : nullptr);

  UpdateImage(old_style ? old_style->BorderImage().GetImage() : nullptr,
              new_style ? new_style->BorderImage().GetImage() : nullptr);
  UpdateImage(old_style ? old_style->MaskBoxImage().GetImage() : nullptr,
              new_style ? new_style->MaskBoxImage().GetImage() : nullptr);

  StyleImage* old_content_image =
      old_style && old_style->GetContentData() &&
              old_style->GetContentData()->IsImage()
          ? To<ImageContentData>(old_style->GetContentData())->GetImage()
          : nullptr;
  StyleImage* new_content_image =
      new_style && new_style->GetContentData() &&
              new_style->GetContentData()->IsImage()
          ? To<ImageContentData>(new_style->GetContentData())->GetImage()
          : nullptr;
  UpdateImage(old_content_image, new_content_image);

  StyleImage* old_box_reflect_mask_image =
      old_style && old_style->BoxReflect()
          ? old_style->BoxReflect()->Mask().GetImage()
          : nullptr;
  StyleImage* new_box_reflect_mask_image =
      new_style && new_style->BoxReflect()
          ? new_style->BoxReflect()->Mask().GetImage()
          : nullptr;
  UpdateImage(old_box_reflect_mask_image, new_box_reflect_mask_image);

  UpdateShapeImage(old_style ? old_style->ShapeOutside() : nullptr,
                   new_style ? new_style->ShapeOutside() : nullptr);
  UpdateCursorImages(old_style ? old_style->Cursors() : nullptr,
                     new_style ? new_style->Cursors() : nullptr);

  UpdateFirstLineImageObservers(new_style);
}

void LayoutObject::UpdateFirstLineImageObservers(
    const ComputedStyle* new_style) {
  bool has_new_first_line_style =
      new_style && new_style->HasPseudoElementStyle(kPseudoIdFirstLine) &&
      BehavesLikeBlockContainer();
  DCHECK(!has_new_first_line_style || new_style == Style());

  if (!bitfields_.RegisteredAsFirstLineImageObserver() &&
      !has_new_first_line_style)
    return;

  using FirstLineStyleMap =
      HashMap<const LayoutObject*, scoped_refptr<const ComputedStyle>>;
  DEFINE_STATIC_LOCAL(FirstLineStyleMap, first_line_style_map, ());
  DCHECK_EQ(bitfields_.RegisteredAsFirstLineImageObserver(),
            first_line_style_map.Contains(this));
  const auto* old_first_line_style =
      bitfields_.RegisteredAsFirstLineImageObserver()
          ? first_line_style_map.at(this)
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
      first_line_style_map.Set(this, std::move(new_first_line_style));
    } else {
      bitfields_.SetRegisteredAsFirstLineImageObserver(false);
      first_line_style_map.erase(this);
    }
    DCHECK_EQ(bitfields_.RegisteredAsFirstLineImageObserver(),
              first_line_style_map.Contains(this));
  }
}

void LayoutObject::StyleWillChange(StyleDifference diff,
                                   const ComputedStyle& new_style) {
  if (style_) {
    bool visibility_changed = style_->Visibility() != new_style.Visibility();
    // If our z-index changes value or our visibility changes,
    // we need to dirty our stacking context's z-order list.
    if (visibility_changed ||
        style_->EffectiveZIndex() != new_style.EffectiveZIndex() ||
        IsStackingContext(*style_) != IsStackingContext(new_style)) {
      GetDocument().SetAnnotatedRegionsDirty(true);
      if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
        if (GetNode())
          cache->ChildrenChanged(GetNode()->parentNode());
        else
          cache->ChildrenChanged(Parent());
      }
    }

    bool background_color_changed =
        ResolveColor(GetCSSPropertyBackgroundColor()) !=
        ResolveColor(new_style, GetCSSPropertyBackgroundColor());

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

    // Keep layer hierarchy visibility bits up to date if visibility changes.
    if (visibility_changed) {
      // We might not have an enclosing layer yet because we might not be in the
      // tree.
      if (PaintLayer* layer = EnclosingLayer())
        layer->DirtyVisibleContentStatus();
    }

    if (IsFloating() &&
        (style_->UnresolvedFloating() != new_style.UnresolvedFloating())) {
      // For changes in float styles, we need to conceivably remove ourselves
      // from the floating objects list.
      ToLayoutBox(this)->RemoveFloatingOrPositionedChildFromBlockLists();
    } else if (IsOutOfFlowPositioned() &&
               (style_->GetPosition() != new_style.GetPosition())) {
      // For changes in positioning styles, we need to conceivably remove
      // ourselves from the positioned objects list.
      ToLayoutBox(this)->RemoveFloatingOrPositionedChildFromBlockLists();
    }

    affects_parent_block_ =
        IsFloatingOrOutOfFlowPositioned() &&
        ((!new_style.IsFloating() || new_style.IsFlexOrGridItem()) &&
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
  if (style_) {
    old_touch_action = is_document_element ? style_->GetEffectiveTouchAction()
                                           : style_->GetTouchAction();
  }
  TouchAction new_touch_action = is_document_element
                                     ? new_style.GetEffectiveTouchAction()
                                     : new_style.GetTouchAction();
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

void LayoutObject::ClearBaseComputedStyle() {
  auto* element = DynamicTo<Element>(GetNode());
  if (!element)
    return;

  if (ElementAnimations* animations = element->GetElementAnimations())
    animations->ClearBaseComputedStyle();
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
  // Walk up the parent chain and find the first scrolling block to disable
  // scroll anchoring on.
  LayoutObject* object = Parent();
  Element* viewport_defining_element = GetDocument().ViewportDefiningElement();
  while (object) {
    auto* block = DynamicTo<LayoutBlock>(object);
    if (block && (block->HasNonVisibleOverflow() ||
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

void LayoutObject::StyleDidChange(StyleDifference diff,
                                  const ComputedStyle* old_style) {
  if (style_->TransformStyle3D() == ETransformStyle3D::kPreserve3d) {
    if (style_->HasNonInitialBackdropFilter() || style_->HasBlendMode() ||
        !style_->HasAutoClip() || style_->ClipPath() ||
        style_->HasIsolation() || style_->HasMask()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kAdditionalGroupingPropertiesForCompat);
    }
  }

  if (HasHiddenBackface()) {
    bool preserve_3d =
        (Parent() && Parent()->StyleRef().UsedTransformStyle3D() ==
                         ETransformStyle3D::kPreserve3d);
    if (style_->HasTransform() || preserve_3d) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHiddenBackfaceWithPossible3D);
    }
    if (preserve_3d) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHiddenBackfaceWithPreserve3D);
    }
  }

  if (ShouldApplyStrictContainment() &&
      (style_->ContentVisibility() == EContentVisibility::kVisible)) {
    if (ShouldApplyStyleContainment()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kCSSContainAllWithoutContentVisibility);
    }
    UseCounter::Count(GetDocument(),
                      WebFeature::kCSSContainStrictWithoutContentVisibility);
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
      if (RuntimeEnabledFeatures::LayoutNGEnabled())
        MarkParentForOutOfFlowPositionedChange();
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

  if (diff.NeedsPaintInvalidation() && old_style) {
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
}

void LayoutObject::ApplyPseudoElementStyleChanges(
    const ComputedStyle* old_style) {
  ApplyFirstLineChanges(old_style);

  if ((old_style && old_style->HasPseudoElementStyle(kPseudoIdSelection)) ||
      StyleRef().HasPseudoElementStyle(kPseudoIdSelection))
    InvalidateSelectedChildrenOnStyleChange();
}

void LayoutObject::ApplyFirstLineChanges(const ComputedStyle* old_style) {
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
        has_diff = true;
      }
    }
  }
  if (!has_diff) {
    diff.SetNeedsPaintInvalidation();
    diff.SetNeedsFullLayout();
  }

  if (BehavesLikeBlockContainer() &&
      (diff.NeedsPaintInvalidation() || diff.TextDecorationOrColorChanged())) {
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

void LayoutObject::PropagateStyleToAnonymousChildren() {
  // FIXME: We could save this call when the change only affected non-inherited
  // properties.
  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsAnonymous() || child->StyleRef().StyleType() != kPseudoIdNone)
      continue;
    if (child->AnonymousHasStylePropagationOverride())
      continue;

    scoped_refptr<ComputedStyle> new_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(
            StyleRef(), child->StyleRef().Display());

    // Preserve the position style of anonymous block continuations as they can
    // have relative position when they contain block descendants of relative
    // positioned inlines.
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (child->IsInFlowPositioned() && child_block_flow &&
        child_block_flow->IsAnonymousBlockContinuation())
      new_style->SetPosition(child->StyleRef().GetPosition());

    UpdateAnonymousChildStyle(child, *new_style);

    child->SetStyle(std::move(new_style));
  }

  PseudoId pseudo_id = StyleRef().StyleType();
  if (pseudo_id == kPseudoIdNone)
    return;

  // Don't propagate style from markers with 'content: normal' because it's not
  // needed and it would be slow.
  if (pseudo_id == kPseudoIdMarker && StyleRef().ContentBehavesAsNormal())
    return;

  // Propagate style from pseudo elements to generated content. We skip children
  // with pseudo element StyleType() in the for-loop above and skip over
  // descendants which are not generated content in this subtree traversal.
  //
  // TODO(futhark): It's possible we could propagate anonymous style from pseudo
  // elements through anonymous table layout objects in the recursive
  // implementation above, but it would require propagating the StyleType()
  // somehow because there is code relying on generated content having a certain
  // StyleType().
  LayoutObject* child = NextInPreOrder(this);
  while (child) {
    if (!child->IsAnonymous()) {
      // Don't propagate into non-anonymous descendants of pseudo elements. This
      // can typically happen for ::first-letter inside ::before. The
      // ::first-letter will propagate to its anonymous children separately.
      child = child->NextInPreOrderAfterChildren(this);
      continue;
    }
    if (child->IsText() || child->IsQuote() || child->IsImage())
      child->SetPseudoElementStyle(Style());
    child = child->NextInPreOrder(this);
  }
}

void LayoutObject::AddAsImageObserver(StyleImage* image) {
  if (!image)
    return;
#if DCHECK_IS_ON()
  ++as_image_observer_count_;
#endif
  image->AddClient(this);
}

void LayoutObject::RemoveAsImageObserver(StyleImage* image) {
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
  if (old_image != new_image) {
    // AddAsImageObserver first, to avoid removing all clients of an image.
    AddAsImageObserver(new_image);
    RemoveAsImageObserver(old_image);
  }
}

void LayoutObject::UpdateShapeImage(const ShapeValue* old_shape_value,
                                    const ShapeValue* new_shape_value) {
  if (old_shape_value || new_shape_value) {
    UpdateImage(old_shape_value ? old_shape_value->GetImage() : nullptr,
                new_shape_value ? new_shape_value->GetImage() : nullptr);
  }
}

void LayoutObject::CheckCounterChanges(const ComputedStyle* old_style,
                                       const ComputedStyle* new_style) {
  DCHECK(new_style);
  if (old_style) {
    if (old_style->CounterDirectivesEqual(*new_style))
      return;
  } else {
    if (!new_style->GetCounterDirectives())
      return;
  }
  LayoutCounter::LayoutObjectStyleChanged(*this, old_style, *new_style);
  View()->SetNeedsCounterUpdate();
}

PhysicalRect LayoutObject::ViewRect() const {
  return View()->ViewRect();
}

FloatPoint LayoutObject::AncestorToLocalFloatPoint(
    const LayoutBoxModelObject* ancestor,
    const FloatPoint& container_point,
    MapCoordinatesFlags mode) const {
  TransformState transform_state(
      TransformState::kUnapplyInverseTransformDirection, container_point);
  MapAncestorToLocal(ancestor, transform_state, mode);
  transform_state.Flatten();

  return transform_state.LastPlanarPoint();
}

FloatQuad LayoutObject::AncestorToLocalQuad(
    const LayoutBoxModelObject* ancestor,
    const FloatQuad& quad,
    MapCoordinatesFlags mode) const {
  TransformState transform_state(
      TransformState::kUnapplyInverseTransformDirection,
      quad.BoundingBox().Center(), quad);
  MapAncestorToLocal(ancestor, transform_state, mode);
  transform_state.Flatten();
  return transform_state.LastPlanarQuad();
}

void LayoutObject::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                      TransformState& transform_state,
                                      MapCoordinatesFlags mode) const {
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
    container_offset -= ToLayoutBoxModelObject(this)->OffsetForInFlowPosition();
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
  bool preserve3d =
      use_transforms &&
      ((container->StyleRef().Preserves3D() && !container->IsText()) ||
       (StyleRef().Preserves3D() && !IsText()));
  if (use_transforms && ShouldUseTransformFromContainer(container)) {
    TransformationMatrix t;
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
    // If the ancestor is fixed, then the rect is already in its coordinates so
    // doesn't need viewport-adjusting.
    auto* layout_view = DynamicTo<LayoutView>(container);
    if (ancestor->StyleRef().GetPosition() != EPosition::kFixed &&
        layout_view && StyleRef().GetPosition() == EPosition::kFixed) {
      transform_state.Move(layout_view->OffsetForFixedPosition());
    }
    return;
  }

  container->MapLocalToAncestor(ancestor, transform_state, mode);
}

const LayoutObject* LayoutObject::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  NOTREACHED();
  return nullptr;
}

void LayoutObject::MapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                                      TransformState& transform_state,
                                      MapCoordinatesFlags mode) const {
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
  bool preserve3d = use_transforms && (container->StyleRef().Preserves3D() ||
                                       StyleRef().Preserves3D());
  if (use_transforms && ShouldUseTransformFromContainer(container)) {
    TransformationMatrix t;
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
            ToLayoutFlowThread(this)->VisualPointToFlowThreadPoint(
                visual_point.ToLayoutPoint())));
  }

  if (skip_info.AncestorSkipped()) {
    container_offset = ancestor->OffsetFromAncestor(container);
    transform_state.Move(-container_offset);
    // If the ancestor is fixed, then the rect is already in its coordinates so
    // doesn't need viewport-adjusting.
    auto* layout_view = DynamicTo<LayoutView>(container);
    if (ancestor->StyleRef().GetPosition() != EPosition::kFixed &&
        layout_view && StyleRef().GetPosition() == EPosition::kFixed) {
      transform_state.Move(layout_view->OffsetForFixedPosition());
    }
  }
}

bool LayoutObject::ShouldUseTransformFromContainer(
    const LayoutObject* container_object) const {
  // hasTransform() indicates whether the object has transform, transform-style
  // or perspective. We just care about transform, so check the layer's
  // transform directly.
  return (HasLayer() && ToLayoutBoxModelObject(this)->Layer()->Transform()) ||
         (container_object && container_object->StyleRef().HasPerspective());
}

void LayoutObject::GetTransformFromContainer(
    const LayoutObject* container_object,
    const PhysicalOffset& offset_in_container,
    TransformationMatrix& transform) const {
  transform.MakeIdentity();
  PaintLayer* layer =
      HasLayer() ? ToLayoutBoxModelObject(this)->Layer() : nullptr;
  if (layer && layer->Transform())
    transform.Multiply(layer->CurrentTransform());

  transform.PostTranslate(offset_in_container.left.ToFloat(),
                          offset_in_container.top.ToFloat());

  bool has_perspective = container_object && container_object->HasLayer() &&
                         container_object->StyleRef().HasPerspective();
  if (has_perspective && RuntimeEnabledFeatures::TransformInteropEnabled() &&
      container_object != NonAnonymousAncestor())
    has_perspective = false;

  if (has_perspective) {
    // Perspective on the container affects us, so we have to factor it in here.
    DCHECK(container_object->HasLayer());
    FloatPoint perspective_origin =
        ToLayoutBoxModelObject(container_object)->Layer()->PerspectiveOrigin();

    TransformationMatrix perspective_matrix;
    perspective_matrix.ApplyPerspective(
        container_object->StyleRef().Perspective());
    perspective_matrix.ApplyTransformOrigin(perspective_origin.X(),
                                            perspective_origin.Y(), 0);

    transform = perspective_matrix * transform;
  }
}

FloatPoint LayoutObject::LocalToAncestorFloatPoint(
    const FloatPoint& local_point,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 local_point);
  MapLocalToAncestor(ancestor, transform_state, mode);
  transform_state.Flatten();

  return transform_state.LastPlanarPoint();
}

bool LayoutObject::LocalToAncestorRectFastPath(
    const PhysicalRect& rect,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode,
    PhysicalRect& result) const {
  if (!(mode & kUseGeometryMapperMode))
    return false;
  // No other modes are supported.
  if (mode & (~kUseGeometryMapperMode))
    return false;

  if (!ancestor)
    ancestor = View();

  if (ancestor == this)
    return true;

  AncestorSkipInfo skip_info(ancestor);
  PropertyTreeStateOrAlias container_properties =
      PropertyTreeState::Uninitialized();
  const LayoutObject* property_container =
      GetPropertyContainer(&skip_info, &container_properties);
  if (!property_container)
    return false;

  FloatRect mapping_rect(rect);

  // This works because it's not possible to have any intervening clips,
  // effects, transforms between |this| and |property_container|, and therefore
  // FirstFragment().PaintOffset() is relative to the transform space defined by
  // FirstFragment().LocalBorderBoxProperties() (if this == property_container)
  // or property_container->FirstFragment().ContentsProperties().
  mapping_rect.Move(FloatSize(FirstFragment().PaintOffset()));

  if (property_container != ancestor) {
    GeometryMapper::SourceToDestinationRect(
        container_properties.Transform(),
        ancestor->FirstFragment().LocalBorderBoxProperties().Transform(),
        mapping_rect);
  }
  mapping_rect.Move(-FloatSize(ancestor->FirstFragment().PaintOffset()));

  result = PhysicalRect::EnclosingRect(mapping_rect);
  return true;
}

PhysicalRect LayoutObject::LocalToAncestorRect(
    const PhysicalRect& rect,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  PhysicalRect result;
  if (LocalToAncestorRectFastPath(rect, ancestor, mode, result))
    return result;

  return PhysicalRect::EnclosingRect(
      LocalToAncestorQuad(FloatRect(rect), ancestor, mode).BoundingBox());
}

FloatQuad LayoutObject::LocalToAncestorQuad(
    const FloatQuad& local_quad,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  // Track the point at the center of the quad's bounding box. As
  // MapLocalToAncestor() calls OffsetFromContainer(), it will use that point
  // as the reference point to decide which column's transform to apply in
  // multiple-column blocks.
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 local_quad.BoundingBox().Center(), local_quad);
  MapLocalToAncestor(ancestor, transform_state, mode);
  transform_state.Flatten();

  return transform_state.LastPlanarQuad();
}

void LayoutObject::LocalToAncestorRects(
    Vector<PhysicalRect>& rects,
    const LayoutBoxModelObject* ancestor,
    const PhysicalOffset& pre_offset,
    const PhysicalOffset& post_offset) const {
  for (wtf_size_t i = 0; i < rects.size(); ++i) {
    PhysicalRect& rect = rects[i];
    rect.Move(pre_offset);
    FloatQuad container_quad =
        LocalToAncestorQuad(FloatQuad(FloatRect(rect)), ancestor);
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

TransformationMatrix LayoutObject::LocalToAncestorTransform(
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  DCHECK(!(mode & kIgnoreTransforms));
  TransformState transform_state(TransformState::kApplyTransformDirection);
  MapLocalToAncestor(ancestor, transform_state, mode);
  return transform_state.AccumulatedTransform();
}

PhysicalOffset LayoutObject::OffsetFromContainer(
    const LayoutObject* o,
    bool ignore_scroll_offset) const {
  return OffsetFromContainerInternal(o, ignore_scroll_offset);
}

PhysicalOffset LayoutObject::OffsetFromContainerInternal(
    const LayoutObject* o,
    bool ignore_scroll_offset) const {
  DCHECK_EQ(o, Container());
  return o->HasNonVisibleOverflow()
             ? OffsetFromScrollableContainer(o, ignore_scroll_offset)
             : PhysicalOffset();
}

PhysicalOffset LayoutObject::OffsetFromScrollableContainer(
    const LayoutObject* container,
    bool ignore_scroll_offset) const {
  DCHECK(container->HasNonVisibleOverflow());
  const LayoutBox* box = ToLayoutBox(container);
  if (!ignore_scroll_offset)
    return -PhysicalOffset(box->ScrolledContentOffset());

  // ScrollOrigin accounts for other writing modes whose content's origin is not
  // at the top-left.
  return PhysicalOffset(box->GetScrollableArea()->ScrollOrigin());
}

PhysicalOffset LayoutObject::OffsetFromAncestor(
    const LayoutObject* ancestor_container) const {
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
  if (extra_width_to_end_of_line)
    *extra_width_to_end_of_line = LayoutUnit();

  return LayoutRect();
}

bool LayoutObject::IsRooted() const {
  const LayoutObject* object = this;
  while (object->Parent() && !object->HasLayer())
    object = object->Parent();
  if (object->HasLayer())
    return ToLayoutBoxModelObject(object)->Layer()->Root()->IsRootLayer();
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

LayoutObject* LayoutObject::Container(AncestorSkipInfo* skip_info) const {
  // TODO(mstensho): Get rid of this. Nobody should call this method with those
  // flags already set.
  if (skip_info)
    skip_info->ResetOutput();

  if (IsTextOrSVGChild())
    return Parent();

  EPosition pos = style_->GetPosition();
  if (pos == EPosition::kFixed)
    return ContainerForFixedPosition(skip_info);

  if (pos == EPosition::kAbsolute) {
    return ContainerForAbsolutePosition(skip_info);
  }

  if (IsColumnSpanAll()) {
    LayoutObject* multicol_container = SpannerPlaceholder()->Container();
    if (skip_info) {
      // We jumped directly from the spanner to the multicol container. Need to
      // check if we skipped |ancestor| or filter/reflection on the way.
      for (LayoutObject* walker = Parent();
           walker && walker != multicol_container; walker = walker->Parent())
        skip_info->Update(*walker);
    }
    return multicol_container;
  }

  if ((IsFloating() && !IsInLayoutNGInlineFormattingContext()) ||
      IsRenderedLegend())
    return ContainingBlock(skip_info);

  return Parent();
}

inline LayoutObject* LayoutObject::ParentCrossingFrames() const {
  if (IsA<LayoutView>(this))
    return GetFrame()->OwnerLayoutObject();
  return Parent();
}

inline void LayoutObject::ClearLayoutRootIfNeeded() const {
  if (LocalFrameView* view = GetFrameView()) {
    if (!DocumentBeingDestroyed())
      view->ClearLayoutSubtreeRoot(*this);
  }
}

void LayoutObject::WillBeDestroyed() {
  // Destroy any leftover anonymous children.
  LayoutObjectChildList* children = VirtualChildren();
  if (children)
    children->DestroyLeftoverChildren();

  if (LocalFrame* frame = GetFrame()) {
    // If this layoutObject is being autoscrolled, stop the autoscrolling.
    if (frame->GetPage())
      frame->GetPage()->GetAutoscrollController().StopAutoscrollIfNeeded(this);
  }

  // For accessibility management, notify the parent of the imminent change to
  // its child set.
  // We do it now, before remove(), while the parent pointer is still available.
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(Parent());

  Remove();

  // The remove() call above may invoke axObjectCache()->childrenChanged() on
  // the parent, which may require the AX layout object for this layoutObject.
  // So we remove the AX layout object now, after the layoutObject is removed.
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

  if (GetFrameView())
    SetIsBackgroundAttachmentFixedObject(false);
}

DISABLE_CFI_PERF
void LayoutObject::InsertedIntoTree() {
  // FIXME: We should DCHECK(isRooted()) here but generated content makes some
  // out-of-order insertion.

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
  DCHECK_EQ(FirstInlineFragment(), nullptr);

  if (Parent()->ChildrenInline())
    Parent()->DirtyLinesFromChangedChild(this);

  if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock())
    flow_thread->FlowThreadDescendantWasInserted(this);
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
  SetNeedsPaintPropertyUpdatePreservingCachedRects();
  InvalidateIntersectionObserverCachedRects();
}

void LayoutObject::SetNeedsPaintPropertyUpdatePreservingCachedRects() {
  if (bitfields_.NeedsPaintPropertyUpdate())
    return;

  // Anytime a layout object needs a paint property update, we should also do
  // intersection observation.
  // TODO(vmpstr): Figure out if there's a cleaner way to do this outside of
  // this function, since this is potentially called many times for a single
  // frame view subtree.
  GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);

  bitfields_.SetNeedsPaintPropertyUpdate(true);
  for (auto* ancestor = ParentCrossingFrames();
       ancestor && !ancestor->DescendantNeedsPaintPropertyUpdate();
       ancestor = ancestor->ParentCrossingFrames()) {
    ancestor->bitfields_.SetDescendantNeedsPaintPropertyUpdate(true);
  }
}

void LayoutObject::SetAncestorsNeedPaintPropertyUpdateForMainThreadScrolling() {
  LayoutObject* ancestor = ParentCrossingFrames();
  while (ancestor) {
    ancestor->SetNeedsPaintPropertyUpdate();
    ancestor = ancestor->ParentCrossingFrames();
  }
}

void LayoutObject::MaybeClearIsScrollAnchorObject() {
  if (!bitfields_.IsScrollAnchorObject())
    return;
  bitfields_.SetIsScrollAnchorObject(
      FindReferencingScrollAnchors(this, kDontClear));
}

void LayoutObject::RemoveFromLayoutFlowThread() {
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
  if (const LayoutObjectChildList* children = VirtualChildren()) {
    for (LayoutObject* child = children->FirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsLayoutFlowThread())
        continue;  // Don't descend into inner fragmentation contexts.
      child->RemoveFromLayoutFlowThreadRecursive(child->IsLayoutFlowThread()
                                                     ? ToLayoutFlowThread(child)
                                                     : layout_flow_thread);
    }
  }

  if (layout_flow_thread && layout_flow_thread != this)
    layout_flow_thread->FlowThreadDescendantWillBeRemoved(this);
  SetIsInsideFlowThread(false);
  CHECK(!SpannerPlaceholder());
}

void LayoutObject::DestroyAndCleanupAnonymousWrappers() {
  // If the tree is destroyed, there is no need for a clean-up phase.
  if (DocumentBeingDestroyed()) {
    Destroy();
    return;
  }

  LayoutObject* destroy_root = this;
  for (LayoutObject *destroy_root_parent = destroy_root->Parent();
       destroy_root_parent && destroy_root_parent->IsAnonymous();
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
      DCHECK(ToLayoutInline(destroy_root)->Continuation());
    }
  }

  destroy_root->Destroy();

  // WARNING: |this| is deleted here.
}

void LayoutObject::Destroy() {
  CHECK(g_allow_destroying_layout_object_in_finalizer ||
        !ThreadState::Current()->InAtomicSweepingPause());

  // Mark as being destroyed to avoid trouble with merges in |RemoveChild()| and
  // other house keepings.
  bitfields_.SetBeingDestroyed(true);
  WillBeDestroyed();
  DeleteThis();
}

void LayoutObject::DeleteThis() {
  delete this;
}

PositionWithAffinity LayoutObject::PositionForPoint(
    const PhysicalOffset&) const {
  return CreatePositionWithAffinity(CaretMinOffset());
}

CompositingState LayoutObject::GetCompositingState() const {
  return HasLayer()
             ? ToLayoutBoxModelObject(this)->Layer()->GetCompositingState()
             : kNotComposited;
}

bool LayoutObject::CanHaveAdditionalCompositingReasons() const {
  return false;
}

CompositingReasons LayoutObject::AdditionalCompositingReasons() const {
  return CompositingReason::kNone;
}

bool LayoutObject::HitTestAllPhases(HitTestResult& result,
                                    const HitTestLocation& hit_test_location,
                                    const PhysicalOffset& accumulated_offset,
                                    HitTestFilter hit_test_filter) {
  bool inside = false;
  if (hit_test_filter != kHitTestSelf) {
    // First test the foreground layer (lines and inlines).
    inside = NodeAtPoint(result, hit_test_location, accumulated_offset,
                         kHitTestForeground);

    // Test floats next.
    if (!inside)
      inside = NodeAtPoint(result, hit_test_location, accumulated_offset,
                           kHitTestFloat);

    // Finally test to see if the mouse is in the background (within a child
    // block's background).
    if (!inside)
      inside = NodeAtPoint(result, hit_test_location, accumulated_offset,
                           kHitTestChildBlockBackgrounds);
  }

  // See if the mouse is inside us but not any of our descendants
  if (hit_test_filter != kHitTestDescendants && !inside)
    inside = NodeAtPoint(result, hit_test_location, accumulated_offset,
                         kHitTestBlockBackground);

  return inside;
}

Node* LayoutObject::NodeForHitTest() const {
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
  if (result.InnerNode())
    return;

  if (Node* n = NodeForHitTest())
    result.SetNodeAndPosition(n, point);
}

bool LayoutObject::NodeAtPoint(HitTestResult&,
                               const HitTestLocation&,
                               const PhysicalOffset&,
                               HitTestAction) {
  return false;
}

void LayoutObject::ScheduleRelayout() {
  if (auto* layout_view = DynamicTo<LayoutView>(this)) {
    if (LocalFrameView* view = layout_view->GetFrameView())
      view->ScheduleRelayout();
  } else {
    if (IsRooted()) {
      if (LayoutView* layout_view = View()) {
        if (LocalFrameView* frame_view = layout_view->GetFrameView())
          frame_view->ScheduleRelayoutOfSubtree(this);
      }
    }
  }
}

void LayoutObject::ForceLayout() {
  SetSelfNeedsLayoutForAvailableSpace(true);
  UpdateLayout();
}

const ComputedStyle* LayoutObject::FirstLineStyleWithoutFallback() const {
  DCHECK(GetDocument().GetStyleEngine().UsesFirstLineRules());

  // Normal markers don't use ::first-line styles in Chromium, so be consistent
  // and return null for content markers. This may need to change depending on
  // https://github.com/w3c/csswg-drafts/issues/4506
  if (IsMarkerContent())
    return nullptr;
  if (IsBeforeOrAfterContent() || IsText()) {
    if (!Parent())
      return nullptr;
    return Parent()->FirstLineStyleWithoutFallback();
  }

  if (BehavesLikeBlockContainer()) {
    if (const ComputedStyle* cached =
            StyleRef().GetCachedPseudoElementStyle(kPseudoIdFirstLine))
      return cached;

    if (const LayoutBlock* first_line_block =
            To<LayoutBlock>(this)->EnclosingFirstLineStyleBlock()) {
      if (first_line_block->Style() == Style()) {
        return first_line_block->GetCachedPseudoElementStyle(
            kPseudoIdFirstLine);
      }

      // We can't use first_line_block->GetCachedPseudoElementStyle() because
      // it's based on first_line_block's style. We need to get the uncached
      // first line style based on this object's style and cache the result in
      // it.
      if (scoped_refptr<ComputedStyle> first_line_style =
              first_line_block->GetUncachedPseudoElementStyle(
                  PseudoElementStyleRequest(kPseudoIdFirstLine), Style())) {
        return StyleRef().AddCachedPseudoElementStyle(
            std::move(first_line_style));
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
              GetUncachedPseudoElementStyle(kPseudoIdFirstLineInherited,
                                            parent_first_line_style)) {
        return StyleRef().AddCachedPseudoElementStyle(
            std::move(first_line_style));
      }
    }
  }
  return nullptr;
}

const ComputedStyle* LayoutObject::GetCachedPseudoElementStyle(
    PseudoId pseudo) const {
  DCHECK_NE(pseudo, kPseudoIdBefore);
  DCHECK_NE(pseudo, kPseudoIdAfter);
  if (!GetNode())
    return nullptr;

  Element* element = Traversal<Element>::FirstAncestorOrSelf(*GetNode());
  if (!element)
    return nullptr;

  return element->CachedStyleForPseudoElement(
      PseudoElementStyleRequest(pseudo));
}

scoped_refptr<ComputedStyle> LayoutObject::GetUncachedPseudoElementStyle(
    const PseudoElementStyleRequest& request,
    const ComputedStyle* parent_style) const {
  DCHECK_NE(request.pseudo_id, kPseudoIdBefore);
  DCHECK_NE(request.pseudo_id, kPseudoIdAfter);
  if (!GetNode())
    return nullptr;

  Element* element = Traversal<Element>::FirstAncestorOrSelf(*GetNode());
  if (!element)
    return nullptr;
  if (element->IsPseudoElement())
    return nullptr;

  return element->StyleForPseudoElement(request, parent_style);
}

void LayoutObject::AddAnnotatedRegions(Vector<AnnotatedRegionValue>& regions) {
  // Convert the style regions to absolute coordinates.
  if (StyleRef().Visibility() != EVisibility::kVisible || !IsBox())
    return;

  if (StyleRef().DraggableRegionMode() == EDraggableRegionMode::kNone)
    return;

  LayoutBox* box = ToLayoutBox(this);
  PhysicalRect local_bounds = box->PhysicalBorderBoxRect();
  PhysicalRect abs_bounds = LocalToAbsoluteRect(local_bounds);

  AnnotatedRegionValue region;
  region.draggable =
      StyleRef().DraggableRegionMode() == EDraggableRegionMode::kDrag;
  region.bounds = abs_bounds;
  regions.push_back(region);
}

bool LayoutObject::WillRenderImage() {
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

bool LayoutObject::GetImageAnimationPolicy(ImageAnimationPolicy& policy) {
  if (!GetDocument().GetSettings())
    return false;
  policy = GetDocument().GetSettings()->GetImageAnimationPolicy();
  return true;
}

bool LayoutObject::IsInsideListMarker() const {
  return (IsListMarkerForNormalContent() &&
          ToLayoutListMarker(this)->IsInside()) ||
         IsInsideListMarkerForCustomContent();
}

bool LayoutObject::IsOutsideListMarker() const {
  return (IsListMarkerForNormalContent() &&
          !ToLayoutListMarker(this)->IsInside()) ||
         IsOutsideListMarkerForCustomContent();
}

int LayoutObject::CaretMinOffset() const {
  return 0;
}

int LayoutObject::CaretMaxOffset() const {
  if (IsAtomicInlineLevel())
    return GetNode() ? std::max(1U, GetNode()->CountChildren()) : 1;
  if (IsHR())
    return 1;
  return 0;
}

bool LayoutObject::IsInert() const {
  const LayoutObject* layout_object = this;
  while (!layout_object->GetNode())
    layout_object = layout_object->Parent();
  return layout_object->GetNode()->IsInert();
}

void LayoutObject::ImageChanged(ImageResourceContent* image,
                                CanDeferInvalidation defer) {
  DCHECK(node_);

  // Image change notifications should not be received during paint because
  // the resulting invalidations will be cleared following paint. This can also
  // lead to modifying the tree out from under paint(), see: crbug.com/616700.
  DCHECK_NE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::LifecycleState::kInPaint);

  ImageChanged(static_cast<WrappedImagePtr>(image), defer);
}

void LayoutObject::ImageNotifyFinished(ImageResourceContent* image) {
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ImageLoaded(this);

  if (LocalDOMWindow* window = GetDocument().domWindow())
    ImageElementTiming::From(*window).NotifyImageFinished(*this, image);
  if (LocalFrameView* frame_view = GetFrameView())
    frame_view->GetPaintTimingDetector().NotifyImageFinished(*this, image);
}

Element* LayoutObject::OffsetParent(const Element* base) const {
  if (IsDocumentElement() || IsBody())
    return nullptr;

  if (IsFixedPositioned())
    return nullptr;

  float effective_zoom = StyleRef().EffectiveZoom();
  Node* node = nullptr;
  for (LayoutObject* ancestor = Parent(); ancestor;
       ancestor = ancestor->Parent()) {
    // Spec: http://www.w3.org/TR/cssom-view/#offset-attributes

    node = ancestor->GetNode();

    if (!node)
      continue;

    // TODO(kochi): If |base| or |node| is nested deep in shadow roots, this
    // loop may get expensive, as isUnclosedNodeOf() can take up to O(N+M) time
    // (N and M are depths).
    if (base && (node->IsClosedShadowHiddenFrom(*base) ||
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
  if (LocalDOMWindow* window = GetDocument().domWindow())
    ImageElementTiming::From(*window).NotifyImageRemoved(this, image);
  if (LocalFrameView* frame_view = GetFrameView())
    frame_view->GetPaintTimingDetector().NotifyImageRemoved(*this, image);
}

PositionWithAffinity LayoutObject::CreatePositionWithAffinity(
    int offset,
    TextAffinity affinity) const {
  // If this is a non-anonymous layoutObject in an editable area, then it's
  // simple.
  if (Node* node = NonPseudoNode()) {
    if (!HasEditableStyle(*node)) {
      // If it can be found, we prefer a visually equivalent position that is
      // editable.
      // TODO(layout-dev): Once we fix callers of |CreatePositionWithAffinity()|
      // we should use |Position| constructor. See http://crbug.com/827923
      const Position position =
          Position::CreateWithoutValidationDeprecated(*node, offset);
      Position candidate =
          MostForwardCaretPosition(position, kCanCrossEditingBoundary);
      if (HasEditableStyle(*candidate.AnchorNode()))
        return PositionWithAffinity(candidate, affinity);
      candidate = MostBackwardCaretPosition(position, kCanCrossEditingBoundary);
      if (HasEditableStyle(*candidate.AnchorNode()))
        return PositionWithAffinity(candidate, affinity);
    }
    // FIXME: Eliminate legacy editing positions
    return PositionWithAffinity(Position::EditingPositionOf(node, offset),
                                affinity);
  }

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

PositionWithAffinity LayoutObject::CreatePositionWithAffinity(
    int offset) const {
  return CreatePositionWithAffinity(offset, TextAffinity::kDownstream);
}

PositionWithAffinity LayoutObject::CreatePositionWithAffinity(
    const Position& position) const {
  if (position.IsNotNull())
    return PositionWithAffinity(position);

  DCHECK(!NonPseudoNode());
  return CreatePositionWithAffinity(0);
}

CursorDirective LayoutObject::GetCursor(const PhysicalOffset&,
                                        ui::Cursor&) const {
  return kSetCursorBasedOnStyle;
}

bool LayoutObject::CanUpdateSelectionOnRootLineBoxes() const {
  if (NeedsLayout())
    return false;

  const LayoutBlock* containing_block = ContainingBlock();
  return containing_block ? !containing_block->NeedsLayout() : false;
}

void LayoutObject::SetNeedsBoundariesUpdate() {
  if (IsSVGChild()) {
    // The boundaries affect mask clip.
    auto* resources = SVGResourcesCache::CachedResourcesForLayoutObject(*this);
    if (resources && resources->Masker())
      SetNeedsPaintPropertyUpdate();
    if (resources && resources->Clipper())
      InvalidateClipPathCache();
  }
  if (LayoutObject* layout_object = Parent())
    layout_object->SetNeedsBoundariesUpdate();
}

FloatRect LayoutObject::ObjectBoundingBox() const {
  NOTREACHED();
  return FloatRect();
}

FloatRect LayoutObject::StrokeBoundingBox() const {
  NOTREACHED();
  return FloatRect();
}

FloatRect LayoutObject::VisualRectInLocalSVGCoordinates() const {
  NOTREACHED();
  return FloatRect();
}

AffineTransform LayoutObject::LocalSVGTransform() const {
  return AffineTransform();
}

bool LayoutObject::IsRelayoutBoundary() const {
  return ObjectIsRelayoutBoundary(this);
}

inline void LayoutObject::SetShouldCheckGeometryForPaintInvalidation() {
  DCHECK(ShouldCheckForPaintInvalidation());
  bitfields_.SetShouldCheckGeometryForPaintInvalidation(true);
  for (auto* ancestor = ParentCrossingFrames();
       ancestor &&
       !ancestor->DescendantShouldCheckGeometryForPaintInvalidation();
       ancestor = ancestor->ParentCrossingFrames()) {
    ancestor->bitfields_.SetDescendantShouldCheckGeometryForPaintInvalidation(
        true);
  }
}

void LayoutObject::SetShouldInvalidateSelection() {
  bitfields_.SetShouldInvalidateSelection(true);
  SetShouldCheckForPaintInvalidation();
}

void LayoutObject::SetShouldDoFullPaintInvalidation(
    PaintInvalidationReason reason) {
  SetShouldDoFullPaintInvalidationWithoutGeometryChange(reason);
  SetShouldCheckGeometryForPaintInvalidation();
}

static PaintInvalidationReason DocumentLifecycleBasedPaintInvalidationReason(
    const DocumentLifecycle& document_lifecycle) {
  switch (document_lifecycle.GetState()) {
    case DocumentLifecycle::kInStyleRecalc:
      return PaintInvalidationReason::kStyle;
    case DocumentLifecycle::kInPreLayout:
    case DocumentLifecycle::kInPerformLayout:
    case DocumentLifecycle::kAfterPerformLayout:
      return PaintInvalidationReason::kGeometry;
    case DocumentLifecycle::kInCompositingAssignmentsUpdate:
      DCHECK(false);
      return PaintInvalidationReason::kFull;
    default:
      return PaintInvalidationReason::kFull;
  }
}

void LayoutObject::SetShouldDoFullPaintInvalidationWithoutGeometryChange(
    PaintInvalidationReason reason) {
  // Only full invalidation reasons are allowed.
  DCHECK(IsFullPaintInvalidationReason(reason));
  // This is before the early return to ensure visual update is always scheduled
  // in case that this is called not during a document lifecycle update.
  GetFrameView()->ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  if (ShouldDoFullPaintInvalidation())
    return;
  SetShouldCheckForPaintInvalidationWithoutGeometryChange();
  if (reason == PaintInvalidationReason::kFull) {
    reason = DocumentLifecycleBasedPaintInvalidationReason(
        GetDocument().Lifecycle());
  }
  full_paint_invalidation_reason_ = reason;
  bitfields_.SetShouldDelayFullPaintInvalidation(false);
}

void LayoutObject::SetShouldCheckForPaintInvalidation() {
  SetShouldCheckForPaintInvalidationWithoutGeometryChange();
  SetShouldCheckGeometryForPaintInvalidation();
}

void LayoutObject::SetShouldCheckForPaintInvalidationWithoutGeometryChange() {
  if (ShouldCheckForPaintInvalidation())
    return;
  GetFrameView()->ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  bitfields_.SetShouldCheckForPaintInvalidation(true);
  for (LayoutObject* parent = ParentCrossingFrames();
       parent && !parent->ShouldCheckForPaintInvalidation();
       parent = parent->ParentCrossingFrames()) {
    parent->bitfields_.SetShouldCheckForPaintInvalidation(true);
  }
}

void LayoutObject::SetSubtreeShouldCheckForPaintInvalidation() {
  if (SubtreeShouldCheckForPaintInvalidation()) {
    DCHECK(ShouldCheckForPaintInvalidation());
    return;
  }
  SetShouldCheckForPaintInvalidation();
  bitfields_.SetSubtreeShouldCheckForPaintInvalidation(true);
}

void LayoutObject::SetMayNeedPaintInvalidationAnimatedBackgroundImage() {
  if (MayNeedPaintInvalidationAnimatedBackgroundImage())
    return;
  bitfields_.SetMayNeedPaintInvalidationAnimatedBackgroundImage(true);
  SetShouldCheckForPaintInvalidationWithoutGeometryChange();
}

void LayoutObject::SetShouldDelayFullPaintInvalidation() {
  // Should have already set a full paint invalidation reason.
  DCHECK(IsFullPaintInvalidationReason(full_paint_invalidation_reason_));

  bitfields_.SetShouldDelayFullPaintInvalidation(true);
  if (!ShouldCheckForPaintInvalidation()) {
    // This will also schedule a visual update.
    SetShouldCheckForPaintInvalidationWithoutGeometryChange();
  } else {
    // Schedule visual update for the next document cycle in which we will
    // check if the delayed invalidation should be promoted to a real
    // invalidation.
    GetFrameView()->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  }
}

void LayoutObject::ClearPaintInvalidationFlags() {
// PaintInvalidationStateIsDirty should be kept in sync with the
// booleans that are cleared below.
#if DCHECK_IS_ON()
  DCHECK(!ShouldCheckForPaintInvalidation() || PaintInvalidationStateIsDirty());
#endif
  fragment_.SetPartialInvalidationLocalRect(PhysicalRect());
  if (!ShouldDelayFullPaintInvalidation()) {
    full_paint_invalidation_reason_ = PaintInvalidationReason::kNone;
    bitfields_.SetBackgroundNeedsFullPaintInvalidation(false);
  }
  bitfields_.SetShouldCheckForPaintInvalidation(false);
  bitfields_.SetSubtreeShouldCheckForPaintInvalidation(false);
  bitfields_.SetSubtreeShouldDoFullPaintInvalidation(false);
  bitfields_.SetMayNeedPaintInvalidationAnimatedBackgroundImage(false);
  bitfields_.SetShouldCheckGeometryForPaintInvalidation(false);
  bitfields_.SetDescendantShouldCheckGeometryForPaintInvalidation(false);
  bitfields_.SetShouldInvalidateSelection(false);
}

#if DCHECK_IS_ON()
bool LayoutObject::PaintInvalidationStateIsDirty() const {
  return BackgroundNeedsFullPaintInvalidation() ||
         ShouldCheckForPaintInvalidation() || ShouldInvalidateSelection() ||
         ShouldCheckGeometryForPaintInvalidation() ||
         DescendantShouldCheckGeometryForPaintInvalidation() ||
         ShouldDoFullPaintInvalidation() ||
         SubtreeShouldDoFullPaintInvalidation() ||
         MayNeedPaintInvalidationAnimatedBackgroundImage() ||
         !fragment_.PartialInvalidationLocalRect().IsEmpty();
}
#endif

void LayoutObject::EnsureIsReadyForPaintInvalidation() {
  DCHECK(!NeedsLayout() || ChildLayoutBlockedByDisplayLock());

  // Force full paint invalidation if the outline may be affected by descendants
  // and this object is marked for checking paint invalidation for any reason.
  if (bitfields_.OutlineMayBeAffectedByDescendants() ||
      bitfields_.PreviousOutlineMayBeAffectedByDescendants()) {
    SetShouldDoFullPaintInvalidationWithoutGeometryChange(
        PaintInvalidationReason::kOutline);
  }
  bitfields_.SetPreviousOutlineMayBeAffectedByDescendants(
      bitfields_.OutlineMayBeAffectedByDescendants());
}

void LayoutObject::ClearPaintFlags() {
  DCHECK_EQ(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
  ClearPaintInvalidationFlags();
  bitfields_.SetNeedsPaintPropertyUpdate(false);
  bitfields_.SetEffectiveAllowedTouchActionChanged(false);

  if (!ChildPrePaintBlockedByDisplayLock()) {
    bitfields_.SetDescendantNeedsPaintPropertyUpdate(false);
    bitfields_.SetDescendantEffectiveAllowedTouchActionChanged(false);
    bitfields_.ResetSubtreePaintPropertyUpdateReasons();
  }
}

bool LayoutObject::IsAllowedToModifyLayoutTreeStructure(Document& document) {
  return document.Lifecycle().StateAllowsLayoutTreeMutations();
}

void LayoutObject::SetSubtreeShouldDoFullPaintInvalidation(
    PaintInvalidationReason reason) {
  SetShouldDoFullPaintInvalidation(reason);
  bitfields_.SetSubtreeShouldDoFullPaintInvalidation(true);
}

void LayoutObject::SetIsBackgroundAttachmentFixedObject(
    bool is_background_attachment_fixed_object) {
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
  return PhysicalRect();
}

void LayoutObject::InvalidateSelectedChildrenOnStyleChange() {
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
  bitfields_.SetEffectiveAllowedTouchActionChanged(true);
  // If we're locked, mark our descendants as needing this change. This is used
  // a signal to ensure we mark the element as needing effective allowed
  // touch action recalculation when the element becomes unlocked.
  if (ChildPrePaintBlockedByDisplayLock()) {
    bitfields_.SetDescendantEffectiveAllowedTouchActionChanged(true);
    return;
  }

  LayoutObject* obj = ParentCrossingFrames();
  while (obj && !obj->DescendantEffectiveAllowedTouchActionChanged()) {
    obj->bitfields_.SetDescendantEffectiveAllowedTouchActionChanged(true);
    if (obj->ChildPrePaintBlockedByDisplayLock())
      break;

    obj = obj->ParentCrossingFrames();
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
      !ToLayoutText(layout_object)->IsTextFragment())
    return layout_object;
  LayoutTextFragment* layout_text_fragment =
      ToLayoutTextFragment(layout_object);
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
  if (SlowFirstChild() || StyleRef().Visibility() != EVisibility::kVisible ||
      DisplayLockUtilities::NearestLockedExclusiveAncestor(*this)) {
    return false;
  }
  return CanBeSelectionLeafInternal();
}

void LayoutObject::InvalidateClipPathCache() {
  SetNeedsPaintPropertyUpdate();
  for (auto* fragment = &fragment_; fragment;
       fragment = fragment->NextFragment())
    fragment->InvalidateClipPathCache();
}

Vector<PhysicalRect> LayoutObject::OutlineRects(
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type) const {
  Vector<PhysicalRect> outline_rects;
  AddOutlineRects(outline_rects, additional_offset, outline_type);
  return outline_rects;
}

void LayoutObject::SetModifiedStyleOutsideStyleRecalc(
    scoped_refptr<const ComputedStyle> style,
    ApplyStyleChanges apply_changes) {
  SetStyle(style, apply_changes);
  if (IsAnonymous() || !GetNode() || !GetNode()->IsElementNode())
    return;
  GetNode()->SetComputedStyle(std::move(style));
}

LayoutUnit LayoutObject::FlipForWritingModeInternal(
    LayoutUnit position,
    LayoutUnit width,
    const LayoutBox* box_for_flipping) const {
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
  if (HasLayer()) {
    auto* box_model_object = ToLayoutBoxModelObject(this);
    if (box_model_object->HasSelfPaintingLayer())
      return box_model_object->Layer()->NeedsVisualOverflowRecalc();
  }
  return false;
}

void LayoutObject::MarkSelfPaintingLayerForVisualOverflowRecalc() {
  if (HasLayer()) {
    auto* box_model_object = ToLayoutBoxModelObject(this);
    if (box_model_object->HasSelfPaintingLayer())
      box_model_object->Layer()->SetNeedsVisualOverflowRecalc();
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

void showTree(const blink::LayoutObject* object) {
  if (object)
    object->ShowTreeForThis();
  else
    DLOG(INFO) << "Cannot showTree. Root is (nil)";
}

void showLineTree(const blink::LayoutObject* object) {
  if (object)
    object->ShowLineTreeForThis();
  else
    DLOG(INFO) << "Cannot showLineTree. Root is (nil)";
}

void showLayoutTree(const blink::LayoutObject* object1) {
  showLayoutTree(object1, nullptr);
}

void showLayoutTree(const blink::LayoutObject* object1,
                    const blink::LayoutObject* object2) {
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
