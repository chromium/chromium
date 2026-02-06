// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/local_network_access_url_loader_interceptor.h"

#include "net/base/transport_info.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/local_network_access_check_result.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace network {

LocalNetworkAccessUrlLoaderInterceptor::LocalNetworkAccessUrlLoaderInterceptor(
    const ResourceRequest& resource_request,
    const mojom::ClientSecurityState* client_security_state,
    int32_t url_load_options)
    : checker_(resource_request, client_security_state, url_load_options) {}

LocalNetworkAccessUrlLoaderInterceptor::
    ~LocalNetworkAccessUrlLoaderInterceptor() = default;

net::Error LocalNetworkAccessUrlLoaderInterceptor::OnConnected(
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
  // this request should be blocked per Local Network Access.
  LocalNetworkAccessCheckResult result =
      DoCheck(url, info, net_log, devtools_observer, devtools_request_id,
              url_loader_network_observer);
  std::optional<mojom::CorsError> cors_error =
      LocalNetworkAccessCheckResultToCorsError(result);
  // If there's no PNA-related CORS error, the connection is allowed (from PNA's
  // perspective).
  if (!cors_error.has_value()) {
    return net::OK;
  }
  if (result == LocalNetworkAccessCheckResult::kBlockedByPolicyBlock &&
      (info.type == net::TransportType::kCached ||
       info.type == net::TransportType::kCachedFromProxy)) {
    // If the cached entry was blocked by the private network access check
    // without a preflight, we'll start over and attempt to request from the
    // network, so resetting the checker.
    net_log.AddEvent(
        net::NetLogEventType::LOCAL_NETWORK_ACCESS_RETRY_DUE_TO_CACHE);
    checker_.ResetForRetry();
    return net::
        ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_LOCAL_NETWORK_ACCESS_POLICY;
  }

  if (result ==
      LocalNetworkAccessCheckResult::kBlockedByRequiredIpAddressSpaceMismatch) {
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
      LocalNetworkAccessCheckResult::kBlockedByInconsistentIpAddressSpace) {
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
  // URLLoaderCompletionStatus reflects the LNA failure reason.
  std::move(set_cors_error_status_callback)
      .Run(CorsErrorStatus(*cors_error, *checker_.ResponseAddressSpace()));

  // Local network access permission is required for this connection.
  if (url_loader_network_observer &&
      result == LocalNetworkAccessCheckResult::kLNAPermissionRequired) {
    // Check if the user has granted permission, otherwise trigger the
    // permission request and wait for the result.
    // `ProcessLocalNetworkAccessPermissionResultOnConnected` is then called
    // to either continue the load (if granted) or result in an error (if
    // denied).
    url_loader_network_observer->OnLocalNetworkAccessPermissionRequired(
        MapTransportTypeToMojomTransportType(info.type),
        *checker_.ResponseAddressSpace(),
        base::BindOnce(
            [](base::WeakPtr<LocalNetworkAccessUrlLoaderInterceptor> weak_self,
               const net::NetLogWithSource& net_log,
               const mojom::TransportType transport_type,
               const mojom::IPAddressSpace address_space,
               base::OnceCallback<void(net::Error)> callback,
               mojom::LocalNetworkAccessResult result) {
              if (!weak_self) {
                // Checking the weak ptr not to call the `callback` after
                // `this` is destructed. This is needed because the observer's
                // pipe may outlive `this` and the owner `URLLoader`.
                return;
              }

              net_log.AddEvent(
                  net::NetLogEventType::
                      LOCAL_NETWORK_ACCESS_PERMISSION_REQUESTED,
                  [&] {
                    return base::DictValue()
                        .Set("address_space",
                             IPAddressSpaceToStringPiece(address_space))
                        .Set("transport_type",
                             TransportTypeToStringPiece(transport_type))
                        .Set("result",
                             LocalNetworkAccessResultToStringPiece(result));
                  });

              if (result == mojom::LocalNetworkAccessResult::kRetryDueToCache) {
                weak_self->checker_.ResetForRetry();
                std::move(callback).Run(
                    net::
                        ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_LOCAL_NETWORK_ACCESS_POLICY);
                return;
              }
              std::move(callback).Run(
                  result == mojom::LocalNetworkAccessResult::kGranted
                      ? net::OK
                      : net::ERR_BLOCKED_BY_LOCAL_NETWORK_ACCESS_CHECKS);
            },
            weak_ptr_factory_.GetWeakPtr(), net_log,
            MapTransportTypeToMojomTransportType(info.type),
            *checker_.ResponseAddressSpace(),
            std::move(callback_getter).Run()));
    return net::ERR_IO_PENDING;
  }

  // Otherwise, if there was a Local Network Access CORS error, block by
  // default.
  return net::ERR_BLOCKED_BY_LOCAL_NETWORK_ACCESS_CHECKS;
}

void LocalNetworkAccessUrlLoaderInterceptor::ResetForRedirect(
    const GURL& new_url) {
  checker_.ResetForRedirect(new_url);
}

std::optional<mojom::IPAddressSpace>
LocalNetworkAccessUrlLoaderInterceptor::ResponseAddressSpace() const {
  return checker_.ResponseAddressSpace();
}

mojom::IPAddressSpace
LocalNetworkAccessUrlLoaderInterceptor::ClientAddressSpace() const {
  return checker_.ClientAddressSpace();
}

mojom::ClientSecurityStatePtr
LocalNetworkAccessUrlLoaderInterceptor::CloneClientSecurityState() const {
  return checker_.CloneClientSecurityState();
}

LocalNetworkAccessCheckResult LocalNetworkAccessUrlLoaderInterceptor::DoCheck(
    const GURL& url,
    const net::TransportInfo& transport_info,
    const net::NetLogWithSource& net_log,
    mojom::DevToolsObserver* devtools_observer,
    const std::optional<std::string>& devtools_request_id,
    mojom::URLLoaderNetworkServiceObserver* url_loader_network_observer) {
  LocalNetworkAccessCheckResult result = checker_.Check(transport_info);

  mojom::IPAddressSpace response_address_space =
      *checker_.ResponseAddressSpace();
  mojom::IPAddressSpace client_address_space = checker_.ClientAddressSpace();

  net_log.AddEvent(net::NetLogEventType::LOCAL_NETWORK_ACCESS_CHECK, [&] {
    return base::DictValue()
        .Set("client_address_space",
             IPAddressSpaceToStringPiece(client_address_space))
        .Set("resource_address_space",
             IPAddressSpaceToStringPiece(response_address_space))
        .Set("result", LocalNetworkAccessCheckResultToStringPiece(result));
  });

  if (url_loader_network_observer) {
    if (response_address_space == mojom::IPAddressSpace::kLoopback ||
        response_address_space == mojom::IPAddressSpace::kLocal) {
      url_loader_network_observer->OnUrlLoaderConnectedToLocalNetwork(
          url, response_address_space, client_address_space,
          checker_.RequiredAddressSpace());
    }
  }

  bool is_warning = false;
  switch (result) {
    case LocalNetworkAccessCheckResult::kLNAAllowedByPolicyWarn:
    case LocalNetworkAccessCheckResult::kAllowedByPolicyWarn:
      is_warning = true;
      break;
    case LocalNetworkAccessCheckResult::kBlockedByPolicyBlock:
      is_warning = false;
      break;
    default:
      // Do not report anything to DevTools in these cases.
      return result;
  }

  // If `security_state` was nullptr, then `result` should not have mentioned
  // the policy set in `security_state->local_network_request_policy`.
  const mojom::ClientSecurityState* security_state =
      checker_.client_security_state();
  DCHECK(security_state);

  if (devtools_observer) {
    devtools_observer->OnLocalNetworkRequest(devtools_request_id, url,
                                             is_warning, response_address_space,
                                             security_state->Clone());
  }

  return result;
}

}  // namespace network
