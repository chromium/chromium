// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROXY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROXY_IMPL_H_

#include "third_party/blink/renderer/modules/sensor/sensor_proxy.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class SensorProviderProxy;

// This class wraps 'Sensor' mojo interface and used by multiple
// JS sensor instances of the same type (within a single frame).
class SensorProxyImpl final : public SensorProxy,
                              public device::mojom::blink::SensorClient {
  USING_PRE_FINALIZER(SensorProxyImpl, Dispose);

 public:
  SensorProxyImpl(device::mojom::blink::SensorType,
                  SensorProviderProxy*,
                  Page*);
  ~SensorProxyImpl() override;

  void Trace(blink::Visitor*) override;

  void Dispose();

 private:
  // SensorProxy overrides.
  void Initialize() override;
  void AddConfiguration(device::mojom::blink::SensorConfigurationPtr,
                        base::OnceCallback<void(bool)>) override;
  void RemoveConfiguration(
      device::mojom::blink::SensorConfigurationPtr) override;
  double GetDefaultFrequency() const override;
  std::pair<double, double> GetFrequencyLimits() const override;
  void ReportError(DOMExceptionCode code, const String& message) override;
  void Suspend() override;
  void Resume() override;

  // Updates sensor reading from shared buffer.
  void UpdateSensorReading();
  void NotifySensorChanged(double timestamp);

  // device::mojom::blink::SensorClient overrides.
  void RaiseError() override;
  void SensorReadingChanged() override;

  // Generic handler for a fatal error.
  void HandleSensorError(
      device::mojom::blink::SensorCreationResult =
          device::mojom::blink::SensorCreationResult::ERROR_NOT_AVAILABLE);

  // mojo call callbacks.
  void OnSensorCreated(device::mojom::blink::SensorCreationResult,
                       device::mojom::blink::SensorInitParamsPtr);

  void OnPollingTimer(TimerBase*);

  // Returns 'true' if readings should be propagated to Observers
  // (i.e. proxy is initialized, not suspended and has active configurations);
  // returns 'false' otherwise.
  bool ShouldProcessReadings() const;

  // Starts or stops polling timer.
  void UpdatePollingStatus();

  void RemoveActiveFrequency(double frequency);
  void AddActiveFrequency(double frequency);

  device::mojom::blink::ReportingMode mode_ =
      device::mojom::blink::ReportingMode::CONTINUOUS;
  mojo::Remote<device::mojom::blink::Sensor> sensor_remote_;
  mojo::Receiver<device::mojom::blink::SensorClient> client_receiver_{this};

  std::unique_ptr<device::SensorReadingSharedBufferReader>
      shared_buffer_reader_;
  double default_frequency_ = 0.0;
  std::pair<double, double> frequency_limits_;
  bool suspended_ = false;

  WTF::Vector<double> active_frequencies_;
  TaskRunnerTimer<SensorProxyImpl> polling_timer_;

  DISALLOW_COPY_AND_ASSIGN(SensorProxyImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROXY_IMPL_H_
