// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/errors.h"

#include "remoting/base/name_value_map.h"
#include "remoting/proto/error_code.pb.h"

namespace remoting {

namespace {

const NameMapElement<ErrorCode> kErrorCodeNames[] = {
    {ErrorCode::OK, "OK"},
    {ErrorCode::PEER_IS_OFFLINE, "PEER_IS_OFFLINE"},
    {ErrorCode::SESSION_REJECTED, "SESSION_REJECTED"},
    {ErrorCode::INCOMPATIBLE_PROTOCOL, "INCOMPATIBLE_PROTOCOL"},
    {ErrorCode::AUTHENTICATION_FAILED, "AUTHENTICATION_FAILED"},
    {ErrorCode::INVALID_ACCOUNT, "INVALID_ACCOUNT"},
    {ErrorCode::CHANNEL_CONNECTION_ERROR, "CHANNEL_CONNECTION_ERROR"},
    {ErrorCode::SIGNALING_ERROR, "SIGNALING_ERROR"},
    {ErrorCode::SIGNALING_TIMEOUT, "SIGNALING_TIMEOUT"},
    {ErrorCode::HOST_OVERLOAD, "HOST_OVERLOAD"},
    {ErrorCode::MAX_SESSION_LENGTH, "MAX_SESSION_LENGTH"},
    {ErrorCode::HOST_CONFIGURATION_ERROR, "HOST_CONFIGURATION_ERROR"},
    {ErrorCode::ELEVATION_ERROR, "ELEVATION_ERROR"},
    {ErrorCode::HOST_CERTIFICATE_ERROR, "HOST_CERTIFICATE_ERROR"},
    {ErrorCode::HOST_REGISTRATION_ERROR, "HOST_REGISTRATION_ERROR"},
    {ErrorCode::UNKNOWN_ERROR, "UNKNOWN_ERROR"},
    {ErrorCode::EXISTING_ADMIN_SESSION, "EXISTING_ADMIN_SESSION"},
    {ErrorCode::AUTHZ_POLICY_CHECK_FAILED, "AUTHZ_POLICY_CHECK_FAILED"},
    {ErrorCode::DISALLOWED_BY_POLICY, "DISALLOWED_BY_POLICY"},
    {ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED,
     "LOCATION_AUTHZ_POLICY_CHECK_FAILED"},
    {ErrorCode::UNAUTHORIZED_ACCOUNT, "UNAUTHORIZED_ACCOUNT"},
    {ErrorCode::REAUTHZ_POLICY_CHECK_FAILED, "REAUTHZ_POLICY_CHECK_FAILED"},
    {ErrorCode::NO_COMMON_AUTH_METHOD, "NO_COMMON_AUTH_METHOD"},
    {ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED, "LOGIN_SCREEN_NOT_SUPPORTED"},
    {ErrorCode::SESSION_POLICIES_CHANGED, "SESSION_POLICIES_CHANGED"},
};

}  // namespace

const char* ErrorCodeToString(ErrorCode error) {
  return ValueToName(kErrorCodeNames, error);
}

bool ParseErrorCode(const std::string& name, ErrorCode* result) {
  return NameToValue(kErrorCodeNames, name, result);
}

proto::ErrorCode ErrorCodeToProtoEnum(ErrorCode error) {
  switch (error) {
    case ErrorCode::OK:
      return proto::ErrorCode::NONE;
    case ErrorCode::PEER_IS_OFFLINE:
      return proto::ErrorCode::CLIENT_IS_OFFLINE;
    case ErrorCode::SESSION_REJECTED:
      return proto::ErrorCode::SESSION_REJECTED;
    case ErrorCode::INCOMPATIBLE_PROTOCOL:
      return proto::ErrorCode::INCOMPATIBLE_PROTOCOL;
    case ErrorCode::AUTHENTICATION_FAILED:
      return proto::ErrorCode::AUTHENTICATION_FAILED;
    case ErrorCode::INVALID_ACCOUNT:
      return proto::ErrorCode::INVALID_ACCOUNT;
    case ErrorCode::CHANNEL_CONNECTION_ERROR:
      return proto::ErrorCode::P2P_FAILURE;
    case ErrorCode::SIGNALING_ERROR:
      return proto::ErrorCode::SIGNALING_ERROR;
    case ErrorCode::SIGNALING_TIMEOUT:
      return proto::ErrorCode::SIGNALING_TIMEOUT;
    case ErrorCode::HOST_OVERLOAD:
      return proto::ErrorCode::HOST_OVERLOAD;
    case ErrorCode::MAX_SESSION_LENGTH:
      return proto::ErrorCode::MAX_SESSION_LENGTH;
    case ErrorCode::HOST_CONFIGURATION_ERROR:
      return proto::ErrorCode::HOST_CONFIGURATION_ERROR;
    case ErrorCode::UNKNOWN_ERROR:
      return proto::ErrorCode::UNEXPECTED;
    case ErrorCode::ELEVATION_ERROR:
      return proto::ErrorCode::ELEVATION_ERROR;
    case ErrorCode::HOST_CERTIFICATE_ERROR:
      return proto::ErrorCode::HOST_CERTIFICATE_ERROR;
    case ErrorCode::HOST_REGISTRATION_ERROR:
      return proto::ErrorCode::HOST_REGISTRATION_ERROR;
    case ErrorCode::EXISTING_ADMIN_SESSION:
      return proto::ErrorCode::EXISTING_ADMIN_SESSION;
    case ErrorCode::AUTHZ_POLICY_CHECK_FAILED:
      return proto::ErrorCode::AUTHZ_POLICY_CHECK_FAILED;
    case ErrorCode::DISALLOWED_BY_POLICY:
      return proto::ErrorCode::DISALLOWED_BY_POLICY;
    case ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED:
      return proto::ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED;
    case ErrorCode::UNAUTHORIZED_ACCOUNT:
      return proto::ErrorCode::UNAUTHORIZED_ACCOUNT;
    case ErrorCode::REAUTHZ_POLICY_CHECK_FAILED:
      return proto::ErrorCode::REAUTHORIZATION_FAILED;
    case ErrorCode::NO_COMMON_AUTH_METHOD:
      return proto::ErrorCode::NO_COMMON_AUTH_METHOD;
    case ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED:
      return proto::ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED;
    case ErrorCode::SESSION_POLICIES_CHANGED:
      return proto::ErrorCode::SESSION_POLICIES_CHANGED;
  }
}

}  // namespace remoting
