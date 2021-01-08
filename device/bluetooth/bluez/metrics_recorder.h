// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_METRICS_RECORDER_H_
#define DEVICE_BLUETOOTH_BLUEZ_METRICS_RECORDER_H_

#include "base/optional.h"

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

// Returns the ConnectToServiceInsecurelyResult type associated with
// |error_string|, or null if no result could be found.
base::Optional<ConnectToServiceInsecurelyResult> ExtractResultFromErrorString(
    const std::string& error_string);

void RecordConnectToServiceInsecurelyResult(
    ConnectToServiceInsecurelyResult result);

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_BLUEZ_METRICS_RECORDER_H_
