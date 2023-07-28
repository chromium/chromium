// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_metrics_helper.h"
#include "dbus/dbus_result.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "dbus/message.h"

namespace bluez {

void RecordSuccess(const std::string& method_name, base::Time start_time) {
  base::TimeDelta latency = base::Time::Now() - start_time;
  std::string result_metric =
      base::StringPrintf(kResultMetric, method_name.c_str());
  std::string latency_metric =
      base::StringPrintf(kLatencyMetric, method_name.c_str());

  base::UmaHistogramMediumTimes(latency_metric, latency);
  base::UmaHistogramEnumeration(result_metric, dbus::DBusResult::kSuccess);
}

void RecordFailure(const std::string& method_name,
                   dbus::ErrorResponse* response) {
  std::string result_metric =
      base::StringPrintf(kResultMetric, method_name.c_str());
  dbus::DBusResult result = dbus::GetResult(response);

  base::UmaHistogramEnumeration(result_metric, result);
}

}  // namespace bluez
