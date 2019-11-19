// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BATTERY_BATTERY_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BATTERY_BATTERY_DISPATCHER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/battery_monitor.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/platform_event_dispatcher.h"
#include "third_party/blink/renderer/modules/battery/battery_manager.h"
#include "third_party/blink/renderer/modules/battery/battery_status.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT BatteryDispatcher final
    : public GarbageCollected<BatteryDispatcher>,
      public PlatformEventDispatcher {
  USING_GARBAGE_COLLECTED_MIXIN(BatteryDispatcher);

 public:
  static BatteryDispatcher& Instance();

  BatteryDispatcher();

  const BatteryStatus* LatestData() const {
    return has_latest_data_ ? &battery_status_ : nullptr;
  }

 private:
  void QueryNextStatus();
  void OnDidChange(device::mojom::blink::BatteryStatusPtr);
  void UpdateBatteryStatus(const BatteryStatus&);

  // Inherited from PlatformEventDispatcher.
  void StartListening(LocalFrame* frame) override;
  void StopListening() override;

  mojo::Remote<device::mojom::blink::BatteryMonitor> monitor_;
  BatteryStatus battery_status_;
  bool has_latest_data_;

  DISALLOW_COPY_AND_ASSIGN(BatteryDispatcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BATTERY_BATTERY_DISPATCHER_H_
