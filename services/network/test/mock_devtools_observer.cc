// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/mock_devtools_observer.h"

#include "base/run_loop.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

MockDevToolsObserver::MockDevToolsObserver() = default;
MockDevToolsObserver::~MockDevToolsObserver() = default;

mojo::PendingRemote<mojom::DevToolsObserver> MockDevToolsObserver::Bind() {
  mojo::PendingRemote<mojom::DevToolsObserver> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void MockDevToolsObserver::OnRawRequest(
    const std::string& devtools_request_id,
    const net::CookieAccessResultList& cookies_with_access_result,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
    const base::TimeTicks timestamp,
    network::mojom::ClientSecurityStatePtr client_security_state,
    network::mojom::OtherPartitionInfoPtr site_has_cookie_in_other_partition) {
  raw_request_cookies_.insert(raw_request_cookies_.end(),
                              cookies_with_access_result.begin(),
                              cookies_with_access_result.end());
  got_raw_request_ = true;
  devtools_request_id_ = devtools_request_id;
  client_security_state_ = std::move(client_security_state);

  if (wait_for_raw_request_ &&
      raw_request_cookies_.size() >= wait_for_raw_request_goal_) {
    std::move(wait_for_raw_request_).Run();
  }
}

void MockDevToolsObserver::OnEarlyHintsResponse(
    const std::string& devtools_request_id,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers) {
  early_hints_headers_ = std::move(headers);
  wait_for_early_hints_.Quit();
}

void MockDevToolsObserver::OnRawResponse(
    const std::string& devtools_request_id,
    const net::CookieAndLineAccessResultList& cookies_with_access_result,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
    const std::optional<std::string>& raw_response_headers,
    network::mojom::IPAddressSpace resource_address_space,
    int32_t http_status_code,
    const std::optional<net::CookiePartitionKey>& cookie_partition_key) {
  raw_response_cookies_.insert(raw_response_cookies_.end(),
                               cookies_with_access_result.begin(),
                               cookies_with_access_result.end());
  got_raw_response_ = true;
  devtools_request_id_ = devtools_request_id;
  resource_address_space_ = resource_address_space;

  response_headers_ = std::move(headers);
  raw_response_headers_ = raw_response_headers;
  raw_response_http_status_code_ = http_status_code;

  response_cookie_partition_key_ = cookie_partition_key;

  if (wait_for_raw_response_ &&
      raw_response_cookies_.size() >= wait_for_raw_response_goal_) {
    std::move(wait_for_raw_response_).Run();
  }
}

void MockDevToolsObserver::OnPrivateNetworkRequest(
    const std::optional<std::string>& devtools_request_id,
    const GURL& url,
    bool is_warning,
    network::mojom::IPAddressSpace resource_address_space,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  params_of_private_network_request_.emplace(devtools_request_id, url,
                                             is_warning, resource_address_space,
                                             std::move(client_security_state));
  wait_for_private_network_request_.Quit();
}

void MockDevToolsObserver::OnCorsPreflightRequest(
    const base::UnguessableToken& devtool_request_id,
    const net::HttpRequestHeaders& request_headers,
    network::mojom::URLRequestDevToolsInfoPtr request_info,
    const GURL& initiator_url,
    const std::string& initiator_devtool_request_id) {}

void MockDevToolsObserver::OnCorsPreflightResponse(
    const base::UnguessableToken& devtool_request_id,
    const GURL& url,
    network::mojom::URLResponseHeadDevToolsInfoPtr head) {}

void MockDevToolsObserver::OnCorsPreflightRequestCompleted(
    const base::UnguessableToken& devtool_request_id,
    const network::URLLoaderCompletionStatus& status) {
  preflight_status_ = status;
}

void MockDevToolsObserver::OnTrustTokenOperationDone(
    const std::string& devtool_request_id,
    network::mojom::TrustTokenOperationResultPtr result) {}

void MockDevToolsObserver::OnCorsError(
    const std::optional<std::string>& devtools_request_id,
    const std::optional<::url::Origin>& initiator_origin,
    mojom::ClientSecurityStatePtr client_security_state,
    const GURL& url,
    const network::CorsErrorStatus& status,
    bool is_warning) {
  // Ignoring kUnexpectedPrivateNetworkAccess because the request will be
  // restarted with a preflight and we care more about the CORS error that comes
  // thereafter.
  if (status.cors_error == mojom::CorsError::kUnexpectedPrivateNetworkAccess) {
    return;
  }

  OnCorsErrorParams params;
  params.devtools_request_id = devtools_request_id;
  params.initiator_origin = initiator_origin;
  params.client_security_state = std::move(client_security_state);
  params.url = url;
  params.status = status;
  params.is_warning = is_warning;

  params_of_cors_error_ = std::move(params);

  wait_for_cors_error_.Quit();
}

void MockDevToolsObserver::Clone(
    mojo::PendingReceiver<DevToolsObserver> observer) {
  receivers_.Add(this, std::move(observer));
}

void MockDevToolsObserver::WaitUntilRawResponse(size_t goal) {
  if (raw_response_cookies_.size() < goal || !got_raw_response_) {
    wait_for_raw_response_goal_ = goal;
    base::RunLoop run_loop;
    wait_for_raw_response_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  EXPECT_EQ(goal, raw_response_cookies_.size());
}

void MockDevToolsObserver::WaitUntilRawRequest(size_t goal) {
  if (raw_request_cookies_.size() < goal || !got_raw_request_) {
    wait_for_raw_request_goal_ = goal;
    base::RunLoop run_loop;
    wait_for_raw_request_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  EXPECT_EQ(goal, raw_request_cookies_.size());
}

void MockDevToolsObserver::WaitUntilPrivateNetworkRequest() {
  wait_for_private_network_request_.Run();
}

void MockDevToolsObserver::WaitUntilCorsError() {
  wait_for_cors_error_.Run();
}

void MockDevToolsObserver::WaitUntilEarlyHints() {
  wait_for_early_hints_.Run();
}

MockDevToolsObserver::OnPrivateNetworkRequestParams::
    OnPrivateNetworkRequestParams(
        const std::optional<std::string>& devtools_request_id,
        const GURL& url,
        bool is_warning,
        network::mojom::IPAddressSpace resource_address_space,
        network::mojom::ClientSecurityStatePtr client_security_state)
    : devtools_request_id(devtools_request_id),
      url(url),
      is_warning(is_warning),
      resource_address_space(resource_address_space),
      client_security_state(std::move(client_security_state)) {}
MockDevToolsObserver::OnPrivateNetworkRequestParams::
    OnPrivateNetworkRequestParams(OnPrivateNetworkRequestParams&&) = default;
MockDevToolsObserver::OnPrivateNetworkRequestParams::
    ~OnPrivateNetworkRequestParams() = default;

MockDevToolsObserver::OnCorsErrorParams::OnCorsErrorParams() = default;
MockDevToolsObserver::OnCorsErrorParams::OnCorsErrorParams(
    OnCorsErrorParams&&) = default;
MockDevToolsObserver::OnCorsErrorParams&
MockDevToolsObserver::OnCorsErrorParams::operator=(OnCorsErrorParams&&) =
    default;
MockDevToolsObserver::OnCorsErrorParams::~OnCorsErrorParams() = default;

}  // namespace network
