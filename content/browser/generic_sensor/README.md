# Generic Sensors, //content part

[TOC]

## Main classes and workflow

This directory contains part of the [Generic Sensors API](https://w3c.github.io/sensors) implementation in Chromium. See the [Blink README.md](/third_party/blink/renderer/modules/sensor/README.md) and the [services README.md](/services/device/generic_sensor/README.md) for more information about the architecture.

From a spec perspective, the code here implements the following steps together with Blink:

* [Check sensor policy-controlled features](https://w3c.github.io/sensors/#check-sensor-policy-controlled-features).
* [Request sensor access](https://w3c.github.io/sensors/#request-sensor-access).

`//content/browser/generic_sensor` implements permission checks invoked by the Blink code, and acts as a bridge between Blink and `//services/device/generic_sensor`. It also translates calls to the [`WebSensorProvider`](/third_party/blink/public/mojom/sensor/web_sensor_provider.mojom) Mojo interface to [`SensorProvider`](/services/device/public/mojom/sensor_provider.mojom) ones.

When code in Blink invokes `WebSensorProvider::GetSensor()`, it reaches `FrameSensorProviderProxy::GetSensor()` in `//content/browser/generic_sensor` rather than `SensorProviderImpl::GetSensor()` in `//services/device/generic_sensor`. If all permission checks pass, `FrameSensorProviderProxy` will forward the request from Blink to `WebContentsSensorProviderProxy::GetSensor()`, which ultimately invokes `SensorProviderImpl::GetSensor()` via Mojo.

`WebContentsSensorProviderProxy` itself does not implement any Mojo interface, but it contains the required `mojo::Remote`s that invoke sensor operations in `SensorProviderImpl`.

`WebContentsSensorProviderProxy` exists on a per-WebContents basis; the Mojo binding between Blink and this class goes through `FrameSensorProviderProxy`, which exists on a per-RenderFrameHost basis.

This directory also contains a few browser tests for the permission checks in `generic_sensor_browsertest.cc`. They are complemented by [`device_sensor_browsertest.cc`](/content/browser/device_sensors/device_sensor_browsertest.cc).

## Virtual Sensor support

The `WebContentsSensorProviderProxy` class has a [SensorProvider](/services/device/public/mojom/sensor_provider.mojom) `mojo::Remote` that is connected to //service's `SensorProviderImpl`. This means that, in addition to `GetSensor()`, it is also able to invoke privileged operations that create and manipulate virtual sensors. Virtual sensors are used for testing: end users can use ChromeDriver to create virtual sensors and use Sensor APIs, while Blink's web tests can use the testdriver.js API to test the implementation of the same Sensor APIs.

The virtual sensor commands are not exposed directly outside of the WebContentsSensorProviderProxy and //services relationship. This section describes how these operations are made available to //content consumers.

All WebDriver calls (from both ChromeDriver and testdriver.js) are implemented as CDP (Chrome DevTools Protocol) commands, so there is a very close relationship between the [`EmulationHandler`](/content/browser/devtools/protocol/emulation_handler.h) class that implements the `getOverriddenSensorInformation`, `setSensorOverrideEnabled`, and `setSensorOverrideReadings` [DevTools commands](/third_party/blink/public/devtools_protocol/browser_protocol.pdl) and the classes in this directory, namely `WebContentsSensorProviderProxy` and [`ScopedVirtualSensorForDevTools`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/generic_sensor/web_contents_sensor_provider_proxy.h?q=symbol:%5Cbcontent::ScopedVirtualSensorForDevTools%5Cb).

The `EmulationHandler` class and its implementation of the commands above need to adhere to some CDP invariants and requirements that apply to all handlers/agents. Most importantly:

- It is possible for multiple agents (i.e. `EmulationHandler` instances) to exist and be attached to the same page (e.g. the DevTools UI and ChromeDriver), but they should not interfere with each other.
- Changes made by an agent should be undone when it is disabled, disconnected or destroyed.

From a sensors perspective, this means that:

- One agent should not be able to remove or update a virtual sensor created by another agent.
- One agent should not be able to create a virtual sensor of a type that has already been created by another agent.
- All virtual sensors created by one agent should be removed when it is disabled or destroyed.

`EmulationHandler` interacts with `WebContentsSensorProviderProxy` by retrieving the instance associated with the RenderFrameHost it is connected to. Even though `WebContentsSensorProviderProxy` has virtual sensor methods that match the ones in the Mojo interface (e.g. `CreateVirtualSensor()` and `RemoveVirtualSensor()`), these methods are private. The entry point for API users is [WebContentsSensorProviderProxy::CreateVirtualSensorForDevTools()](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/generic_sensor/web_contents_sensor_provider_proxy.h?q=symbol%3A%5Cbcontent%3A%3AWebContentsSensorProviderProxy%3A%3ACreateVirtualSensorForDevTools%5Cb%20case%3Ayes). A `ScopedVirtualSensorForDevTools` invokes the `CreateVirtualSensor()` operation automatically on creation and calls `RemoveVirtualSensor()` on destruction. It acts as an RAII wrapper for a virtual sensor.

Each `EmulationHandler` instance parses and validates the sensor CDP commands and keeps a mapping of sensor types to `ScopedVirtualSensorForDevTools` instances. The mapping is updated when `setSensorOverrideEnabled` is invoked, and it is cleared automatically when the `EmulationHandler` is destroyed or disabled.

This, together with the bookkeping that `WebContentsSensorProviderProxy` implements to only return one `ScopedVirtualSensorForDevTools` instance per sensor type, fulfills all the CDP requirements outlined above.

### content_shell and web tests

When the Blink web tests are run in the Chromium CI (i.e. via `run_web_tests.py`), they are run with content_shell rather than Chromium and ChromeDriver. Consequently, the WebDriver and CDP code path described above is not used.

In this case, content_shell uses a separate implementation of the testdriver.js API that relies on Blink's Internals JS API. The [Internals code in Blink](/third_party/blink/renderer/modules/sensor/testing/internals_sensor.h) uses a special Mojo interface, [WebSensorProviderAutomation](/third_party/blink/public/mojom/sensor/web_sensor_provider_automation.mojom), to make the virtual sensor calls.

On the browser side, this Mojo interface is implemented only by [`WebTestSensorProviderManager`](/content/web_test/browser/web_test_sensor_provider_manager.h), which is created only when the content_shell binary is run with `--run-web-tests` (therefore, it is not exposed by the `chrome` binary).

The `WebTestSensorProviderManager` implementation is not very different from `EmulationHandler`'s, in that it also keeps a map of sensor types to `std::unique_ptr<ScopedVirtualSensorForDevTools>` instances that is manipulated in its create/remove/get/update methods.
