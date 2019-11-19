// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_support/grpc_util.h"

#include <chrono>

#include "third_party/grpc/src/include/grpcpp/client_context.h"

namespace remoting {

void SetDeadline(grpc_impl::ClientContext* context, base::Time deadline) {
  context->set_deadline(
      std::chrono::system_clock::from_time_t(deadline.ToTimeT()));
}

base::Time GetDeadline(const grpc_impl::ClientContext& context) {
  auto deadline_tp = context.deadline();
  if (deadline_tp == std::chrono::system_clock::time_point::max()) {
    return base::Time::Max();
  }
  return base::Time::FromTimeT(
      std::chrono::system_clock::to_time_t(deadline_tp));
}

}  // namespace remoting
