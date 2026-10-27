// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_sandbox_init.h"

#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"
#include "services/webnn/public/mojom/features.mojom-features.h"

#if BUILDFLAG(IS_LINUX)
#include <dlfcn.h>
#endif

namespace webnn {

void PreSandboxWebNNInitialization(bool is_gpu_process) {
#if BUILDFLAG(WEBNN_USE_WEBGPU_ACCELERATOR) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX))
#if BUILDFLAG(IS_WIN)
  if (!base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebMachineLearningNeuralNetwork)) {
    return;
  }

  // On Windows, loading the accelerator DLL in every renderer or GPU process
  // before sandbox lockdown causes a startup latency, CPU, and idle power
  // regression (crbug.com/553778325). Only preload in the process that will
  // actually host LiteRT WebGPU execution.
  const bool run_in_renderer = base::FeatureList::IsEnabled(
      webnn::mojom::features::kWebNNLiteRTGpuInRenderer);
  if (is_gpu_process == run_in_renderer) {
    return;
  }

  constexpr base::FilePath::StringViewType kLibraryName =
      FILE_PATH_LITERAL("libLiteRtWebGpuAccelerator.dll");
#elif BUILDFLAG(IS_LINUX)
  // On Linux, the library is preloaded once in the Zygote process during
  // PreSandboxInit(), allowing all forked renderers to share the library
  // pages via copy-on-write without repeating initialization. The Zygote
  // runs before FeatureList registration, so this is not feature-gated here.
  constexpr base::FilePath::StringViewType kLibraryName =
      FILE_PATH_LITERAL("libLiteRtWebGpuAccelerator.so");
#endif

  TRACE_EVENT("gpu,startup", "PreSandboxWebNNInitialization");
  base::FilePath library_path;
  if (base::PathService::Get(base::DIR_ASSETS, &library_path)) {
    library_path = library_path.Append(kLibraryName);
  } else {
    library_path = base::FilePath(kLibraryName);
  }

#if BUILDFLAG(IS_WIN)
  base::LoadNativeLibrary(library_path, nullptr);
#elif BUILDFLAG(IS_LINUX)
  dlopen(library_path.value().c_str(), RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE);
#endif
#endif  // BUILDFLAG(WEBNN_USE_WEBGPU_ACCELERATOR) && (BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_LINUX))
}

}  // namespace webnn
