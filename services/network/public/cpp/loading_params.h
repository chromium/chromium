// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_LOADING_PARAMS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_LOADING_PARAMS_H_

#include <stdint.h>

#include <cstdlib>

#include "base/component_export.h"

namespace network {

// The maximal number of bytes consumed in a loading task. When there are more
// bytes in the data pipe, they will be consumed in following tasks. Setting too
// small of a number will generate many tasks but setting a too large of a
// number will lead to thread janks. This value was optimized via Finch:
// see crbug.com/1041006.
inline constexpr size_t kMaxNumConsumedBytesInTask = 1024 * 1024;

enum class DataPipeAllocationSize {
  kDefaultSizeOnly,
  kLargerSizeIfPossible,
};

COMPONENT_EXPORT(NETWORK_CPP)
extern uint32_t GetDataPipeDefaultAllocationSize(
    DataPipeAllocationSize = DataPipeAllocationSize::kDefaultSizeOnly);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_LOADING_PARAMS_H_
