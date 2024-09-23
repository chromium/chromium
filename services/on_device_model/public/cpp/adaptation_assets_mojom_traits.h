// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_ADAPTATION_ASSETS_MOJOM_TRAITS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_ADAPTATION_ASSETS_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/read_only_file_mojom_traits.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(ON_DEVICE_MODEL_ASSETS_CPP)
    StructTraits<on_device_model::mojom::AdaptationAssetsDataView,
                 on_device_model::AdaptationAssets> {
  static base::File weights(on_device_model::AdaptationAssets& assets) {
    return std::move(assets.weights);
  }

  static base::FilePath weights_path(
      on_device_model::AdaptationAssets& assets) {
    return std::move(assets.weights_path);
  }

  static bool Read(on_device_model::mojom::AdaptationAssetsDataView data,
                   on_device_model::AdaptationAssets* assets);
};

}  // namespace mojo

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_ADAPTATION_ASSETS_MOJOM_TRAITS_H_
