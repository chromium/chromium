// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_DELEGATED_INK_METADATA_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_DELEGATED_INK_METADATA_MOJOM_TRAITS_H_

#include <memory>

#include "components/viz/common/delegated_ink_metadata.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "services/viz/public/mojom/compositing/delegated_ink_metadata.mojom-shared.h"
#include "skia/public/mojom/skcolor_mojom_traits.h"
#include "ui/gfx/mojom/rrect_f_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::DelegatedInkMetadataDataView,
                    std::unique_ptr<viz::DelegatedInkMetadata>> {
  static bool IsNull(const std::unique_ptr<viz::DelegatedInkMetadata>& input) {
    return !input;
  }

  static void SetToNull(std::unique_ptr<viz::DelegatedInkMetadata>* input) {
    input->reset();
  }

  static const gfx::PointF& point(
      const std::unique_ptr<viz::DelegatedInkMetadata>& input) {
    return input->point();
  }

  static double diameter(
      const std::unique_ptr<viz::DelegatedInkMetadata>& input) {
    return input->diameter();
  }

  static SkColor color(
      const std::unique_ptr<viz::DelegatedInkMetadata>& input) {
    return input->color();
  }

  static base::TimeTicks timestamp(
      const std::unique_ptr<viz::DelegatedInkMetadata>& input) {
    return input->timestamp();
  }

  static const gfx::RectF& presentation_area(
      const std::unique_ptr<viz::DelegatedInkMetadata>& input) {
    return input->presentation_area();
  }

  static base::TimeTicks frame_time(
      const std::unique_ptr<viz::DelegatedInkMetadata>& input) {
    return input->frame_time();
  }

  static bool is_hovering(
      const std::unique_ptr<viz::DelegatedInkMetadata>& input) {
    return input->is_hovering();
  }

  static bool Read(viz::mojom::DelegatedInkMetadataDataView data,
                   std::unique_ptr<viz::DelegatedInkMetadata>* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_DELEGATED_INK_METADATA_MOJOM_TRAITS_H_
