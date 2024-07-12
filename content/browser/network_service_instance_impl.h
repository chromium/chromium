// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SERVICE_INSTANCE_IMPL_H_
#define CONTENT_BROWSER_NETWORK_SERVICE_INSTANCE_IMPL_H_

#include <stdint.h>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom-forward.h"
#include "services/network/public/mojom/cert_verifier_service_updater.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace content {

// Creates the network::NetworkService object on the IO thread directly instead
// of trying to go through the ServiceManager.
// This also calls ForceInProcessNetworkService().
CONTENT_EXPORT void ForceCreateNetworkServiceDirectlyForTesting();

// Resets the interface ptr to the network service.
CONTENT_EXPORT void ResetNetworkServiceForTesting();

using NetworkServiceProcessGoneHandler =
    base::RepeatingCallback<void(bool crashed)>;

// Registers |handler| to run (on UI thread) after mojo::Remote<NetworkService>
// encounters an error, in which case `crashed` will be true, or after the
// NetworkService is purposely restarted by the browser, in which case `crashed`
// will be false.  Note that there are no ordering guarantees wrt error
// handlers for other interfaces (e.g. mojo::Remote<NetworkContext> and/or
// mojo::Remote<URLLoaderFactory>).
//
// Can only be called on the UI thread.  No-op if NetworkService is disabled.
CONTENT_EXPORT base::CallbackListSubscription
RegisterNetworkServiceProcessGoneHandler(
    NetworkServiceProcessGoneHandler handler);

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

// Returns a CertVerifierParams that can be placed into a new
// network::mojom::NetworkContextParams.
//
// Like |GetCertVerifierParams| but the |cert_verifier_updater_remote| pipe
// passed in can be used to update the returned CertVerifierService with new
// verification parameters.
CONTENT_EXPORT network::mojom::CertVerifierServiceRemoteParamsPtr
GetCertVerifierParamsWithUpdater(
    cert_verifier::mojom::CertVerifierCreationParamsPtr
        cert_verifier_creation_params,
    mojo::PendingReceiver<cert_verifier::mojom::CertVerifierServiceUpdater>
        cert_verifier_updater_remote);

CONTENT_EXPORT uint64_t GetNetLogMaximumFileSizeFromCommandLineForTesting(
    const base::CommandLine& command_line);
}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SERVICE_INSTANCE_IMPL_H_
