// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/corp_session_authz_service_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/session_authz_service.h"

namespace remoting {

namespace {

template <typename Type>
using ResponseCallback =
    base::OnceCallback<void(const ProtobufHttpStatus&, std::unique_ptr<Type>)>;

template <typename ProtoType, typename StructType>
using ConversionFunction = std::unique_ptr<StructType> (*)(const ProtoType&);

// Creates a callback that takes a response of `ProtoType`, which, upon
// invocation, converts it to a response of `StructType` by calling
// `conversion_function`, then calls `struct_callback`
template <typename ProtoType, typename StructType>
ResponseCallback<ProtoType> ConvertCallback(
    ResponseCallback<StructType> struct_callback,
    ConversionFunction<ProtoType, StructType> conversion_function) {
  return base::BindOnce(
      [](ResponseCallback<StructType> struct_callback,
         ConversionFunction<ProtoType, StructType> conversion_function,
         const ProtobufHttpStatus& status,
         std::unique_ptr<ProtoType> proto_type) {
        std::move(struct_callback)
            .Run(status,
                 proto_type ? conversion_function(*proto_type) : nullptr);
      },
      std::move(struct_callback), conversion_function);
}

}  // namespace

CorpSessionAuthzServiceClient::CorpSessionAuthzServiceClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter)
    : oauth_token_getter_(std::move(oauth_token_getter)),
      http_client_(ServiceUrls::GetInstance()->remoting_corp_endpoint(),
                   oauth_token_getter_.get(),
                   url_loader_factory) {}

CorpSessionAuthzServiceClient::~CorpSessionAuthzServiceClient() = default;

void CorpSessionAuthzServiceClient::GenerateHostToken(
    GenerateHostTokenCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "remoting_corp_session_authz_generate_host_token",
          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Generates a host token to be used to authorize a Chrome Remote "
            "Desktop connection from a corp user to a corp-managed device "
            "(this device)."
          trigger:
            "Corp user connects to a corp-managed machine. Note that this "
            "functionality is not available outside of the corp network so "
            "external users will never see this request being made."
          user_data {
            type: ACCESS_TOKEN
          }
          data:
            "Access token of the Chrome Remote Desktop host's robot account."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2024-02-01"
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
  ExecuteRequest(
      traffic_annotation, internal::GetGenerateHostTokenRequestPath(),
      internal::GetGenerateHostTokenRequest({}),
      ConvertCallback(std::move(callback),
                      &internal::GetGenerateHostTokenResponseStruct));
}

void CorpSessionAuthzServiceClient::VerifySessionToken(
    const internal::VerifySessionTokenRequestStruct& request,
    VerifySessionTokenCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "remoting_corp_session_authz_verify_session_token",
          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Verifies a session token returned by the client device and "
            "decodes the shared secret from the session token, to be used to "
            "authorize a Chrome Remote Desktop connection from a corp user to "
            "a corp-managed device (this device)."
          trigger:
            "Corp user connects to a corp-managed machine. Note that this "
            "functionality is not available outside of the corp network so "
            "external users will never see this request being made."
          user_data {
            type: ACCESS_TOKEN
          }
          data:
            "Access token of the Chrome Remote Desktop host's robot account, "
            "session token returned by the Chrome Remote Desktop client."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2024-02-01"
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
  ExecuteRequest(
      traffic_annotation, internal::GetVerifySessionTokenRequestPath(),
      internal::GetVerifySessionTokenRequest(request),
      ConvertCallback(std::move(callback),
                      &internal::GetVerifySessionTokenResponseStruct));
}

void CorpSessionAuthzServiceClient::ReauthorizeHost(
    const internal::ReauthorizeHostRequestStruct& request,
    ReauthorizeHostCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "remoting_corp_session_reauthorize_host",
          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Reauthorizes the current Chrome Remote Desktop connection, "
            "initiated by a corp user."
          trigger:
            "Corp user connects to a corp-managed machine. Note that this "
            "functionality is not available outside of the corp network so "
            "external users will never see this request being made."
          user_data {
            type: ACCESS_TOKEN
          }
          data:
            "Access token of the Chrome Remote Desktop host's robot account, "
            "session token returned by the Chrome Remote Desktop client."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2024-02-01"
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
  ExecuteRequest(traffic_annotation, internal::GetReauthorizeHostRequestPath(),
                 internal::GetReauthorizeHostRequest(request),
                 ConvertCallback(std::move(callback),
                                 &internal::GetReauthorizeHostResponseStruct));
}

template <typename CallbackType>
void CorpSessionAuthzServiceClient::ExecuteRequest(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& path,
    std::unique_ptr<google::protobuf::MessageLite> request_message,
    CallbackType callback) {
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(traffic_annotation);
  request_config->path = path;
  request_config->api_key = internal::GetRemotingCorpApiKey();
  request_config->authenticated = true;
  request_config->provide_certificate = true;
  request_config->request_message = std::move(request_message);
  auto request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  request->SetResponseCallback(std::move(callback));
  http_client_.ExecuteRequest(std::move(request));
}

}  // namespace remoting
