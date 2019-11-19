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

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
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
      is_in_layout_(false),
      completed_invalidations_mask_(0),
      is_invalidating_(false) {}

LayoutSVGResourceContainer::~LayoutSVGResourceContainer() = default;

void LayoutSVGResourceContainer::UpdateLayout() {
  // FIXME: Investigate a way to detect and break resource layout dependency
  // cycles early. Then we can remove this method altogether, and fall back onto
  // LayoutSVGHiddenContainer::layout().
  DCHECK(NeedsLayout());
  if (is_in_layout_)
    return;

  base::AutoReset<bool> in_layout_change(&is_in_layout_, true);

  LayoutSVGHiddenContainer::UpdateLayout();

  ClearInvalidationMask();
}

void LayoutSVGResourceContainer::WillBeDestroyed() {
  LayoutSVGHiddenContainer::WillBeDestroyed();
  // The resource is being torn down.
  // TODO(fs): Remove this when SVGResources is gone.
  if (LocalSVGResource* resource = ResourceForContainer(*this))
    resource->NotifyResourceDestroyed(*this);
}

void LayoutSVGResourceContainer::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  LayoutSVGHiddenContainer::StyleDidChange(diff, old_style);
  // The resource has been attached. Notify any pending clients that
  // they can now try to add themselves as clients to the resource.
  // TODO(fs): Remove this when SVGResources is gone.
  if (old_style)
    return;
  if (LocalSVGResource* resource = ResourceForContainer(*this))
    resource->NotifyResourceAttached(*this);
}

void LayoutSVGResourceContainer::MarkAllClientsForInvalidation(
    InvalidationModeMask invalidation_mask) {
  if (is_invalidating_)
    return;
  LocalSVGResource* resource = ResourceForContainer(*this);
  if (!resource || !resource->HasClients())
    return;
  // Remove modes for which invalidations have already been
  // performed. If no modes remain we are done.
  invalidation_mask &= ~completed_invalidations_mask_;
  if (invalidation_mask == 0)
    return;
  completed_invalidations_mask_ |= invalidation_mask;

  is_invalidating_ = true;

  // Invalidate clients registered via an SVGResource.
  if (resource)
    resource->NotifyContentChanged(invalidation_mask);

  is_invalidating_ = false;
}

void LayoutSVGResourceContainer::MarkClientForInvalidation(
    LayoutObject& client,
    InvalidationModeMask invalidation_mask) {
  if (invalidation_mask & SVGResourceClient::kPaintInvalidation) {
    // Since LayoutSVGInlineTexts don't have SVGResources (they use their
    // parent's), they will not be notified of changes to paint servers. So
    // if the client is one that could have a LayoutSVGInlineText use a
    // paint invalidation reason that will force paint invalidation of the
    // entire <text>/<tspan>/... subtree.
    client.SetSubtreeShouldDoFullPaintInvalidation(
        PaintInvalidationReason::kSVGResource);
    client.InvalidateClipPathCache();
    // Invalidate paint properties to update effects if any.
    client.SetNeedsPaintPropertyUpdate();
  }

  if (invalidation_mask & SVGResourceClient::kBoundariesInvalidation)
    client.SetNeedsBoundariesUpdate();
}

void LayoutSVGResourceContainer::InvalidateCacheAndMarkForLayout(
    LayoutInvalidationReasonForTracing reason,
    SubtreeLayoutScope* layout_scope) {
  if (SelfNeedsLayout())
    return;

  SetNeedsLayoutAndFullPaintInvalidation(reason, kMarkContainerChain,
                                         layout_scope);

  if (EverHadLayout())
    RemoveAllClientsFromCache();
}

void LayoutSVGResourceContainer::InvalidateCacheAndMarkForLayout(
    SubtreeLayoutScope* layout_scope) {
  InvalidateCacheAndMarkForLayout(
      layout_invalidation_reason::kSvgResourceInvalidated, layout_scope);
}

static inline void RemoveFromCacheAndInvalidateDependencies(
    LayoutObject& object,
    bool needs_layout) {
  auto* element = DynamicTo<SVGElement>(object.GetNode());
  if (!element)
    return;

  if (SVGResources* resources =
          SVGResourcesCache::CachedResourcesForLayoutObject(object)) {
    SVGResourceClient* client = element->GetSVGResourceClient();
    if (InvalidationModeMask invalidation_mask =
            resources->RemoveClientFromCacheAffectingObjectBounds(*client)) {
      LayoutSVGResourceContainer::MarkClientForInvalidation(object,
                                                            invalidation_mask);
    }
  }

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
      ToLayoutSVGResourceContainer(current)->RemoveAllClientsFromCache();
      break;
    }

    current = current->Parent();
  }
}

}  // namespace blink
