// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GRID_INTEGER_REPEAT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GRID_INTEGER_REPEAT_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {
namespace cssvalue {

// CSSGridIntegerRepeatValue stores the track sizes and line numbers when the
// integer-repeat syntax is used.
//
// Right now the integer-repeat syntax is as follows:
// <track-repeat> = repeat( [ <positive-integer> ],
//                          [ <line-names>? <track-size> ]+ <line-names>? )
// <fixed-repeat> = repeat( [ <positive-integer> ],
//                          [ <line-names>? <fixed-size> ]+ <line-names>? )
class CSSGridIntegerRepeatValue : public CSSValueList {
 public:
  CSSGridIntegerRepeatValue(size_t repetitions)
      : CSSValueList(kGridIntegerRepeatClass, kSpaceSeparator),
        repetitions_(repetitions) {
    DCHECK_GT(repetitions, 0UL);
  }

  String CustomCSSText() const;
  bool Equals(const CSSGridIntegerRepeatValue&) const;

  size_t Repetitions() const { return repetitions_; }

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValueList::TraceAfterDispatch(visitor);
  }

 private:
  const size_t repetitions_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSGridIntegerRepeatValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsGridIntegerRepeatValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GRID_INTEGER_REPEAT_VALUE_H_
