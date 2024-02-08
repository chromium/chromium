// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/adaptation_assets_mojom_traits.h"

#include "services/on_device_model/public/mojom/on_device_model.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<on_device_model::mojom::AdaptationAssetsDataView,
                  on_device_model::AdaptationAssets>::
    Read(on_device_model::mojom::AdaptationAssetsDataView data,
         on_device_model::AdaptationAssets* assets) {
  return data.ReadModel(&assets->model) && data.ReadWeights(&assets->weights);
}

}  // namespace mojo
