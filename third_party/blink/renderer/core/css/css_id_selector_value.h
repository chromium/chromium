// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ID_SELECTOR_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ID_SELECTOR_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace cssvalue {

class CORE_EXPORT CSSIdSelectorValue : public CSSValue {
 public:
  explicit CSSIdSelectorValue(const String&);

  const AtomicString& Id() const { return id_; }

  String CustomCSSText() const;

  bool Equals(const CSSIdSelectorValue& other) const {
    return id_ == other.id_;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  AtomicString id_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSIdSelectorValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsIdSelectorValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ID_SELECTOR_VALUE_H_
