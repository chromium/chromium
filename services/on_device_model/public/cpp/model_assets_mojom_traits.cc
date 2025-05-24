// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/model_assets_mojom_traits.h"

#include <optional>

#include "base/files/file_path.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<on_device_model::mojom::ModelAssetsDataView,
                  on_device_model::ModelAssets>::
    Read(on_device_model::mojom::ModelAssetsDataView data,
         on_device_model::ModelAssets* assets) {
  // base::FilePath doesn't have nullable StructTraits, so we need to use
  // optional.
  std::optional<base::FilePath> sp_model_path;
  bool ok = data.ReadWeights(&assets->weights) &&
            data.ReadSpModelPath(&sp_model_path) &&
            data.ReadCache(&assets->cache);
  if (!ok) {
    return false;
  }
  if (sp_model_path.has_value()) {
    assets->sp_model_path = *sp_model_path;
  }
  return true;
}

}  // namespace mojo
