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
  // base::FilePath doesn't have nullable StructTraits, so we need to use
  // optional.
  std::optional<base::FilePath> weights_path;
  bool ok =
      data.ReadWeights(&assets->weights) && data.ReadWeightsPath(&weights_path);
  if (!ok) {
    return false;
  }
  if (weights_path.has_value()) {
    assets->weights_path = *weights_path;
  }
  return true;
}

}  // namespace mojo
