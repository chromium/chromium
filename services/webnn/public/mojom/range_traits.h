// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_RANGE_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_RANGE_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/range.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::RangeDataView, webnn::Range> {
  static uint32_t start(const webnn::Range& range) { return range.start; }
  static uint32_t size(const webnn::Range& range) { return range.size; }
  static uint32_t stride(const webnn::Range& range) { return range.stride; }

  static bool Read(webnn::mojom::RangeDataView data, webnn::Range* out) {
    // Size and stride must be greater than or equal to 1.
    if (data.is_null() || data.size() < 1 || data.stride() < 1) {
      return false;
    }
    out->start = data.start();
    out->size = data.size();
    out->stride = data.stride();

    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_RANGE_TRAITS_H_
