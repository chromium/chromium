// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_WEBNN_TYPES_H_
#define SERVICES_WEBNN_PUBLIC_CPP_WEBNN_TYPES_H_

#include <cstddef>
#include <cstdint>

#include "base/types/strong_alias.h"

namespace webnn {

// Index into `GraphInfo.operands`.
// Use uint32_t here because Mojo requires an explicitly sized type and
// this is big enough.
using OperandId = base::StrongAlias<class OperandIdTag, uint32_t>;

// Index into `GraphInfo.operations`.
using OperationId = size_t;

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_WEBNN_TYPES_H_
