// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_SUPPORT_GRPC_EXECUTOR_H_
#define REMOTING_BASE_GRPC_SUPPORT_GRPC_EXECUTOR_H_

#include <memory>

#include "base/macros.h"

namespace remoting {

class GrpcAsyncRequest;

// Interface for executing a gRPC request.
class GrpcExecutor {
 public:
  GrpcExecutor() = default;
  virtual ~GrpcExecutor() = default;

  // Executes a GrpcAsyncRequest and calls its callback when a response is
  // received. All pending requests will be silently dropped when the
  // GrpcExecutor instance is destroyed.
  virtual void ExecuteRpc(std::unique_ptr<GrpcAsyncRequest> request) = 0;

  // Cancels all pending requests.
  virtual void CancelPendingRequests() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GrpcExecutor);
};

}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_SUPPORT_GRPC_EXECUTOR_H_
