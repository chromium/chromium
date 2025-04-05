// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/context_properties.h"

namespace webnn {

ContextProperties::ContextProperties(mojo::DefaultConstruct::Tag tag)
    : data_type_limits(tag) {}

ContextProperties::ContextProperties(
    InputOperandLayout input_operand_layout,
    Resample2DAxes resample_2d_axes,
    BatchNormalizationAxis batch_normalization_axis,
    uint64_t tensor_byte_length_limit,
    DataTypeLimits data_type_limits)
    : input_operand_layout(input_operand_layout),
      resample_2d_axes(resample_2d_axes),
      batch_normalization_axis(batch_normalization_axis),
      tensor_byte_length_limit(tensor_byte_length_limit),
      data_type_limits(std::move(data_type_limits)) {}

ContextProperties::ContextProperties(const ContextProperties&) = default;
ContextProperties& ContextProperties::operator=(const ContextProperties&) =
    default;
ContextProperties::ContextProperties(ContextProperties&&) noexcept = default;
ContextProperties& ContextProperties::operator=(ContextProperties&&) noexcept =
    default;

ContextProperties::~ContextProperties() = default;

}  // namespace webnn
