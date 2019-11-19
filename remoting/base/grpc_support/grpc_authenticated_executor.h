// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_SUPPORT_GRPC_AUTHENTICATED_EXECUTOR_H_
#define REMOTING_BASE_GRPC_SUPPORT_GRPC_AUTHENTICATED_EXECUTOR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/grpc_support/grpc_executor.h"
#include "remoting/base/oauth_token_getter.h"

namespace remoting {

class GrpcAsyncRequest;

// Class to execute gRPC request with OAuth authentication.
class GrpcAuthenticatedExecutor final : public GrpcExecutor {
 public:
  // |token_getter| must outlive |this|.
  explicit GrpcAuthenticatedExecutor(OAuthTokenGetter* token_getter);

  ~GrpcAuthenticatedExecutor() override;

  // GrpcExecutor implementation.
  void ExecuteRpc(std::unique_ptr<GrpcAsyncRequest> request) override;
  void CancelPendingRequests() override;

 private:
  friend class GrpcAuthenticatedExecutorTest;

  void ExecuteRpcWithFetchedOAuthToken(
      std::unique_ptr<GrpcAsyncRequest> request,
      OAuthTokenGetter::Status status,
      const std::string& user_email,
      const std::string& access_token);

  OAuthTokenGetter* token_getter_;
  std::unique_ptr<GrpcExecutor> executor_;

  base::WeakPtrFactory<GrpcAuthenticatedExecutor> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(GrpcAuthenticatedExecutor);
};

}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_SUPPORT_GRPC_AUTHENTICATED_EXECUTOR_H_
