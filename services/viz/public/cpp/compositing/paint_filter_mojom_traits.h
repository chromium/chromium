// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_PAINT_FILTER_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_PAINT_FILTER_MOJOM_TRAITS_H_

#include <vector>

#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "services/viz/public/mojom/compositing/paint_filter.mojom-shared.h"

namespace cc {
class PaintFilter;
}

namespace mojo {

template <>
struct StructTraits<viz::mojom::PaintFilterDataView, sk_sp<cc::PaintFilter>> {
  static base::Optional<std::vector<uint8_t>> data(
      const sk_sp<cc::PaintFilter>& filter);

  static bool Read(viz::mojom::PaintFilterDataView data,
                   sk_sp<cc::PaintFilter>* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_PAINT_FILTER_MOJOM_TRAITS_H_
