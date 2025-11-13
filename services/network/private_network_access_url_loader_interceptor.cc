// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/private_network_access_url_loader_interceptor.h"

#include "net/base/transport_info.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/private_network_access_check_result.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace network {

PrivateNetworkAccessUrlLoaderInterceptor::
    PrivateNetworkAccessUrlLoaderInterceptor(
        const ResourceRequest& resource_request,
        const mojom::ClientSecurityState* client_security_state,
        int32_t url_load_options)
    : checker_(resource_request, client_security_state, url_load_options) {}

PrivateNetworkAccessUrlLoaderInterceptor::
    ~PrivateNetworkAccessUrlLoaderInterceptor() = default;

net::Error PrivateNetworkAccessUrlLoaderInterceptor::OnConnected(
    const GURL& url,
    const net::TransportInfo& info,
    base::OnceCallback<base::OnceCallback<void(net::Error)>()> callback_getter,
    base::OnceCallback<void(CorsErrorStatus)> set_cors_error_status_callback,
    const net::NetLogWithSource& net_log,
    mojom::DevToolsObserver* devtools_observer,
    const std::optional<std::string>& devtools_request_id,
    mojom::URLLoaderNetworkServiceObserver* url_loader_network_observer) {
  // Save the last response address space, if any, in case it is needed for an
  // error message.
  std::optional<mojom::IPAddressSpace> last_response_address_space =
      checker_.ResponseAddressSpace();

  // Now that the request endpoint's address has been resolved, check if
  // this request should be blocked per Private Network Access.
  PrivateNetworkAccessCheckResult result =
      DoCheck(url, info, net_log, devtools_observer, devtools_request_id,
              url_loader_network_observer);
  std::optional<mojom::CorsError> cors_error =
      PrivateNetworkAccessCheckResultToCorsError(result);
  // If there's no PNA-related CORS error, the connection is allowed (from PNA's
  // perspective).
  if (!cors_error.has_value()) {
    return net::OK;
  }
  if (result == PrivateNetworkAccessCheckResult::kBlockedByPolicyBlock &&
      (info.type == net::TransportType::kCached ||
       info.type == net::TransportType::kCachedFromProxy)) {
    // If the cached entry was blocked by the private network access check
    // without a preflight, we'll start over and attempt to request from the
    // network, so resetting the checker.
    checker_.ResetForRetry();
    return net::
        ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_LOCAL_NETWORK_ACCESS_POLICY;
  }

  if (result == PrivateNetworkAccessCheckResult::
                    kBlockedByRequiredIpAddressSpaceMismatch) {
    // Error here is that we were expecting the resource to be in the
    // required_address_space, but the resource was served in the
    // response_address_space.
    std::move(set_cors_error_status_callback)
        .Run(CorsErrorStatus(
            *cors_error,
            /*resource_address_space=*/*checker_.ResponseAddressSpace(),
            /*inconsistent_address_space=*/checker_.RequiredAddressSpace()));
    return net::ERR_INCONSISTENT_IP_ADDRESS_SPACE;
  }

  if (result ==
      PrivateNetworkAccessCheckResult::kBlockedByInconsistentIpAddressSpace) {
    // Error here is that we initially saw the request served from an IP in the
    // last_response_address_space, but then saw it served again from an IP the
    // response_address_space.
    DCHECK(last_response_address_space);
    std::move(set_cors_error_status_callback)
        .Run(CorsErrorStatus(
            *cors_error,
            /*resource_address_space=*/*checker_.ResponseAddressSpace(),
            /*inconsistent_address_space=*/
            last_response_address_space.value_or(
                mojom::IPAddressSpace::kUnknown)));
    return net::ERR_INCONSISTENT_IP_ADDRESS_SPACE;
  }

  // Report the CORS error back to the URLLoader. This ensures the final
  // URLLoaderCompletionStatus reflects the PNA failure reason.
  std::move(set_cors_error_status_callback)
      .Run(CorsErrorStatus(*cors_error, *checker_.ResponseAddressSpace()));

  // Local network access permission is required for this connection.
  if (url_loader_network_observer &&
      result == PrivateNetworkAccessCheckResult::kLNAPermissionRequired) {
    // Check if the user has granted permission, otherwise trigger the
    // permission request and wait for the result.
    // `ProcessLocalNetworkAccessPermissionResultOnConnected` is then called
    // to either continue the load (if granted) or result in an error (if
    // denied).
    url_loader_network_observer->OnLocalNetworkAccessPermissionRequired(
        base::BindOnce(
            [](base::WeakPtr<PrivateNetworkAccessUrlLoaderInterceptor>
                   weak_self,
               base::OnceCallback<void(net::Error)> callback,
               bool permission_granted) {
              if (!weak_self) {
                // Checking the weak ptr not to call the `callback` after
                // `this` is destructed. This is needed because the observer's
                // pipe may outlive `this` and the owner `URLLoader`.
                return;
              }
              std::move(callback).Run(
                  permission_granted
                      ? net::OK
                      : net::ERR_BLOCKED_BY_LOCAL_NETWORK_ACCESS_CHECKS);
            },
            weak_ptr_factory_.GetWeakPtr(), std::move(callback_getter).Run()));
    return net::ERR_IO_PENDING;
  }

  // Otherwise, if there was a Private Network Access CORS error, block by
  // default.
  return net::ERR_BLOCKED_BY_LOCAL_NETWORK_ACCESS_CHECKS;
}

