// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_ORIENTATION_DEVICE_SENSOR_ENTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_ORIENTATION_DEVICE_SENSOR_ENTRY_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/sensor.mojom-blink-forward.h"
#include "services/device/public/mojom/sensor_provider.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace device {
union SensorReading;
class SensorReadingSharedBufferReader;
}  // namespace device

namespace blink {

class DeviceSensorEventPump;

class DeviceSensorEntry : public GarbageCollected<DeviceSensorEntry>,
                          public device::mojom::blink::SensorClient {
  USING_PRE_FINALIZER(DeviceSensorEntry, Dispose);

 public:
  // The sensor state is an automaton with allowed transitions as follows:
  // NOT_INITIALIZED -> INITIALIZING
  // INITIALIZING -> ACTIVE
  // INITIALIZING -> SHOULD_SUSPEND
  // ACTIVE -> SUSPENDED
  // SHOULD_SUSPEND -> INITIALIZING
  // SHOULD_SUSPEND -> SUSPENDED
  // SUSPENDED -> ACTIVE
  // { INITIALIZING, ACTIVE, SHOULD_SUSPEND, SUSPENDED } -> NOT_INITIALIZED
  enum class State {
    NOT_INITIALIZED,
    INITIALIZING,
    ACTIVE,
    SHOULD_SUSPEND,
    SUSPENDED
  };

  DeviceSensorEntry(DeviceSensorEventPump* pump,
                    device::mojom::blink::SensorType sensor_type);
  void Dispose();
  ~DeviceSensorEntry() override;

  void Start(device::mojom::blink::SensorProvider* sensor_provider);
  void Stop();
  bool IsConnected() const;
  bool ReadyOrErrored() const;
  bool GetReading(device::SensorReading* reading);

  State state() const { return state_; }

  void Trace(Visitor* visitor);

 private:
  // device::mojom::SensorClient:
  void RaiseError() override;
  void SensorReadingChanged() override;

  // Mojo callback for SensorProvider::GetSensor().
  void OnSensorCreated(device::mojom::blink::SensorCreationResult result,
                       device::mojom::blink::SensorInitParamsPtr params);

  // Mojo callback for Sensor::AddConfiguration().
  void OnSensorAddConfiguration(bool success);

  void HandleSensorError();

  Member<DeviceSensorEventPump> event_pump_;

  State state_ = State::NOT_INITIALIZED;

  mojo::Remote<device::mojom::blink::Sensor> sensor_remote_;
  mojo::Receiver<device::mojom::blink::SensorClient> client_receiver_{this};

  device::mojom::blink::SensorType type_;

  std::unique_ptr<device::SensorReadingSharedBufferReader>
      shared_buffer_reader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_ORIENTATION_DEVICE_SENSOR_ENTRY_H_
