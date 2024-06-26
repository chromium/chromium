// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_SUPPORTED_DATA_TYPES_H_
#define SERVICES_WEBNN_PUBLIC_CPP_SUPPORTED_DATA_TYPES_H_

#include "base/containers/enum_set.h"
#include "services/webnn/public/cpp/operand_descriptor.h"

namespace webnn {

using SupportedDataTypes = base::EnumSet<OperandDataType,
                                         OperandDataType::kMinValue,
                                         OperandDataType::kMaxValue>;

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_SUPPORTED_DATA_TYPES_H_
