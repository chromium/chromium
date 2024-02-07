// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/metrics_recorder.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"

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

// Note: These values are sourced from the "BR/EDR connection failure reasons"
// in BlueZ's error.h file, and should be kept in sync.
const char kBlueZConnectionAlreadyConnected[] =
    "br-connection-already-connected";
const char kBlueZPageTimeout[] = "br-connection-page-timeout";
const char kBlueZProfileUnavailable[] = "br-connection-profile-unavailable";
const char kBlueZSdpSearch[] = "br-connection-sdp-search";
const char kBlueZCreateSocket[] = "br-connection-create-socket";
const char kBlueZInvalidArgument[] = "br-connection-invalid-argument";
const char kBlueZAdapterNotPowered[] = "br-connection-adapter-not-powered";
const char kBlueZNotSupported[] = "br-connection-not-supported";
const char kBlueZBadSocket[] = "br-connection-bad-socket";
const char kBlueZMemoryAllocation[] = "br-connection-memory-allocation";
const char kBlueZBusy[] = "br-connection-busy";
const char kBlueZConcurrentConnectionLimit[] =
    "br-connection-concurrent-connection-limit";
const char kBlueZTimeout[] = "br-connection-timeout";
const char kBlueZRefused[] = "br-connection-refused";
const char kBlueZAbortedByRemote[] = "br-connection-aborted-by-remote";
const char kBlueZAbortedByLocal[] = "br-connection-aborted-by-local";
const char kBlueZLmpProtocolError[] = "br-connection-lmp-protocol-error";
const char kBlueZCanceled[] = "br-connection-canceled";
const char kBlueZUnknown[] = "br-connection-unknown";

}  // namespace

std::optional<ConnectToServiceInsecurelyResult> ExtractResultFromErrorString(
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

  return std::nullopt;
}

ConnectToServiceFailureReason ExtractFailureReasonFromErrorString(
    const std::string& error_string) {
  if (base::Contains(error_string, kBlueZConnectionAlreadyConnected))
    return ConnectToServiceFailureReason::kReasonConnectionAlreadyConnected;

  if (base::Contains(error_string, kBlueZPageTimeout))
    return ConnectToServiceFailureReason::kReasonPageTimeout;

  if (base::Contains(error_string, kBlueZProfileUnavailable))
    return ConnectToServiceFailureReason::kReasonProfileUnavailable;

  if (base::Contains(error_string, kBlueZSdpSearch))
    return ConnectToServiceFailureReason::kReasonSdpSearch;

  if (base::Contains(error_string, kBlueZCreateSocket))
    return ConnectToServiceFailureReason::kReasonCreateSocket;

  if (base::Contains(error_string, kBlueZInvalidArgument))
    return ConnectToServiceFailureReason::kReasonInvalidArgument;

  if (base::Contains(error_string, kBlueZAdapterNotPowered))
    return ConnectToServiceFailureReason::kReasonAdapterNotPowered;

  if (base::Contains(error_string, kBlueZNotSupported))
    return ConnectToServiceFailureReason::kReasonNotSupported;

  if (base::Contains(error_string, kBlueZBadSocket))
    return ConnectToServiceFailureReason::kReasonBadSocket;

  if (base::Contains(error_string, kBlueZMemoryAllocation))
    return ConnectToServiceFailureReason::kReasonMemoryAllocation;

  if (base::Contains(error_string, kBlueZBusy))
    return ConnectToServiceFailureReason::kReasonBusy;

  if (base::Contains(error_string, kBlueZConcurrentConnectionLimit))
    return ConnectToServiceFailureReason::kReasonConcurrentConnectionLimit;

  if (base::Contains(error_string, kBlueZTimeout))
    return ConnectToServiceFailureReason::kReasonTimeout;

  if (base::Contains(error_string, kBlueZRefused))
    return ConnectToServiceFailureReason::kReasonRefused;

  if (base::Contains(error_string, kBlueZAbortedByRemote))
    return ConnectToServiceFailureReason::kReasonAbortedByRemote;

  if (base::Contains(error_string, kBlueZAbortedByLocal))
    return ConnectToServiceFailureReason::kReasonAbortedByLocal;

  if (base::Contains(error_string, kBlueZLmpProtocolError))
    return ConnectToServiceFailureReason::kReasonLmpProtocolError;

  if (base::Contains(error_string, kBlueZCanceled))
    return ConnectToServiceFailureReason::kReasonCanceled;

  if (base::Contains(error_string, kBlueZUnknown))
    return ConnectToServiceFailureReason::kReasonUnknown;

  return ConnectToServiceFailureReason::kReasonUnknown;
}

void RecordConnectToServiceInsecurelyResult(
    ConnectToServiceInsecurelyResult result) {
  base::UmaHistogramEnumeration(
      "Bluetooth.Linux.ConnectToServiceInsecurelyResult", result);
}

void RecordConnectToServiceFailureReason(ConnectToServiceFailureReason reason) {
  base::UmaHistogramEnumeration(
      "Bluetooth.Linux.ConnectToService.FailureReason", reason);
}

void RecordBondedConnectToServiceFailureReason(
    ConnectToServiceFailureReason reason) {
  base::UmaHistogramEnumeration(
      "Bluetooth.Linux.ConnectToService.Bonded.FailureReason", reason);
}

}  // namespace bluetooth
