// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_NUMBER_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_NUMBER_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "services/webnn/public/cpp/ml_number.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(WEBNN_MOJOM_TRAITS)
    UnionTraits<webnn::mojom::NumberDataView, webnn::MLNumber> {
  static webnn::mojom::NumberDataView::Tag GetTag(
      const webnn::MLNumber& number) {
    switch (number.GetBaseType()) {
      case webnn::MLNumber::BaseType::kFloatingPoint:
        return webnn::mojom::NumberDataView::Tag::kFloatingPoint;
      case webnn::MLNumber::BaseType::kSignedInteger:
        return webnn::mojom::NumberDataView::Tag::kSignedInteger;
      case webnn::MLNumber::BaseType::kUnsignedInteger:
        return webnn::mojom::NumberDataView::Tag::kUnsignedInteger;
    }
  }

  static double floating_point(const webnn::MLNumber& number) {
    return number.AsFloat64();
  }

  static int64_t signed_integer(const webnn::MLNumber& number) {
    return number.AsInt64();
  }

  static int64_t unsigned_integer(const webnn::MLNumber& number) {
    return number.AsUint64();
  }

  static bool Read(webnn::mojom::NumberDataView data, webnn::MLNumber* out) {
    switch (data.tag()) {
      case webnn::mojom::NumberDataView::Tag::kFloatingPoint:
        *out = webnn::MLNumber::FromFloat64(data.floating_point());
        break;
      case webnn::mojom::NumberDataView::Tag::kSignedInteger:
        *out = webnn::MLNumber::FromInt64(data.signed_integer());
        break;
      case webnn::mojom::NumberDataView::Tag::kUnsignedInteger:
        *out = webnn::MLNumber::FromUint64(data.unsigned_integer());
        break;
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_NUMBER_MOJOM_TRAITS_H_
