// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_WEBNN_SANDBOX_INIT_H_
#define SERVICES_WEBNN_PUBLIC_CPP_WEBNN_SANDBOX_INIT_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace webnn {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// Preloads WebNN LiteRT libraries (like the GPU accelerator DLL) before the
// GPU process sandbox is locked down.
COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
void PreSandboxWebNNInitialization();
#endif

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_WEBNN_SANDBOX_INIT_H_
