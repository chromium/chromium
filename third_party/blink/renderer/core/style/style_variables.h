// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_VARIABLES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_VARIABLES_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
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
// base::nullopt is returned. This allows us to differentiate between the case
// where we want to try to find the variable elsewhere (e.g. StyleInitialData,
// in the case of base::nullopt), or return nullptr without looking further.
//
// Due to the subtleties introduced by the root-bucket optimization in
// StyleInheritedVariables, there is deliberately no way to erase an entry
// from StyleVariables. This means that non-implicit initial/inherited values
// must be explicitly stored.
//
// [1] https://drafts.csswg.org/css-variables/#invalid-at-computed-value-time
class CORE_EXPORT StyleVariables {
  USING_FAST_MALLOC(StyleVariables);

 public:
  using DataMap = HashMap<AtomicString, scoped_refptr<CSSVariableData>>;
  using ValueMap = HeapHashMap<AtomicString, Member<const CSSValue>>;

  StyleVariables();
  StyleVariables(const StyleVariables&);

  bool operator==(const StyleVariables& other) const;
  bool operator!=(const StyleVariables& other) const {
    return !(*this == other);
  }

  using OptionalData = base::Optional<CSSVariableData*>;
  using OptionalValue = base::Optional<Member<const CSSValue>>;

  OptionalValue GetValue(const AtomicString&) const;
  OptionalData GetData(const AtomicString&) const;
  void SetData(const AtomicString&, scoped_refptr<CSSVariableData>);
  void SetValue(const AtomicString&, const CSSValue*);

  bool IsEmpty() const;
  HashSet<AtomicString> GetNames() const;

  const DataMap& Data() const { return data_; }
  const ValueMap& Values() const { return *values_; }

 private:
  DataMap data_;
  Persistent<ValueMap> values_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_VARIABLES_H_
