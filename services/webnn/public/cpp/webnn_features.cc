// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "services/webnn/public/mojom/features.mojom-features.h"

namespace webnn {

bool IsWebNNUnsafe() {
  if (!base::FeatureList::IsEnabled(
          mojom::features::kWebNNLiteRTGpuInRenderer)) {
    return true;
  }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  if (!base::FeatureList::IsEnabled(mojom::features::kWebNNCompilerProcess)) {
    return true;
  }
#endif
  return false;
}

}  // namespace webnn
