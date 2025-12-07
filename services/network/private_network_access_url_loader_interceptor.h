// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_URL_LOADER_INTERCEPTOR_H_
#define SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_URL_LOADER_INTERCEPTOR_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "services/network/private_network_access_checker.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"

namespace net {
class NetLogWithSource;
}  // namespace net

namespace network {

namespace mojom {
class DevToolsObserver;
enum class IPAddressSpace;
enum class PrivateNetworkAccessCheckResult;
class URLLoaderNetworkServiceObserver;
}  // namespace mojom

// This class encapsulates the Private Network Access (PNA) checks and related
// logic (like Local Network Access permission handling) that need to occur
// during a URLLoader's lifecycle, specifically when a connection is
// established.
//
// It acts as an interceptor, taking connection information and determining
// whether the request should proceed based on PNA rules, potentially handling
// asynchronous permission prompts. It uses PrivateNetworkAccessChecker
// internally for the core address space checks.
class PrivateNetworkAccessUrlLoaderInterceptor {
 public:
  // Constructs an interceptor for a given request.
  //
  // `client_security_state` should point to the client security to use for the
  // request, and must outlive the PrivateNetworkAccessChecker, if non-null.
  PrivateNetworkAccessUrlLoaderInterceptor(
      const ResourceRequest& resource_request,
      const mojom::ClientSecurityState* client_security_state,
      int32_t url_load_options);

  PrivateNetworkAccessUrlLoaderInterceptor(
      const PrivateNetworkAccessUrlLoaderInterceptor&) = delete;
  PrivateNetworkAccessUrlLoaderInterceptor& operator=(
      const PrivateNetworkAccessUrlLoaderInterceptor&) = delete;

  ~PrivateNetworkAccessUrlLoaderInterceptor();

  // Called when the URLLoader establishes a connection (or retrieves a cached
  // entry) for the given `url` with resolved `info`. This method performs
  // PNA checks.
  // `callback_getter` is a function that, when run, returns the final
  // completion callback to be invoked after any asynchronous PNA steps (like
  // LNA permission) complete. `callback_getter` itself is called synchronously
  // inside this method.
  // `set_cors_error_status_callback` allows this interceptor to synchronously
  // report a PNA-related CORS error status back to the owning URLLoader.
  //
  // Returns `net::OK` if the PNA checks pass synchronously and the request
  // should proceed.
  // Returns `net::ERR_IO_PENDING` if a Local Network Access permission prompt
  // is required; the final result will be delivered asynchronously via the
  // callback obtained from `callback_getter`.
  // Otherwise, returns a specific `net::Error` code (e.g.,
  // `net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS`) if the request is
  // blocked by PNA checks synchronously.
  net::Error OnConnected(
      const GURL& url,
      const net::TransportInfo& info,
      base::OnceCallback<base::OnceCallback<void(net::Error)>()>
          callback_getter,
      base::OnceCallback<void(CorsErrorStatus)> set_cors_error_status_callback,
      const net::NetLogWithSource& net_log,
      mojom::DevToolsObserver* devtools_observer,
      const std::optional<std::string>& devtools_request_id,
      mojom::URLLoaderNetworkServiceObserver* url_loader_network_observer);

  // Resets the internal state of the PNA checker when a redirect occurs.
  void ResetForRedirect(const GURL& new_url);

  // The following methods provide access to PNA-related state derived from the
  // checks. They delegate to the internal PrivateNetworkAccessChecker. See
  // `private_network_access_checker.h` for detailed semantics.
  std::optional<mojom::IPAddressSpace> ResponseAddressSpace() const;
  mojom::IPAddressSpace ClientAddressSpace() const;
  mojom::ClientSecurityStatePtr CloneClientSecurityState() const;

 private:
  // Internal helper for `OnConnected()`: performs the core PNA check using the
  // provided `transport_info`, logs events, and notifies observers. Returns
  // the raw check result.
  PrivateNetworkAccessCheckResult DoCheck(
      const GURL& url,
      const net::TransportInfo& transport_info,
      const net::NetLogWithSource& net_log,
      mojom::DevToolsObserver* devtools_observer,
      const std::optional<std::string>& devtools_request_id,
      mojom::URLLoaderNetworkServiceObserver* url_loader_network_observer);

  // The underlying checker used to perform the address space comparisons and
  // policy checks.
  PrivateNetworkAccessChecker checker_;

  base::WeakPtrFactory<PrivateNetworkAccessUrlLoaderInterceptor>
      weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_URL_LOADER_INTERCEPTOR_H_
