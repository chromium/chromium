// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NETWORK_SERVICE_INSTANCE_H_
#define CONTENT_PUBLIC_BROWSER_NETWORK_SERVICE_INSTANCE_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom-forward.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/cert_verifier_service.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace base {
class SequencedTaskRunner;
}

namespace cert_verifier {
class CertVerifierServiceFactoryImpl;
}

namespace net {
class NetworkChangeNotifier;
}  // namespace net

namespace network {
namespace mojom {
class NetworkService;
}
}  // namespace network

namespace content {

// Returns a pointer to the NetworkService, creating / re-creating it as needed.
// NetworkService will be running in-process if
//   1) kNetworkService feature is disabled, or
//   2) kNetworkService and kNetworkServiceInProcess are enabled
// Otherwise it runs out of process.
// This method can only be called on the UI thread.
CONTENT_EXPORT network::mojom::NetworkService* GetNetworkService();

// Only on ChromeOS since it's only used there.
#if BUILDFLAG(IS_CHROMEOS)
// Returns the global NetworkChangeNotifier instance.
CONTENT_EXPORT net::NetworkChangeNotifier* GetNetworkChangeNotifier();
#endif

// Call |FlushForTesting()| on cached |mojo::Remote<NetworkService>|. For
// testing only. Must only be called on the UI thread.
CONTENT_EXPORT void FlushNetworkServiceInstanceForTesting();

// Returns a NetworkConnectionTracker that can be used to subscribe for
// network change events.
// Must only be called on the UI thread.
CONTENT_EXPORT network::NetworkConnectionTracker* GetNetworkConnectionTracker();

// Asynchronously calls the given callback with a NetworkConnectionTracker that
// can be used to subscribe to network change events.
//
// This is a helper method for classes that can't easily call
// GetNetworkConnectionTracker from the UI thread.
CONTENT_EXPORT void GetNetworkConnectionTrackerFromUIThread(
    base::OnceCallback<void(network::NetworkConnectionTracker*)> callback);

// Helper method to create a NetworkConnectionTrackerAsyncGetter.
CONTENT_EXPORT network::NetworkConnectionTrackerAsyncGetter
CreateNetworkConnectionTrackerAsyncGetter();

// Sets the NetworkConnectionTracker instance to use. For testing only.
// Must be called on the UI thread. Must be called before the first call to
// GetNetworkConnectionTracker.
CONTENT_EXPORT void SetNetworkConnectionTrackerForTesting(
    network::NetworkConnectionTracker* network_connection_tracker);

// Gets the task runner for the thread the network service will be running on
// when running in-process. Can only be called when network service is in
// process.
CONTENT_EXPORT const scoped_refptr<base::SequencedTaskRunner>&
GetNetworkTaskRunner();

// Returns a CertVerifierParams that can be placed into a new
// network::mojom::NetworkContextParams.
//
// The |cert_verifier_creation_params| will be used to configure a new
// CertVerifierService, and a pipe to the new CertVerifierService will be placed
// in the CertVerifierParams.
CONTENT_EXPORT network::mojom::CertVerifierServiceRemoteParamsPtr
GetCertVerifierParams(cert_verifier::mojom::CertVerifierCreationParamsPtr
                          cert_verifier_creation_params);

// Sets the CertVerifierServiceFactory used to instantiate
// CertVerifierServices.
CONTENT_EXPORT void SetCertVerifierServiceFactoryForTesting(
    cert_verifier::mojom::CertVerifierServiceFactory* service_factory);

// Returns a pointer to the CertVerifierServiceFactory, creating / re-creating
// it as needed.
//
// This method can only be called on the UI thread.
CONTENT_EXPORT cert_verifier::mojom::CertVerifierServiceFactory*
GetCertVerifierServiceFactory();

// Returns the |mojo::Remote<CertVerifierServiceFactory>|. For testing only.
// Must only be called on the UI thread.
CONTENT_EXPORT
mojo::Remote<cert_verifier::mojom::CertVerifierServiceFactory>&
GetCertVerifierServiceFactoryRemoteForTesting();

// Returns the |CertVerifierServiceFactoryImpl|. For testing only.
// Must only be called on the same thread the CertVerifierServiceFactoryImpl
// storage was created on, which can be either the UI or IO thread depending on
// the platform. (Note that if the unittest uses a default
// BrowserTaskEnvironment, both UI and IO sequences share the same thread.)
CONTENT_EXPORT
cert_verifier::CertVerifierServiceFactoryImpl*
GetCertVerifierServiceFactoryForTesting();

// Convenience function to create a NetworkContext from the given set of
// |params|. Any creation of network contexts should be done through this
// function.
// This must be called on the UI thread.
CONTENT_EXPORT void CreateNetworkContextInNetworkService(
    mojo::PendingReceiver<network::mojom::NetworkContext> context,
    network::mojom::NetworkContextParamsPtr params);

// Shuts down the in-process network service or disconnects from the out-of-
// process one, allowing it to shut down. Then, restarts it.
CONTENT_EXPORT void RestartNetworkService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NETWORK_SERVICE_INSTANCE_H_
