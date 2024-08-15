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
  static webnn::SupportedDataTypes add_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.add_input;
  }
  static webnn::SupportedDataTypes sub_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.sub_input;
  }
  static webnn::SupportedDataTypes mul_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.mul_input;
  }
  static webnn::SupportedDataTypes div_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.div_input;
  }
  static webnn::SupportedDataTypes max_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.max_input;
  }
  static webnn::SupportedDataTypes min_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.min_input;
  }
  static webnn::SupportedDataTypes pow_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.pow_input;
  }
  static webnn::SupportedDataTypes equal_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.equal_input;
  }
  static webnn::SupportedDataTypes greater_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.greater_input;
  }
  static webnn::SupportedDataTypes greater_or_equal_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.greater_or_equal_input;
  }
  static webnn::SupportedDataTypes lesser_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.lesser_input;
  }
  static webnn::SupportedDataTypes lesser_or_equal_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.lesser_or_equal_input;
  }
  static webnn::SupportedDataTypes logical_not_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.logical_not_input;
  }
  static webnn::SupportedDataTypes logical_output(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.logical_output;
  }
  static webnn::SupportedDataTypes abs_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.abs_input;
  }
  static webnn::SupportedDataTypes ceil_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.ceil_input;
  }
  static webnn::SupportedDataTypes cos_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.cos_input;
  }
  static webnn::SupportedDataTypes erf_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.erf_input;
  }
  static webnn::SupportedDataTypes exp_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.exp_input;
  }
  static webnn::SupportedDataTypes floor_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.floor_input;
  }
  static webnn::SupportedDataTypes identity_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.identity_input;
  }
  static webnn::SupportedDataTypes log_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.log_input;
  }
  static webnn::SupportedDataTypes neg_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.neg_input;
  }
  static webnn::SupportedDataTypes reciprocal_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.reciprocal_input;
  }
  static webnn::SupportedDataTypes sin_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.sin_input;
  }
  static webnn::SupportedDataTypes sqrt_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.sqrt_input;
  }
  static webnn::SupportedDataTypes tan_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.tan_input;
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
  static webnn::SupportedDataTypes sigmoid_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.sigmoid_input;
  }
  static webnn::SupportedDataTypes slice_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.slice_input;
  }
  static webnn::SupportedDataTypes softmax_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.softmax_input;
  }
  static webnn::SupportedDataTypes softplus_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.softplus_input;
  }
  static webnn::SupportedDataTypes softsign_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.softsign_input;
  }
  static webnn::SupportedDataTypes split_input(
      const webnn::DataTypeLimits& data_type_limits) {
    return data_type_limits.split_input;
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
           data.ReadAddInput(&out->add_input) &&
           data.ReadSubInput(&out->sub_input) &&
           data.ReadMulInput(&out->mul_input) &&
           data.ReadDivInput(&out->div_input) &&
           data.ReadMaxInput(&out->max_input) &&
           data.ReadMinInput(&out->min_input) &&
           data.ReadPowInput(&out->pow_input) &&
           data.ReadEqualInput(&out->equal_input) &&
           data.ReadGreaterInput(&out->greater_input) &&
           data.ReadGreaterOrEqualInput(&out->greater_or_equal_input) &&
           data.ReadLesserInput(&out->lesser_input) &&
           data.ReadLesserOrEqualInput(&out->lesser_or_equal_input) &&
           data.ReadLogicalNotInput(&out->logical_not_input) &&
           data.ReadLogicalOutput(&out->logical_output) &&
           data.ReadAbsInput(&out->abs_input) &&
           data.ReadCeilInput(&out->ceil_input) &&
           data.ReadCosInput(&out->cos_input) &&
           data.ReadErfInput(&out->erf_input) &&
           data.ReadExpInput(&out->exp_input) &&
           data.ReadFloorInput(&out->floor_input) &&
           data.ReadIdentityInput(&out->identity_input) &&
           data.ReadLogInput(&out->log_input) &&
           data.ReadNegInput(&out->neg_input) &&
           data.ReadReciprocalInput(&out->reciprocal_input) &&
           data.ReadSinInput(&out->sin_input) &&
           data.ReadSqrtInput(&out->sqrt_input) &&
           data.ReadTanInput(&out->tan_input) &&
           data.ReadEluInput(&out->elu_input) &&
           data.ReadGatherInput(&out->gather_input) &&
           data.ReadGatherIndices(&out->gather_indices) &&
           data.ReadGeluInput(&out->gelu_input) &&
           data.ReadLeakyReluInput(&out->leaky_relu_input) &&
           data.ReadReluInput(&out->relu_input) &&
           data.ReadSigmoidInput(&out->sigmoid_input) &&
           data.ReadSliceInput(&out->slice_input) &&
           data.ReadSoftmaxInput(&out->softmax_input) &&
           data.ReadSoftplusInput(&out->softplus_input) &&
           data.ReadSoftsignInput(&out->softsign_input) &&
           data.ReadSplitInput(&out->split_input) &&
           data.ReadWhereCondition(&out->where_condition) &&
           data.ReadWhereValue(&out->where_value);
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_DATA_TYPE_LIMITS_MOJOM_TRAITS_H_
