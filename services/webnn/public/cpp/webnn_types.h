// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_WEBNN_TYPES_H_
#define SERVICES_WEBNN_PUBLIC_CPP_WEBNN_TYPES_H_

#include <cstddef>
#include <cstdint>

namespace webnn {

// Key in `GraphInfo.id_to_operand_map`.
using OperandId = uint64_t;

// Index to `GraphInfo.operations`.
using OperationId = size_t;

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_WEBNN_TYPES_H_
