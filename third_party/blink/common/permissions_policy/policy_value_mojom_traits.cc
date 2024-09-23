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
      out->SetType(blink::mojom::PolicyValueType::kBool);
      out->SetBoolValue(in.bool_value());
      return true;
    case blink::mojom::PolicyValueDataView::Tag::kDecDoubleValue:
      out->SetType(blink::mojom::PolicyValueType::kDecDouble);
      out->SetDoubleValue(in.dec_double_value());
      return true;
    case blink::mojom::PolicyValueDataView::Tag::kEnumValue:
      out->SetType(blink::mojom::PolicyValueType::kEnum);
      out->SetIntValue(in.enum_value());
      return true;
    case blink::mojom::PolicyValueDataView::Tag::kNullValue:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
