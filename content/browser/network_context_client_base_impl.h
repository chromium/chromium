// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_CONTEXT_CLIENT_BASE_IMPL_H_
#define CONTENT_BROWSER_NETWORK_CONTEXT_CLIENT_BASE_IMPL_H_

#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

// Helper method that NetworkContext::OnFileUploadRequested need to use for
// their implementation.
void NetworkContextOnFileUploadRequested(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    network::mojom::NetworkContextClient::OnFileUploadRequestedCallback
        callback);

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_CONTEXT_CLIENT_BASE_IMPL_H_
