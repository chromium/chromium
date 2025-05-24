// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_COREML_INITIALIZER_H_
#define SERVICES_WEBNN_PUBLIC_CPP_COREML_INITIALIZER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace webnn {

// This sets up cache directory that's needed for WebNN. WebNN underlying uses
// Apple's Core ML framework that needs access to this directory. By creating
// the directory in the browser process, we don't need to give the GPU process
// the ability to create directories in the sandbox policy.
COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
void InitializeCacheDirAndRun(base::OnceClosure callback);

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_COREML_INITIALIZER_H_
