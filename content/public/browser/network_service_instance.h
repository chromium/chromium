// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NETWORK_SERVICE_INSTANCE_H_
#define CONTENT_PUBLIC_BROWSER_NETWORK_SERVICE_INSTANCE_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/id_type.h"

namespace network {
class NetworkConnectionTracker;
class NetworkService;
namespace mojom {
class NetworkService;
}
}  // namespace network

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace content {

// Returns a pointer to the NetworkService, creating / re-creating it as needed.
// NetworkService will be running in-process if
//   1) kNetworkService feature is disabled, or
//   2) kNetworkService and kNetworkServiceInProcess are enabled
// Otherwise it runs out of process.
// This method can only be called on the UI thread.
CONTENT_EXPORT network::mojom::NetworkService* GetNetworkService();

// Similar to GetNetworkService(), but will create the NetworkService from a
// service manager connector if needed. If network service is disabled,
// |connector| will be ignored and this method is identical to
// GetNetworkService().
// This method can only be called on the UI thread.
CONTENT_EXPORT network::mojom::NetworkService* GetNetworkServiceFromConnector(
    service_manager::Connector* connector);

// Opaque type identifying a registered NetworkService crash handler.
class CrashHandlerDummyType;
using NetworkServiceCrashHandlerId = gpu::IdType32<CrashHandlerDummyType>;

// Registers |handler| to run (on UI thread) after NetworkServicePtr encounters
// an error.  Note that there are no ordering wrt error handlers for other
// interfaces (e.g. NetworkContextPtr and/or URLLoaderFactoryPtr).
//
// The return value can be passed to UnregisterNetworkServiceCrashHandler to
// unregister the |handler|.
//
// Can only be called on the UI thread.  No-op if NetworkService is disabled.
CONTENT_EXPORT NetworkServiceCrashHandlerId
RegisterNetworkServiceCrashHandler(base::RepeatingClosure handler);

// Unregisters the crash handler with the given |handler_id|.  See also
// RegisterNetworkServiceCrashHandler.
//
// Can only be called on the UI thread.  No-op if NetworkService is disabled.
CONTENT_EXPORT void UnregisterNetworkServiceCrashHandler(
    NetworkServiceCrashHandlerId handler_id);

// When network service is disabled, returns the in-process NetworkService
// pointer which is used to ease transition to network service.
// Must only be called on the IO thread.  Must not be called if the network
// service is enabled.
CONTENT_EXPORT network::NetworkService* GetNetworkServiceImpl();

// Call |FlushForTesting()| on cached |NetworkServicePtr|. For testing only.
// Must only be called on the UI thread.
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

// Sets the NetworkConnectionTracker instance to use. For testing only.
// Must be called on the UI thread. Must be called before the first call to
// GetNetworkConnectionTracker.
CONTENT_EXPORT void SetNetworkConnectionTrackerForTesting(
    network::NetworkConnectionTracker* network_connection_tracker);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NETWORK_SERVICE_INSTANCE_H_
