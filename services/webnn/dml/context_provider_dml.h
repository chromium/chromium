// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_CONTEXT_PROVIDER_DML_H_
#define SERVICES_WEBNN_DML_CONTEXT_PROVIDER_DML_H_

#include "base/types/expected.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"

namespace gpu {
class SharedContextState;
struct GpuFeatureInfo;
struct GPUInfo;
}  // namespace gpu

namespace webnn {

class WebNNContextImpl;
class WebNNContextProviderImpl;

namespace dml {

bool ShouldCreateDmlContext(const mojom::CreateContextOptions& options);

// Create a WebNN context that satisfies the requested preferences in a
// CreateContextOptions. This corresponds to the
// ML.createContext(MLContextOptions) overload in the WebNN API.
base::expected<std::unique_ptr<WebNNContextImpl>, mojom::ErrorPtr>
CreateContextFromOptions(mojom::CreateContextOptionsPtr options,
                         const gpu::GpuFeatureInfo& gpu_feature_info,
                         const gpu::GPUInfo& gpu_info,
                         const gpu::SharedContextState* shared_context_state,
                         mojo::PendingReceiver<mojom::WebNNContext> receiver,
                         WebNNContextProviderImpl* context_provider);

}  // namespace dml

}  // namespace webnn

#endif  // SERVICES_WEBNN_DML_CONTEXT_PROVIDER_DML_H_
