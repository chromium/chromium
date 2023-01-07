// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "dbus/message.h"

namespace bluez {

namespace {

// This enum is tied directly to a UMA enum defined in
// tools/metrics/histograms/enums.xml, existing entries should not be modified.
enum class DBusResult {
  kSuccess = 0,
  // The following three are all different types of timeouts reported by the
  // DBus service:
  // "No reply to a message expecting one, usually means a timeout occurred."
  kErrorNoReply = 1,
  // "Certain timeout errors, possibly ETIMEDOUT on a socket."
  kErrorTimeout = 2,
  // "Certain timeout errors, e.g. while starting a service"
  kErrorTimedOut = 3,

  kErrorNotSupported = 4,
  kErrorAccessDenied = 5,
  kErrorDisconnected = 6,
  kErrorResponseMissing = 7,
  kErrorUnknown = 8,
  kMaxValue = kErrorUnknown
};

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

  return DBusResult::kErrorUnknown;
}

}  // namespace

void RecordSuccess(const std::string& method_name, base::Time start_time) {
  base::TimeDelta latency = base::Time::Now() - start_time;
  std::string result_metric =
      base::StringPrintf(kResultMetric, method_name.c_str());
  std::string latency_metric =
      base::StringPrintf(kLatencyMetric, method_name.c_str());

  base::UmaHistogramMediumTimes(latency_metric, latency);
  base::UmaHistogramEnumeration(result_metric, DBusResult::kSuccess);
}

void RecordFailure(const std::string& method_name,
                   dbus::ErrorResponse* response) {
  std::string result_metric =
      base::StringPrintf(kResultMetric, method_name.c_str());
  DBusResult result = GetResult(response);

  base::UmaHistogramEnumeration(result_metric, result);
}

}  // namespace bluez
