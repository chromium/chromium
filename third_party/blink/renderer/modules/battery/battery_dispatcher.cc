// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/battery/battery_dispatcher.h"

#include "services/device/public/mojom/battery_status.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

BatteryDispatcher& BatteryDispatcher::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<BatteryDispatcher>, battery_dispatcher,
                      (MakeGarbageCollected<BatteryDispatcher>()));
  return *battery_dispatcher;
}

BatteryDispatcher::BatteryDispatcher() : has_latest_data_(false) {}

void BatteryDispatcher::QueryNextStatus() {
  monitor_->QueryNextStatus(
      WTF::Bind(&BatteryDispatcher::OnDidChange, WrapPersistent(this)));
}

void BatteryDispatcher::OnDidChange(
    device::mojom::blink::BatteryStatusPtr battery_status) {
  QueryNextStatus();

  DCHECK(battery_status);

  UpdateBatteryStatus(
      BatteryStatus(battery_status->charging, battery_status->charging_time,
                    battery_status->discharging_time, battery_status->level));
}

void BatteryDispatcher::UpdateBatteryStatus(
    const BatteryStatus& battery_status) {
  battery_status_ = battery_status;
  has_latest_data_ = true;
  NotifyControllers();
}

void BatteryDispatcher::StartListening(LocalFrame* frame) {
  DCHECK(!monitor_.is_bound());
  // See https://bit.ly/2S0zRAS for task types.
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      monitor_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  QueryNextStatus();
}

void BatteryDispatcher::StopListening() {
  monitor_.reset();
  has_latest_data_ = false;
}

}  // namespace blink
