// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_sandbox_init.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"
#include "services/webnn/public/mojom/features.mojom-features.h"

#if BUILDFLAG(IS_LINUX)
#include <dlfcn.h>
#endif

namespace webnn {

void PreSandboxWebNNInitialization(bool is_gpu_process) {
#if BUILDFLAG(WEBNN_USE_WEBGPU_ACCELERATOR)
#if BUILDFLAG(IS_WIN)
  // On Windows, loading the accelerator DLL in every renderer process before
  // sandbox lockdown causes a startup CPU and idle power regression
  // (crbug.com/553778325). Gate preloading behind the feature flag for renderer
  // processes. For the GPU process, load it directly without gating.
  if (!is_gpu_process &&
      !base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebNNLiteRTGpuInRenderer)) {
    return;
  }

  base::FilePath library_path(
      FILE_PATH_LITERAL("libLiteRtWebGpuAccelerator.dll"));
  base::LoadNativeLibrary(library_path, nullptr);
#elif BUILDFLAG(IS_LINUX)
  // On Linux, the library is preloaded once in the Zygote process during
  // PreSandboxInit(), allowing all forked renderers to share the library
  // pages via copy-on-write without repeating initialization. The Zygote
  // runs before FeatureList registration, so this is not feature-gated here.
  base::FilePath library_path;
  if (base::PathService::Get(base::DIR_MODULE, &library_path)) {
    library_path =
        library_path.Append(FILE_PATH_LITERAL("libLiteRtWebGpuAccelerator.so"));
  } else {
    library_path =
        base::FilePath(FILE_PATH_LITERAL("libLiteRtWebGpuAccelerator.so"));
  }

  dlopen(library_path.value().c_str(), RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE);
#endif
#endif  // BUILDFLAG(WEBNN_USE_WEBGPU_ACCELERATOR)
}

}  // namespace webnn
