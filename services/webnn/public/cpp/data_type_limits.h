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
                 SupportedDataTypes gather_input,
                 SupportedDataTypes gather_indices,
                 SupportedDataTypes where_condition,
                 SupportedDataTypes where_true_value,
                 SupportedDataTypes where_false_value);

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
  SupportedDataTypes gather_input;
  SupportedDataTypes gather_indices;
  SupportedDataTypes where_condition;
  SupportedDataTypes where_true_value;
  SupportedDataTypes where_false_value;
};

inline bool operator==(const DataTypeLimits& lhs, const DataTypeLimits& rhs) {
  return lhs.input == rhs.input && lhs.constant == rhs.constant &&
         lhs.arg_min_max_input == rhs.arg_min_max_input &&
         lhs.arg_min_max_output == rhs.arg_min_max_output &&
         lhs.concat_inputs == rhs.concat_inputs &&
         lhs.gather_input == rhs.gather_input &&
         lhs.gather_indices == rhs.gather_indices &&
         lhs.where_condition == rhs.where_condition &&
         lhs.where_true_value == rhs.where_true_value &&
         lhs.where_false_value == rhs.where_false_value;
}

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_DATA_TYPE_LIMITS_H_
