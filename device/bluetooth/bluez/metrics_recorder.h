// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_METRICS_RECORDER_H_
#define DEVICE_BLUETOOTH_BLUEZ_METRICS_RECORDER_H_

#include <optional>
#include <string>

namespace bluetooth {

// Result types for ConnectToServiceInsecurely(). Numerical values are used for
// metrics and should not be changed or reused.
enum class ConnectToServiceInsecurelyResult {
  kSuccess = 0,
  kInvalidArgumentsError = 1,
  kInProgressError = 2,
  kAlreadyExistsError = 3,
  kNotSupportedError = 4,
  kNotConnectedError = 5,
  kAlreadyConnectedError = 6,
  kNotAvailableError = 7,
  kDoesNotExistError = 8,
  kNotAuthorizedError = 9,
  kNotPermittedError = 10,
  kNoSuchAdapterError = 11,
  kAgentNotAvailableError = 12,
  kNotReadyError = 13,
  kFailedError = 14,
  kMojoReceivingPipeError = 15,
  kMojoSendingPipeError = 16,
  kCouldNotConnectError = 17,
  kUnknownError = 18,
  kMaxValue = kUnknownError
};

// Failure reasons for connection failures. Numerical values are used for
// metrics and should not be changed or reused.
enum class ConnectToServiceFailureReason {
  kReasonConnectionAlreadyConnected = 0,
  kReasonPageTimeout = 1,
  kReasonProfileUnavailable = 2,
  kReasonSdpSearch = 3,
  kReasonCreateSocket = 4,
  kReasonInvalidArgument = 5,
  kReasonAdapterNotPowered = 6,
  kReasonNotSupported = 7,
  kReasonBadSocket = 8,
  kReasonMemoryAllocation = 9,
  kReasonBusy = 10,
  kReasonConcurrentConnectionLimit = 11,
  kReasonTimeout = 12,
  kReasonRefused = 13,
  kReasonAbortedByRemote = 14,
  kReasonAbortedByLocal = 15,
  kReasonLmpProtocolError = 16,
  kReasonCanceled = 17,
  kReasonUnknown = 18,
  kMaxValue = kReasonUnknown
};

// Returns the ConnectToServiceInsecurelyResult type associated with
// |error_string|, or null if no result could be found.
std::optional<ConnectToServiceInsecurelyResult> ExtractResultFromErrorString(
    const std::string& error_string);

// Returns the ConnectToServiceFailureReason type associated with
// |error_string|. Returns |kReasonUnknown| if the error is not recognized.
ConnectToServiceFailureReason ExtractFailureReasonFromErrorString(
    const std::string& error_string);

void RecordConnectToServiceInsecurelyResult(
    ConnectToServiceInsecurelyResult result);

void RecordConnectToServiceFailureReason(ConnectToServiceFailureReason reason);

// Records a specific scenario in which we fail to connect to a remote device
// that is considered to be bonded.
void RecordBondedConnectToServiceFailureReason(
    ConnectToServiceFailureReason reason);

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_BLUEZ_METRICS_RECORDER_H_
