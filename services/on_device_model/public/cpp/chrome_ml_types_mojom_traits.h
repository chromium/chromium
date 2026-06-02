// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CHROME_ML_TYPES_MOJOM_TRAITS_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CHROME_ML_TYPES_MOJOM_TRAITS_H_

#include "services/on_device_model/ml/chrome_ml_types.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom-shared.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

template <>
struct EnumTraits<on_device_model::mojom::Token, ml::Token> {
  static on_device_model::mojom::Token ToMojom(ml::Token input);
  static ml::Token FromMojom(on_device_model::mojom::Token input);
};

template <>
struct EnumTraits<on_device_model::mojom::ModelBackendType,
                  ml::ModelBackendType> {
  static on_device_model::mojom::ModelBackendType ToMojom(
      ml::ModelBackendType input);
  static ml::ModelBackendType FromMojom(
      on_device_model::mojom::ModelBackendType input);
};

template <>
struct EnumTraits<on_device_model::mojom::ModelPerformanceHint,
                  ml::ModelPerformanceHint> {
  static on_device_model::mojom::ModelPerformanceHint ToMojom(
      ml::ModelPerformanceHint input);
  static ml::ModelPerformanceHint FromMojom(
      on_device_model::mojom::ModelPerformanceHint input);
};

}  // namespace mojo

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_CHROME_ML_TYPES_MOJOM_TRAITS_H_
