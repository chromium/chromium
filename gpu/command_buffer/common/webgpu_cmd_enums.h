// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_ENUMS_H_
#define GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_ENUMS_H_

namespace gpu {
namespace webgpu {

enum class PowerPreference : uint32_t {
  kLowPower,
  kHighPerformance,
  kNumPowerPreferences
};

enum class DawnReturnDataType : uint32_t {
  kDawnCommands,
  kNumDawnReturnDataType
};

}  // namespace webgpu
}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_ENUMS_H_
