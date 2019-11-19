// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_SUPPORT_GRPC_UTIL_H_
#define REMOTING_BASE_GRPC_SUPPORT_GRPC_UTIL_H_

#include "base/time/time.h"

namespace grpc_impl {
class ClientContext;
}  // namespace grpc_impl

namespace remoting {

// Sets the deadline on |context|.
void SetDeadline(grpc_impl::ClientContext* context, base::Time deadline);

// Gets the deadline in base::Time. Returns base::Time::Max if the deadline is
// not set.
base::Time GetDeadline(const grpc_impl::ClientContext& context);

}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_SUPPORT_GRPC_UTIL_H_
