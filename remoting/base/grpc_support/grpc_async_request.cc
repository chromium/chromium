// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_support/grpc_async_request.h"

#include "third_party/grpc/src/include/grpcpp/client_context.h"

namespace remoting {

GrpcAsyncRequest::GrpcAsyncRequest() = default;

GrpcAsyncRequest::~GrpcAsyncRequest() = default;

void GrpcAsyncRequest::CancelRequest() {
  VLOG(1) << "Canceling request: " << this;
  context_.TryCancel();
  OnRequestCanceled();
}

base::WeakPtr<GrpcAsyncRequest> GrpcAsyncRequest::GetGrpcAsyncRequestWeakPtr() {
  return grpc_async_request_weak_factory_.GetWeakPtr();
}

}  // namespace remoting
