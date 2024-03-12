// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/corp_logging_service_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/empty.pb.h"

namespace remoting {

CorpLoggingServiceClient::CorpLoggingServiceClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter)
    : oauth_token_getter_(std::move(oauth_token_getter)),
      http_client_(ServiceUrls::GetInstance()->remoting_corp_endpoint(),
                   oauth_token_getter_.get(),
                   url_loader_factory) {}

CorpLoggingServiceClient::~CorpLoggingServiceClient() = default;

void CorpLoggingServiceClient::ReportSessionDisconnected(
    const internal::ReportSessionDisconnectedRequestStruct& request_struct,
    StatusCallback done) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "remoting_corp_logging_report_session_disconnected",
          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Reports to the corp logging service that the current Chrome "
            "Remote Desktop connection has disconnected."
          trigger:
            "Corp user disconnects from a corp-managed machine. Note that this "
            "request is not made outside of the corp network so external users "
            "will never see this request being made."
          user_data {
            type: ACCESS_TOKEN
          }
          data:
            "Access token of the Chrome Remote Desktop host's robot account; "
            "session token returned by the Chrome Remote Desktop client, "
            "which identifies the disconnected session; error code explaining "
            "why the session was disconnected."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2024-03-07"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the Chrome Remote Desktop host is not registered on a "
            "corp-managed device."
          policy_exception_justification:
            "Not implemented."
        })");
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(traffic_annotation);
  request_config->path = internal::GetReportSessionDisconnectedRequestPath();
  request_config->api_key = internal::GetRemotingCorpApiKey();
  request_config->authenticated = true;
  request_config->provide_certificate = true;
  request_config->request_message =
      internal::GetReportSessionDisconnectedRequest(request_struct);
  auto request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  request->SetResponseCallback(
      base::BindOnce([](const ProtobufHttpStatus& status,
                        std::unique_ptr<Empty> proto) {
        return status;
      }).Then(std::move(done)));
  http_client_.ExecuteRequest(std::move(request));
}

}  // namespace remoting
