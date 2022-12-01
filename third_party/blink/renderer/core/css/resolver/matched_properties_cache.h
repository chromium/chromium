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

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ComputedStyle;
class ComputedStyleBuilder;
class StyleResolverState;

class CORE_EXPORT CachedMatchedProperties final
    : public GarbageCollected<CachedMatchedProperties> {
 public:
  // Caches data of MatchedProperties. See |MatchedPropertiesCache::Cache| for
  // semantics.
  // We use UntracedMember<> here because WeakMember<> would require using a
  // HeapHashSet which is slower to iterate.
  Vector<UntracedMember<CSSPropertyValueSet>> matched_properties;
  Vector<MatchedProperties::Data> matched_properties_types;

  scoped_refptr<const ComputedStyle> computed_style;
  scoped_refptr<const ComputedStyle> parent_computed_style;

  void Set(scoped_refptr<const ComputedStyle>&& style,
           scoped_refptr<const ComputedStyle>&& parent_style,
           const MatchedPropertiesVector&);
  void Clear();

  bool DependenciesEqual(const StyleResolverState&);

  void Trace(Visitor*) const {}

  bool operator==(const MatchedPropertiesVector& properties);
  bool operator!=(const MatchedPropertiesVector& properties);
};

class CORE_EXPORT MatchedPropertiesCache {
  DISALLOW_NEW();

 public:
  MatchedPropertiesCache();
  MatchedPropertiesCache(const MatchedPropertiesCache&) = delete;
  MatchedPropertiesCache& operator=(const MatchedPropertiesCache&) = delete;
  ~MatchedPropertiesCache() { DCHECK(cache_.empty()); }

  class CORE_EXPORT Key {
    STACK_ALLOCATED();

   public:
    explicit Key(const MatchResult&);

    bool IsValid() const {
      // If hash_ happens to compute to the empty value or the deleted value,
      // the corresponding MatchResult can't be cached.
      return hash_ != HashTraits<unsigned>::EmptyValue() &&
             !HashTraits<unsigned>::IsDeletedValue(hash_);
    }

   private:
    friend class MatchedPropertiesCache;
    friend class MatchedPropertiesCacheTestKey;

    Key(const MatchResult&, unsigned hash);

    const MatchResult& result_;
    unsigned hash_;
  };

  const CachedMatchedProperties* Find(const Key&, const StyleResolverState&);
  void Add(const Key&,
           scoped_refptr<const ComputedStyle>&&,
           scoped_refptr<const ComputedStyle>&& parent_style);

  void Clear();
  void ClearViewportDependent();

  static bool IsCacheable(const StyleResolverState&);
  static bool IsStyleCacheable(const ComputedStyleBuilder&);

  void Trace(Visitor*) const;

 private:
  // The cache is mapping a hash to a cached entry where the entry is kept as
  // long as *all* properties referred to by the entry are alive. This requires
  // custom weakness which is managed through
  // |RemoveCachedMatchedPropertiesWithDeadEntries|.
  using Cache = HeapHashMap<unsigned,
                            Member<CachedMatchedProperties>,
                            DefaultHash<unsigned>,
                            HashTraits<unsigned>>;

  void RemoveCachedMatchedPropertiesWithDeadEntries(const LivenessBroker&);

  Cache cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCHED_PROPERTIES_CACHE_H_
