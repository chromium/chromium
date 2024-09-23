// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_ENUMS_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_ENUMS_H_

#include <stdint.h>

namespace gpu {
namespace raster {

enum MsaaMode : uint32_t {
  kNoMSAA,
  kMSAA,   // legacy
  kDMSAA,  // new and improved
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_ENUMS_H_
