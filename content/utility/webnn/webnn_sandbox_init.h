// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_WEBNN_WEBNN_SANDBOX_INIT_H_
#define CONTENT_UTILITY_WEBNN_WEBNN_SANDBOX_INIT_H_

#include "build/build_config.h"

namespace webnn {

#if BUILDFLAG(IS_WIN)
// Must be called in the WebNN model-compilation utility process before
// LowerToken(). Runs the third-party execution-provider preload helper
// that loads the backend DLLs (today ORT and EP-specific DLLs; LiteRT and other
// backends may follow). See WebNNModelCompilationInitializeConfig in
// content/browser/service_host/utility_sandbox_delegate_win.cc and the
// kWebNNModelCompilation case in content/utility/utility_main.cc for
// the full two-stage sandbox lifecycle.
[[nodiscard]] bool PreSandboxInit();
#endif  // BUILDFLAG(IS_WIN)

}  // namespace webnn

#endif  // CONTENT_UTILITY_WEBNN_WEBNN_SANDBOX_INIT_H_
