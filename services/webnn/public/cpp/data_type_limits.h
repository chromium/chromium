// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_
#define SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_

#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "services/webnn/public/cpp/supported_data_types.h"

namespace webnn {

struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) DataTypeLimits {
  explicit DataTypeLimits(mojo::DefaultConstruct::Tag);

  DataTypeLimits(SupportedDataTypes input,
                 SupportedDataTypes constant,
                 SupportedDataTypes arg_min_max_input,
                 SupportedDataTypes arg_min_max_output,
                 SupportedDataTypes concat_inputs,
                 SupportedDataTypes abs_input,
                 SupportedDataTypes ceil_input,
                 SupportedDataTypes cos_input,
                 SupportedDataTypes erf_input,
                 SupportedDataTypes exp_input,
                 SupportedDataTypes floor_input,
                 SupportedDataTypes identity_input,
                 SupportedDataTypes log_input,
                 SupportedDataTypes neg_input,
                 SupportedDataTypes reciprocal_input,
                 SupportedDataTypes sin_input,
                 SupportedDataTypes sqrt_input,
                 SupportedDataTypes tan_input,
                 SupportedDataTypes elu_input,
                 SupportedDataTypes gather_input,
                 SupportedDataTypes gather_indices,
                 SupportedDataTypes gelu_input,
                 SupportedDataTypes leaky_relu_input,
                 SupportedDataTypes relu_input,
                 SupportedDataTypes sigmoid_input,
                 SupportedDataTypes slice_input,
                 SupportedDataTypes softmax_input,
                 SupportedDataTypes softplus_input,
                 SupportedDataTypes softsign_input,
                 SupportedDataTypes split_input,
                 SupportedDataTypes where_condition,
                 SupportedDataTypes where_value);

  // Copyable and movable.
  DataTypeLimits(const DataTypeLimits&);
  DataTypeLimits& operator=(const DataTypeLimits&);
  DataTypeLimits(DataTypeLimits&&) noexcept;
  DataTypeLimits& operator=(DataTypeLimits&&) noexcept;
  ~DataTypeLimits();

  // Output supported data types are the same as inputs.
  SupportedDataTypes output() const { return input; }

  SupportedDataTypes input;
  SupportedDataTypes constant;
  SupportedDataTypes arg_min_max_input;
  SupportedDataTypes arg_min_max_output;
  SupportedDataTypes concat_inputs;
  SupportedDataTypes abs_input;
  SupportedDataTypes ceil_input;
  SupportedDataTypes cos_input;
  SupportedDataTypes erf_input;
  SupportedDataTypes exp_input;
  SupportedDataTypes floor_input;
  SupportedDataTypes identity_input;
  SupportedDataTypes log_input;
  SupportedDataTypes neg_input;
  SupportedDataTypes reciprocal_input;
  SupportedDataTypes sin_input;
  SupportedDataTypes sqrt_input;
  SupportedDataTypes tan_input;
  SupportedDataTypes elu_input;
  SupportedDataTypes gather_input;
  SupportedDataTypes gather_indices;
  SupportedDataTypes gelu_input;
  SupportedDataTypes leaky_relu_input;
  SupportedDataTypes relu_input;
  SupportedDataTypes sigmoid_input;
  SupportedDataTypes slice_input;
  SupportedDataTypes softmax_input;
  SupportedDataTypes softplus_input;
  SupportedDataTypes softsign_input;
  SupportedDataTypes split_input;
  SupportedDataTypes where_condition;
  SupportedDataTypes where_value;
};

// clang-format off
inline bool operator==(const DataTypeLimits& lhs, const DataTypeLimits& rhs) {
  return lhs.input == rhs.input &&
         lhs.constant == rhs.constant &&
         lhs.arg_min_max_input == rhs.arg_min_max_input &&
         lhs.arg_min_max_output == rhs.arg_min_max_output &&
         lhs.concat_inputs == rhs.concat_inputs &&
         lhs.abs_input == rhs.abs_input &&
         lhs.ceil_input == rhs.ceil_input &&
         lhs.cos_input == rhs.cos_input &&
         lhs.erf_input == rhs.erf_input &&
         lhs.exp_input == rhs.exp_input &&
         lhs.floor_input == rhs.floor_input &&
         lhs.identity_input == rhs.identity_input &&
         lhs.log_input == rhs.log_input &&
         lhs.neg_input == rhs.neg_input &&
         lhs.reciprocal_input == rhs.reciprocal_input &&
         lhs.sin_input == rhs.sin_input &&
         lhs.sqrt_input == rhs.sqrt_input &&
         lhs.tan_input == rhs.tan_input &&
         lhs.elu_input == rhs.elu_input &&
         lhs.gelu_input == rhs.gelu_input &&
         lhs.leaky_relu_input == rhs.leaky_relu_input &&
         lhs.relu_input == rhs.relu_input &&
         lhs.sigmoid_input == rhs.sigmoid_input &&
         lhs.slice_input == rhs.slice_input &&
         lhs.softmax_input == rhs.softmax_input &&
         lhs.softplus_input == rhs.softplus_input &&
         lhs.softsign_input == rhs.softsign_input &&
         lhs.split_input == rhs.split_input &&
         lhs.where_condition == rhs.where_condition &&
         lhs.where_value == rhs.where_value;
}
// clang-format on

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_
