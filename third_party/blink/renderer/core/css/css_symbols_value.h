// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYMBOLS_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYMBOLS_VALUE_H_

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace cssvalue {

// Represents the counter style defined inline via the symbols() function.
// https://drafts.csswg.org/css-counter-styles-3/#symbols-function
class CSSSymbolsValue : public CSSValue {
 public:
  // `system` is one of cyclic|numeric|alphabetic|symbolic|fixed, or `kSymbolic`
  // when the <symbols-type> was omitted. `symbols` is a space-separated list of
  // <string> values that represent the counter's symbols.
  CSSSymbolsValue(CSSValueID system, const CSSValueList* symbols)
      : CSSValue(kSymbolsClass), system_(system), symbols_(symbols) {}

  CSSValueID GetSystem() const { return system_; }
  const CSSValueList& Symbols() const { return *symbols_; }

  String CustomCSSText() const;

  bool Equals(const CSSSymbolsValue& other) const {
    return system_ == other.system_ &&
           base::ValuesEquivalent(symbols_, other.symbols_);
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  CSSValueID system_;
  Member<const CSSValueList> symbols_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSSymbolsValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsSymbolsValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYMBOLS_VALUE_H_
