// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_BITMAP_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_BITMAP_ID_MOJOM_TRAITS_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "components/viz/common/resources/shared_bitmap_id.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "services/viz/public/mojom/compositing/shared_bitmap_id.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::SharedBitmapIdDataView, viz::SharedBitmapId> {
  static base::span<const uint8_t> name(
      const viz::SharedBitmapId& shared_bitmap_id) {
    return shared_bitmap_id.name;
  }
  static bool Read(viz::mojom::SharedBitmapIdDataView data,
                   viz::SharedBitmapId* out) {
    base::span<uint8_t> shared_bitmap_id_name(out->name);
    return data.ReadName(&shared_bitmap_id_name);
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_BITMAP_ID_MOJOM_TRAITS_H_
