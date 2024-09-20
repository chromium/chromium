// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_client.h"

#include "base/strings/stringprintf.h"
#include "google_apis/common/api_key_request_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_request_base.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/url_loader_network_service_observer.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace {

constexpr char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";

}  // namespace

namespace remoting {

ProtobufHttpClient::ProtobufHttpClient(
    const std::string& server_endpoint,
    OAuthTokenGetter* token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : server_endpoint_(server_endpoint),
      token_getter_(token_getter),
      url_loader_factory_(url_loader_factory) {}

ProtobufHttpClient::~ProtobufHttpClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProtobufHttpClient::ExecuteRequest(
    std::unique_ptr<ProtobufHttpRequestBase> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!request->config().authenticated) {
    DoExecuteRequest(std::move(request), OAuthTokenGetter::Status::SUCCESS, {},
                     {}, {});
    return;
  }

  DCHECK(token_getter_);
  token_getter_->CallWithToken(
      base::BindOnce(&ProtobufHttpClient::DoExecuteRequest,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void ProtobufHttpClient::CancelPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_factory_.InvalidateWeakPtrs();
  pending_requests_.clear();
}

bool ProtobufHttpClient::HasPendingRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return !pending_requests_.empty();
}

void ProtobufHttpClient::DoExecuteRequest(
    std::unique_ptr<ProtobufHttpRequestBase> request,
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token,
    const std::string& scopes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != OAuthTokenGetter::Status::SUCCESS) {
    std::string error_message =
        base::StringPrintf("Failed to fetch access token. Status: %d", status);
    LOG(ERROR) << error_message;
    ProtobufHttpStatus::Code code;
    switch (status) {
      case OAuthTokenGetter::Status::AUTH_ERROR:
        code = ProtobufHttpStatus::Code::UNAUTHENTICATED;
        break;
      // TODO: yuweih - this should be mapped to `NETWORK_ERROR`. Fix this and
      // downstream code that relies on this behavior.
      case OAuthTokenGetter::Status::NETWORK_ERROR:
        code = ProtobufHttpStatus::Code::UNAVAILABLE;
        break;
      default:
        NOTREACHED() << "Unknown OAuthTokenGetter Status: " << status;
    }
    request->OnAuthFailed(ProtobufHttpStatus(code, error_message));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GURL("https://" + server_endpoint_ + request->config().path);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = request->config().method;

  if (status == OAuthTokenGetter::Status::SUCCESS && !access_token.empty()) {
    resource_request->headers.AddHeaderFromString(
        base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));
  } else {
    VLOG(1) << "Attempting to execute request without access token";
  }

  if (!request->config().api_key.empty()) {
    google_apis::AddAPIKeyToRequest(*resource_request,
                                    request->config().api_key);
  }

  if (request->config().provide_certificate) {
    if (!resource_request->trusted_params.has_value()) {
      resource_request->trusted_params.emplace();
    }

    service_observer_.emplace();
    resource_request->trusted_params->url_loader_network_observer =
        service_observer_->Bind();
  }

  std::unique_ptr<network::SimpleURLLoader> send_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       request->config().traffic_annotation);
  base::TimeDelta timeout_duration = request->GetRequestTimeoutDuration();
  if (!timeout_duration.is_zero()) {
    send_url_loader->SetTimeoutDuration(request->GetRequestTimeoutDuration());
  }
  send_url_loader->AttachStringForUpload(
      request->config().request_message->SerializeAsString(),
      "application/x-protobuf");
  send_url_loader->SetAllowHttpErrorResults(true);
  auto* unowned_request = request.get();
  base::OnceClosure invalidator = base::BindOnce(
      &ProtobufHttpClient::CancelRequest, weak_factory_.GetWeakPtr(),
      pending_requests_.insert(pending_requests_.end(), std::move(request)));
  unowned_request->StartRequest(url_loader_factory_.get(),
                                std::move(send_url_loader),
                                std::move(invalidator));
}

void ProtobufHttpClient::CancelRequest(
    const PendingRequestListIterator& request_iterator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_requests_.erase(request_iterator);
}

}  // namespace remoting
