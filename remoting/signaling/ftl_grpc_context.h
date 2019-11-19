// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_GRPC_CONTEXT_H_
#define REMOTING_SIGNALING_FTL_GRPC_CONTEXT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "net/base/backoff_entry.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"

namespace grpc_impl {
class ClientContext;
}  // namespace grpc_impl

namespace remoting {

// This is the class for creating context objects to be used when connecting
// to FTL backend.
class FtlGrpcContext final {
 public:
  static constexpr base::TimeDelta kBackoffInitialDelay =
      base::TimeDelta::FromSeconds(1);
  static constexpr base::TimeDelta kBackoffMaxDelay =
      base::TimeDelta::FromMinutes(1);

  static const net::BackoffEntry::Policy& GetBackoffPolicy();
  static std::string GetChromotingAppIdentifier();
  static ftl::Id CreateIdFromString(const std::string& ftl_id);
  static GrpcChannelSharedPtr CreateChannel();
  static void FillClientContext(grpc_impl::ClientContext* context);
  static ftl::RequestHeader CreateRequestHeader(
      const std::string& ftl_auth_token = {});

  static void SetChannelForTesting(GrpcChannelSharedPtr channel);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FtlGrpcContext);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_GRPC_CONTEXT_H_
