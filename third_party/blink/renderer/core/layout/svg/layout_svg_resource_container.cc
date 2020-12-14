/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"

#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cycle_solver.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/core/svg/svg_tree_scope_resources.h"

namespace blink {

namespace {

LocalSVGResource* ResourceForContainer(
    const LayoutSVGResourceContainer& resource_container) {
  const SVGElement& element = *resource_container.GetElement();
  return element.GetTreeScope()
      .EnsureSVGTreeScopedResources()
      .ExistingResourceForId(element.GetIdAttribute());
}

}  // namespace

LayoutSVGResourceContainer::LayoutSVGResourceContainer(SVGElement* node)
    : LayoutSVGHiddenContainer(node),
      completed_invalidations_mask_(0),
      is_invalidating_(false) {}

LayoutSVGResourceContainer::~LayoutSVGResourceContainer() = default;

void LayoutSVGResourceContainer::UpdateLayout() {
  NOT_DESTROYED();
  // TODO(fs): This is only here to clear the invalidation mask, without that
  // we wouldn't need to override LayoutSVGHiddenContainer::UpdateLayout().
  DCHECK(NeedsLayout());
  LayoutSVGHiddenContainer::UpdateLayout();
  ClearInvalidationMask();
}

bool LayoutSVGResourceContainer::FindCycle(
    SVGResourcesCycleSolver& solver) const {
  NOT_DESTROYED();
  if (solver.IsKnownAcyclic(this))
    return false;
  SVGResourcesCycleSolver::Scope scope(solver);
  if (!scope.Enter(this) || FindCycleFromSelf(solver))
    return true;
  solver.AddAcyclicSubgraph(this);
  return false;
}

static HeapVector<Member<SVGResource>> CollectResources(
    const LayoutObject& layout_object) {
  const ComputedStyle& style = layout_object.StyleRef();
  HeapVector<Member<SVGResource>> resources;
  if (auto* reference_clip =
          DynamicTo<ReferenceClipPathOperation>(style.ClipPath())) {
    resources.push_back(reference_clip->Resource());
  }
  for (const auto& operation : style.Filter().Operations()) {
    if (auto* reference_operation =
            DynamicTo<ReferenceFilterOperation>(*operation))
      resources.push_back(reference_operation->Resource());
  }
  const SVGComputedStyle& svg_style = style.SvgStyle();
  if (auto* masker = svg_style.MaskerResource())
    resources.push_back(masker->Resource());
  if (auto* marker = svg_style.MarkerStartResource())
    resources.push_back(marker->Resource());
  if (auto* marker = svg_style.MarkerMidResource())
    resources.push_back(marker->Resource());
  if (auto* marker = svg_style.MarkerEndResource())
    resources.push_back(marker->Resource());
  if (auto* paint_resource = svg_style.FillPaint().Resource())
    resources.push_back(paint_resource->Resource());
  if (auto* paint_resource = svg_style.StrokePaint().Resource())
    resources.push_back(paint_resource->Resource());
  return resources;
}

bool LayoutSVGResourceContainer::FindCycleInResources(
    SVGResourcesCycleSolver& solver,
    const LayoutObject& layout_object) {
  if (!layout_object.IsSVG() || layout_object.IsText())
    return false;
  SVGResourceClient* client = SVGResources::GetClient(layout_object);
  // Without an associated client, we will not reference any resources.
  if (!client)
    return false;
  // Fetch all the referenced resources.
  HeapVector<Member<SVGResource>> resources = CollectResources(layout_object);
  // This performs a depth-first search for a back-edge in all the
  // (potentially disjoint) graphs formed by the referenced resources.
  for (const auto& local_resource : resources) {
    if (local_resource->FindCycle(*client, solver))
      return true;
  }
  return false;
}

bool LayoutSVGResourceContainer::FindCycleFromSelf(
    SVGResourcesCycleSolver& solver) const {
  NOT_DESTROYED();
  // Resources don't generally apply to other resources, so require
  // the specific cases that do (like <clipPath>) to implement an
  // override.
  return FindCycleInDescendants(solver, *this);
}

bool LayoutSVGResourceContainer::FindCycleInDescendants(
    SVGResourcesCycleSolver& solver,
    const LayoutObject& root) {
  LayoutObject* node = root.SlowFirstChild();
  while (node) {
    // Skip subtrees which are themselves resources. (They will be
    // processed - if needed - when they are actually referenced.)
    if (node->IsSVGResourceContainer()) {
      node = node->NextInPreOrderAfterChildren(&root);
      continue;
    }
    if (FindCycleInResources(solver, *node))
      return true;
    node = node->NextInPreOrder(&root);
  }
  return false;
}

bool LayoutSVGResourceContainer::FindCycleInSubtree(
    SVGResourcesCycleSolver& solver,
    const LayoutObject& root) {
  if (FindCycleInResources(solver, root))
    return true;
  return FindCycleInDescendants(solver, root);
}

void LayoutSVGResourceContainer::MarkAllClientsForInvalidation(
    InvalidationModeMask invalidation_mask) {
  NOT_DESTROYED();
  if (is_invalidating_)
    return;
  LocalSVGResource* resource = ResourceForContainer(*this);
  if (!resource)
    return;
  // Remove modes for which invalidations have already been
  // performed. If no modes remain we are done.
  invalidation_mask &= ~completed_invalidations_mask_;
  if (invalidation_mask == 0)
    return;
  completed_invalidations_mask_ |= invalidation_mask;

  is_invalidating_ = true;

  // Invalidate clients registered via an SVGResource.
  resource->NotifyContentChanged(invalidation_mask);

  is_invalidating_ = false;
}

void LayoutSVGResourceContainer::InvalidateCacheAndMarkForLayout(
    LayoutInvalidationReasonForTracing reason,
    SubtreeLayoutScope* layout_scope) {
  NOT_DESTROYED();
  if (SelfNeedsLayout())
    return;

  SetNeedsLayoutAndFullPaintInvalidation(reason, kMarkContainerChain,
                                         layout_scope);

  if (EverHadLayout())
    RemoveAllClientsFromCache();
}

void LayoutSVGResourceContainer::InvalidateCacheAndMarkForLayout(
    SubtreeLayoutScope* layout_scope) {
  NOT_DESTROYED();
  InvalidateCacheAndMarkForLayout(
      layout_invalidation_reason::kSvgResourceInvalidated, layout_scope);
}

static inline void RemoveFromCacheAndInvalidateDependencies(
    LayoutObject& object,
    bool needs_layout) {
  auto* element = DynamicTo<SVGElement>(object.GetNode());
  if (!element)
    return;

  // TODO(fs): Do we still need this? (If bounds are invalidated on a leaf
  // LayoutObject, we will propagate that during the required layout and
  // invalidate effects of self and any ancestors at that time.)
  SVGResourceInvalidator(object).InvalidateEffects();

  element->NotifyIncomingReferences([needs_layout](SVGElement& element) {
    DCHECK(element.GetLayoutObject());
    LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
        *element.GetLayoutObject(), needs_layout);
  });
}

void LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
    LayoutObject& object,
    bool needs_layout) {
  DCHECK(object.GetNode());

  if (needs_layout && !object.DocumentBeingDestroyed()) {
    object.SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kSvgResourceInvalidated);
  }

  RemoveFromCacheAndInvalidateDependencies(object, needs_layout);

  // Invalidate resources in ancestor chain, if needed.
  LayoutObject* current = object.Parent();
  while (current) {
    RemoveFromCacheAndInvalidateDependencies(*current, needs_layout);

    if (current->IsSVGResourceContainer()) {
      // This will process the rest of the ancestors.
      To<LayoutSVGResourceContainer>(current)->RemoveAllClientsFromCache();
      break;
    }

    current = current->Parent();
  }
}

static inline bool IsLayoutObjectOfResourceContainer(
    const LayoutObject& layout_object) {
  const LayoutObject* current = &layout_object;
  while (current) {
    if (current->IsSVGResourceContainer())
      return true;
    current = current->Parent();
  }
  return false;
}

void LayoutSVGResourceContainer::StyleChanged(LayoutObject& object,
                                              StyleDifference diff) {
  // If this LayoutObject is the child of a resource container and
  // it requires repainting because of changes to CSS properties
  // such as 'visibility', upgrade to invalidate layout.
  bool needs_layout = diff.NeedsPaintInvalidation() &&
                      IsLayoutObjectOfResourceContainer(object);
  MarkForLayoutAndParentResourceInvalidation(object, needs_layout);
}

}  // namespace blink
