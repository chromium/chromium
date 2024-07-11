// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_CONTEXT_PROPERTIES_H_
#define SERVICES_WEBNN_PUBLIC_CPP_CONTEXT_PROPERTIES_H_

#include "services/webnn/public/cpp/data_type_limits.h"

namespace webnn {

// Represents the `MLInputOperandLayout` that specifies the layout format of
// the input tensor. N is the batch, C is input channels, H is height and W is
// the width of the tensor.
enum class InputOperandLayout { kNchw, kNhwc };

struct ContextProperties {
  InputOperandLayout conv2d_input_layout;
  DataTypeLimits data_type_limits;
};

inline bool operator==(const ContextProperties& lhs,
                       const ContextProperties& rhs) {
  return lhs.conv2d_input_layout == rhs.conv2d_input_layout &&
         lhs.data_type_limits == rhs.data_type_limits;
}

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_CONTEXT_PROPERTIES_H_
