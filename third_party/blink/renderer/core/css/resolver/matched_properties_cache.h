/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCHED_PROPERTIES_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCHED_PROPERTIES_CACHE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class ComputedStyle;
class StyleResolverState;

class CachedMatchedProperties final
    : public GarbageCollected<CachedMatchedProperties> {
 public:
  // Caches data of MachedProperties. See MatchedPropertiesCache::Cache for
  // semantics.
  Vector<UntracedMember<CSSPropertyValueSet>> matched_properties;
  Vector<MatchedProperties::Data> matched_properties_types;

  scoped_refptr<ComputedStyle> computed_style;
  scoped_refptr<ComputedStyle> parent_computed_style;

  void Set(const ComputedStyle&,
           const ComputedStyle& parent_style,
           const MatchedPropertiesVector&);
  void Clear();

  void Trace(blink::Visitor*) {}

  bool operator==(const MatchedPropertiesVector& properties);
  bool operator!=(const MatchedPropertiesVector& properties);
};

class CORE_EXPORT MatchedPropertiesCache {
  DISALLOW_NEW();

 public:
  MatchedPropertiesCache();
  ~MatchedPropertiesCache() { DCHECK(cache_.IsEmpty()); }

  const CachedMatchedProperties* Find(unsigned hash,
                                      const StyleResolverState&,
                                      const MatchedPropertiesVector&);
  void Add(const ComputedStyle&,
           const ComputedStyle& parent_style,
           unsigned hash,
           const MatchedPropertiesVector&);

  void Clear();
  void ClearViewportDependent();

  static bool IsCacheable(const StyleResolverState&);
  static bool IsStyleCacheable(const ComputedStyle&);

  void Trace(blink::Visitor*);

 private:
  // The cache is mapping a hash to a cached entry where the entry is kept as
  // long as *all* properties referred to by the entry are alive. This requires
  // custom weakness which is managed through
  // |RemoveCachedMatchedPropertiesWithDeadEntries|.
  using Cache = HeapHashMap<unsigned,
                            Member<CachedMatchedProperties>,
                            DefaultHash<unsigned>::Hash,
                            HashTraits<unsigned>>;

  void RemoveCachedMatchedPropertiesWithDeadEntries(const WeakCallbackInfo&);

  Cache cache_;
  DISALLOW_COPY_AND_ASSIGN(MatchedPropertiesCache);
};

}  // namespace blink

#endif
