// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_sandbox_init.h"

#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"

#if BUILDFLAG(IS_LINUX)
#include <dlfcn.h>
#endif

namespace webnn {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
void PreSandboxWebNNInitialization() {
#if BUILDFLAG(WEBNN_USE_WEBGPU_ACCELERATOR)
#if BUILDFLAG(IS_WIN)
  base::FilePath library_path(
      FILE_PATH_LITERAL("libLiteRtWebGpuAccelerator.dll"));
  base::LoadNativeLibrary(library_path, nullptr);
#elif BUILDFLAG(IS_LINUX)
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
#endif
}
#endif

}  // namespace webnn
