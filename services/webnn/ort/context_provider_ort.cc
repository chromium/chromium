// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_provider_ort.h"

#include "base/feature_list.h"
#include "services/webnn/public/mojom/features.mojom.h"

namespace webnn::ort {

bool ShouldTryCreateOrtContext() {
  if (!base::FeatureList::IsEnabled(mojom::features::kWebNNOnnxRuntime)) {
    return false;
  }

  // While it might be tempting, ShouldTryCreateOrtContext intentionally does
  // NOT call `PlatformFunctions::EnsureInitialized()`. `EnsureInitialized()`
  // loads onnxruntime.dll so the first call must always be done on a background
  // Chromium thread.
  return true;
}

}  // namespace webnn::ort
