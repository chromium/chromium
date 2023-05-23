# Generic Sensor API

This directory contains the Blink part (including the JavaScript APIs exposed
to users) of the [Generic Sensor API](https://w3c.github.io/sensors).

The following concrete sensor interfaces are currently implemented:

1. [Ambient Light Sensor](https://w3c.github.io/ambient-light)
1. [Accelerometer, Gravity Sensor and Linear Acceleration Sensor](https://w3c.github.io/accelerometer)
1. [Gyroscope](https://w3c.github.io/gyroscope)
1. [Magnetometer](https://w3c.github.io/magnetometer)
1. [Absolute Orientation Sensor and Relative Orientation Sensor](https://w3c.github.io/orientation-sensor)

Some of the interfaces above depend on the `SensorExtraClasses` runtime flag.

The platform-specific parts of the implementation are located in
[`services/device/generic_sensor`](/services/device/generic_sensor).

## Testing

Sensors web tests are part of the
[web-platform-tests](https://web-platform-tests.org) project and are located in
multiple directories under `web_tests/external/wpt`. For example ,
[`web_tests/external/wpt/accelerometer`](/third_party/blink/web_tests/external/wpt/accelerometer).
The sensor-agnostic parts of the tests are located in
[`web_tests/external/wpt/generic-sensor`](/third_party/blink/web_tests/external/wpt/generic-sensor).

Browser tests are located in
[`content/browser/generic_sensor`](/content/browser/generic_sensor).

## Overall architecture

The current design of the Chromium implementation of the Generic Sensor API is
described in
[`services/device/generic_sensor`](/services/device/generic_sensor/README.md).
