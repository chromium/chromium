// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_OPERAND_DESCRIPTOR_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_OPERAND_DESCRIPTOR_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(WEBNN_MOJOM_TRAITS)
    StructTraits<webnn::mojom::OperandDescriptorDataView,
                 webnn::OperandDescriptor> {
  static webnn::mojom::DataType data_type(
      const webnn::OperandDescriptor& descriptor);
  static const std::vector<uint32_t>& shape(
      const webnn::OperandDescriptor& descriptor) {
    return descriptor.shape();
  }

  static bool Read(webnn::mojom::OperandDescriptorDataView data,
                   webnn::OperandDescriptor* out);
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_OPERAND_DESCRIPTOR_MOJOM_TRAITS_H_
