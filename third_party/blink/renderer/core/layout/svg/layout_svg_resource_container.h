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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_RESOURCE_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_RESOURCE_CONTAINER_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_hidden_container.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/core/svg/svg_resource_client.h"

namespace blink {

enum LayoutSVGResourceType {
  kMaskerResourceType,
  kMarkerResourceType,
  kPatternResourceType,
  kLinearGradientResourceType,
  kRadialGradientResourceType,
  kFilterResourceType,
  kClipperResourceType
};

class LayoutSVGResourceContainer : public LayoutSVGHiddenContainer {
 public:
  explicit LayoutSVGResourceContainer(SVGElement*);
  ~LayoutSVGResourceContainer() override;

  virtual void RemoveAllClientsFromCache() = 0;

  // Like RemoveAllClientsFromCache(), but predicated on if layout has been
  // performed at all.
  void InvalidateCache();

  // Remove any cached data for the |client|, and return true if so.
  virtual bool RemoveClientFromCache(SVGResourceClient&) {
    NOT_DESTROYED();
    return false;
  }

  void UpdateLayout() override;
  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectSVGResourceContainer ||
           LayoutSVGHiddenContainer::IsOfType(type);
  }

  virtual LayoutSVGResourceType ResourceType() const = 0;

  bool IsSVGPaintServer() const {
    NOT_DESTROYED();
    LayoutSVGResourceType resource_type = ResourceType();
    return resource_type == kPatternResourceType ||
           resource_type == kLinearGradientResourceType ||
           resource_type == kRadialGradientResourceType;
  }

  bool FindCycle() const;

  static void InvalidateDependentElements(LayoutObject&, bool needs_layout);
  static void InvalidateAncestorChainResources(LayoutObject&,
                                               bool needs_layout);
  static void MarkForLayoutAndParentResourceInvalidation(
      LayoutObject&,
      bool needs_layout = true);
  static void StyleChanged(LayoutObject&, StyleDifference);

  void ClearInvalidationMask() {
    NOT_DESTROYED();
    completed_invalidations_mask_ = 0;
  }

 protected:
  typedef unsigned InvalidationModeMask;

  // When adding modes, make sure we don't overflow
  // |completed_invalidation_mask_|.
  enum InvalidationMode {
    kLayoutInvalidation = 1 << 0,
    kBoundariesInvalidation = 1 << 1,
    kPaintInvalidation = 1 << 2,
    kPaintPropertiesInvalidation = 1 << 3,
    kClipCacheInvalidation = 1 << 4,
    kFilterCacheInvalidation = 1 << 5,
    kInvalidateAll = kLayoutInvalidation | kBoundariesInvalidation |
                     kPaintInvalidation | kPaintPropertiesInvalidation |
                     kClipCacheInvalidation | kFilterCacheInvalidation,
  };

  // Used from RemoveAllClientsFromCache methods.
  void MarkAllClientsForInvalidation(InvalidationModeMask);

  virtual bool FindCycleFromSelf() const;
  static bool FindCycleInDescendants(const LayoutObject& root);
  static bool FindCycleInResources(const LayoutObject& object);
  static bool FindCycleInSubtree(const LayoutObject& root);

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void WillBeDestroyed() override;

 private:
  void InvalidateClientsIfActiveResource();

  // Track global (MarkAllClientsForInvalidation) invalidations to avoid
  // redundant crawls.
  unsigned completed_invalidations_mask_ : 8;

  unsigned is_invalidating_ : 1;
  // 23 padding bits available
};

template <>
struct DowncastTraits<LayoutSVGResourceContainer> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsSVGResourceContainer();
  }
};

template <typename ContainerType>
inline ContainerType* GetSVGResourceAsType(SVGResourceClient& client,
                                           const SVGResource* resource) {
  if (!resource) {
    return nullptr;
  }
  return DynamicTo<ContainerType>(resource->ResourceContainer(client));
}

template <typename ContainerType>
inline ContainerType* GetSVGResourceAsType(
    SVGResourceClient& client,
    const StyleSVGResource* style_resource) {
  if (!style_resource) {
    return nullptr;
  }
  return GetSVGResourceAsType<ContainerType>(client,
                                             style_resource->Resource());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_RESOURCE_CONTAINER_H_
