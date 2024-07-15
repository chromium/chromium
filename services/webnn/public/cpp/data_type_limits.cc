// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/data_type_limits.h"

namespace webnn {

DataTypeLimits::DataTypeLimits(mojo::DefaultConstruct::Tag) {}

DataTypeLimits::DataTypeLimits(SupportedDataTypes input,
                               SupportedDataTypes constant,
                               SupportedDataTypes arg_min_max_input,
                               SupportedDataTypes arg_min_max_output,
                               SupportedDataTypes concat_inputs,
                               SupportedDataTypes gather_input,
                               SupportedDataTypes gather_indices,
                               SupportedDataTypes where_condition,
                               SupportedDataTypes where_true_value,
                               SupportedDataTypes where_false_value)
    : input(input),
      constant(constant),
      arg_min_max_input(arg_min_max_input),
      arg_min_max_output(arg_min_max_output),
      concat_inputs(concat_inputs),
      gather_input(gather_input),
      gather_indices(gather_indices),
      where_condition(where_condition),
      where_true_value(where_true_value),
      where_false_value(where_false_value) {}

DataTypeLimits::DataTypeLimits(const DataTypeLimits&) = default;
DataTypeLimits& DataTypeLimits::operator=(const DataTypeLimits&) = default;
DataTypeLimits::DataTypeLimits(DataTypeLimits&&) noexcept = default;
DataTypeLimits& DataTypeLimits::operator=(DataTypeLimits&&) noexcept = default;

DataTypeLimits::~DataTypeLimits() = default;

}  // namespace webnn
