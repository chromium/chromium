// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/resource_id_mojom_traits.h"

#include "components/viz/common/resources/resource_id.h"

namespace mojo {

// static
uint32_t StructTraits<viz::mojom::ResourceIdDataView, viz::ResourceId>::value(
    const viz::ResourceId& id) {
  // We cannot send resource ids in the viz reserved range.
  DCHECK_LT(id, viz::kVizReservedRangeStartId);
  return static_cast<uint32_t>(id);
}

// static
bool StructTraits<viz::mojom::ResourceIdDataView, viz::ResourceId>::Read(
    viz::mojom::ResourceIdDataView data,
    viz::ResourceId* out) {
  viz::ResourceId result(data.value());
  // We cannot receive resource ids in the viz reserved range.
  if (result >= viz::kVizReservedRangeStartId)
    return false;
  *out = result;
  return true;
}

}  // namespace mojo
