// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_OFFSET_TAG_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_OFFSET_TAG_MOJOM_TRAITS_H_

#include "components/viz/common/quads/offset_tag.h"
#include "mojo/public/cpp/base/token_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_range_mojom_traits.h"
#include "services/viz/public/mojom/compositing/offset_tag.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::OffsetTagDataView, viz::OffsetTag> {
  static const base::Token& token(const viz::OffsetTag& input) {
    return input.token_;
  }

  static inline bool Read(viz::mojom::OffsetTagDataView data,
                          viz::OffsetTag* out) {
    return data.ReadToken(&out->token_);
  }
};

template <>
struct StructTraits<viz::mojom::OffsetTagValueDataView, viz::OffsetTagValue> {
  static const viz::OffsetTag& tag(const viz::OffsetTagValue& input) {
    DCHECK(input.IsValid());
    return input.tag;
  }

  static const gfx::Vector2dF& offset(const viz::OffsetTagValue& input) {
    return input.offset;
  }

  static inline bool Read(viz::mojom::OffsetTagValueDataView data,
                          viz::OffsetTagValue* out) {
    return data.ReadTag(&out->tag) && data.ReadOffset(&out->offset) &&
           out->IsValid();
  }
};

template <>
struct StructTraits<viz::mojom::OffsetTagDefinitionDataView,
                    viz::OffsetTagDefinition> {
  static const viz::OffsetTag& tag(const viz::OffsetTagDefinition& input) {
    CHECK(input.IsValid());
    return input.tag;
  }

  static const viz::SurfaceRange& provider(
      const viz::OffsetTagDefinition& input) {
    return input.provider;
  }

  static const gfx::Vector2dF& min_offset(
      const viz::OffsetTagDefinition& input) {
    return input.constraints.min_offset;
  }

  static const gfx::Vector2dF& max_offset(
      const viz::OffsetTagDefinition& input) {
    return input.constraints.max_offset;
  }

  static inline bool Read(viz::mojom::OffsetTagDefinitionDataView data,
                          viz::OffsetTagDefinition* out) {
    return data.ReadTag(&out->tag) && data.ReadProvider(&out->provider) &&
           data.ReadMinOffset(&out->constraints.min_offset) &&
           data.ReadMaxOffset(&out->constraints.max_offset) && out->IsValid();
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_OFFSET_TAG_MOJOM_TRAITS_H_
