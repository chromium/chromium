// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/directory_service_client.h"

#include "base/functional/bind.h"
#include "base/strings/stringize_macros.h"
#include "base/system/sys_info.h"
#include "remoting/base/fqdn.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/directory_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
constexpr net::NetworkTrafficAnnotationTag kDeleteHostTrafficAnnotation =
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

constexpr net::NetworkTrafficAnnotationTag kGetHostListTrafficAnnotation =
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

constexpr net::NetworkTrafficAnnotationTag kLegacyHeartbeatTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remoting_directory_legacy_heartbeat",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Sends updated information about the Chrome Remote Desktop host."
          trigger:
            "Sent when host is initially set up or restarted."
          data:
            "Includes an internal UUID to identify the remote access host "
            "instance, the name and version of the operating system, the "
            "version of the CRD package installed, and a set of signaling ids "
            "which the CRD client can use to send the host messages."
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

constexpr net::NetworkTrafficAnnotationTag kRegisterHostTrafficAnnotation =
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

constexpr net::NetworkTrafficAnnotationTag kSendHeartbeatTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remoting_directory_send_heartbeat",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "A lightweight heartbeat message that can be used to identify "
            "the last time the Chrome Remote Desktop host was online."
          trigger:
            "Sent periodically from the host to indicate that it is alive."
          data:
            "Includes an internal UUID to identify the remote access host."
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

}  // namespace

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
  constexpr char path[] = "/v1/directory:deletehost";

  auto delete_host_request = std::make_unique<apis::v1::DeleteHostRequest>();
  delete_host_request->set_host_id(host_id);
  ExecuteRequest(kDeleteHostTrafficAnnotation, path,
                 std::move(delete_host_request), std::move(callback));
}

void DirectoryServiceClient::GetHostList(GetHostListCallback callback) {
  constexpr char path[] = "/v1/directory:gethostlist";

  ExecuteRequest(kGetHostListTrafficAnnotation, path,
                 std::make_unique<apis::v1::GetHostListRequest>(),
                 std::move(callback));
}

void DirectoryServiceClient::LegacyHeartbeat(
    const std::string& directory_id,
    std::optional<std::string> signaling_id,
    std::optional<std::string> offline_reason,
    bool is_initial_heartbeat,
    bool set_fqdn,
    const std::string& os_name,
    const std::string& os_version,
    LegacyHeartbeatCallback callback) {
  constexpr char path[] = "/v1/directory:heartbeat";

  auto request = std::make_unique<apis::v1::HeartbeatRequest>();
  request->set_host_id(directory_id);
  if (signaling_id) {
    request->set_tachyon_id(*signaling_id);
  }
  if (offline_reason) {
    request->set_host_offline_reason(*offline_reason);
  }
  request->set_host_version(STRINGIZE(VERSION));
  request->set_host_os_name(os_name);
  request->set_host_os_version(os_version);
  request->set_host_cpu_type(base::SysInfo::OperatingSystemArchitecture());
  request->set_is_initial_heartbeat(is_initial_heartbeat);

  if (set_fqdn) {
    std::string fqdn = GetFqdn();
    if (!fqdn.empty()) {
      request->set_hostname(fqdn);
    }
  }

  ExecuteRequest(kLegacyHeartbeatTrafficAnnotation, path, std::move(request),
                 std::move(callback));
}

void DirectoryServiceClient::RegisterHost(const std::string& host_id,
                                          const std::string& host_name,
                                          const std::string& public_key,
                                          const std::string& host_client_id,
                                          RegisterHostCallback callback) {
  constexpr char path[] = "/v1/directory:registerhost";

  auto register_host_request =
      std::make_unique<apis::v1::RegisterHostRequest>();
  if (!host_id.empty()) {
    register_host_request->set_host_id(host_id);
  }
  register_host_request->set_host_name(host_name);
  register_host_request->set_public_key(public_key);
  register_host_request->set_host_client_id(host_client_id);
  ExecuteRequest(kRegisterHostTrafficAnnotation, path,
                 std::move(register_host_request), std::move(callback));
}

void DirectoryServiceClient::SendHeartbeat(const std::string& directory_id,
                                           SendHeartbeatCallback callback) {
  constexpr char path[] = "/v1/directory:sendheartbeat";

  auto request = std::make_unique<apis::v1::SendHeartbeatRequest>();
  request->set_host_id(directory_id);

  ExecuteRequest(kSendHeartbeatTrafficAnnotation, path, std::move(request),
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
  request->SetTimeoutDuration(base::Seconds(30));
  request->SetResponseCallback(std::move(callback));
  http_client_.ExecuteRequest(std::move(request));
}

}  // namespace remoting
