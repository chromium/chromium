// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_provider_ort.h"

#include "base/win/windows_version.h"
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"
#include "services/webnn/public/mojom/features.mojom.h"

namespace webnn::ort {

bool ShouldCreateOrtContext(const mojom::CreateContextOptions& options) {
  return base::win::GetVersion() >= kWinAppRuntimeSupportedMinVersion &&
         base::FeatureList::IsEnabled(mojom::features::kWebNNOnnxRuntime);
}

}  // namespace webnn::ort
