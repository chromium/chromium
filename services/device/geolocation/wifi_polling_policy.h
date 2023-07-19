// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_POLLING_POLICY_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_POLLING_POLICY_H_

#include <memory>

#include "base/check.h"
#include "base/time/time.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"

namespace device {

// Allows sharing and mocking of the update polling policy function.
class WifiPollingPolicy {
 public:
  WifiPollingPolicy(const WifiPollingPolicy&) = delete;
  WifiPollingPolicy& operator=(const WifiPollingPolicy&) = delete;

  virtual ~WifiPollingPolicy() = default;

  // Methods for managing the single instance of WifiPollingPolicy. The WiFi
  // policy is global so it can outlive the WifiDataProvider instance, which is
  // shut down and destroyed when no WiFi scanning is active.
  static void Initialize(std::unique_ptr<WifiPollingPolicy>);
  static void Shutdown();
  static WifiPollingPolicy* Get();
  static bool IsInitialized();

  // Calculates the new polling interval for wifi scans, given the previous
  // interval and whether the last scan produced new results.
  virtual void UpdatePollingInterval(bool scan_results_differ) = 0;

  // Use InitialInterval to schedule the initial scan when the wifi data
  // provider is first started. Returns the number of milliseconds before the
  // initial scan should be performed. May return zero if the policy allows a
  // scan to be performed immediately.
  virtual int InitialInterval() = 0;

  // Use PollingInterval to schedule a new scan after the previous scan results
  // are available. Only use PollingInterval if WLAN hardware is available and
  // can perform scans for nearby access points. If the current interval is
  // complete, PollingInterval returns the duration for a new interval starting
  // at the current time.
  virtual int PollingInterval() = 0;

  // Use NoWifiInterval to schedule a new scan after the previous scan results
  // are available. NoWifiInterval is typically shorter than PollingInterval
  // and should not be used if wifi scanning is available in order to conserve
  // power. If the current interval is complete, NoWifiInterval returns the
  // duration for a new interval starting at the current time.
  virtual int NoWifiInterval() = 0;

  // Use FillDiagnostics to fill diagnostic info for WifiPollingPolicy.
  virtual void FillDiagnostics(
      mojom::WifiPollingPolicyDiagnostics& diagnostics) = 0;

 protected:
  WifiPollingPolicy() = default;
};

// Generic polling policy, constants are compile-time parameterized to allow
// tuning on a per-platform basis.
template <int DEFAULT_INTERVAL,
          int NO_CHANGE_INTERVAL,
          int TWO_NO_CHANGE_INTERVAL,
          int NO_WIFI_INTERVAL>
class GenericWifiPollingPolicy : public WifiPollingPolicy {
 public:
  GenericWifiPollingPolicy() = default;

  // WifiPollingPolicy
  void UpdatePollingInterval(bool scan_results_differ) override {
    if (scan_results_differ) {
      polling_interval_ = DEFAULT_INTERVAL;
    } else if (polling_interval_ == DEFAULT_INTERVAL) {
      polling_interval_ = NO_CHANGE_INTERVAL;
    } else {
      DCHECK(polling_interval_ == NO_CHANGE_INTERVAL ||
             polling_interval_ == TWO_NO_CHANGE_INTERVAL);
      polling_interval_ = TWO_NO_CHANGE_INTERVAL;
    }
  }
  int InitialInterval() override { return ComputeInterval(polling_interval_); }
  int PollingInterval() override {
    int interval = ComputeInterval(polling_interval_);
    return interval <= 0 ? polling_interval_ : interval;
  }
  int NoWifiInterval() override {
    int interval = ComputeInterval(NO_WIFI_INTERVAL);
    return interval <= 0 ? NO_WIFI_INTERVAL : interval;
  }
  void FillDiagnostics(
      mojom::WifiPollingPolicyDiagnostics& diagnostics) override {
    diagnostics.interval_start = interval_start_;
    diagnostics.interval_duration = base::Milliseconds(interval_duration_);
    diagnostics.polling_interval = base::Milliseconds(polling_interval_);
    diagnostics.default_interval = base::Milliseconds(DEFAULT_INTERVAL);
    diagnostics.no_change_interval = base::Milliseconds(NO_CHANGE_INTERVAL);
    diagnostics.two_no_change_interval =
        base::Milliseconds(TWO_NO_CHANGE_INTERVAL);
    diagnostics.no_wifi_interval = base::Milliseconds(NO_WIFI_INTERVAL);
    return;
  }

 private:
  int ComputeInterval(int polling_interval) {
    base::Time now = base::Time::Now();

    int64_t remaining_millis = 0;
    if (!interval_start_.is_null()) {
      // If the new interval duration differs from the initial duration, use the
      // shorter duration.
      if (polling_interval < interval_duration_)
        interval_duration_ = polling_interval;

      // Compute the remaining duration of the current interval. If the interval
      // is not yet complete, we will schedule a scan to occur once it is.
      base::TimeDelta remaining =
          interval_start_ + base::Milliseconds(interval_duration_) - now;
      remaining_millis = remaining.InMilliseconds();
    }

    // If the current interval is complete (or if this is our first scan),
    // start a new interval beginning now.
    if (remaining_millis <= 0) {
      interval_start_ = now;
      interval_duration_ = polling_interval;
      remaining_millis = 0;
    }

    return remaining_millis;
  }

  // The current duration of the polling interval. When wifi data is
  // substantially the same from one scan to the next, this may be increased to
  // reduce the frequency of wifi scanning.
  int polling_interval_ = DEFAULT_INTERVAL;

  // The start time for the most recent interval. Initialized to the "null" time
  // value.
  base::Time interval_start_;

  // Duration for the interval starting at |interval_start_|.
  int interval_duration_ = DEFAULT_INTERVAL;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_POLLING_POLICY_H_
