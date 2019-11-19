// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/service_client.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "remoting/base/grpc_support/grpc_async_executor.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"
#include "third_party/grpc/src/include/grpcpp/security/credentials.h"

namespace remoting {

namespace {

// Class for making Directory Service requests via gRPC, used by
// ServiceClient::Core.
class DirectoryServiceClient {
 public:
  using RegisterHostCallback =
      base::OnceCallback<void(const grpc::Status&,
                              const apis::v1::RegisterHostResponse&)>;
  using DeleteHostCallback =
      base::OnceCallback<void(const grpc::Status&,
                              const apis::v1::DeleteHostResponse&)>;

  explicit DirectoryServiceClient(const std::string& remoting_server_endpoint);
  ~DirectoryServiceClient();

  void RegisterHost(const std::string& host_id,
                    const std::string& host_name,
                    const std::string& public_key,
                    const std::string& host_client_id,
                    const std::string& oauth_access_token,
                    RegisterHostCallback callback);

  void DeleteHost(const std::string& host_id,
                  const std::string& oauth_access_token,
                  DeleteHostCallback callback);

 private:
  using RemotingDirectoryService = apis::v1::RemotingDirectoryService;

  GrpcAsyncExecutor grpc_executor_;
  std::unique_ptr<apis::v1::RemotingDirectoryService::Stub> stub_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryServiceClient);
};

DirectoryServiceClient::DirectoryServiceClient(
    const std::string& remoting_server_endpoint) {
  GrpcChannelSharedPtr channel =
      CreateSslChannelForEndpoint(remoting_server_endpoint);
  stub_ = RemotingDirectoryService::NewStub(channel);
}

DirectoryServiceClient::~DirectoryServiceClient() = default;

void DirectoryServiceClient::RegisterHost(const std::string& host_id,
                                          const std::string& host_name,
                                          const std::string& public_key,
                                          const std::string& host_client_id,
                                          const std::string& oauth_access_token,
                                          RegisterHostCallback callback) {
  auto register_host_request = apis::v1::RegisterHostRequest();
  register_host_request.set_host_id(host_id);
  register_host_request.set_host_name(host_name);
  register_host_request.set_public_key(public_key);
  register_host_request.set_host_client_id(host_client_id);

  auto async_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&RemotingDirectoryService::Stub::AsyncRegisterHost,
                     base::Unretained(stub_.get())),
      register_host_request, std::move(callback));

  async_request->context()->set_credentials(
      grpc::AccessTokenCredentials(oauth_access_token));
  grpc_executor_.ExecuteRpc(std::move(async_request));
}

void DirectoryServiceClient::DeleteHost(const std::string& host_id,
                                        const std::string& oauth_access_token,
                                        DeleteHostCallback callback) {
  auto delete_host_request = apis::v1::DeleteHostRequest();
  delete_host_request.set_host_id(host_id);

  auto async_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&RemotingDirectoryService::Stub::AsyncDeleteHost,
                     base::Unretained(stub_.get())),
      delete_host_request, std::move(callback));

  async_request->context()->set_credentials(
      grpc::AccessTokenCredentials(oauth_access_token));
  grpc_executor_.ExecuteRpc(std::move(async_request));
}

}  // namespace

class ServiceClient::Core
    : public base::RefCountedThreadSafe<ServiceClient::Core> {
 public:
  explicit Core(const std::string& remoting_server_endpoint)
      : directory_service_client_(remoting_server_endpoint) {}

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

  void OnRegisterHostResponse(const grpc::Status& status,
                              const apis::v1::RegisterHostResponse& response);

  void OnDeleteHostResponse(const grpc::Status& status,
                            const apis::v1::DeleteHostResponse& response);

  void NotifyError(const grpc::Status& status);

  ServiceClient::Delegate* delegate_ = nullptr;
  PendingRequestType pending_request_type_ = PENDING_REQUEST_NONE;
  DirectoryServiceClient directory_service_client_;
};

void ServiceClient::Core::RegisterHost(
    const std::string& host_id,
    const std::string& host_name,
    const std::string& public_key,
    const std::string& host_client_id,
    const std::string& oauth_access_token,
    Delegate* delegate) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_NONE);
  pending_request_type_ = PENDING_REQUEST_REGISTER_HOST;

  delegate_ = delegate;

  directory_service_client_.RegisterHost(
      host_id, host_name, public_key, host_client_id, oauth_access_token,
      base::Bind(&ServiceClient::Core::OnRegisterHostResponse,
                 base::Unretained(this)));
}

void ServiceClient::Core::DeleteHost(const std::string& host_id,
                                     const std::string& oauth_access_token,
                                     Delegate* delegate) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_NONE);
  pending_request_type_ = PENDING_REQUEST_DELETE_HOST;

  delegate_ = delegate;

  directory_service_client_.DeleteHost(
      host_id, oauth_access_token,
      base::Bind(&ServiceClient::Core::OnDeleteHostResponse,
                 base::Unretained(this)));
}

void ServiceClient::Core::OnRegisterHostResponse(
    const grpc::Status& status,
    const apis::v1::RegisterHostResponse& response) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_REGISTER_HOST);
  pending_request_type_ = PENDING_REQUEST_NONE;

  if (!status.ok()) {
    NotifyError(status);
    return;
  }

  if (!response.has_auth_code()) {
    LOG(ERROR) << "No auth_code in server response.";
    delegate_->OnOAuthError();
    return;
  }

  delegate_->OnHostRegistered(response.auth_code());
}

void ServiceClient::Core::OnDeleteHostResponse(
    const grpc::Status& status,
    const apis::v1::DeleteHostResponse& response) {
  DCHECK(pending_request_type_ == PENDING_REQUEST_DELETE_HOST);
  pending_request_type_ = PENDING_REQUEST_NONE;

  if (!status.ok()) {
    NotifyError(status);
    return;
  }

  delegate_->OnHostUnregistered();
}

void ServiceClient::Core::NotifyError(const grpc::Status& status) {
  grpc::StatusCode error_code = status.error_code();
  LOG(ERROR) << "Received error code: " << error_code
             << ", message: " << status.error_message();

  // TODO(crbug.com/968326): Update the Delegate interface and reporting to
  // better reflect the errors that gRPC returns.
  switch (error_code) {
    case grpc::StatusCode::PERMISSION_DENIED:
    case grpc::StatusCode::UNAUTHENTICATED:
      delegate_->OnOAuthError();
      return;
    default:
      delegate_->OnNetworkError(error_code);
  }
}

ServiceClient::ServiceClient(const std::string& remoting_server_endpoint) {
  core_ = new Core(remoting_server_endpoint);
}

ServiceClient::~ServiceClient() = default;

void ServiceClient::RegisterHost(
    const std::string& host_id,
    const std::string& host_name,
    const std::string& public_key,
    const std::string& host_client_id,
    const std::string& oauth_access_token,
    Delegate* delegate) {
  return core_->RegisterHost(host_id, host_name, public_key, host_client_id,
                             oauth_access_token, delegate);
}

void ServiceClient::UnregisterHost(
    const std::string& host_id,
    const std::string& oauth_access_token,
    Delegate* delegate) {
  return core_->DeleteHost(host_id, oauth_access_token, delegate);
}

}  // namespace remoting
