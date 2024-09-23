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

  static bool Read(viz::mojom::OffsetTagDataView data, viz::OffsetTag* out);
};

template <>
struct StructTraits<viz::mojom::OffsetTagValueDataView, viz::OffsetTagValue> {
  static const viz::OffsetTag& tag(const viz::OffsetTagValue& input) {
    CHECK(input.IsValid());
    return input.tag;
  }

  static const gfx::Vector2dF& offset(const viz::OffsetTagValue& input) {
    return input.offset;
  }

  static bool Read(viz::mojom::OffsetTagValueDataView data,
                   viz::OffsetTagValue* out);
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

  static bool Read(viz::mojom::OffsetTagDefinitionDataView data,
                   viz::OffsetTagDefinition* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_OFFSET_TAG_MOJOM_TRAITS_H_
