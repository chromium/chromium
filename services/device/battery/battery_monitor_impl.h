// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_
#define SERVICES_DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/battery/battery_status_service.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"

namespace device {

class BatteryMonitorImpl : public mojom::BatteryMonitor {
 public:
  static void Create(mojo::PendingReceiver<mojom::BatteryMonitor> receiver);

  BatteryMonitorImpl();
  ~BatteryMonitorImpl() override;

 private:
  // mojom::BatteryMonitor methods:
  void QueryNextStatus(QueryNextStatusCallback callback) override;

  void RegisterSubscription();
  void DidChange(const mojom::BatteryStatus& battery_status);
  void ReportStatus();

  mojo::SelfOwnedReceiverRef<mojom::BatteryMonitor> receiver_;
  std::unique_ptr<BatteryStatusService::BatteryUpdateSubscription>
      subscription_;
  QueryNextStatusCallback callback_;
  mojom::BatteryStatus status_;
  bool status_to_report_;

  DISALLOW_COPY_AND_ASSIGN(BatteryMonitorImpl);
};

}  // namespace device

#endif  // SERVICES_DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_
