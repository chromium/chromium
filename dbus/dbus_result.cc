// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/dbus_result.h"
#include "dbus/message.h"

namespace dbus {

DBusResult GetResult(dbus::ErrorResponse* response) {
  if (!response) {
    return DBusResult::kErrorResponseMissing;
  }

  const std::string& error_name = response->GetErrorName();
  if (error_name == DBUS_ERROR_NO_REPLY) {
    return DBusResult::kErrorNoReply;
  }

  if (error_name == DBUS_ERROR_TIMEOUT) {
    return DBusResult::kErrorTimeout;
  }

  if (error_name == DBUS_ERROR_TIMED_OUT) {
    return DBusResult::kErrorTimedOut;
  }

  if (error_name == DBUS_ERROR_NOT_SUPPORTED) {
    return DBusResult::kErrorNotSupported;
  }

  if (error_name == DBUS_ERROR_ACCESS_DENIED) {
    return DBusResult::kErrorAccessDenied;
  }

  if (error_name == DBUS_ERROR_DISCONNECTED) {
    return DBusResult::kErrorDisconnected;
  }

  if (error_name == DBUS_ERROR_FAILED) {
    return DBusResult::kErrorFailed;
  }

  if (error_name == DBUS_ERROR_NO_MEMORY) {
    return DBusResult::kErrorNoMemory;
  }

  if (error_name == DBUS_ERROR_SERVICE_UNKNOWN) {
    return DBusResult::kErrorServiceUnknown;
  }

  if (error_name == DBUS_ERROR_NAME_HAS_NO_OWNER) {
    return DBusResult::kErrorNameHasNoOwner;
  }

  if (error_name == DBUS_ERROR_IO_ERROR) {
    return DBusResult::kErrorIOError;
  }

  if (error_name == DBUS_ERROR_BAD_ADDRESS) {
    return DBusResult::kErrorBadAddress;
  }

  if (error_name == DBUS_ERROR_LIMITS_EXCEEDED) {
    return DBusResult::kErrorLimitsExceeded;
  }

  if (error_name == DBUS_ERROR_AUTH_FAILED) {
    return DBusResult::kErrorAuthFailed;
  }

  if (error_name == DBUS_ERROR_NO_SERVER) {
    return DBusResult::kErrorNoServer;
  }

  if (error_name == DBUS_ERROR_NO_NETWORK) {
    return DBusResult::kErrorNoNetwork;
  }

  if (error_name == DBUS_ERROR_ADDRESS_IN_USE) {
    return DBusResult::kErrorAddressInUse;
  }

  if (error_name == DBUS_ERROR_DISCONNECTED) {
    return DBusResult::kErrorDisconnected;
  }

  if (error_name == DBUS_ERROR_INVALID_ARGS) {
    return DBusResult::kErrorInvalidArgs;
  }

  if (error_name == DBUS_ERROR_FILE_NOT_FOUND) {
    return DBusResult::kErrorFileNotFound;
  }

  if (error_name == DBUS_ERROR_FILE_EXISTS) {
    return DBusResult::kErrorFileExists;
  }

  if (error_name == DBUS_ERROR_UNKNOWN_METHOD) {
    return DBusResult::kErrorUnknownMethod;
  }

  if (error_name == DBUS_ERROR_UNKNOWN_OBJECT) {
    return DBusResult::kErrorUnknownObject;
  }

  if (error_name == DBUS_ERROR_UNKNOWN_INTERFACE) {
    return DBusResult::kErrorUnknownInterface;
  }

  if (error_name == DBUS_ERROR_UNKNOWN_PROPERTY) {
    return DBusResult::kErrorUnknownProperty;
  }

  if (error_name == DBUS_ERROR_PROPERTY_READ_ONLY) {
    return DBusResult::kErrorPropertyReadOnly;
  }

  if (error_name == DBUS_ERROR_MATCH_RULE_NOT_FOUND) {
    return DBusResult::kErrorMatchRuleNotFound;
  }

  if (error_name == DBUS_ERROR_MATCH_RULE_INVALID) {
    return DBusResult::kErrorMatchRuleInvalid;
  }

  if (error_name == DBUS_ERROR_SPAWN_EXEC_FAILED) {
    return DBusResult::kErrorSpawnExecFailed;
  }

  if (error_name == DBUS_ERROR_SPAWN_FORK_FAILED) {
    return DBusResult::kErrorSpawnForkFailed;
  }

  if (error_name == DBUS_ERROR_SPAWN_CHILD_EXITED) {
    return DBusResult::kErrorSpawnChildExited;
  }

  if (error_name == DBUS_ERROR_SPAWN_CHILD_SIGNALED) {
    return DBusResult::kErrorSpawnChildSignaled;
  }

  if (error_name == DBUS_ERROR_SPAWN_FAILED) {
    return DBusResult::kErrorSpawnFailed;
  }

  if (error_name == DBUS_ERROR_SPAWN_SETUP_FAILED) {
    return DBusResult::kErrorSpawnSetupFailed;
  }

  if (error_name == DBUS_ERROR_SPAWN_CONFIG_INVALID) {
    return DBusResult::kErrorSpawnConfigInvalid;
  }

  if (error_name == DBUS_ERROR_SPAWN_SERVICE_INVALID) {
    return DBusResult::kErrorSpawnServiceInvalid;
  }

  if (error_name == DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND) {
    return DBusResult::kErrorSpawnServiceNotFound;
  }

  if (error_name == DBUS_ERROR_SPAWN_PERMISSIONS_INVALID) {
    return DBusResult::kErrorSpawnPermissionsInvalid;
  }

  if (error_name == DBUS_ERROR_SPAWN_FILE_INVALID) {
    return DBusResult::kErrorSpawnFileInvalid;
  }

  if (error_name == DBUS_ERROR_SPAWN_NO_MEMORY) {
    return DBusResult::kErrorSpawnNoMemory;
  }

  if (error_name == DBUS_ERROR_UNIX_PROCESS_ID_UNKNOWN) {
    return DBusResult::kErrorUnixProcessIDUnknown;
  }

  if (error_name == DBUS_ERROR_INVALID_SIGNATURE) {
    return DBusResult::kErrorInvalidSignature;
  }

  if (error_name == DBUS_ERROR_INVALID_FILE_CONTENT) {
    return DBusResult::kErrorInvalidFileContent;
  }

  if (error_name == DBUS_ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN) {
    return DBusResult::kErrorSELinuxSecurityContextUnknown;
  }

  if (error_name == DBUS_ERROR_ADT_AUDIT_DATA_UNKNOWN) {
    return DBusResult::kErrorAdtAuditDataUnknown;
  }

  if (error_name == DBUS_ERROR_OBJECT_PATH_IN_USE) {
    return DBusResult::kErrorObjectPathInUse;
  }

  if (error_name == DBUS_ERROR_INCONSISTENT_MESSAGE) {
    return DBusResult::kErrorInconsistentMessage;
  }

  if (error_name == DBUS_ERROR_INTERACTIVE_AUTHORIZATION_REQUIRED) {
    return DBusResult::kErrorInteractiveAuthorizationRequired;
  }

  return DBusResult::kErrorUnknown;
}

}  // namespace dbus
