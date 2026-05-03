// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_CONTEXT_PROVIDER_ORT_H_
#define SERVICES_WEBNN_ORT_CONTEXT_PROVIDER_ORT_H_

namespace webnn {

namespace ort {

// Returns true if CreateContext requests should be attempted by the ORT-based
// WebNN context implementation.
bool ShouldTryCreateOrtContext();

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_CONTEXT_PROVIDER_ORT_H_
