// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/logging.h"

#if defined(GPU_CLIENT_DEBUG)
#include "base/command_line.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#endif  // defined(GPU_CLIENT_DEBUG)

namespace gpu {

LogSettings::LogSettings() : enabled_(false) {
  GPU_CLIENT_LOG_CODE_BLOCK({
    enabled_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableGPUClientLogging);
  });
}

LogSettings::~LogSettings() = default;

}  // namespace gpu
