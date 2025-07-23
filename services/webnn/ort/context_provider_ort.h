// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_CONTEXT_PROVIDER_ORT_H_
#define SERVICES_WEBNN_ORT_CONTEXT_PROVIDER_ORT_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"

namespace webnn {

namespace ort {

bool ShouldCreateOrtContext(const mojom::CreateContextOptions& options);

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_CONTEXT_PROVIDER_ORT_H_
