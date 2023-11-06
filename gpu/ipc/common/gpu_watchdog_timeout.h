// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_WATCHDOG_TIMEOUT_H_
#define GPU_IPC_COMMON_GPU_WATCHDOG_TIMEOUT_H_

#include "base/time/time.h"
#include "build/build_config.h"

namespace gpu {

// TODO(magchen): crbug.com/949839. Move all constants back to
// gpu/ipc/service/gpu_watchdog_thread.h after the GPU watchdog V2 is fully
// launched.

#if defined(CYGPROFILE_INSTRUMENTATION)
constexpr base::TimeDelta kGpuWatchdogTimeout = base::Seconds(30);
#elif BUILDFLAG(IS_MAC)
#if defined(ADDRESS_SANITIZER)
// Use a longer timeout because of slower execution time leading to
// intermittent flakes. http://crbug.com/1270755
constexpr base::TimeDelta kGpuWatchdogTimeout = base::Seconds(50);
#else
constexpr base::TimeDelta kGpuWatchdogTimeout = base::Seconds(25);
#endif
#elif BUILDFLAG(IS_WIN)
constexpr base::TimeDelta kGpuWatchdogTimeout = base::Seconds(30);
#else
constexpr base::TimeDelta kGpuWatchdogTimeout = base::Seconds(15);
#endif

// It usually takes longer to finish a GPU task when the system just resumes
// from power suspension or when the Android app switches from the background to
// the foreground. This is the factor the original timeout will be multiplied.
inline constexpr int kRestartFactor = 2;

// Software rasterizer runs slower than hardware accelerated.
inline constexpr int kSoftwareRenderingFactor = 2;

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_WATCHDOG_TIMEOUT_H_
