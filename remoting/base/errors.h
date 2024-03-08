// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ERRORS_H_
#define REMOTING_BASE_ERRORS_H_

#include <string>

#include "remoting/proto/error_code.pb.h"

namespace remoting {

// The UI implementations maintain corresponding definitions of this
// enumeration in remoting/protocol/errors.cc and
// android/java/src/org/chromium/chromoting/jni/ConnectionListener.java
// Be sure to update these locations if you make any changes to the ordering.
enum class ErrorCode {
  OK = 0,
  PEER_IS_OFFLINE,
  SESSION_REJECTED,
  INCOMPATIBLE_PROTOCOL,
  AUTHENTICATION_FAILED,
  INVALID_ACCOUNT,
  CHANNEL_CONNECTION_ERROR,
  SIGNALING_ERROR,
  SIGNALING_TIMEOUT,
  HOST_OVERLOAD,
  MAX_SESSION_LENGTH,
  HOST_CONFIGURATION_ERROR,
  UNKNOWN_ERROR,
  ELEVATION_ERROR,
  HOST_CERTIFICATE_ERROR,
  HOST_REGISTRATION_ERROR,
  EXISTING_ADMIN_SESSION,
  AUTHZ_POLICY_CHECK_FAILED,
  DISALLOWED_BY_POLICY,
  LOCATION_AUTHZ_POLICY_CHECK_FAILED,
  UNAUTHORIZED_ACCOUNT,
  ERROR_CODE_MAX = UNAUTHORIZED_ACCOUNT,
};

bool ParseErrorCode(const std::string& name, ErrorCode* result);

// Returns the literal string of |error|.
const char* ErrorCodeToString(ErrorCode error);

// Converts a protocol ErrorCode to the protobuf ErrorCode.
proto::ErrorCode ErrorCodeToProtoEnum(ErrorCode error);

}  // namespace remoting

#endif  // REMOTING_BASE_ERRORS_H_
