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

enum class Resample2DAxes { kAny, kChannelsFirst, kChannelsLast };

struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) ContextProperties {
  explicit ContextProperties(mojo::DefaultConstruct::Tag);

  ContextProperties(InputOperandLayout input_operand_layout,
                    Resample2DAxes resample_2d_axes,
                    DataTypeLimits data_type_limits);

  // Copyable and movable.
  ContextProperties(const ContextProperties&);
  ContextProperties& operator=(const ContextProperties&);
  ContextProperties(ContextProperties&&) noexcept;
  ContextProperties& operator=(ContextProperties&&) noexcept;
  ~ContextProperties();

  InputOperandLayout input_operand_layout;
  Resample2DAxes resample_2d_axes;
  DataTypeLimits data_type_limits;
};

inline bool operator==(const ContextProperties& lhs,
                       const ContextProperties& rhs) {
  return lhs.input_operand_layout == rhs.input_operand_layout &&
         lhs.resample_2d_axes == rhs.resample_2d_axes &&
         lhs.data_type_limits == rhs.data_type_limits;
}

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_CONTEXT_PROPERTIES_H_
