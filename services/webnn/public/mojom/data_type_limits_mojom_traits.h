// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_DATA_TYPE_LIMITS_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_DATA_TYPE_LIMITS_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/data_type_limits.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::DataTypeLimitsDataView,
                    webnn::DataTypeLimits> {
  static webnn::SupportedDataTypes input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.input;
  }
  static webnn::SupportedDataTypes constant(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.constant;
  }
  static webnn::SupportedDataTypes arg_min_max_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.arg_min_max_input;
  }
  static webnn::SupportedDataTypes arg_min_max_output(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.arg_min_max_output;
  }
  static webnn::SupportedDataTypes concat_inputs(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.concat_inputs;
  }
  static webnn::SupportedDataTypes elu_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.elu_input;
  }
  static webnn::SupportedDataTypes gather_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gather_input;
  }
  static webnn::SupportedDataTypes gather_indices(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gather_indices;
  }
  static webnn::SupportedDataTypes gelu_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.gelu_input;
  }
  static webnn::SupportedDataTypes leaky_relu_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.leaky_relu_input;
  }
  static webnn::SupportedDataTypes relu_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.relu_input;
  }
  static webnn::SupportedDataTypes where_condition(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.where_condition;
  }
  static webnn::SupportedDataTypes where_value(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.where_value;
  }

  static bool Read(webnn::mojom::DataTypeLimitsDataView data,
                   webnn::DataTypeLimits* out) {
    return data.ReadInput(&out->input) && data.ReadConstant(&out->constant) &&
           data.ReadArgMinMaxInput(&out->arg_min_max_input) &&
           data.ReadArgMinMaxOutput(&out->arg_min_max_output) &&
           data.ReadConcatInputs(&out->concat_inputs) &&
           data.ReadEluInput(&out->elu_input) &&
           data.ReadGatherInput(&out->gather_input) &&
           data.ReadGatherIndices(&out->gather_indices) &&
           data.ReadGeluInput(&out->gelu_input) &&
           data.ReadLeakyReluInput(&out->leaky_relu_input) &&
           data.ReadReluInput(&out->relu_input) &&
           data.ReadWhereCondition(&out->where_condition) &&
           data.ReadWhereValue(&out->where_value);
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_DATA_TYPE_LIMITS_MOJOM_TRAITS_H_
