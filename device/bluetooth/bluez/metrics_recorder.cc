// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/metrics_recorder.h"

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
  if (error_string.contains(kBlueZInvalidArgumentsError)) {
    return ConnectToServiceInsecurelyResult::kInvalidArgumentsError;
  }

  if (error_string.contains(kBlueZInProgressError)) {
    return ConnectToServiceInsecurelyResult::kInProgressError;
  }

  if (error_string.contains(kBlueZAlreadyExistsError)) {
    return ConnectToServiceInsecurelyResult::kAlreadyExistsError;
  }

  if (error_string.contains(kBlueZNotSupportedError)) {
    return ConnectToServiceInsecurelyResult::kNotSupportedError;
  }

  if (error_string.contains(kBlueZNotConnectedError)) {
    return ConnectToServiceInsecurelyResult::kNotConnectedError;
  }

  if (error_string.contains(kBlueZAlreadyConnectedError)) {
    return ConnectToServiceInsecurelyResult::kAlreadyConnectedError;
  }

  if (error_string.contains(kBlueZNotAvailableError)) {
    return ConnectToServiceInsecurelyResult::kNotAvailableError;
  }

  if (error_string.contains(kBlueZDoesNotExistError)) {
    return ConnectToServiceInsecurelyResult::kDoesNotExistError;
  }

  if (error_string.contains(kBlueZNotAuthorizedError)) {
    return ConnectToServiceInsecurelyResult::kNotAuthorizedError;
  }

  if (error_string.contains(kBlueZNotPermittedError)) {
    return ConnectToServiceInsecurelyResult::kNotPermittedError;
  }

  if (error_string.contains(kBlueZNoSuchAdapterError)) {
    return ConnectToServiceInsecurelyResult::kNoSuchAdapterError;
  }

  if (error_string.contains(kBlueZAgentNotAvailableError)) {
    return ConnectToServiceInsecurelyResult::kAgentNotAvailableError;
  }

  if (error_string.contains(kBlueZNotReadyError)) {
    return ConnectToServiceInsecurelyResult::kNotReadyError;
  }

  if (error_string.contains(kBlueZFailedError)) {
    return ConnectToServiceInsecurelyResult::kFailedError;
  }

  return std::nullopt;
}

ConnectToServiceFailureReason ExtractFailureReasonFromErrorString(
    const std::string& error_string) {
  if (error_string.contains(kBlueZConnectionAlreadyConnected)) {
    return ConnectToServiceFailureReason::kReasonConnectionAlreadyConnected;
  }

  if (error_string.contains(kBlueZPageTimeout)) {
    return ConnectToServiceFailureReason::kReasonPageTimeout;
  }

  if (error_string.contains(kBlueZProfileUnavailable)) {
    return ConnectToServiceFailureReason::kReasonProfileUnavailable;
  }

  if (error_string.contains(kBlueZSdpSearch)) {
    return ConnectToServiceFailureReason::kReasonSdpSearch;
  }

  if (error_string.contains(kBlueZCreateSocket)) {
    return ConnectToServiceFailureReason::kReasonCreateSocket;
  }

  if (error_string.contains(kBlueZInvalidArgument)) {
    return ConnectToServiceFailureReason::kReasonInvalidArgument;
  }

  if (error_string.contains(kBlueZAdapterNotPowered)) {
    return ConnectToServiceFailureReason::kReasonAdapterNotPowered;
  }

  if (error_string.contains(kBlueZNotSupported)) {
    return ConnectToServiceFailureReason::kReasonNotSupported;
  }

  if (error_string.contains(kBlueZBadSocket)) {
    return ConnectToServiceFailureReason::kReasonBadSocket;
  }

  if (error_string.contains(kBlueZMemoryAllocation)) {
    return ConnectToServiceFailureReason::kReasonMemoryAllocation;
  }

  if (error_string.contains(kBlueZBusy)) {
    return ConnectToServiceFailureReason::kReasonBusy;
  }

  if (error_string.contains(kBlueZConcurrentConnectionLimit)) {
    return ConnectToServiceFailureReason::kReasonConcurrentConnectionLimit;
  }

  if (error_string.contains(kBlueZTimeout)) {
    return ConnectToServiceFailureReason::kReasonTimeout;
  }

  if (error_string.contains(kBlueZRefused)) {
    return ConnectToServiceFailureReason::kReasonRefused;
  }

  if (error_string.contains(kBlueZAbortedByRemote)) {
    return ConnectToServiceFailureReason::kReasonAbortedByRemote;
  }

  if (error_string.contains(kBlueZAbortedByLocal)) {
    return ConnectToServiceFailureReason::kReasonAbortedByLocal;
  }

  if (error_string.contains(kBlueZLmpProtocolError)) {
    return ConnectToServiceFailureReason::kReasonLmpProtocolError;
  }

  if (error_string.contains(kBlueZCanceled)) {
    return ConnectToServiceFailureReason::kReasonCanceled;
  }

  if (error_string.contains(kBlueZUnknown)) {
    return ConnectToServiceFailureReason::kReasonUnknown;
  }

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
