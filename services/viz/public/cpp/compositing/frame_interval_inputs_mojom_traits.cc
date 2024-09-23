// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/frame_interval_inputs_mojom_traits.h"

#include "base/notreached.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

viz::mojom::ContentFrameIntervalType
EnumTraits<viz::mojom::ContentFrameIntervalType,
           viz::ContentFrameIntervalType>::ToMojom(viz::ContentFrameIntervalType
                                                       type) {
  switch (type) {
    case viz::ContentFrameIntervalType::kVideo:
      return viz::mojom::ContentFrameIntervalType::kVideo;
    case viz::ContentFrameIntervalType::kAnimatingImage:
      return viz::mojom::ContentFrameIntervalType::kAnimatingImage;
    case viz::ContentFrameIntervalType::kScrollBarFadeOutAnimation:
      return viz::mojom::ContentFrameIntervalType::kScrollBarFadeOutAnimation;
  }
  NOTREACHED();
}

bool EnumTraits<viz::mojom::ContentFrameIntervalType,
                viz::ContentFrameIntervalType>::
    FromMojom(viz::mojom::ContentFrameIntervalType input,
              viz::ContentFrameIntervalType* out) {
  switch (input) {
    case viz::mojom::ContentFrameIntervalType::kVideo:
      *out = viz::ContentFrameIntervalType::kVideo;
      return true;
    case viz::mojom::ContentFrameIntervalType::kAnimatingImage:
      *out = viz::ContentFrameIntervalType::kAnimatingImage;
      return true;
    case viz::mojom::ContentFrameIntervalType::kScrollBarFadeOutAnimation:
      *out = viz::ContentFrameIntervalType::kScrollBarFadeOutAnimation;
      return true;
  }
  return false;
}

bool StructTraits<viz::mojom::ContentFrameIntervalInfoDataView,
                  viz::ContentFrameIntervalInfo>::
    Read(viz::mojom::ContentFrameIntervalInfoDataView info,
         viz::ContentFrameIntervalInfo* out) {
  if (!info.ReadType(&out->type)) {
    return false;
  }
  if (!info.ReadFrameInterval(&out->frame_interval)) {
    return false;
  }
  out->duplicate_count = info.duplicate_count();
  return true;
}

bool StructTraits<viz::mojom::FrameIntervalInputsDataView,
                  viz::FrameIntervalInputs>::
    Read(viz::mojom::FrameIntervalInputsDataView inputs,
         viz::FrameIntervalInputs* out) {
  if (!inputs.ReadFrameTime(&out->frame_time)) {
    return false;
  }
  out->has_input = inputs.has_input();
  if (!inputs.ReadContentIntervalInfo(&out->content_interval_info)) {
    return false;
  }
  out->has_only_content_frame_interval_updates =
      inputs.has_only_content_frame_interval_updates();
  return true;
}

}  // namespace mojo
