// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_provider_ort.h"

#include "base/win/windows_version.h"
#include "services/webnn/public/mojom/features.mojom.h"

namespace webnn::ort {

// Windows ML works on all Windows 11 PCs running version 24H2 (build 26100)
// or greater.
bool ShouldCreateOrtContext(const mojom::CreateContextOptions& options) {
  return base::win::GetVersion() >= base::win::Version::WIN11_24H2 &&
         base::FeatureList::IsEnabled(mojom::features::kWebNNOnnxRuntime);
}

}  // namespace webnn::ort
