// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/directory_service_client.h"

#include "base/functional/bind.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/directory_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

DirectoryServiceClient::DirectoryServiceClient(
    OAuthTokenGetter* token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : http_client_(ServiceUrls::GetInstance()->remoting_server_endpoint(),
                   token_getter,
                   url_loader_factory) {}

DirectoryServiceClient::~DirectoryServiceClient() = default;

void DirectoryServiceClient::DeleteHost(const std::string& host_id,
                                        DeleteHostCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("remoting_directory_delete_host",
                                          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Deletes a Chrome Remote Desktop host from the user's account."
          trigger:
            "User deletes a Chrome Remote Desktop host from the host list."
          data:
            "User's Chrome Remote Desktop credentials and the Chrome Remote "
            "Desktop host ID that identifies the host."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");
  constexpr char path[] = "/v1/directory:deletehost";

  auto delete_host_request = std::make_unique<apis::v1::DeleteHostRequest>();
  delete_host_request->set_host_id(host_id);
  ExecuteRequest(traffic_annotation, path, std::move(delete_host_request),
                 std::move(callback));
}

void DirectoryServiceClient::GetHostList(GetHostListCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("remoting_directory_get_host_list",
                                          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Retrieves information about Chrome Remote Desktop hosts that are "
            "registered under the user's account."
          trigger:
            "User opens the Chrome Remote Desktop app."
          data:
            "User's Chrome Remote Desktop credentials."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");
  constexpr char path[] = "/v1/directory:gethostlist";

  ExecuteRequest(traffic_annotation, path,
                 std::make_unique<apis::v1::GetHostListRequest>(),
                 std::move(callback));
}

void DirectoryServiceClient::RegisterHost(const std::string& host_id,
                                          const std::string& host_name,
                                          const std::string& public_key,
                                          const std::string& host_client_id,
                                          RegisterHostCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("remoting_directory_register_host",
                                          R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Registers a new Chrome Remote Desktop host under the user's "
            "account."
          trigger:
            "User runs the remoting_start_host command by typing it on the "
            "terminal. Third party administrators might implement scripts to "
            "run this automatically, but the Chrome Remote Desktop product "
            "does not come with such scripts."
          data:
            "User's Chrome Remote Desktop credentials and information about "
            "the new Chrome Remote Desktop host such as host ID and host name."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");
  constexpr char path[] = "/v1/directory:registerhost";

  auto register_host_request =
      std::make_unique<apis::v1::RegisterHostRequest>();
  register_host_request->set_host_id(host_id);
  register_host_request->set_host_name(host_name);
  register_host_request->set_public_key(public_key);
  register_host_request->set_host_client_id(host_client_id);
  ExecuteRequest(traffic_annotation, path, std::move(register_host_request),
                 std::move(callback));
}

void DirectoryServiceClient::CancelPendingRequests() {
  http_client_.CancelPendingRequests();
}

template <typename CallbackType>
void DirectoryServiceClient::ExecuteRequest(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& path,
    std::unique_ptr<google::protobuf::MessageLite> request_message,
    CallbackType callback) {
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(traffic_annotation);
  request_config->path = path;
  request_config->request_message = std::move(request_message);
  auto request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  request->SetResponseCallback(std::move(callback));
  http_client_.ExecuteRequest(std::move(request));
}

}  // namespace remoting
