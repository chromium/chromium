// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"

namespace bluetooth {
namespace {

// Note: These values must stay in sync with BlueZ's error.c file.
const char kBlueZInvalidArgumentsError[] = "org.bluez.Error.InvalidArguments";
const char kBlueZInProgressError[] = "org.bluez.Error.InProgress";
const char kBlueZAlreadyExistsError[] = "org.bluez.Error.AlreadyExists";
const char kBlueZNotSupportedError[] = "org.bluez.Error.NotSupported";
const char kBlueZNotConnectedError[] = "org.bluez.Error.NotConnected";
const char kBlueZAlreadyConnectedError[] = "org.bluez.Error.AlreadyConnected";
const char kBlueZNotAvailableError[] = "org.bluez.Error.NotAvailable";
const char kBlueZDoesNotExistError[] = "org.bluez.Error.DoesNotExist";
const char kBlueZNotAuthorizedError[] = "org.bluez.Error.NotAuthorized";
const char kBlueZNotPermittedError[] = "org.bluez.Error.NotPermitted";
const char kBlueZNoSuchAdapterError[] = "org.bluez.Error.NoSuchAdapter";
const char kBlueZAgentNotAvailableError[] = "org.bluez.Error.AgentNotAvailable";
const char kBlueZNotReadyError[] = "org.bluez.Error.NotReady";
const char kBlueZFailedError[] = "org.bluez.Error.Failed";

}  // namespace

base::Optional<ConnectToServiceInsecurelyResult> ExtractResultFromErrorString(
    const std::string& error_string) {
  if (base::Contains(error_string, kBlueZInvalidArgumentsError))
    return ConnectToServiceInsecurelyResult::kInvalidArgumentsError;

  if (base::Contains(error_string, kBlueZInProgressError))
    return ConnectToServiceInsecurelyResult::kInProgressError;

  if (base::Contains(error_string, kBlueZAlreadyExistsError))
    return ConnectToServiceInsecurelyResult::kAlreadyExistsError;

  if (base::Contains(error_string, kBlueZNotSupportedError))
    return ConnectToServiceInsecurelyResult::kNotSupportedError;

  if (base::Contains(error_string, kBlueZNotConnectedError))
    return ConnectToServiceInsecurelyResult::kNotConnectedError;

  if (base::Contains(error_string, kBlueZAlreadyConnectedError))
    return ConnectToServiceInsecurelyResult::kAlreadyConnectedError;

  if (base::Contains(error_string, kBlueZNotAvailableError))
    return ConnectToServiceInsecurelyResult::kNotAvailableError;

  if (base::Contains(error_string, kBlueZDoesNotExistError))
    return ConnectToServiceInsecurelyResult::kDoesNotExistError;

  if (base::Contains(error_string, kBlueZNotAuthorizedError))
    return ConnectToServiceInsecurelyResult::kNotAuthorizedError;

  if (base::Contains(error_string, kBlueZNotPermittedError))
    return ConnectToServiceInsecurelyResult::kNotPermittedError;

  if (base::Contains(error_string, kBlueZNoSuchAdapterError))
    return ConnectToServiceInsecurelyResult::kNoSuchAdapterError;

  if (base::Contains(error_string, kBlueZAgentNotAvailableError))
    return ConnectToServiceInsecurelyResult::kAgentNotAvailableError;

  if (base::Contains(error_string, kBlueZNotReadyError))
    return ConnectToServiceInsecurelyResult::kNotReadyError;

  if (base::Contains(error_string, kBlueZFailedError))
    return ConnectToServiceInsecurelyResult::kFailedError;

  return base::nullopt;
}

void RecordConnectToServiceInsecurelyResult(
    ConnectToServiceInsecurelyResult result) {
  base::UmaHistogramEnumeration(
      "Bluetooth.Linux.ConnectToServiceInsecurelyResult", result);
}

}  // namespace bluetooth
