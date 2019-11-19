// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_POWER_MONITOR_TEST_IMPL_H_
#define CONTENT_SHELL_COMMON_POWER_MONITOR_TEST_IMPL_H_

#include "base/macros.h"
#include "base/power_monitor/power_monitor.h"
#include "content/shell/common/power_monitor_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class PowerMonitorTestImpl : public base::PowerObserver,
                             public mojom::PowerMonitorTest {
 public:
  static void MakeSelfOwnedReceiver(
      mojo::PendingReceiver<mojom::PowerMonitorTest> receiver);

  PowerMonitorTestImpl();
  ~PowerMonitorTestImpl() override;

 private:
  // mojom::PowerMonitorTest:
  void QueryNextState(QueryNextStateCallback callback) override;

  // base::PowerObserver:
  void OnPowerStateChange(bool on_battery_power) override;
  void OnSuspend() override {}
  void OnResume() override {}

  void ReportState();

  QueryNextStateCallback callback_;
  bool on_battery_power_ = false;
  bool need_to_report_ = false;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorTestImpl);
};

}  // namespace content

#endif  // CONTENT_SHELL_COMMON_POWER_MONITOR_TEST_IMPL_H_
