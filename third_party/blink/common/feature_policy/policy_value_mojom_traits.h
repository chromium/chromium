// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_COMMON_FEATURE_POLICY_POLICY_VALUE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_COMMON_FEATURE_POLICY_POLICY_VALUE_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/feature_policy/policy_value.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    UnionTraits<blink::mojom::PolicyValueDataView, blink::PolicyValue> {
 public:
  static blink::mojom::PolicyValueDataView::Tag GetTag(
      const blink::PolicyValue& value) {
    switch (value.Type()) {
      case blink::mojom::PolicyValueType::kNull:
        return blink::mojom::PolicyValueDataView::Tag::NULL_VALUE;
      case blink::mojom::PolicyValueType::kBool:
        return blink::mojom::PolicyValueDataView::Tag::BOOL_VALUE;
      case blink::mojom::PolicyValueType::kDecDouble:
        return blink::mojom::PolicyValueDataView::Tag::DEC_DOUBLE_VALUE;
      case blink::mojom::PolicyValueType::kEnum:
        return blink::mojom::PolicyValueDataView::Tag::ENUM_VALUE;
    }

    NOTREACHED();
    return blink::mojom::PolicyValueDataView::Tag::NULL_VALUE;
  }
  static bool null_value(const blink::PolicyValue& value) { return false; }
  static bool bool_value(const blink::PolicyValue& value) {
    return value.BoolValue();
  }
  static double dec_double_value(const blink::PolicyValue& value) {
    return value.DoubleValue();
  }
  static int32_t enum_value(const blink::PolicyValue& value) {
    return value.IntValue();
  }
  static bool Read(blink::mojom::PolicyValueDataView in,
                   blink::PolicyValue* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_COMMON_FEATURE_POLICY_POLICY_VALUE_MOJOM_TRAITS_H_
