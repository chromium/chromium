// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_MOJOM_TRAITS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_MOJOM_TRAITS_H_

#include <utility>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/model_file_mojom_traits.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
    StructTraits<on_device_model::mojom::ModelAssetsDataView,
                 on_device_model::ModelAssets> {
  static on_device_model::ModelFile weights(
      on_device_model::ModelAssets& assets) {
    return std::move(assets.weights);
  }

  static base::FilePath sp_model_path(on_device_model::ModelAssets& assets) {
    return std::move(assets.sp_model_path);
  }

  static base::File cache(on_device_model::ModelAssets& assets) {
    return std::move(assets.cache);
  }

  static bool Read(on_device_model::mojom::ModelAssetsDataView data,
                   on_device_model::ModelAssets* assets);
};

}  // namespace mojo

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_MOJOM_TRAITS_H_
