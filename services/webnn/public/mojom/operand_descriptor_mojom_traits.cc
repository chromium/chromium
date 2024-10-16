// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/mojom/operand_descriptor_mojom_traits.h"

#include "base/types/expected.h"

namespace mojo {

namespace {

webnn::OperandDataType FromMojoDataType(webnn::mojom::DataType data_type) {
  switch (data_type) {
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
}

webnn::mojom::DataType ToMojoDataType(webnn::OperandDataType data_type) {
  switch (data_type) {
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
}

}  // namespace

// static
webnn::mojom::DataType StructTraits<webnn::mojom::OperandDescriptorDataView,
                                    webnn::OperandDescriptor>::
    data_type(const webnn::OperandDescriptor& descriptor) {
  return ToMojoDataType(descriptor.data_type());
}

// static
bool StructTraits<webnn::mojom::OperandDescriptorDataView,
                  webnn::OperandDescriptor>::
    Read(webnn::mojom::OperandDescriptorDataView data,
         webnn::OperandDescriptor* out) {
  mojo::ArrayDataView<uint32_t> shape;
  data.GetShapeDataView(&shape);

  base::expected<webnn::OperandDescriptor, std::string> descriptor =
      webnn::OperandDescriptor::Create(FromMojoDataType(data.data_type()),
                                       base::make_span(shape));

  if (!descriptor.has_value()) {
    return false;
  }

  *out = *std::move(descriptor);
  return true;
}

}  // namespace mojo
