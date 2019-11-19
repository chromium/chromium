// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_support/grpc_authenticated_executor.h"

#include <utility>

#include "base/bind.h"
#include "remoting/base/grpc_support/grpc_async_executor.h"
#include "remoting/base/grpc_support/grpc_async_request.h"
#include "third_party/grpc/src/include/grpcpp/client_context.h"
#include "third_party/grpc/src/include/grpcpp/security/credentials.h"

namespace remoting {

GrpcAuthenticatedExecutor::GrpcAuthenticatedExecutor(
    OAuthTokenGetter* token_getter) {
  DCHECK(token_getter);
  token_getter_ = token_getter;
  executor_ = std::make_unique<GrpcAsyncExecutor>();
}

GrpcAuthenticatedExecutor::~GrpcAuthenticatedExecutor() = default;

void GrpcAuthenticatedExecutor::ExecuteRpc(
    std::unique_ptr<GrpcAsyncRequest> request) {
  token_getter_->CallWithToken(base::BindOnce(
      &GrpcAuthenticatedExecutor::ExecuteRpcWithFetchedOAuthToken,
      weak_factory_.GetWeakPtr(), std::move(request)));
}

void GrpcAuthenticatedExecutor::ExecuteRpcWithFetchedOAuthToken(
    std::unique_ptr<GrpcAsyncRequest> request,
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token) {
  if (status != OAuthTokenGetter::Status::SUCCESS) {
    LOG(ERROR) << "Failed to fetch access token. Status: " << status;
  }
  if (status == OAuthTokenGetter::Status::SUCCESS && !access_token.empty()) {
    request->context()->set_credentials(
        grpc::AccessTokenCredentials(access_token));
  } else {
    LOG(WARNING) << "Attempting to execute RPC without access token.";
  }
  executor_->ExecuteRpc(std::move(request));
}

void GrpcAuthenticatedExecutor::CancelPendingRequests() {
  // Drop all CallWithToken() callbacks.
  weak_factory_.InvalidateWeakPtrs();
  executor_->CancelPendingRequests();
}

}  // namespace remoting
