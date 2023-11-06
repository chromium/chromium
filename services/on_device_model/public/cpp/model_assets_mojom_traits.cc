// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/model_assets_mojom_traits.h"

#include "services/on_device_model/public/mojom/on_device_model.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<on_device_model::mojom::ModelAssetsDataView,
                  on_device_model::ModelAssets>::
    Read(on_device_model::mojom::ModelAssetsDataView data,
         on_device_model::ModelAssets* assets) {
  return data.ReadSpModel(&assets->sp_model) &&
         data.ReadModel(&assets->model) && data.ReadWeights(&assets->weights);
}

}  // namespace mojo
