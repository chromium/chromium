// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_CONTEXT_PROPERTIES_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_CONTEXT_PROPERTIES_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-shared.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::ContextPropertiesDataView,
                    webnn::ContextProperties> {
  static webnn::mojom::InputOperandLayout input_operand_layout(
      const webnn::ContextProperties& context_properties) {
    switch (context_properties.input_operand_layout) {
      case webnn::InputOperandLayout::kNchw:
        return webnn::mojom::InputOperandLayout::kChannelsFirst;
      case webnn::InputOperandLayout::kNhwc:
        return webnn::mojom::InputOperandLayout::kChannelsLast;
    };
  }
  static webnn::DataTypeLimits data_type_limits(
      const webnn::ContextProperties& context_properties) {
    return context_properties.data_type_limits;
  }

  static bool Read(webnn::mojom::ContextPropertiesDataView data,
                   webnn::ContextProperties* out) {
    switch (data.input_operand_layout()) {
      case webnn::mojom::InputOperandLayout::kChannelsFirst:
        out->input_operand_layout = webnn::InputOperandLayout::kNchw;
        break;
      case webnn::mojom::InputOperandLayout::kChannelsLast:
        out->input_operand_layout = webnn::InputOperandLayout::kNhwc;
        break;
    }
    return data.ReadDataTypeLimits(&out->data_type_limits);
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_CONTEXT_PROPERTIES_MOJOM_TRAITS_H_
