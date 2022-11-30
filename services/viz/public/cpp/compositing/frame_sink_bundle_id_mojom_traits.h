// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_SINK_BUNDLE_ID_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_SINK_BUNDLE_ID_MOJOM_TRAITS_H_

#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle_id.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::FrameSinkBundleIdDataView,
                    viz::FrameSinkBundleId> {
  static uint32_t client_id(const viz::FrameSinkBundleId& frame_sink_id) {
    return frame_sink_id.client_id();
  }

  static uint32_t bundle_id(const viz::FrameSinkBundleId& frame_sink_id) {
    return frame_sink_id.bundle_id();
  }

  static bool Read(viz::mojom::FrameSinkBundleIdDataView data,
                   viz::FrameSinkBundleId* out) {
    *out = viz::FrameSinkBundleId(data.client_id(), data.bundle_id());
    return out->is_valid();
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_SINK_BUNDLE_ID_MOJOM_TRAITS_H_
