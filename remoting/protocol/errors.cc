// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/errors.h"

#include "remoting/base/name_value_map.h"

namespace remoting::protocol {

namespace {

const NameMapElement<ErrorCode> kErrorCodeNames[] = {
    {OK, "OK"},
    {PEER_IS_OFFLINE, "PEER_IS_OFFLINE"},
    {SESSION_REJECTED, "SESSION_REJECTED"},
    {INCOMPATIBLE_PROTOCOL, "INCOMPATIBLE_PROTOCOL"},
    {AUTHENTICATION_FAILED, "AUTHENTICATION_FAILED"},
    {INVALID_ACCOUNT, "INVALID_ACCOUNT"},
    {CHANNEL_CONNECTION_ERROR, "CHANNEL_CONNECTION_ERROR"},
    {SIGNALING_ERROR, "SIGNALING_ERROR"},
    {SIGNALING_TIMEOUT, "SIGNALING_TIMEOUT"},
    {HOST_OVERLOAD, "HOST_OVERLOAD"},
    {MAX_SESSION_LENGTH, "MAX_SESSION_LENGTH"},
    {HOST_CONFIGURATION_ERROR, "HOST_CONFIGURATION_ERROR"},
    {ELEVATION_ERROR, "ELEVATION_ERROR"},
    {HOST_CERTIFICATE_ERROR, "HOST_CERTIFICATE_ERROR"},
    {HOST_REGISTRATION_ERROR, "HOST_REGISTRATION_ERROR"},
    {UNKNOWN_ERROR, "UNKNOWN_ERROR"},
    {EXISTING_ADMIN_SESSION, "EXISTING_ADMIN_SESSION"},
    {AUTHZ_POLICY_CHECK_FAILED, "AUTHZ_POLICY_CHECK_FAILED"},
    {DISALLOWED_BY_POLICY, "DISALLOWED_BY_POLICY"},
    {LOCATION_AUTHZ_POLICY_CHECK_FAILED, "LOCATION_AUTHZ_POLICY_CHECK_FAILED"},
    {UNAUTHORIZED_ACCOUNT, "UNAUTHORIZED_ACCOUNT"},
};

}  // namespace

const char* ErrorCodeToString(ErrorCode error) {
  return ValueToName(kErrorCodeNames, error);
}

bool ParseErrorCode(const std::string& name, ErrorCode* result) {
  return NameToValue(kErrorCodeNames, name, result);
}

}  // namespace remoting::protocol
