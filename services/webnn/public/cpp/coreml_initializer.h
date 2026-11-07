// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_COREML_INITIALIZER_H_
#define SERVICES_WEBNN_PUBLIC_CPP_COREML_INITIALIZER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}  // namespace base

namespace webnn {

// This sets up cache directory that's needed for WebNN. WebNN underlying uses
// Apple's Core ML framework that needs access to this directory. By creating
// the directory in the browser process, we don't need to give the GPU process
// the ability to create directories in the sandbox policy.
// The directory is named after the bundle ID of `gpu_helper_app_path`. The GPU
// sandbox only allows the bundle ID of the helper the GPU process runs in.
// Setup runs once per process; later paths are ignored.
COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
void InitializeCacheDirAndRun(const base::FilePath& gpu_helper_app_path,
                              base::OnceClosure callback);

// Sets up the directory synchronously, bypassing the once-per-process state.
COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
void SetupCacheDirForTesting(const base::FilePath& gpu_helper_app_path);

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_COREML_INITIALIZER_H_
