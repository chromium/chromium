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

class SVGResourcesCycleSolver;

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

  // Remove any cached data for the |client|, and return true if so.
  virtual bool RemoveClientFromCache(SVGResourceClient&) { return false; }

  void UpdateLayout() override;
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGResourceContainer ||
           LayoutSVGHiddenContainer::IsOfType(type);
  }

  virtual LayoutSVGResourceType ResourceType() const = 0;

  bool IsSVGPaintServer() const {
    LayoutSVGResourceType resource_type = ResourceType();
    return resource_type == kPatternResourceType ||
           resource_type == kLinearGradientResourceType ||
           resource_type == kRadialGradientResourceType;
  }

  void InvalidateCacheAndMarkForLayout(LayoutInvalidationReasonForTracing,
                                       SubtreeLayoutScope* = nullptr);
  void InvalidateCacheAndMarkForLayout(SubtreeLayoutScope* = nullptr);

  bool FindCycle(SVGResourcesCycleSolver&) const;

  static void MarkForLayoutAndParentResourceInvalidation(
      LayoutObject&,
      bool needs_layout = true);
  static void MarkClientForInvalidation(LayoutObject&, InvalidationModeMask);

  void ClearInvalidationMask() { completed_invalidations_mask_ = 0; }

 protected:
  // Used from RemoveAllClientsFromCache methods.
  void MarkAllClientsForInvalidation(InvalidationModeMask);

  virtual bool FindCycleFromSelf(SVGResourcesCycleSolver&) const;
  static bool FindCycleInDescendants(SVGResourcesCycleSolver&,
                                     const LayoutObject& root);
  static bool FindCycleInResources(SVGResourcesCycleSolver&,
                                   const LayoutObject& object);
  static bool FindCycleInSubtree(SVGResourcesCycleSolver&,
                                 const LayoutObject& root);

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void WillBeDestroyed() override;

  bool is_in_layout_;

 private:
  // Track global (markAllClientsForInvalidation) invalidations to avoid
  // redundant crawls.
  unsigned completed_invalidations_mask_ : 8;

  unsigned is_invalidating_ : 1;
  // 23 padding bits available
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGResourceContainer,
                                IsSVGResourceContainer());

#define DEFINE_LAYOUT_SVG_RESOURCE_TYPE_CASTS(thisType, typeName)   \
  DEFINE_TYPE_CASTS(thisType, LayoutSVGResourceContainer, resource, \
                    resource->ResourceType() == typeName,           \
                    resource.ResourceType() == typeName)

template <typename ContainerType>
inline bool IsResourceOfType(const LayoutSVGResourceContainer* container) {
  return container->ResourceType() == ContainerType::kResourceType;
}

template <typename ContainerType>
inline ContainerType* GetSVGResourceAsType(const SVGResource* resource) {
  if (!resource)
    return nullptr;
  if (LayoutSVGResourceContainer* container = resource->ResourceContainer()) {
    if (IsResourceOfType<ContainerType>(container))
      return static_cast<ContainerType*>(container);
  }
  return nullptr;
}

template <typename ContainerType>
inline ContainerType* GetSVGResourceAsType(
    const StyleSVGResource* style_resource) {
  if (!style_resource)
    return nullptr;
  return GetSVGResourceAsType<ContainerType>(style_resource->Resource());
}

}  // namespace blink

#endif
