// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_switches.h"

#include <array>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "services/webnn/buildflags.h"

namespace switches {

// If the GetWebNNSwitchesCopiedFromGpuProcessHost array is empty, the compiler
// has trouble deducing the type and size of the array, even if you specify the
// type. If you add new build flags to items of the array, be sure and also add
// them to the #if right below guarding the definition.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(IS_WIN)

// Returns the list of WebNN switches passed from the GpuProcessHost to the
// GPU process. Add your switch to this list if you need to read it in the
// GPU process.
base::span<const char* const> GetWebNNSwitchesCopiedFromGpuProcessHost() {
  static constexpr auto flags = std::to_array({
#if BUILDFLAG(IS_MAC)
      kWebNNCoreMlDumpModel,
#endif
#if BUILDFLAG(WEBNN_USE_TFLITE)
      kWebNNTfliteDumpModel,
#endif
#if BUILDFLAG(IS_WIN)
      kWebNNOrtLoggingLevel,
      kWebNNOrtDumpModel,
      kWebNNOrtLibraryPathForTesting,
      kWebNNOrtEpLibraryPathForTesting,
      kWebNNOrtEpDevice,
      kWebNNOrtIgnoreEpBlocklist,
      kWebNNOrtGraphOptimizationLevel,
      kWebNNOrtEnableProfiling,
      kWebNNOrtDisableCpuFallback,
#endif
  });
  return flags;
}

#else
base::span<const char* const> GetWebNNSwitchesCopiedFromGpuProcessHost() {
  return {};
}
#endif

}  // namespace switches
