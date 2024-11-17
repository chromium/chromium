// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_SUPPORTED_DATA_TYPES_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_SUPPORTED_DATA_TYPES_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_context_properties.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::SupportedDataTypesDataView,
                    webnn::SupportedDataTypes> {
  static bool float32(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kFloat32);
  }
  static bool float16(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kFloat16);
  }
  static bool int32(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kInt32);
  }
  static bool uint32(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kUint32);
  }
  static bool int64(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kInt64);
  }
  static bool uint64(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kUint64);
  }
  static bool int8(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kInt8);
  }
  static bool uint8(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kUint8);
  }
  static bool int4(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kInt4);
  }
  static bool uint4(const webnn::SupportedDataTypes& supported_data_types) {
    return supported_data_types.Has(webnn::OperandDataType::kUint4);
  }
  static bool Read(webnn::mojom::SupportedDataTypesDataView data,
                   webnn::SupportedDataTypes* out) {
    if (data.float32()) {
      out->Put(webnn::OperandDataType::kFloat32);
    }
    if (data.float16()) {
      out->Put(webnn::OperandDataType::kFloat16);
    }
    if (data.int32()) {
      out->Put(webnn::OperandDataType::kInt32);
    }
    if (data.uint32()) {
      out->Put(webnn::OperandDataType::kUint32);
    }
    if (data.int64()) {
      out->Put(webnn::OperandDataType::kInt64);
    }
    if (data.uint64()) {
      out->Put(webnn::OperandDataType::kUint64);
    }
    if (data.int8()) {
      out->Put(webnn::OperandDataType::kInt8);
    }
    if (data.uint8()) {
      out->Put(webnn::OperandDataType::kUint8);
    }
    if (data.int4()) {
      out->Put(webnn::OperandDataType::kInt4);
    }
    if (data.uint4()) {
      out->Put(webnn::OperandDataType::kUint4);
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_SUPPORTED_DATA_TYPES_MOJOM_TRAITS_H_
