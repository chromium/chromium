// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_CONTEXT_PROPERTIES_H_
#define SERVICES_WEBNN_PUBLIC_CPP_CONTEXT_PROPERTIES_H_

#include "services/webnn/public/cpp/supported_data_types.h"

namespace webnn {

// Represents the `MLInputOperandLayout` that specifies the layout format of
// the input tensor. N is the batch, C is input channels, H is height and W is
// the width of the tensor.
enum class InputOperandLayout { kNchw, kNhwc };

struct ContextProperties {
  // Output supported data types are the same as inputs.
  SupportedDataTypes OutputSupportedDataTypes() const {
    return input_supported_data_types;
  }
  InputOperandLayout conv2d_input_layout;
  SupportedDataTypes input_supported_data_types;
  SupportedDataTypes constant_supported_data_types;
  SupportedDataTypes gather_input_supported_data_types;
  SupportedDataTypes gather_indices_supported_data_types;
};

inline bool operator==(const ContextProperties& lhs,
                       const ContextProperties& rhs) {
  return lhs.conv2d_input_layout == rhs.conv2d_input_layout &&
         lhs.input_supported_data_types == rhs.input_supported_data_types &&
         lhs.constant_supported_data_types ==
             rhs.constant_supported_data_types &&
         lhs.gather_input_supported_data_types ==
             rhs.gather_input_supported_data_types &&
         lhs.gather_indices_supported_data_types ==
             rhs.gather_indices_supported_data_types;
}

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_CONTEXT_PROPERTIES_H_
