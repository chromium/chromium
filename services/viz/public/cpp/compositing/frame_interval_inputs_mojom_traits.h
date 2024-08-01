// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_INTERVAL_INPUTS_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_INTERVAL_INPUTS_MOJOM_TRAITS_H_

#include <vector>

#include "components/viz/common/quads/frame_interval_inputs.h"
#include "services/viz/public/mojom/compositing/frame_interval_inputs.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::ContentFrameIntervalType,
                  viz::ContentFrameIntervalType> {
  static viz::mojom::ContentFrameIntervalType ToMojom(
      viz::ContentFrameIntervalType type);
  static bool FromMojom(viz::mojom::ContentFrameIntervalType input,
                        viz::ContentFrameIntervalType* out);
};

template <>
struct StructTraits<viz::mojom::ContentFrameIntervalInfoDataView,
                    viz::ContentFrameIntervalInfo> {
  static viz::ContentFrameIntervalType type(
      const viz::ContentFrameIntervalInfo& info) {
    return info.type;
  }

  static base::TimeDelta frame_interval(
      const viz::ContentFrameIntervalInfo& info) {
    return info.frame_interval;
  }

  static uint32_t duplicate_count(const viz::ContentFrameIntervalInfo& info) {
    return info.duplicate_count;
  }

  static bool Read(viz::mojom::ContentFrameIntervalInfoDataView info,
                   viz::ContentFrameIntervalInfo* out);
};

template <>
struct StructTraits<viz::mojom::FrameIntervalInputsDataView,
                    viz::FrameIntervalInputs> {
  static base::TimeTicks frame_time(const viz::FrameIntervalInputs& inputs) {
    return inputs.frame_time;
  }

  static bool has_input(const viz::FrameIntervalInputs& inputs) {
    return inputs.has_input;
  }

  static const std::vector<viz::ContentFrameIntervalInfo>&
  content_interval_info(const viz::FrameIntervalInputs& inputs) {
    return inputs.content_interval_info;
  }

  static bool has_only_content_frame_interval_updates(
      const viz::FrameIntervalInputs& inputs) {
    return inputs.has_only_content_frame_interval_updates;
  }

  static bool Read(viz::mojom::FrameIntervalInputsDataView inputs,
                   viz::FrameIntervalInputs* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_INTERVAL_INPUTS_MOJOM_TRAITS_H_
