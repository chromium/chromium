// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTO_LOGGING_SERVICE_H_
#define REMOTING_PROTO_LOGGING_SERVICE_H_

#include <optional>
#include <string>

#include "remoting/base/errors.h"
#include "remoting/base/session_policies.h"

namespace remoting::internal {

struct ReportSessionDisconnectedRequestStruct {
  bool operator==(const ReportSessionDisconnectedRequestStruct&) const =
      default;

  std::string session_authz_id;
  std::string session_authz_reauth_token;
  ErrorCode error_code;
  std::optional<SessionPolicies> effective_session_policies;
};

}  // namespace remoting::internal

#endif  // REMOTING_PROTO_LOGGING_SERVICE_H_
