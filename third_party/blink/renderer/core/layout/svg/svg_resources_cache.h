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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_RESOURCES_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_RESOURCES_CACHE_H_

#include <memory>
#include "base/macros.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class LayoutObject;
class ComputedStyle;
class SVGResources;

class SVGResourcesCache {
  USING_FAST_MALLOC(SVGResourcesCache);

 public:
  SVGResourcesCache();
  ~SVGResourcesCache();

  static SVGResources* CachedResourcesForLayoutObject(const LayoutObject&);

  // Called from all SVG layoutObjects addChild() methods.
  static void ClientWasAddedToTree(LayoutObject&,
                                   const ComputedStyle& new_style);

  // Called from all SVG layoutObjects removeChild() methods.
  static void ClientWillBeRemovedFromTree(LayoutObject&);

  // Called from all SVG layoutObjects destroy() methods - except for
  // LayoutSVGResourceContainer.
  static void ClientDestroyed(LayoutObject&);

  // Called from all SVG layoutObjects layout() methods.
  static void ClientLayoutChanged(LayoutObject&);

  // Called from all SVG layoutObjects styleDidChange() methods.
  static void ClientStyleChanged(LayoutObject&,
                                 StyleDifference,
                                 const ComputedStyle& new_style);

  // Called when the target element of a resource referenced by the
  // LayoutObject may have changed.
  static void ResourceReferenceChanged(LayoutObject&);

  class TemporaryStyleScope {
    STACK_ALLOCATED();

   public:
    TemporaryStyleScope(LayoutObject&,
                        const ComputedStyle& original_style,
                        const ComputedStyle& temporary_style);
    ~TemporaryStyleScope();

   private:
    void SwitchTo(const ComputedStyle&);

    LayoutObject& layout_object_;
    const ComputedStyle& original_style_;
    const ComputedStyle& temporary_style_;
    const bool styles_are_equal_;
    DISALLOW_COPY_AND_ASSIGN(TemporaryStyleScope);
  };

 private:
  bool AddResourcesFromLayoutObject(LayoutObject&, const ComputedStyle&);
  bool RemoveResourcesFromLayoutObject(LayoutObject&);
  bool UpdateResourcesFromLayoutObject(LayoutObject&, const ComputedStyle&);

  typedef HashMap<const LayoutObject*, std::unique_ptr<SVGResources>> CacheMap;
  CacheMap cache_;
  DISALLOW_COPY_AND_ASSIGN(SVGResourcesCache);
};

}  // namespace blink

#endif
