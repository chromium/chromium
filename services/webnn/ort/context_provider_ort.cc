// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_provider_ort.h"

#include "base/feature_list.h"
#include "base/types/expected_macros.h"
#include "base/win/windows_version.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/public/mojom/features.mojom.h"

namespace webnn::ort {

// Windows ML works on all Windows 11 PCs running version 24H2 (build 26100)
// or greater.
bool ShouldCreateOrtContext(const mojom::CreateContextOptions& options) {
  return base::win::GetVersion() >= base::win::Version::WIN11_24H2 &&
         base::FeatureList::IsEnabled(mojom::features::kWebNNOnnxRuntime);
}

base::expected<std::unique_ptr<WebNNContextImpl>, mojom::ErrorPtr>
CreateContextFromOptions(mojom::CreateContextOptionsPtr options,
                         const gpu::GPUInfo& gpu_info,
                         mojo::PendingReceiver<mojom::WebNNContext> receiver,
                         WebNNContextProviderImpl* context_provider) {
  ASSIGN_OR_RETURN(scoped_refptr<Environment> env,
                   Environment::Create(gpu_info));

  ASSIGN_OR_RETURN(scoped_refptr<SessionOptions> session_options,
                   SessionOptions::Create(options->device));

  return std::make_unique<ContextImplOrt>(std::move(receiver), context_provider,
                                          std::move(options), std::move(env),
                                          std::move(session_options));
}

}  // namespace webnn::ort
