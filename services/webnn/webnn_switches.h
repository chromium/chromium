// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_SWITCHES_H_
#define SERVICES_WEBNN_WEBNN_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

#if BUILDFLAG(IS_MAC)
// Copy the generated Core ML model to the folder specified
// by --webnn-coreml-dump-model, note: folder needs to be accessible from
// the GPU sandbox or use --no-sandbox.
// Usage: --no-sandbox --webnn-coreml-dump-model=/tmp/CoreMLModels
inline constexpr char kWebNNCoreMlDumpModel[] = "webnn-coreml-dump-model";
#endif  // BUILDFLAG(IS_MAC)

}  // namespace switches

#endif  // SERVICES_WEBNN_WEBNN_SWITCHES_H_
