// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_SINK_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_SINK_ID_MOJOM_TRAITS_H_

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "services/viz/public/mojom/compositing/frame_sink_id.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::FrameSinkIdDataView, viz::FrameSinkId> {
  static uint32_t client_id(const viz::FrameSinkId& frame_sink_id) {
    return frame_sink_id.client_id();
  }

  static uint32_t sink_id(const viz::FrameSinkId& frame_sink_id) {
    return frame_sink_id.sink_id();
  }

  static bool Read(viz::mojom::FrameSinkIdDataView data,
                   viz::FrameSinkId* out) {
    *out = viz::FrameSinkId(data.client_id(), data.sink_id());
    return out->is_valid();
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_SINK_ID_MOJOM_TRAITS_H_
