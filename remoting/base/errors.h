// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ERRORS_H_
#define REMOTING_BASE_ERRORS_H_

#include <string>

namespace remoting {

// Workaround a dependency issue with management_ui_handler_unittest.cc, which
// includes chrome/browser/ash without checking the platform.
namespace proto {
enum ErrorCode : int;
}  // namespace proto

// If this enum is modified, please also modify the enums in these file:
// * chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h:
//     ExtendedStartCrdSessionResultCode
// * chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.cc:
//     ToExtendedStartCrdSessionResultCode, ToStartCrdSessionResultCode
// * remoting/base/errors.cc: kErrorCodeNames
// * remoting/host/mojom/desktop_session.mojom: ProtocolErrorCode
// * remoting/host/mojom/remoting_mojom_traits.h:
//     EnumTraits<remoting::mojom::ProtocolErrorCode,
//                ::remoting::protocol::ErrorCode>
// * tools/metrics/histograms/metadata/enterprise/enums.xml:
//     EnterpriseCrdSessionResultCode
//
// DO NOT change existing values in this enum, since it would lead to version
// skew problems.
enum class ErrorCode {
  OK = 0,
  PEER_IS_OFFLINE = 1,
  SESSION_REJECTED = 2,
  INCOMPATIBLE_PROTOCOL = 3,
  AUTHENTICATION_FAILED = 4,
  INVALID_ACCOUNT = 5,
  CHANNEL_CONNECTION_ERROR = 6,
  SIGNALING_ERROR = 7,
  SIGNALING_TIMEOUT = 8,
  HOST_OVERLOAD = 9,
  MAX_SESSION_LENGTH = 10,
  HOST_CONFIGURATION_ERROR = 11,
  UNKNOWN_ERROR = 12,
  ELEVATION_ERROR = 13,
  HOST_CERTIFICATE_ERROR = 14,
  HOST_REGISTRATION_ERROR = 15,
  EXISTING_ADMIN_SESSION = 16,
  AUTHZ_POLICY_CHECK_FAILED = 17,
  DISALLOWED_BY_POLICY = 18,
  LOCATION_AUTHZ_POLICY_CHECK_FAILED = 19,
  UNAUTHORIZED_ACCOUNT = 20,
  REAUTHZ_POLICY_CHECK_FAILED = 21,
  NO_COMMON_AUTH_METHOD = 22,
  LOGIN_SCREEN_NOT_SUPPORTED = 23,
  SESSION_POLICIES_CHANGED = 24,
  ERROR_CODE_MAX = SESSION_POLICIES_CHANGED,
};

bool ParseErrorCode(const std::string& name, ErrorCode* result);

// Returns the literal string of |error|.
const char* ErrorCodeToString(ErrorCode error);

// Converts a protocol ErrorCode to the protobuf ErrorCode.
proto::ErrorCode ErrorCodeToProtoEnum(ErrorCode error);

}  // namespace remoting

#endif  // REMOTING_BASE_ERRORS_H_
