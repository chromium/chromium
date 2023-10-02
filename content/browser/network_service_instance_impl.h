// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SERVICE_INSTANCE_IMPL_H_
#define CONTENT_BROWSER_NETWORK_SERVICE_INSTANCE_IMPL_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "content/common/content_export.h"

namespace content {

// Creates the network::NetworkService object on the IO thread directly instead
// of trying to go through the ServiceManager.
// This also calls ForceInProcessNetworkService().
CONTENT_EXPORT void ForceCreateNetworkServiceDirectlyForTesting();

// Resets the interface ptr to the network service.
CONTENT_EXPORT void ResetNetworkServiceForTesting();

// Registers |handler| to run (on UI thread) after mojo::Remote<NetworkService>
// encounters an error.  Note that there are no ordering guarantees wrt error
// handlers for other interfaces (e.g. mojo::Remote<NetworkContext> and/or
// mojo::Remote<URLLoaderFactory>).
//
// Can only be called on the UI thread.  No-op if NetworkService is disabled.
CONTENT_EXPORT base::CallbackListSubscription
RegisterNetworkServiceCrashHandler(base::RepeatingClosure handler);

constexpr char kSSLKeyLogFileHistogram[] = "Net.SSLKeyLogFileUse";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SSLKeyLogFileAction {
  kLogFileEnabled = 0,
  kSwitchFound = 1,
  kEnvVarFound = 2,
  kMaxValue = kEnvVarFound,
};

// Shuts down the in-process network service or disconnects from the out-of-
// process one, allowing it to shut down.
CONTENT_EXPORT void ShutDownNetworkService();

// `on_restart` will be called at the end of every RestartNetworkService().
CONTENT_EXPORT void OnRestartNetworkServiceForTesting(
    base::RepeatingClosure on_restart);

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SERVICE_INSTANCE_IMPL_H_
