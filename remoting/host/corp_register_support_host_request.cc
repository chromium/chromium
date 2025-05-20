// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/corp_register_support_host_request.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/http_status.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remote_support_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kSupportIdLifetime = base::Minutes(5);

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("corp_register_support_host_request",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Request used by Chrome Remote Desktop for a corp user to register "
            "a new remote support host."
          trigger:
            "Corp user requests for remote assistance using Chrome Remote "
            "Desktop."
          user_data {
            type: CREDENTIALS
            type: ACCESS_TOKEN
          }
          data:
            "The user's OAuth token for Chrome Remote Desktop (CRD) and CRD "
            "host information such as CRD host public key, host version, and "
            "OS version."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2025-05-14"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop, or the user is "
            "not a corp user (specified by the isCorpUser flag)."
          chrome_policy {
            RemoteAccessHostAllowRemoteSupportConnections {
              RemoteAccessHostAllowRemoteSupportConnections: false
            }
            RemoteAccessHostAllowEnterpriseRemoteSupportConnections {
              RemoteAccessHostAllowEnterpriseRemoteSupportConnections: false
            }
          }
        })");

}  // namespace

CorpRegisterSupportHostRequest::CorpRegisterSupportHostRequest(
    std::unique_ptr<OAuthTokenGetter> token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : token_getter_(std::move(token_getter)),
      url_loader_factory_(url_loader_factory) {}

CorpRegisterSupportHostRequest::~CorpRegisterSupportHostRequest() = default;

void CorpRegisterSupportHostRequest::Initialize(
    std::unique_ptr<net::ClientCertStore> client_cert_store) {
  http_client_ = std::make_unique<ProtobufHttpClient>(
      ServiceUrls::GetInstance()->remoting_corp_endpoint(), token_getter_.get(),
      url_loader_factory_, std::move(client_cert_store));
}

void CorpRegisterSupportHostRequest::RegisterHost(
    const internal::RemoteSupportHostStruct& host,
    const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
    RegisterHostCallback callback) {
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(kTrafficAnnotation);
  request_config->path = internal::GetCreateRemoteSupportHostRequestPath();
  request_config->api_key = internal::GetRemotingCorpApiKey();
  request_config->provide_certificate = true;
  request_config->request_message = internal::GetRemoteSupportHost(host);
  auto http_request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  http_request->SetResponseCallback(base::BindOnce(
      [](RegisterHostCallback callback, const HttpStatus& status,
         std::unique_ptr<internal::RemoteSupportHost> response) {
        if (response) {
          std::move(callback).Run(status, internal::GetSupportId(*response),
                                  kSupportIdLifetime);
        } else {
          std::move(callback).Run(status, {}, {});
        }
      },
      std::move(callback)));
  http_client_->ExecuteRequest(std::move(http_request));
}

void CorpRegisterSupportHostRequest::CancelPendingRequests() {
  http_client_->CancelPendingRequests();
}

}  // namespace remoting
