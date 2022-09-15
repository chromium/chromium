// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ERRORS_H_
#define REMOTING_PROTOCOL_ERRORS_H_

#include <string>

namespace remoting::protocol {

// The UI implementations maintain corresponding definitions of this
// enumeration in remoting/protocol/errors.cc and
// android/java/src/org/chromium/chromoting/jni/ConnectionListener.java
// Be sure to update these locations if you make any changes to the ordering.
enum ErrorCode {
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
  ERROR_CODE_MAX = LOCATION_AUTHZ_POLICY_CHECK_FAILED,
};

bool ParseErrorCode(const std::string& name, ErrorCode* result);

// Returns the literal string of |error|.
const char* ErrorCodeToString(ErrorCode error);

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_ERRORS_H_
