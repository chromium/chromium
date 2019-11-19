// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_proxy_inspector_impl.h"

#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/sensor/sensor_reading_remapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

SensorProxyInspectorImpl::SensorProxyInspectorImpl(
    device::mojom::blink::SensorType sensor_type,
    SensorProviderProxy* provider,
    Page* page)
    : SensorProxy(sensor_type, provider, page) {}

SensorProxyInspectorImpl::~SensorProxyInspectorImpl() {}

void SensorProxyInspectorImpl::Trace(blink::Visitor* visitor) {
  SensorProxy::Trace(visitor);
}

void SensorProxyInspectorImpl::Initialize() {
  if (state_ != kUninitialized)
    return;

  state_ = kInitializing;

  auto callback = WTF::Bind(&SensorProxyInspectorImpl::OnSensorCreated,
                            WrapWeakPersistent(this));

  Thread::Current()->GetTaskRunner()->PostTask(FROM_HERE, std::move(callback));
}

void SensorProxyInspectorImpl::AddConfiguration(
    device::mojom::blink::SensorConfigurationPtr configuration,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(IsInitialized());
  std::move(callback).Run(true);
}

void SensorProxyInspectorImpl::RemoveConfiguration(
    device::mojom::blink::SensorConfigurationPtr configuration) {
  DCHECK(IsInitialized());
}

double SensorProxyInspectorImpl::GetDefaultFrequency() const {
  DCHECK(IsInitialized());
  return device::GetSensorDefaultFrequency(type_);
}

std::pair<double, double> SensorProxyInspectorImpl::GetFrequencyLimits() const {
  DCHECK(IsInitialized());
  return {1.0, device::GetSensorMaxAllowedFrequency(type_)};
}

void SensorProxyInspectorImpl::Suspend() {
  suspended_ = true;
}

void SensorProxyInspectorImpl::Resume() {
  suspended_ = false;
}

void SensorProxyInspectorImpl::SetReadingForInspector(
    const device::SensorReading& reading) {
  if (!ShouldProcessReadings())
    return;

  reading_ = reading;
  for (Observer* observer : observers_)
    observer->OnSensorReadingChanged();
}

void SensorProxyInspectorImpl::ReportError(DOMExceptionCode code,
                                           const String& message) {
  state_ = kUninitialized;
  reading_ = device::SensorReading();
  SensorProxy::ReportError(code, message);
}

void SensorProxyInspectorImpl::OnSensorCreated() {
  DCHECK_EQ(kInitializing, state_);

  state_ = kInitialized;

  UpdateSuspendedStatus();

  for (Observer* observer : observers_)
    observer->OnSensorInitialized();
}

bool SensorProxyInspectorImpl::ShouldProcessReadings() const {
  return IsInitialized() && !suspended_;
}

}  // namespace blink
