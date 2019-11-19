// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_ORIENTATION_DEVICE_ORIENTATION_EVENT_PUMP_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_ORIENTATION_DEVICE_ORIENTATION_EVENT_PUMP_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/device_orientation/device_sensor_event_pump.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class DeviceOrientationData;
class DeviceSensorEntry;
class PlatformEventController;

class MODULES_EXPORT DeviceOrientationEventPump
    : public GarbageCollected<DeviceOrientationEventPump>,
      public DeviceSensorEventPump {
  USING_GARBAGE_COLLECTED_MIXIN(DeviceOrientationEventPump);

 public:
  // Angle threshold beyond which two orientation events are considered
  // sufficiently different.
  static const double kOrientationThreshold;

  explicit DeviceOrientationEventPump(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool absolute);
  ~DeviceOrientationEventPump() override;

  void SetController(PlatformEventController*);
  void RemoveController();

  // Note that the returned object is owned by this class.
  DeviceOrientationData* LatestDeviceOrientationData();

  void Trace(blink::Visitor*) override;

  // DeviceSensorEventPump:
  void SendStartMessage(LocalFrame* frame) override;
  void SendStopMessage() override;

 protected:
  // DeviceSensorEventPump:
  void FireEvent(TimerBase*) override;
  void DidStartIfPossible() override;

  Member<DeviceSensorEntry> relative_orientation_sensor_;
  Member<DeviceSensorEntry> absolute_orientation_sensor_;

 private:
  friend class DeviceOrientationEventPumpTest;
  friend class DeviceAbsoluteOrientationEventPumpTest;

  void StartListening(LocalFrame*);
  void StopListening();
  void NotifyController();

  // DeviceSensorEventPump:
  bool SensorsReadyOrErrored() const override;

  DeviceOrientationData* GetDataFromSharedMemory();

  bool ShouldFireEvent(const DeviceOrientationData* data) const;

  bool absolute_;
  bool fall_back_to_absolute_orientation_sensor_;
  bool should_suspend_absolute_orientation_sensor_ = false;
  Member<DeviceOrientationData> data_;
  Member<PlatformEventController> controller_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPump);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_ORIENTATION_DEVICE_ORIENTATION_EVENT_PUMP_H_
