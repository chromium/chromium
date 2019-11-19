// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_ID_TEMPLATES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_ID_TEMPLATES_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace WTF {
template <>
struct HashTraits<blink::CSSPropertyID>
    : GenericHashTraits<blink::CSSPropertyID> {
  static const bool kEmptyValueIsZero = true;
  static void ConstructDeletedValue(blink::CSSPropertyID& slot, bool) {
    slot = static_cast<blink::CSSPropertyID>(blink::numCSSPropertyIDs);
  }
  static bool IsDeletedValue(blink::CSSPropertyID value) {
    return static_cast<int>(value) == blink::numCSSPropertyIDs;
  }
};
}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_ID_TEMPLATES_H_
