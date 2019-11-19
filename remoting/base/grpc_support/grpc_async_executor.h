// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_EXECUTOR_H_
#define REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_EXECUTOR_H_

#include <list>
#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/base/grpc_support/grpc_executor.h"

namespace remoting {

class GrpcAsyncRequest;

// This class helps adapting the gRPC async completion queue handling logic into
// Chromium's callback paradigm. See README.md for detailed usage.
class GrpcAsyncExecutor final : public GrpcExecutor {
 public:
  GrpcAsyncExecutor();
  ~GrpcAsyncExecutor() override;

  // GrpcExecutor implementation.
  void ExecuteRpc(std::unique_ptr<GrpcAsyncRequest> request) override;
  void CancelPendingRequests() override;

 private:
  using PendingRequestList = std::list<base::WeakPtr<GrpcAsyncRequest>>;
  using PendingRequestListIter = PendingRequestList::iterator;

  void OnDequeue(std::unique_ptr<GrpcAsyncRequest> request,
                 bool operation_succeeded);

  // Helper methods for GrpcAsyncRequest to run callback only when the executor
  // is still alive.
  void PostTaskToRunClosure(base::OnceClosure closure);
  void RunClosure(base::OnceClosure closure);

  PendingRequestListIter FindRequest(GrpcAsyncRequest* request);

  SEQUENCE_CHECKER(sequence_checker_);

  // Keep the list of pending requests so that we can cancel them at
  // destruction.
  PendingRequestList pending_requests_;

  base::WeakPtrFactory<GrpcAsyncExecutor> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncExecutor);
};

}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_EXECUTOR_H_
