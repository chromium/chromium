// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_VARIABLES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_VARIABLES_H_

#include <iosfwd>
#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

// Contains values for custom properties.
//
// Each custom property has "variable data" and optionally a "variable value".
//
// * Data:  A CSSVariableData that contains the tokens used for substitution.
// * Value: An optional CSSValue that may be present if the custom property
//          is registered with a non-universal syntax descriptor.
//
// Note that StyleVariables may explicitly contain a nullptr value for a given
// custom property. This is necessary to be able to mark variables that become
// invalid at computed-value time [1] as such.
//
// If StyleVariables does not contain an entry at all for a given property,
// std::nullopt is returned. This allows us to differentiate between the case
// where we want to try to find the variable elsewhere (e.g. StyleInitialData,
// in the case of std::nullopt), or return nullptr without looking further.
//
// Due to the subtleties introduced by the root-bucket optimization in
// StyleInheritedVariables, there is deliberately no way to erase an entry
// from StyleVariables. This means that non-implicit initial/inherited values
// must be explicitly stored.
//
// [1] https://drafts.csswg.org/css-variables/#invalid-at-computed-value-time
class CORE_EXPORT StyleVariables {
  DISALLOW_NEW();

 public:
  using DataMap = HeapHashMap<AtomicString, Member<CSSVariableData>>;
  using ValueMap = HeapHashMap<AtomicString, Member<const CSSValue>>;

  StyleVariables() = default;
  StyleVariables(const StyleVariables&) = default;
  StyleVariables(StyleVariables&&) = default;
  StyleVariables& operator=(const StyleVariables&) = default;
  StyleVariables& operator=(StyleVariables&&) = default;

  void Trace(Visitor* visitor) const {
    visitor->Trace(data_);
    visitor->Trace(values_);
  }

  bool operator==(const StyleVariables& other) const;
  bool operator!=(const StyleVariables& other) const {
    return !(*this == other);
  }

  using OptionalData = std::optional<CSSVariableData*>;
  using OptionalValue = std::optional<const CSSValue*>;

  OptionalValue GetValue(const AtomicString&) const;
  OptionalData GetData(const AtomicString&) const;
  void SetData(const AtomicString&, CSSVariableData*);
  void SetValue(const AtomicString&, const CSSValue*);

  bool IsEmpty() const;
  void CollectNames(HashSet<AtomicString>&) const;

  const DataMap& Data() const { return data_; }
  const ValueMap& Values() const { return values_; }

 private:
  DataMap data_;
  ValueMap values_;

  // Cache for speeding up operator==. Some pages tend to set large numbers
  // of custom properties on elements high up in the DOM, and the sets of
  // custom properties generally inherit wholesale, requiring us to
  // compare the same pair of StyleVariables against each other over and over
  // again. Thus, we cache the last comparison we did, with its result.
  //
  // For the cache to be valid, the two elements must have each other as
  // cached partner. This allows us to easily and safely invalidate the cache
  // from either side when a mutation happens: Just set our side to
  // nullptr, without worrying about invalidating the other side (which may have
  // been freed in the meantime). It also lets us easily catch the (relatively
  // obscure) case where the other side has been deallocated and a newly
  // constructed object has reused its address, since it will be constructed
  // with a nullptr partner.
  mutable const StyleVariables* equality_cache_partner_ = nullptr;
  mutable bool equality_cached_result_;

  friend CORE_EXPORT std::ostream& operator<<(std::ostream& stream,
                                              const StyleVariables& variables);
};

CORE_EXPORT std::ostream& operator<<(std::ostream& stream,
                                     const StyleVariables& variables);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_VARIABLES_H_
