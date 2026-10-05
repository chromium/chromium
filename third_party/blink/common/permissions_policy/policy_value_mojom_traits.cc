// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/permissions_policy/policy_value_mojom_traits.h"

namespace mojo {

bool UnionTraits<blink::mojom::PolicyValueDataView, blink::PolicyValue>::Read(
    blink::mojom::PolicyValueDataView in,
    blink::PolicyValue* out) {
  switch (in.tag()) {
    case blink::mojom::PolicyValueDataView::Tag::kBoolValue:
      *out = blink::PolicyValue::CreateBool(in.bool_value());
      return true;
    case blink::mojom::PolicyValueDataView::Tag::kDecDoubleValue:
      *out = blink::PolicyValue::CreateDecDouble(in.dec_double_value());
      return true;
    case blink::mojom::PolicyValueDataView::Tag::kEnumValue:
      *out = blink::PolicyValue::CreateEnum(in.enum_value());
      return true;
  }
  return false;
}

bool UnionTraits<blink::mojom::PolicyValueDataView, blink::PolicyValue>::IsNull(
    const blink::PolicyValue& in) {
  return in.Type() == blink::mojom::PolicyValueType::kNull;
}

void UnionTraits<blink::mojom::PolicyValueDataView,
                 blink::PolicyValue>::SetToNull(blink::PolicyValue* out) {
  *out = blink::PolicyValue();
}

}  // namespace mojo
