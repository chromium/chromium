// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/offset_tag_mojom_traits.h"

namespace mojo {

bool StructTraits<viz::mojom::OffsetTagDataView, viz::OffsetTag>::Read(
    viz::mojom::OffsetTagDataView data,
    viz::OffsetTag* out) {
  return data.ReadToken(&out->token_);
}

bool StructTraits<viz::mojom::OffsetTagValueDataView, viz::OffsetTagValue>::
    Read(viz::mojom::OffsetTagValueDataView data, viz::OffsetTagValue* out) {
  return data.ReadTag(&out->tag) && data.ReadOffset(&out->offset) &&
         out->IsValid();
}

bool StructTraits<viz::mojom::OffsetTagDefinitionDataView,
                  viz::OffsetTagDefinition>::
    Read(viz::mojom::OffsetTagDefinitionDataView data,
         viz::OffsetTagDefinition* out) {
  return data.ReadTag(&out->tag) && data.ReadProvider(&out->provider) &&
         data.ReadMinOffset(&out->constraints.min_offset) &&
         data.ReadMaxOffset(&out->constraints.max_offset) && out->IsValid();
}

}  // namespace mojo
