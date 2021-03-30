// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_sensor_event_pump.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

void DeviceSensorEventPump::Start(LocalFrame& frame) {
  DVLOG(2) << "requested start";

  if (state_ != PumpState::STOPPED)
    return;

  DCHECK(!timer_.IsActive());

  state_ = PumpState::PENDING_START;

  SendStartMessage(frame);
}

void DeviceSensorEventPump::Stop() {
  DVLOG(2) << "requested stop";

  if (state_ == PumpState::STOPPED)
    return;

  DCHECK((state_ == PumpState::PENDING_START && !timer_.IsActive()) ||
         (state_ == PumpState::RUNNING && timer_.IsActive()));

  if (timer_.IsActive())
    timer_.Stop();

  SendStopMessage();

  state_ = PumpState::STOPPED;
}

void DeviceSensorEventPump::HandleSensorProviderError() {
  sensor_provider_.reset();
}

void DeviceSensorEventPump::SetSensorProviderForTesting(
    mojo::PendingRemote<device::mojom::blink::SensorProvider> sensor_provider) {
  sensor_provider_.Bind(std::move(sensor_provider), task_runner_);
  sensor_provider_.set_disconnect_handler(
      WTF::Bind(&DeviceSensorEventPump::HandleSensorProviderError,
                WrapWeakPersistent(this)));
}

DeviceSensorEventPump::PumpState
DeviceSensorEventPump::GetPumpStateForTesting() {
  return state_;
}

void DeviceSensorEventPump::Trace(Visitor* visitor) const {
  visitor->Trace(sensor_provider_);
  visitor->Trace(timer_);
}

DeviceSensorEventPump::DeviceSensorEventPump(LocalFrame& frame)
    : sensor_provider_(frame.DomWindow()),
      task_runner_(frame.GetTaskRunner(TaskType::kSensor)),
      state_(PumpState::STOPPED),
      timer_(frame.GetTaskRunner(TaskType::kSensor),
             this,
             &DeviceSensorEventPump::FireEvent) {}

DeviceSensorEventPump::~DeviceSensorEventPump() = default;

void DeviceSensorEventPump::DidStartIfPossible() {
  DVLOG(2) << "did start sensor event pump";

  if (state_ != PumpState::PENDING_START)
    return;

  if (!SensorsReadyOrErrored())
    return;

  DCHECK(!timer_.IsActive());

  timer_.StartRepeating(
      base::TimeDelta::FromMicroseconds(kDefaultPumpDelayMicroseconds),
      FROM_HERE);
  state_ = PumpState::RUNNING;
}

}  // namespace blink
