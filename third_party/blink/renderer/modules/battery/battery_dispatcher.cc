// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/battery/battery_dispatcher.h"

#include "services/device/public/mojom/battery_status.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

BatteryDispatcher::BatteryDispatcher(ExecutionContext* context)
    : monitor_(context), has_latest_data_(false) {}

void BatteryDispatcher::Trace(Visitor* visitor) const {
  visitor->Trace(monitor_);
  PlatformEventDispatcher::Trace(visitor);
}

void BatteryDispatcher::QueryNextStatus() {
  monitor_->QueryNextStatus(
      WTF::BindOnce(&BatteryDispatcher::OnDidChange, WrapPersistent(this)));
}

void BatteryDispatcher::OnDidChange(
    device::mojom::blink::BatteryStatusPtr battery_status) {
  QueryNextStatus();

  DCHECK(battery_status);

  UpdateBatteryStatus(BatteryStatus(
      battery_status->charging, base::Seconds(battery_status->charging_time),
      base::Seconds(battery_status->discharging_time), battery_status->level));
}

void BatteryDispatcher::UpdateBatteryStatus(
    const BatteryStatus& battery_status) {
  battery_status_ = battery_status;
  has_latest_data_ = true;
  NotifyControllers();
}

void BatteryDispatcher::StartListening(LocalDOMWindow* window) {
  DCHECK(!monitor_.is_bound());
  // See https://bit.ly/2S0zRAS for task types.
  window->GetBrowserInterfaceBroker().GetInterface(
      monitor_.BindNewPipeAndPassReceiver(
          window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  QueryNextStatus();
}

void BatteryDispatcher::StopListening() {
  monitor_.reset();
  has_latest_data_ = false;
}

}  // namespace blink
