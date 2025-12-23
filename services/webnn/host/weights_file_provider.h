// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_HOST_WEIGHTS_FILE_PROVIDER_H_
#define SERVICES_WEBNN_HOST_WEIGHTS_FILE_PROVIDER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"

namespace webnn {

using CreateWeightsFileCallback = base::OnceCallback<void(base::File)>;

// Create a file in browser process to save all weights in the WebNN service.
void CreateWeightsFile(CreateWeightsFileCallback callback);

}  // namespace webnn

#endif  // SERVICES_WEBNN_HOST_WEIGHTS_FILE_PROVIDER_H_
