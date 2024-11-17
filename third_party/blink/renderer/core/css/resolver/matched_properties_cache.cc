/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/css/resolver/matched_properties_cache.h"

#include <algorithm>
#include <array>
#include <utility>

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace blink {

static unsigned ComputeMatchedPropertiesHash(const MatchResult& result) {
  DCHECK(result.IsCacheable());
  const MatchedPropertiesHashVector& hashes = result.GetMatchedPropertiesHash();
  DCHECK(!std::any_of(hashes.begin(), hashes.end(),
                      [](const MatchedPropertiesHash& hash) {
                        return hash.hash ==
                               WTF::HashTraits<unsigned>::DeletedValue();
                      }))
      << "This should have been checked in AddMatchedProperties()";
  unsigned hash = StringHasher::HashMemory(base::as_byte_span(hashes));

  // See CSSPropertyValueSet::ComputeHash() for asserts that this is safe.
  if (hash == WTF::HashTraits<unsigned>::EmptyValue() ||
      hash == WTF::HashTraits<unsigned>::DeletedValue()) {
    hash ^= 0x80000000;
  }

  return hash;
}

CachedMatchedProperties::CachedMatchedProperties(
    const ComputedStyle* style,
    const ComputedStyle* parent_style,
    const MatchedPropertiesVector& properties,
    unsigned clock)
    : entries({Entry{style, parent_style, clock}}) {
  matched_properties.ReserveInitialCapacity(properties.size());
  for (const auto& new_matched_properties : properties) {
    matched_properties.emplace_back(new_matched_properties.properties,
                                    new_matched_properties.data_);
  }
}

void CachedMatchedProperties::Clear() {
  matched_properties.clear();
  entries.clear();
}

MatchedPropertiesCache::MatchedPropertiesCache() = default;

MatchedPropertiesCache::Key::Key(const MatchResult& result)
    : Key(result,
          result.IsCacheable() ? ComputeMatchedPropertiesHash(result)
                               : HashTraits<unsigned>::EmptyValue()) {}

MatchedPropertiesCache::Key::Key(const MatchResult& result, unsigned hash)
    : result_(result), hash_(hash) {}

const CachedMatchedProperties::Entry* MatchedPropertiesCache::Find(
    const Key& key,
    const StyleResolverState& style_resolver_state) {
  // Matches the corresponding test in IsStyleCacheable().
  if (style_resolver_state.TextAutosizingMultiplier() != 1.0f) {
    return nullptr;
  }

  Cache::iterator it = cache_.find(key.hash_);
  if (it == cache_.end()) {
    return nullptr;
  }
  CachedMatchedProperties* cache_item = it->value.Get();
  if (!cache_item->CorrespondsTo(key.result_.GetMatchedProperties())) {
    // A hash collision (rare), or the key is not usable anymore.
    // Take out the existing entry entirely and start anew.
    // (We could possibly have reused its memory, but for simplicity,
    // we just treat it as a miss.)
    if (it->value) {
      cache_entries_ -= it->value->entries.size();
    }
    cache_.erase(it);
    return nullptr;
  }
  for (CachedMatchedProperties::Entry& entry : cache_item->entries) {
    if (IsAtShadowBoundary(&style_resolver_state.GetElement()) &&
        entry.parent_computed_style->UserModify() !=
            ComputedStyleInitialValues::InitialUserModify()) {
      // An element at a shadow boundary will reset UserModify() back to its
      // initial value for inheritance. If the cached item was computed for an
      // element not at a shadow boundary, the cached computed style will not
      // have that reset, and we cannot use it as a cache hit unless the parent
      // UserModify() is the initial value.
      continue;
    }
    if ((entry.parent_computed_style->IsEnsuredInDisplayNone() ||
         entry.computed_style->IsEnsuredOutsideFlatTree()) &&
        !style_resolver_state.ParentStyle()->IsEnsuredInDisplayNone() &&
        !style_resolver_state.IsOutsideFlatTree()) {
      // If we cached a ComputedStyle in a display:none subtree, or outside the
      // flat tree,  we would not have triggered fetches for external resources
      // and have StylePendingImages in the ComputedStyle. Instead of having to
      // inspect the cached ComputedStyle for such resources, don't use a cached
      // ComputedStyle when it was cached in display:none but is now rendered.
      continue;
    }
    if (style_resolver_state.ParentStyle()->InheritedDataShared(
            *entry.parent_computed_style)) {
      entry.last_used = clock_++;

      // Since we have a cache hit, refresh it using the most recent property
      // sets (in case they have differing pointers but same content); the key
      // is weak, and using more recently seen sets make it less likely that
      // they will go away and GC the entry.
      //
      // Ideally, we would not be using weak pointers in the MPC at all,
      // but CSSValues keep StyleImages alive (see
      // StyleImageCacheTest.WeakReferenceGC), so if we used regular pointers,
      // we'd need to find some other way of making sure these images do not
      // live forever in the cache.
      cache_item->RefreshKey(key.result_.GetMatchedProperties());

      return &entry;
    }
  }
  return nullptr;
}

bool CachedMatchedProperties::CorrespondsTo(
    const MatchedPropertiesVector& lookup_properties) const {
  if (lookup_properties.size() != matched_properties.size()) {
    return false;
  }

  // These incantations are to make Clang realize it does not have to
  // bounds-check.
  auto lookup_it = lookup_properties.begin();
  auto cached_it = matched_properties.begin();
  for (; lookup_it != lookup_properties.end();
       std::advance(lookup_it, 1), std::advance(cached_it, 1)) {
    CSSPropertyValueSet* cached_properties = cached_it->first.Get();
    DCHECK(!lookup_it->properties->ModifiedSinceHashing())
        << "This should have been checked in AddMatchedProperties()";
    if (cached_properties->ModifiedSinceHashing()) {
      // These properties were mutated as some point after original
      // insertion, so it is not safe to use them in the MPC
      // (Equals() below would be comparing against the current state,
      // not the state it had when the ComputedStyle in the cache
      // was built). Note that this is very unlikely to actually
      // happen in practice, since even getting here would also require
      // a hash collision.
      return false;
    }
    if (!lookup_it->properties->Equals(*cached_properties)) {
      return false;
    }
    if (lookup_it->data_ != cached_it->second) {
      return false;
    }
  }
  return true;
}

void CachedMatchedProperties::RefreshKey(
    const MatchedPropertiesVector& lookup_properties) {
  DCHECK(CorrespondsTo(lookup_properties));
  auto lookup_it = lookup_properties.begin();
  auto cached_it = matched_properties.begin();
  for (; lookup_it != lookup_properties.end();
       std::advance(lookup_it, 1), std::advance(cached_it, 1)) {
    cached_it->first = lookup_it->properties;
  }
}

void MatchedPropertiesCache::Add(const Key& key,
                                 const ComputedStyle* style,
                                 const ComputedStyle* parent_style) {
  Member<CachedMatchedProperties>& cache_item =
      cache_.insert(key.hash_, nullptr).stored_value->value;

  if (!cache_item) {
    cache_item = MakeGarbageCollected<CachedMatchedProperties>(
        style, parent_style, key.result_.GetMatchedProperties(), clock_++);
  } else {
    cache_item->entries.emplace_back(style, parent_style, clock_++);
  }
  ++cache_entries_;
}

void MatchedPropertiesCache::Clear() {
  // MatchedPropertiesCache must be cleared promptly because some
  // destructors in the properties (e.g., ~FontFallbackList) expect that
  // the destructors are called promptly without relying on a GC timing.
  for (auto& cache_entry : cache_) {
    if (cache_entry.value) {
      cache_entry.value->Clear();
    }
  }
  cache_.clear();
  cache_entries_ = 0;
}

void MatchedPropertiesCache::ClearViewportDependent() {
  EraseEntriesIf([](const CachedMatchedProperties::Entry& entry) {
    return entry.computed_style->HasViewportUnits();
  });
}

bool MatchedPropertiesCache::IsStyleCacheable(
    const ComputedStyleBuilder& builder) {
  // Properties with attr() values depend on the attribute value of the
  // originating element, thus we cannot cache based on the matched properties
  // because the value of content is retrieved from the attribute at apply time.
  if (builder.HasAttrFunction()) {
    return false;
  }
  if (builder.Zoom() != ComputedStyleInitialValues::InitialZoom()) {
    return false;
  }
  if (builder.TextAutosizingMultiplier() != 1) {
    return false;
  }
  if (builder.HasContainerRelativeUnits()) {
    return false;
  }
  if (builder.HasAnchorFunctions()) {
    // The result of anchor() and anchor-size() functions can depend on
    // the 'anchor' attribute on the element.
    return false;
  }
  // Avoiding cache for ::highlight styles, and the originating styles they are
  // associated with, because the style depends on the highlight names involved
  // and they're not cached.
  if (builder.HasPseudoElementStyle(kPseudoIdHighlight) ||
      builder.StyleType() == kPseudoIdHighlight) {
    return false;
  }
  return true;
}

bool MatchedPropertiesCache::IsCacheable(const StyleResolverState& state) {
  const ComputedStyle& parent_style = *state.ParentStyle();

  if (!IsStyleCacheable(state.StyleBuilder())) {
    return false;
  }

  // If we allowed styles with explicit inheritance in, we would have to mark
  // them as partial hits (different parents could mean that _non-inherited_
  // properties would need to be reapplied, similar to the situation with
  // ForcedColors). We don't bother tracking this, and instead just never
  // insert them.
  //
  // The “explicit inheritance” flag is stored on the parent, not the style
  // itself, since that's where we need it 90%+ of the time. This means that
  // if we do not know the flat-tree parent, StyleBuilder::ApplyProperty() will
  // not SetChildHasExplicitInheritance() on the parent style, and we do not
  // know whether this flag is true or false. However, the only two cases where
  // this can happen (root element, and unused slots in shadow trees),
  // it doesn't actually matter whether we have explicit inheritance or not,
  // since the parent style is the initial style. So even if the test returns
  // a false positive, that's fine.
  if (parent_style.ChildHasExplicitInheritance()) {
    return false;
  }

  // Matched properties can be equal for style resolves from elements in
  // different TreeScopes if StyleSheetContents is shared between stylesheets in
  // different trees. In those cases ScopedCSSNames need to be constructed with
  // the correct TreeScope and cannot be cached.
  //
  // We used to include TreeScope pointer hashes in the MPC key, but that
  // didn't allow for MPC cache hits across instances of the same web component.
  // That also caused an ever-growing cache because the TreeScopes were not
  // handled in CleanMatchedPropertiesCache().
  // See: https://crbug,com/1473836
  if (state.HasTreeScopedReference()) {
    return false;
  }

  // Do not cache computed styles for shadow root children which have a
  // different UserModify value than its shadow host.
  //
  // UserModify is modified to not inherit from the shadow host for shadow root
  // children. That means that if we get a MatchedPropertiesCache match for a
  // style stored for a shadow root child against a non shadow root child, we
  // would end up with an incorrect match.
  if (IsAtShadowBoundary(&state.GetElement()) &&
      state.StyleBuilder().UserModify() != parent_style.UserModify()) {
    return false;
  }

  // See StyleResolver::ApplyMatchedCache() for comments.
  if (state.UsesHighlightPseudoInheritance()) {
    return false;
  }
  if (!state.GetElement().GetCascadeFilter().IsEmpty()) {
    // The result of applying properties with the same matching declarations can
    // be different if the cascade filter is different.
    return false;
  }

  return true;
}

void MatchedPropertiesCache::Trace(Visitor* visitor) const {
  visitor->Trace(cache_);
  visitor->RegisterWeakCallbackMethod<
      MatchedPropertiesCache,
      &MatchedPropertiesCache::CleanMatchedPropertiesCache>(this);
}

static inline bool ShouldRemoveMPCEntry(CachedMatchedProperties& value,
                                        const LivenessBroker& info) {
  for (const auto& [properties, metadata] : value.matched_properties) {
    if (!info.IsHeapObjectAlive(properties) ||
        properties->ModifiedSinceHashing()) {
      return true;
    }
  }
  return false;
}

// Erase all MPC entries where the given predicate returns true,
// and updates the counter. Removes all keys that have no entries left.
template <class Predicate>
void MatchedPropertiesCache::EraseEntriesIf(Predicate&& pred) {
  cache_.erase_if([inner_pred{std::forward<Predicate>(pred)},
                   this](const auto& entry_pair) {
    if (!entry_pair.value) {
      return false;
    }
    HeapVector<CachedMatchedProperties::Entry, 4>& entries =
        entry_pair.value->entries;
    auto new_end = std::remove_if(entries.begin(), entries.end(), inner_pred);
    cache_entries_ -= entries.end() - new_end;
    if (new_end == entries.begin()) {
      return true;
    } else {
      entries.erase(new_end, entries.end());
      return false;
    }
  });
}

void MatchedPropertiesCache::CleanMatchedPropertiesCache(
    const LivenessBroker& info) {
  constexpr unsigned kCacheLimit = 500;
  constexpr unsigned kPruneCacheTarget = 300;

  if (cache_entries_ <= kCacheLimit) {
    // Fast path with no LRU pruning.
    cache_.erase_if([&info, this](const auto& entry_pair) {
      // A nullptr value indicates that the entry is currently being
      // created; see |MatchedPropertiesCache::Add|. Keep such entries.
      if (!entry_pair.value) {
        return false;
      }
      if (ShouldRemoveMPCEntry(*entry_pair.value, info)) {
        cache_entries_ -= entry_pair.value->entries.size();
        return true;
      }
      return false;
    });
    // Allocation of Oilpan memory is forbidden during executing weak callbacks,
    // so the data structure will not be rehashed here (ShouldShrink() internal
    // to the map returns false during such callbacks). The next
    // insertion/deletion from regular code will take care of shrinking
    // accordingly.
    return;
  }

  // Our MPC is larger than the cap; since GC happens when we are under
  // memory pressure and we are iterating over the entire map already,
  // this is a good time to enforce the cap and remove the entries that
  // are least recently used. In order not to have to do work for every
  // call, we don't prune down to the cap (500 entries), but a little
  // further (300 entries).

  Vector<unsigned> live_entries;
  live_entries.ReserveInitialCapacity(cache_entries_);
  cache_.erase_if([&info, &live_entries, this](const auto& entry_pair) {
    if (!entry_pair.value) {
      return false;
    }
    if (ShouldRemoveMPCEntry(*entry_pair.value, info)) {
      cache_entries_ -= entry_pair.value->entries.size();
      return true;
    } else {
      for (const auto& entry : entry_pair.value->entries) {
        live_entries.emplace_back(entry.last_used);
      }
      return false;
    }
  });

  DCHECK_EQ(live_entries.size(), cache_entries_);

  // If removals didn't take us back under the pruning limit,
  // remove everything older than the 300th newest LRU entry.
  if (live_entries.size() > kPruneCacheTarget) {
    unsigned cutoff_idx = live_entries.size() - kPruneCacheTarget - 1;
    // SAFETY: We just bounds-checked it above.
    std::nth_element(live_entries.begin(),
                     UNSAFE_BUFFERS(live_entries.begin() + cutoff_idx),
                     live_entries.end());
    unsigned min_last_used = live_entries[cutoff_idx];

    EraseEntriesIf(
        [min_last_used](const CachedMatchedProperties::Entry& entry) {
          return entry.last_used <= min_last_used;
        });
  }
}

std::ostream& operator<<(std::ostream& stream,
                         MatchedPropertiesCache::Key& key) {
  stream << "Key{";
  for (const MatchedProperties& matched_properties :
       key.result_.GetMatchedProperties()) {
    stream << matched_properties.properties->AsText();
    stream << ",";
  }
  stream << "}";
  return stream;
}

}  // namespace blink