void PrivateNetworkAccessUrlLoaderInterceptor::ResetForRedirect(
    const GURL& new_url) {
  checker_.ResetForRedirect(new_url);
}

std::optional<mojom::IPAddressSpace>
PrivateNetworkAccessUrlLoaderInterceptor::ResponseAddressSpace() const {
  return checker_.ResponseAddressSpace();
}

mojom::IPAddressSpace
PrivateNetworkAccessUrlLoaderInterceptor::ClientAddressSpace() const {
  return checker_.ClientAddressSpace();
}

mojom::ClientSecurityStatePtr
PrivateNetworkAccessUrlLoaderInterceptor::CloneClientSecurityState() const {
  return checker_.CloneClientSecurityState();
}

PrivateNetworkAccessCheckResult
PrivateNetworkAccessUrlLoaderInterceptor::DoCheck(
    const GURL& url,
    const net::TransportInfo& transport_info,
    const net::NetLogWithSource& net_log,
    mojom::DevToolsObserver* devtools_observer,
    const std::optional<std::string>& devtools_request_id,
    mojom::URLLoaderNetworkServiceObserver* url_loader_network_observer) {
  PrivateNetworkAccessCheckResult result = checker_.Check(transport_info);

  mojom::IPAddressSpace response_address_space =
      *checker_.ResponseAddressSpace();
  mojom::IPAddressSpace client_address_space = checker_.ClientAddressSpace();

  net_log.AddEvent(net::NetLogEventType::PRIVATE_NETWORK_ACCESS_CHECK, [&] {
    return base::Value::Dict()
        .Set("client_address_space",
             IPAddressSpaceToStringPiece(client_address_space))
        .Set("resource_address_space",
             IPAddressSpaceToStringPiece(response_address_space))
        .Set("result", PrivateNetworkAccessCheckResultToStringPiece(result));
  });

  if (url_loader_network_observer) {
    if (response_address_space == mojom::IPAddressSpace::kLoopback ||
        response_address_space == mojom::IPAddressSpace::kLocal) {
      // We use the required_address_space as opposed to the
      // target_address_space here because target_address_space is an overloaded
      // term and has to do with PNA preflights.
      url_loader_network_observer->OnUrlLoaderConnectedToPrivateNetwork(
          url, response_address_space, client_address_space,
          checker_.RequiredAddressSpace());
    }
  }

  bool is_warning = false;
  switch (result) {
    case PrivateNetworkAccessCheckResult::kLNAAllowedByPolicyWarn:
    case PrivateNetworkAccessCheckResult::kAllowedByPolicyWarn:
      is_warning = true;
      break;
    case PrivateNetworkAccessCheckResult::kBlockedByPolicyBlock:
      is_warning = false;
      break;
    default:
      // Do not report anything to DevTools in these cases.
      return result;
  }

  // If `security_state` was nullptr, then `result` should not have mentioned
  // the policy set in `security_state->private_network_request_policy`.
  const mojom::ClientSecurityState* security_state =
      checker_.client_security_state();
  DCHECK(security_state);

  if (devtools_observer) {
    devtools_observer->OnPrivateNetworkRequest(
        devtools_request_id, url, is_warning, response_address_space,
        security_state->Clone());
  }

  return result;
}

}  // namespace network
