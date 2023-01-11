// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SANDBOX_H_
#define CONTENT_BROWSER_NETWORK_SANDBOX_H_

#include "base/functional/callback.h"
#include "services/network/public/mojom/network_context.mojom.h"

// As of 2022-03 there is no plan to sandbox the network service in any special
// way on Android.
#if BUILDFLAG(IS_ANDROID)
#error "Sandboxing disk access to a subdirectory is not implemented on Android"
#endif

namespace content {

enum class SandboxGrantResult;

// Attempts to grant the sandbox access to the file data specified in the
// `params`. This function will also perform a migration of existing data from
// `unsandboxed_data_path` to `data_directory` as necessary.
//
// Various failures can occur during this process, and those are represented by
// the SandboxGrantResult. These values are described in more detail above. The
// `result_callback` is posted back to the caller's task runner. As arguments it
// receives the original `params` and the SandboxGrantResult.
void GrantSandboxAccessOnThreadPool(
    network::mojom::NetworkContextParamsPtr params,
    base::OnceCallback<void(network::mojom::NetworkContextParamsPtr,
                            SandboxGrantResult)> result_callback);

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SANDBOX_H_
