// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_CONSTANTS_H_
#define SANDBOX_CONSTANTS_H_

#include <limits>

#include "build/build_config.h"

namespace sandbox {

// kDataSizeLimit is used for RLIMIT_DATA on POSIX and for
// JOBOBJECT_EXTENDED_LIMIT_INFORMATION.JobMemoryLimit on Windows.
//
#if defined(ARCH_CPU_64_BITS)
// Note: On Linux and Windows, the sandbox may set a higher limit for
// renderer and GPU processes if the system has enough physical memory.
constexpr size_t kDataSizeLimit = size_t{1} << 32;  // 4 GB
#else
// Limit the data memory to a size that prevents allocations that can't be
// indexed by an int.
constexpr size_t kDataSizeLimit =
    static_cast<size_t>(std::numeric_limits<int>::max());
#endif

}  // namespace sandbox

#endif  // SANDBOX_CONSTANTS_H_
