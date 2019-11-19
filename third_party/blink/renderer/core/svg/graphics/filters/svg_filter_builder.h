/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_FILTERS_SVG_FILTER_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_FILTERS_SVG_FILTER_BUILDER_H_

#include "third_party/blink/renderer/core/style/svg_computed_style_defs.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class FloatRect;
class LayoutObject;
class SVGFilterElement;

// A map from LayoutObject -> FilterEffect and FilterEffect -> dependent
// (downstream) FilterEffects ("reverse DAG").  Used during invalidations from
// changes to the primitives (graph nodes).
class SVGFilterGraphNodeMap final
    : public GarbageCollected<SVGFilterGraphNodeMap> {
 public:
  SVGFilterGraphNodeMap();

  typedef HeapHashSet<Member<FilterEffect>> FilterEffectSet;

  void AddBuiltinEffect(FilterEffect*);
  void AddPrimitive(LayoutObject*, FilterEffect*);

  inline FilterEffectSet& EffectReferences(FilterEffect* effect) {
    // Only allowed for effects belongs to this builder.
    DCHECK(effect_references_.Contains(effect));
    return effect_references_.find(effect)->value;
  }

  // Required to change the attributes of a filter during an
  // svgAttributeChanged.
  inline FilterEffect* EffectByRenderer(LayoutObject* object) {
    return effect_renderer_.at(object);
  }

  void InvalidateDependentEffects(FilterEffect*);

  void Trace(blink::Visitor*);

 private:
  // The value is a list, which contains those filter effects,
  // which depends on the key filter effect.
  HeapHashMap<Member<FilterEffect>, FilterEffectSet> effect_references_;
  HeapHashMap<LayoutObject*, Member<FilterEffect>> effect_renderer_;
};

class SVGFilterBuilder {
  STACK_ALLOCATED();

 public:
  SVGFilterBuilder(FilterEffect* source_graphic,
                   SVGFilterGraphNodeMap* = nullptr,
                   const PaintFlags* fill_flags = nullptr,
                   const PaintFlags* stroke_flags = nullptr);

  void BuildGraph(Filter*, SVGFilterElement&, const FloatRect&);

  FilterEffect* GetEffectById(const AtomicString& id) const;
  FilterEffect* LastEffect() const { return last_effect_.Get(); }

  static InterpolationSpace ResolveInterpolationSpace(EColorInterpolation);

 private:
  void Add(const AtomicString& id, FilterEffect*);
  void AddBuiltinEffects();

  typedef HeapHashMap<AtomicString, Member<FilterEffect>> NamedFilterEffectMap;

  NamedFilterEffectMap builtin_effects_;
  NamedFilterEffectMap named_effects_;

  Member<FilterEffect> last_effect_;
  Member<SVGFilterGraphNodeMap> node_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_FILTERS_SVG_FILTER_BUILDER_H_
