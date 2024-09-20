// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_POWER_MONITOR_TEST_IMPL_H_
#define CONTENT_SHELL_COMMON_POWER_MONITOR_TEST_IMPL_H_

#include "base/power_monitor/power_monitor.h"
#include "content/shell/common/power_monitor_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class PowerMonitorTestImpl : public base::PowerStateObserver,
                             public mojom::PowerMonitorTest {
 public:
  static void MakeSelfOwnedReceiver(
      mojo::PendingReceiver<mojom::PowerMonitorTest> receiver);

  PowerMonitorTestImpl();

  PowerMonitorTestImpl(const PowerMonitorTestImpl&) = delete;
  PowerMonitorTestImpl& operator=(const PowerMonitorTestImpl&) = delete;

  ~PowerMonitorTestImpl() override;

 private:
  // mojom::PowerMonitorTest:
  void QueryNextState(QueryNextStateCallback callback) override;

  // base::PowerStateObserver:
  void OnBatteryPowerStatusChange(base::PowerStateObserver::BatteryPowerStatus
                                      battery_power_status) override;

  void ReportState();

  QueryNextStateCallback callback_;
  PowerStateObserver::BatteryPowerStatus battery_power_status_ =
      PowerStateObserver::BatteryPowerStatus::kUnknown;
  bool need_to_report_ = false;
};

}  // namespace content

#endif  // CONTENT_SHELL_COMMON_POWER_MONITOR_TEST_IMPL_H_
