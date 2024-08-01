// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_UTILS_H_
#define SERVICES_WEBNN_WEBNN_UTILS_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"

namespace webnn {

std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpTagToString(mojom::Operation::Tag tag);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpKindToString(mojom::ArgMinMax::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpKindToString(mojom::ElementWiseBinary::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpKindToString(mojom::ElementWiseUnary::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpKindToString(mojom::Reduce::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_UTILS) GetOpName(const mojom::Operation& op);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedOperatorError(const mojom::Operation& op);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedOperatorError(const mojom::ElementWiseUnary& op);
// TODO: crbug.com/345271830 - remove these after all data type error reports
// are moved to blink.
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedArgumentTypeError(std::string_view op_name,
                                  std::string_view argument_name,
                                  OperandDataType type);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedInputArgumentTypeError(std::string_view op_name,
                                       OperandDataType type);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedOptionTypeError(std::string_view op_name,
                                std::string_view option_name,
                                OperandDataType type);

// The length of `permutation` must be the same as `array`. The values in
// `permutation` must be within the range [0, N-1] where N is the length of
// `array`. There must be no two or more same values in `permutation`.
//
// e.g., Given an array of [10, 11, 12, 13] and a permutation of [0, 2, 3, 1],
// the permuted array would be [10, 12, 13, 11].
std::vector<uint32_t> COMPONENT_EXPORT(WEBNN_UTILS)
    PermuteArray(base::span<const uint32_t> array,
                 base::span<const uint32_t> permutation);

bool COMPONENT_EXPORT(WEBNN_UTILS)
    IsLogicalElementWiseBinary(mojom::ElementWiseBinary::Kind kind);

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_UTILS_H_
