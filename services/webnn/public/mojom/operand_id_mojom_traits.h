// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_OPERAND_ID_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_OPERAND_ID_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(WEBNN_MOJOM_TRAITS)
    StructTraits<webnn::mojom::OperandIdDataView, webnn::OperandId> {
  static webnn::OperandId::underlying_type id(
      const webnn::OperandId& operand_id) {
    return operand_id.value();
  }

  static bool Read(webnn::mojom::OperandIdDataView data,
                   webnn::OperandId* out) {
    out->value() = data.id();
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_OPERAND_ID_MOJOM_TRAITS_H_
