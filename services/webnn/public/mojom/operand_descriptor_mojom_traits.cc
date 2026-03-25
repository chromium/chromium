// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/mojom/operand_descriptor_mojom_traits.h"

#include "base/notreached.h"
#include "base/types/expected.h"

namespace mojo {

// static
webnn::OperandDataType StructTraits<webnn::mojom::OperandDescriptorDataView,
                                    webnn::OperandDescriptor>::
    data_type(const webnn::OperandDescriptor& descriptor) {
  return descriptor.data_type();
}

// static
bool StructTraits<webnn::mojom::OperandDescriptorDataView,
                  webnn::OperandDescriptor>::
    Read(webnn::mojom::OperandDescriptorDataView data,
         webnn::OperandDescriptor* out) {
  mojo::ArrayDataView<uint32_t> shape;
  data.GetShapeDataView(&shape);

  mojo::ArrayDataView<uint32_t> pending_permutation;
  data.GetPendingPermutationDataView(&pending_permutation);

  webnn::OperandDataType data_type;
  if (!data.ReadDataType(&data_type)) {
    return false;
  }
  base::expected<webnn::OperandDescriptor, std::string> descriptor =
      webnn::OperandDescriptor::CreateForDeserialization(
          data_type, base::span(shape), base::span(pending_permutation));

  if (!descriptor.has_value()) {
    return false;
  }

  *out = *std::move(descriptor);
  return true;
}

// static
webnn::mojom::DataType
EnumTraits<webnn::mojom::DataType, webnn::OperandDataType>::ToMojom(
    webnn::OperandDataType input) {
  switch (input) {
    case webnn::OperandDataType::kFloat32:
      return webnn::mojom::DataType::kFloat32;
    case webnn::OperandDataType::kFloat16:
      return webnn::mojom::DataType::kFloat16;
    case webnn::OperandDataType::kInt32:
      return webnn::mojom::DataType::kInt32;
    case webnn::OperandDataType::kUint32:
      return webnn::mojom::DataType::kUint32;
    case webnn::OperandDataType::kInt64:
      return webnn::mojom::DataType::kInt64;
    case webnn::OperandDataType::kUint64:
      return webnn::mojom::DataType::kUint64;
    case webnn::OperandDataType::kInt8:
      return webnn::mojom::DataType::kInt8;
    case webnn::OperandDataType::kUint8:
      return webnn::mojom::DataType::kUint8;
    case webnn::OperandDataType::kInt4:
      return webnn::mojom::DataType::kInt4;
    case webnn::OperandDataType::kUint4:
      return webnn::mojom::DataType::kUint4;
  }
  NOTREACHED();
}

// static
webnn::OperandDataType
EnumTraits<webnn::mojom::DataType, webnn::OperandDataType>::FromMojom(
    webnn::mojom::DataType input) {
  switch (input) {
    case webnn::mojom::DataType::kFloat32:
      return webnn::OperandDataType::kFloat32;
    case webnn::mojom::DataType::kFloat16:
      return webnn::OperandDataType::kFloat16;
    case webnn::mojom::DataType::kInt32:
      return webnn::OperandDataType::kInt32;
    case webnn::mojom::DataType::kUint32:
      return webnn::OperandDataType::kUint32;
    case webnn::mojom::DataType::kInt64:
      return webnn::OperandDataType::kInt64;
    case webnn::mojom::DataType::kUint64:
      return webnn::OperandDataType::kUint64;
    case webnn::mojom::DataType::kInt8:
      return webnn::OperandDataType::kInt8;
    case webnn::mojom::DataType::kUint8:
      return webnn::OperandDataType::kUint8;
    case webnn::mojom::DataType::kInt4:
      return webnn::OperandDataType::kInt4;
    case webnn::mojom::DataType::kUint4:
      return webnn::OperandDataType::kUint4;
  }
  NOTREACHED();
}

}  // namespace mojo
