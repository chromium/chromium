# Generic Sensors, //content part

This directory contains part of the [Generic Sensors API](https://w3c.github.io/sensors) implementation in Chromium. See the [Blink README.md](/third_party/blink/renderer/modules/sensor/README.md) and the [services README.md](/services/device/generic_sensor/README.md) for more information about the architecture.

From a spec perspective, the code here implements the following steps together with Blink:

* [Check sensor policy-controlled features](https://w3c.github.io/sensors/#check-sensor-policy-controlled-features).
* [Request sensor access](https://w3c.github.io/sensors/#request-sensor-access).

`//content/browser/generic_sensor` implements permission checks invoked by the Blink code, and acts as a bridge between Blink and `//services/device/generic_sensor`. It also translates calls to the [`WebSensorProvider`](/third_party/blink/public/mojom/sensor/web_sensor_provider.mojom) Mojo interface to [`SensorProvider`](/services/device/public/mojom/sensor_provider.mojom) ones.

When code in Blink invokes `WebSensorProvider::GetSensor()`, it reaches `FrameSensorProviderProxy::GetSensor()` in `//content/browser/generic_sensor` rather than `SensorProviderImpl::GetSensor()` in `//services/device/generic_sensor`. If all permission checks pass, `FrameSensorProviderProxy` will forward the request from Blink to `WebContentsSensorProviderProxy::GetSensor()`, which ultimately invokes `SensorProviderImpl::GetSensor()` via Mojo.

`WebContentsSensorProviderProxy` itself does not implement any Mojo interface, but it contains the required `mojo::Remote`s that invoke sensor operations in `SensorProviderImpl`.

`WebContentsSensorProviderProxy` exists on a per-WebContents basis; the Mojo binding between Blink and this class goes through `FrameSensorProviderProxy`, which exists on a per-RenderFrameHost basis.

This directory also contains a few browser tests for the permission checks in `generic_sensor_browsertest.cc`. They are complemented by [`device_sensor_browsertest.cc`](/content/browser/device_sensors/device_sensor_browsertest.cc).
