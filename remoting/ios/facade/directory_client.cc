// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/facade/directory_client.h"

#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/base/service_urls.h"

namespace remoting {

DirectoryClient::DirectoryClient(OAuthTokenGetter* token_getter)
    : DirectoryClient(
          std::make_unique<GrpcAuthenticatedExecutor>(token_getter),
          CreateSslChannelForEndpoint(
              ServiceUrls::GetInstance()->remoting_server_endpoint())) {}

DirectoryClient::DirectoryClient(std::unique_ptr<GrpcExecutor> executor,
                                 GrpcChannelSharedPtr channel)
    : grpc_executor_(std::move(executor)),
      stub_(DirectoryService::NewStub(channel)) {}

DirectoryClient::~DirectoryClient() = default;

void DirectoryClient::GetHostList(GetHostListCallback callback) {
  grpc_executor_->ExecuteRpc(CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&DirectoryService::Stub::AsyncGetHostList,
                     base::Unretained(stub_.get())),
      apis::v1::GetHostListRequest(), std::move(callback)));
}

void DirectoryClient::CancelPendingRequests() {
  grpc_executor_->CancelPendingRequests();
}

}  // namespace remoting
