// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_proxy_impl.h"

namespace blink {

// SensorProviderProxy
SensorProviderProxy::SensorProviderProxy(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), sensor_provider_(&window) {}

void SensorProviderProxy::InitializeIfNeeded() {
  if (sensor_provider_.is_bound())
    return;

  GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
      sensor_provider_.BindNewPipeAndPassReceiver(
          GetSupplementable()->GetTaskRunner(TaskType::kSensor)));
  sensor_provider_.set_disconnect_handler(
      WTF::BindOnce(&SensorProviderProxy::OnSensorProviderConnectionError,
                    WrapWeakPersistent(this)));
}

// static
const char SensorProviderProxy::kSupplementName[] = "SensorProvider";

// static
SensorProviderProxy* SensorProviderProxy::From(LocalDOMWindow* window) {
  DCHECK(window);
  SensorProviderProxy* provider_proxy =
      Supplement<LocalDOMWindow>::From<SensorProviderProxy>(*window);
  if (!provider_proxy) {
    provider_proxy = MakeGarbageCollected<SensorProviderProxy>(*window);
    Supplement<LocalDOMWindow>::ProvideTo(*window, provider_proxy);
  }
  return provider_proxy;
}

SensorProviderProxy::~SensorProviderProxy() = default;

void SensorProviderProxy::Trace(Visitor* visitor) const {
  visitor->Trace(sensor_proxies_);
  visitor->Trace(sensor_provider_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

SensorProxy* SensorProviderProxy::CreateSensorProxy(
    device::mojom::blink::SensorType type,
    Page* page) {
  DCHECK(!GetSensorProxy(type));

  SensorProxy* sensor = static_cast<SensorProxy*>(
      MakeGarbageCollected<SensorProxyImpl>(type, this, page));
  sensor_proxies_.insert(sensor);

  return sensor;
}

SensorProxy* SensorProviderProxy::GetSensorProxy(
    device::mojom::blink::SensorType type) {
  for (SensorProxy* sensor : sensor_proxies_) {
    // TODO(Mikhail) : Hash sensors by type for efficiency.
    if (sensor->type() == type)
      return sensor;
  }

  return nullptr;
}

void SensorProviderProxy::OnSensorProviderConnectionError() {
  sensor_provider_.reset();
  for (SensorProxy* sensor : sensor_proxies_) {
    sensor->ReportError(DOMExceptionCode::kNotReadableError,
                        SensorProxy::kDefaultErrorDescription);
  }
}

void SensorProviderProxy::RemoveSensorProxy(SensorProxy* proxy) {
  DCHECK(sensor_proxies_.Contains(proxy));
  sensor_proxies_.erase(proxy);
}

void SensorProviderProxy::GetSensor(
    device::mojom::blink::SensorType type,
    mojom::blink::WebSensorProviderProxy::GetSensorCallback callback) {
  InitializeIfNeeded();
  sensor_provider_->GetSensor(type, std::move(callback));
}

}  // namespace blink
