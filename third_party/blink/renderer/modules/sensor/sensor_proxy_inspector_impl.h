// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROXY_INSPECTOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROXY_INSPECTOR_IMPL_H_

#include "third_party/blink/renderer/modules/sensor/sensor_proxy.h"

namespace blink {

class SensorProxyInspectorImpl final : public SensorProxy {
 public:
  SensorProxyInspectorImpl(device::mojom::blink::SensorType sensor_type,
                           SensorProviderProxy* provider,
                           Page* page);

  SensorProxyInspectorImpl(const SensorProxyInspectorImpl&) = delete;
  SensorProxyInspectorImpl& operator=(const SensorProxyInspectorImpl&) = delete;

  ~SensorProxyInspectorImpl() override;

  void Trace(Visitor*) const override;

 private:
  // SensorProxy overrides.
  void Initialize() override;
  void AddConfiguration(device::mojom::blink::SensorConfigurationPtr,
                        base::OnceCallback<void(bool)>) override;
  void RemoveConfiguration(
      device::mojom::blink::SensorConfigurationPtr) override;
  double GetDefaultFrequency() const override;
  std::pair<double, double> GetFrequencyLimits() const override;
  void SetReadingForInspector(const device::SensorReading&) override;

  void ReportError(DOMExceptionCode, const String&) override;

 private:
  void OnSensorCreated();

  // Returns 'true' if readings should be propagated to Observers
  // (i.e. proxy is initialized, not suspended);
  // returns 'false' otherwise.
  bool ShouldProcessReadings() const;

  void Suspend() override;
  void Resume() override;

  bool suspended_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROXY_INSPECTOR_IMPL_H_
