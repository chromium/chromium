// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_DBUS_RESULT_H_
#define DBUS_DBUS_RESULT_H_

#include "base/functional/callback.h"
#include "dbus/dbus_export.h"
#include "dbus/message.h"

namespace dbus {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DBusResult {
  kSuccess = 0,
  kErrorNoReply = 1,
  kErrorTimeout = 2,
  kErrorTimedOut = 3,
  kErrorNotSupported = 4,
  kErrorAccessDenied = 5,
  kErrorDisconnected = 6,
  kErrorResponseMissing = 7,
  kErrorUnknown = 8,
  kErrorFailed = 9,
  kErrorNoMemory = 10,
  kErrorServiceUnknown = 11,
  kErrorNameHasNoOwner = 12,
  kErrorIOError = 13,
  kErrorBadAddress = 14,
  kErrorLimitsExceeded = 15,
  kErrorAuthFailed = 16,
  kErrorNoServer = 17,
  kErrorNoNetwork = 18,
  kErrorAddressInUse = 19,
  kErrorInvalidArgs = 20,
  kErrorFileNotFound = 21,
  kErrorFileExists = 22,
  kErrorUnknownMethod = 23,
  kErrorUnknownObject = 24,
  kErrorUnknownInterface = 25,
  kErrorUnknownProperty = 26,
  kErrorPropertyReadOnly = 27,
  kErrorMatchRuleNotFound = 28,
  kErrorMatchRuleInvalid = 29,
  kErrorSpawnExecFailed = 30,
  kErrorSpawnForkFailed = 31,
  kErrorSpawnChildExited = 32,
  kErrorSpawnChildSignaled = 33,
  kErrorSpawnFailed = 34,
  kErrorSpawnSetupFailed = 35,
  kErrorSpawnConfigInvalid = 36,
  kErrorSpawnServiceInvalid = 37,
  kErrorSpawnServiceNotFound = 38,
  kErrorSpawnPermissionsInvalid = 39,
  kErrorSpawnFileInvalid = 40,
  kErrorSpawnNoMemory = 41,
  kErrorUnixProcessIDUnknown = 42,
  kErrorInvalidSignature = 43,
  kErrorInvalidFileContent = 44,
  kErrorSELinuxSecurityContextUnknown = 45,
  kErrorAdtAuditDataUnknown = 46,
  kErrorObjectPathInUse = 47,
  kErrorInconsistentMessage = 48,
  kErrorInteractiveAuthorizationRequired = 49,
  kErrorNotContainer = 50,
  kMaxValue = kErrorNotContainer
};

CHROME_DBUS_EXPORT DBusResult GetResult(dbus::ErrorResponse* response);

}  // namespace dbus

#endif  // DBUS_DBUS_RESULT_H_
