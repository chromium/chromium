// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_SUPPORTED_TENSORS_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_SUPPORTED_TENSORS_MOJOM_TRAITS_H_

#include <sys/stat.h>
#include <sys/types.h>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/supported_tensors.h"
#include "services/webnn/public/mojom/webnn_context_properties.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::SupportedRanksDataView,
                    webnn::SupportedRanks> {
  static uint32_t min(const webnn::SupportedRanks& supported_ranks) {
    return supported_ranks.min;
  }

  static uint32_t max(const webnn::SupportedRanks& supported_ranks) {
    return supported_ranks.max;
  }

  static bool Read(webnn::mojom::SupportedRanksDataView data,
                   webnn::SupportedRanks* out) {
    out->min = data.min();
    out->max = data.max();
    return true;
  }
};

template <>
struct StructTraits<webnn::mojom::SupportedTensorsDataView,
                    webnn::SupportedTensors> {
  static webnn::SupportedDataTypes data_types(
      const webnn::SupportedTensors& supported_tensors) {
    return supported_tensors.data_types;
  }

  static webnn::SupportedRanks ranks(
      const webnn::SupportedTensors& supported_tensors) {
    return supported_tensors.ranks;
  }

  static bool Read(webnn::mojom::SupportedTensorsDataView data,
                   webnn::SupportedTensors* out) {
    return data.ReadDataTypes(&out->data_types) && data.ReadRanks(&out->ranks);
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_SUPPORTED_TENSORS_MOJOM_TRAITS_H_
