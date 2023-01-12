// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/service_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "remoting/base/directory_service_client.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/proto/remoting/v1/directory_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

class ServiceClient::Core
    : public base::RefCountedThreadSafe<ServiceClient::Core> {
 public:
  explicit Core(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : directory_service_client_(&token_getter_, url_loader_factory) {}

  void RegisterHost(const std::string& host_id,
                    const std::string& host_name,
                    const std::string& public_key,
                    const std::string& host_client_id,
                    const std::string& oauth_access_token,
                    ServiceClient::Delegate* delegate);

  void DeleteHost(const std::string& host_id,
                  const std::string& oauth_access_token,
                  ServiceClient::Delegate* delegate);

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() = default;

  enum PendingRequestType {
    PENDING_REQUEST_NONE,
    PENDING_REQUEST_REGISTER_HOST,
    PENDING_REQUEST_DELETE_HOST
  };

  void OnRegisterHostResponse(
      const ProtobufHttpStatus& status,
      std::unique_ptr<apis::v1::RegisterHostResponse> response);

  void OnDeleteHostResponse(
      const ProtobufHttpStatus& status,
      std::unique_ptr<apis::v1::DeleteHostResponse> response);

  void NotifyError(const ProtobufHttpStatus& status);

  raw_ptr<ServiceClient::Delegate> delegate_ = nullptr;
  PendingRequestType pending_request_type_ = PENDING_REQUEST_NONE;
  PassthroughOAuthTokenGetter token_getter_;
  DirectoryServiceClient directory_service_client_;
};

void ServiceClient::Core::RegisterHost(const std::string& host_id,
                                       const std::string& host_name,
                                       const std::string& public_key,
                                       const std::string& host_client_id,
                                       const std::string& oauth_access_token,
                                       Delegate* delegate) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_NONE);
  pending_request_type_ = PENDING_REQUEST_REGISTER_HOST;

  delegate_ = delegate;

  token_getter_.set_access_token(oauth_access_token);
  directory_service_client_.RegisterHost(
      host_id, host_name, public_key, host_client_id,
      base::BindOnce(&ServiceClient::Core::OnRegisterHostResponse,
                     base::Unretained(this)));
}

void ServiceClient::Core::DeleteHost(const std::string& host_id,
                                     const std::string& oauth_access_token,
                                     Delegate* delegate) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_NONE);
  pending_request_type_ = PENDING_REQUEST_DELETE_HOST;

  delegate_ = delegate;

  token_getter_.set_access_token(oauth_access_token);
  directory_service_client_.DeleteHost(
      host_id, base::BindOnce(&ServiceClient::Core::OnDeleteHostResponse,
                              base::Unretained(this)));
}

void ServiceClient::Core::OnRegisterHostResponse(
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::RegisterHostResponse> response) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_REGISTER_HOST);
  pending_request_type_ = PENDING_REQUEST_NONE;

  if (!status.ok()) {
    NotifyError(status);
    return;
  }

  if (!response->has_auth_code()) {
    LOG(ERROR) << "No auth_code in server response.";
    delegate_->OnOAuthError();
    return;
  }

  delegate_->OnHostRegistered(response->auth_code());
}

void ServiceClient::Core::OnDeleteHostResponse(
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::DeleteHostResponse> response) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_DELETE_HOST);
  pending_request_type_ = PENDING_REQUEST_NONE;

  if (!status.ok()) {
    NotifyError(status);
    return;
  }

  delegate_->OnHostUnregistered();
}

void ServiceClient::Core::NotifyError(const ProtobufHttpStatus& status) {
  ProtobufHttpStatus::Code error_code = status.error_code();
  LOG(ERROR) << "Received error code: " << static_cast<int>(error_code)
             << ", message: " << status.error_message();

  // TODO(crbug.com/968326): Update the Delegate interface and reporting to
  // better reflect the errors that gRPC returns.
  switch (error_code) {
    case ProtobufHttpStatus::Code::PERMISSION_DENIED:
    case ProtobufHttpStatus::Code::UNAUTHENTICATED:
      delegate_->OnOAuthError();
      return;
    default:
      delegate_->OnNetworkError(static_cast<int>(error_code));
  }
}

ServiceClient::ServiceClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  core_ = new Core(url_loader_factory);
}

ServiceClient::~ServiceClient() = default;

void ServiceClient::RegisterHost(const std::string& host_id,
                                 const std::string& host_name,
                                 const std::string& public_key,
                                 const std::string& host_client_id,
                                 const std::string& oauth_access_token,
                                 Delegate* delegate) {
  return core_->RegisterHost(host_id, host_name, public_key, host_client_id,
                             oauth_access_token, delegate);
}

void ServiceClient::UnregisterHost(const std::string& host_id,
                                   const std::string& oauth_access_token,
                                   Delegate* delegate) {
  return core_->DeleteHost(host_id, oauth_access_token, delegate);
}

}  // namespace remoting
