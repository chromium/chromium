// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/feature_policy/policy_value_mojom_traits.h"

namespace mojo {

bool UnionTraits<blink::mojom::PolicyValueDataView, blink::PolicyValue>::Read(
    blink::mojom::PolicyValueDataView in,
    blink::PolicyValue* out) {
  switch (in.tag()) {
    case blink::mojom::PolicyValueDataView::Tag::BOOL_VALUE:
      out->SetType(blink::mojom::PolicyValueType::kBool);
      out->SetBoolValue(in.bool_value());
      return true;
    case blink::mojom::PolicyValueDataView::Tag::DEC_DOUBLE_VALUE:
      out->SetType(blink::mojom::PolicyValueType::kDecDouble);
      out->SetDoubleValue(in.dec_double_value());
      return true;
    case blink::mojom::PolicyValueDataView::Tag::ENUM_VALUE:
      out->SetType(blink::mojom::PolicyValueType::kEnum);
      out->SetIntValue(in.enum_value());
      return true;
    case blink::mojom::PolicyValueDataView::Tag::NULL_VALUE:
      break;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
