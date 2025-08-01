// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_client.h"

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "google_apis/common/api_key_request_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "remoting/base/http_status.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_request_base.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/url_loader_network_service_observer.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace remoting {

ProtobufHttpClient::ProtobufHttpClient(
    const std::string& server_endpoint,
    OAuthTokenGetter* token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<net::ClientCertStore> client_cert_store)
    : server_endpoint_(server_endpoint),
      token_getter_(token_getter),
      url_loader_factory_(url_loader_factory),
      client_cert_store_(std::move(client_cert_store)) {}

ProtobufHttpClient::~ProtobufHttpClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProtobufHttpClient::ExecuteRequest(
    std::unique_ptr<ProtobufHttpRequestBase> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!request->config().authenticated) {
    DoExecuteRequest(std::move(request), OAuthTokenGetter::Status::SUCCESS,
                     OAuthTokenInfo());
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
    const OAuthTokenInfo& token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != OAuthTokenGetter::Status::SUCCESS) {
    std::string error_message =
        base::StringPrintf("Failed to fetch access token. Status: %d", status);
    LOG(ERROR) << error_message;
    HttpStatus::Code code;
    switch (status) {
      case OAuthTokenGetter::Status::AUTH_ERROR:
        code = HttpStatus::Code::UNAUTHENTICATED;
        break;
      case OAuthTokenGetter::Status::NETWORK_ERROR:
        code = HttpStatus::Code::NETWORK_ERROR;
        break;
      default:
        NOTREACHED() << "Unknown OAuthTokenGetter Status: " << status;
    }
    request->OnAuthFailed(HttpStatus(code, error_message));
    return;
  }

  // SimpleURLLoader can only be used once, so we just bind and pass a creator
  // callback.
  // Can't bind a WeakPtr here since the callback returns a value. Using
  // Unretained is safe since `request` is owned by `this`.
  auto create_simple_url_loader = base::BindRepeating(
      &ProtobufHttpClient::CreateSimpleUrlLoader, base::Unretained(this),
      status == OAuthTokenGetter::Status::SUCCESS ? token_info.access_token()
                                                  : std::string(),
      request->GetRequestTimeoutDuration());
  auto* unowned_request = request.get();
  base::OnceClosure invalidator = base::BindOnce(
      &ProtobufHttpClient::CancelRequest, weak_factory_.GetWeakPtr(),
      pending_requests_.insert(pending_requests_.end(), std::move(request)));
  unowned_request->StartRequest(url_loader_factory_.get(),
                                std::move(create_simple_url_loader),
                                std::move(invalidator));
}

std::unique_ptr<network::SimpleURLLoader>
ProtobufHttpClient::CreateSimpleUrlLoader(
    const std::string& access_token,
    base::TimeDelta timeout_duration,
    const ProtobufHttpRequestConfig& config) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("https://" + server_endpoint_ + config.path);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = config.method;

  if (!access_token.empty()) {
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                        "Bearer " + access_token);
  } else {
    VLOG(1) << "Attempting to execute request without access token";
  }

  if (!config.api_key.empty()) {
    google_apis::AddAPIKeyToRequest(*resource_request, config.api_key);
  }

  if (config.provide_certificate) {
    if (!resource_request->trusted_params.has_value()) {
      resource_request->trusted_params.emplace();
    }

    if (!service_observer_.has_value()) {
      CHECK(client_cert_store_);
      service_observer_.emplace(std::move(client_cert_store_));
    }
    resource_request->trusted_params->url_loader_network_observer =
        service_observer_->Bind();
  }

  std::unique_ptr<network::SimpleURLLoader> send_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       config.traffic_annotation);
  if (!timeout_duration.is_zero()) {
    send_url_loader->SetTimeoutDuration(timeout_duration);
  }
  send_url_loader->AttachStringForUpload(
      config.request_message->SerializeAsString(), "application/x-protobuf");
  send_url_loader->SetAllowHttpErrorResults(true);
  return send_url_loader;
}

void ProtobufHttpClient::CancelRequest(
    const PendingRequestListIterator& request_iterator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_requests_.erase(request_iterator);
}

}  // namespace remoting
