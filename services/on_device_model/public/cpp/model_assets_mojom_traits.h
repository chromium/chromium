// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_MOJOM_TRAITS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
    StructTraits<on_device_model::mojom::ModelAssetsDataView,
                 on_device_model::ModelAssets> {
  static base::File sp_model(on_device_model::ModelAssets& assets) {
    return std::move(assets.sp_model);
  }

  static base::File model(on_device_model::ModelAssets& assets) {
    return std::move(assets.model);
  }

  static base::File weights(on_device_model::ModelAssets& assets) {
    return std::move(assets.weights);
  }

  static bool Read(on_device_model::mojom::ModelAssetsDataView data,
                   on_device_model::ModelAssets* assets);
};

}  // namespace mojo

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_MODEL_ASSETS_MOJOM_TRAITS_H_
