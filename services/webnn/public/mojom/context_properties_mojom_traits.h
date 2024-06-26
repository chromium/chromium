// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_CONTEXT_PROPERTIES_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_CONTEXT_PROPERTIES_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-shared.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::ContextPropertiesDataView,
                    webnn::ContextProperties> {
  static webnn::mojom::InputOperandLayout conv2d_input_layout(
      const webnn::ContextProperties& context_properties) {
    switch (context_properties.conv2d_input_layout) {
      case webnn::InputOperandLayout::kNchw:
        return webnn::mojom::InputOperandLayout::kChannelsFirst;
      case webnn::InputOperandLayout::kNhwc:
        return webnn::mojom::InputOperandLayout::kChannelsLast;
    };
  }
  static webnn::SupportedDataTypes input_supported_data_types(
      const webnn::ContextProperties& context_properties) {
    return context_properties.input_supported_data_types;
  }
  static webnn::SupportedDataTypes constant_supported_data_types(
      const webnn::ContextProperties& context_properties) {
    return context_properties.constant_supported_data_types;
  }
  static webnn::SupportedDataTypes gather_input_supported_data_types(
      const webnn::ContextProperties& context_properties) {
    return context_properties.gather_input_supported_data_types;
  }
  static webnn::SupportedDataTypes gather_indices_supported_data_types(
      const webnn::ContextProperties& context_properties) {
    return context_properties.gather_indices_supported_data_types;
  }

  static bool Read(webnn::mojom::ContextPropertiesDataView data,
                   webnn::ContextProperties* out) {
    switch (data.conv2d_input_layout()) {
      case webnn::mojom::InputOperandLayout::kChannelsFirst:
        out->conv2d_input_layout = webnn::InputOperandLayout::kNchw;
        break;
      case webnn::mojom::InputOperandLayout::kChannelsLast:
        out->conv2d_input_layout = webnn::InputOperandLayout::kNhwc;
        break;
    }
    return data.ReadInputSupportedDataTypes(&out->input_supported_data_types) &&
           data.ReadConstantSupportedDataTypes(
               &out->constant_supported_data_types) &&
           data.ReadGatherInputSupportedDataTypes(
               &out->gather_input_supported_data_types) &&
           data.ReadGatherIndicesSupportedDataTypes(
               &out->gather_indices_supported_data_types);
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_CONTEXT_PROPERTIES_MOJOM_TRAITS_H_
