// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_
#define SERVICES_DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/battery/battery_status_service.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"

namespace device {

class BatteryMonitorImpl : public mojom::BatteryMonitor {
 public:
  static void Create(mojo::PendingReceiver<mojom::BatteryMonitor> receiver);

  BatteryMonitorImpl();

  BatteryMonitorImpl(const BatteryMonitorImpl&) = delete;
  BatteryMonitorImpl& operator=(const BatteryMonitorImpl&) = delete;

  ~BatteryMonitorImpl() override;

 private:
  // mojom::BatteryMonitor methods:
  void QueryNextStatus(QueryNextStatusCallback callback) override;

  void RegisterSubscription();
  void DidChange(const mojom::BatteryStatus& battery_status);
  void ReportStatus();

  mojo::SelfOwnedReceiverRef<mojom::BatteryMonitor> receiver_;
  base::CallbackListSubscription subscription_;
  QueryNextStatusCallback callback_;
  mojom::BatteryStatus status_;
  bool status_to_report_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_
