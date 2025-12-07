// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COUNTER_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COUNTER_VALUE_H_

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"

namespace blink {

class CSSPrimitiveValue;

namespace cssvalue {

// Represents a parsed single counter value for 'counter-increment',
// 'counter-set', or 'counter-reset' properties.
//
// This class captures a single counter definition, which related to the
// grammar:
//   [ <counter-name> <integer>? ]+ | none
//
// Or specifically for 'counter-reset':
//   [ <counter-name> <integer>? | <reversed-counter-name> <integer>? ]+ | none
//
// It holds the counter identifier, an optional integer value, and a flag
// indicating if the `reversed()` function syntax was used. This value is
// produced by the parser and consumed to populate |CounterDirectives|.
class CSSCounterValue final : public CSSValue {
 public:
  // |value| may be null if the integer component is omitted.
  // Note: For standard counters, the parser supplies a default value. But, for
  // `counter-reset: reversed(foo)`, the value is explicitly null.
  CSSCounterValue(const CSSCustomIdentValue& identifier,
                  const CSSPrimitiveValue* value,
                  bool is_reversed)
      : CSSValue(kCounterClass),
        identifier_(identifier),
        value_(value),
        is_reversed_(is_reversed) {}

  const CSSCustomIdentValue* Identifier() const { return identifier_; }

  // Returns the explicit integer value associated with the counter, or nullptr
  // if no value was specified (e.g., `counter-reset: reversed(foo)`).
  const CSSPrimitiveValue* Value() const { return value_; }

  bool IsReversed() const { return is_reversed_; }

  bool Equals(const CSSCounterValue& other) const {
    return Identifier() == other.Identifier() && Value() == other.Value() &&
           IsReversed() == other.IsReversed();
  }

  void TraceAfterDispatch(blink::Visitor* v) const {
    v->Trace(identifier_);
    v->Trace(value_);
    CSSValue::TraceAfterDispatch(v);
  }

  String CustomCSSText() const;

 private:
  Member<const CSSCustomIdentValue> identifier_;
  Member<const CSSPrimitiveValue> value_;
  // Indicates if the counter is declared with the reversed() function.
  // https://drafts.csswg.org/css-lists-3/#counter-reset
  bool is_reversed_ = false;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSCounterValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsCounterValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COUNTER_VALUE_H_
