# Bluetooth Testing

This page describes testing APIs for both **clients** and **implementation** of
the Bluetooth component.

There are also notable higher level bluetooth tests:

*   [Extensions](/extensions/browser/api/bluetooth/)
*   [Web Bluetooth](/third_party/blink/renderer/modules/bluetooth/README.md)


## Client Testing

### Mock Bluetooth Objects

`test/mock_bluetooth_*` files provide GoogleMock based fake objects that
subclass the cross platform C++ device/bluetooth API.

These are used by numerous clients and are stable.


### Fake Bluetooth Mojo Testing Interface Implementation

WORK IN PROGRESS.

Web Platform Tests for Web Bluetooth are being refactored to use
`third_party/WebKit/LayoutTests/resources/bluetooth/web-bluetooth-test.js`.

That library is implemented over a mojo interface `fake_bluetooth.mojom` in
[bluetooth/public/mojom/emulation](/device/bluetooth/public/mojom/emulation)
and is implemented in the `bluetooth/emulation/fake_*` files.

The `fake_bluetooth.mojom` interface is not intended to be used directly.
`web-bluetooth-test.js` makes the Fake Bluetooth interface easier to work with.

*   Calls are synchronous.
*   IDs are cached.

If another C++ client intends to use Fake Bluetooth a C++ wrapper similar to
`web-bluetooth-test.js` should be created.

When a Bluetooth service is created the `fake_bluetooth.mojom` and
`bluetooth/emulation/fake_*` files should be removed and the client facing test
wrapper `web-bluetooth-test.js` converted to implement the Bluetooth service as
needed for tests.

Design Doc:
https://docs.google.com/document/d/1Nhv_oVDCodd1pEH_jj9k8gF4rPGb_84VYaZ9IG8M_WY

## BluetoothEmulation support in Chrome Devtools Protocol

The `BluetoothEmulation` domain in
[Chrome Devtools Protocol](third_party/blink/public/devtools_protocol/browser_protocol.pdl)
now supports Bluetooth Device Emulation via the FakeBluetooth backend implemented in
content/browser/devtools/protocol/bluetooth_emulation_handler.h.

## Implementation Testing

### Cross Platform Unit Tests

New feature development uses cross platform unit tests. This reduces test code
redundancy and produces consistency across all implementations.

Unit tests operate at the public `device/bluetooth` API layer and the
`BluetoothTest` fixture controls fake operating system behavior as close to the
platfom as possible. The resulting test coverage spans the cross platform API,
common implementation, and platform specific implementation as close to
operating system APIs as possible.

`test/bluetooth_test.h` defines the cross platform test fixture
`BluetoothTestBase`. Platform implementations provide subclasses, such as
`test/bluetooth_test_android.h` and typedef to the name `BluetoothTest`.

[More testing information](https://docs.google.com/document/d/1mBipxn1sJu6jMqP0RQZpkYXC1o601bzLCpCxwTA2yGA/edit?usp=sharing)


### Legacy Platform Specific Unit Tests

Early code (Classic on most platforms, and Low Energy on BlueZ) was tested with
platform specific unit tests, e.g. `bluetooth_bluez_unittest.cc` &
`bluetooth_adapter_win_unittest.cc`. The BlueZ style has platform specific
methods to create fake devices and the public API is used to interact with them.

Maintenance of these earlier implementation featuress should update tests in
place. Long term these tests should be [refactored into cross platform
tests](https://crbug.com/580403).


### Chrome OS Blueooth Controller Tests

Bluetooth controller system tests generating radio signals are run and managed
by the Chrome OS team. See:
https://chromium.googlesource.com/chromiumos/third_party/autotest/+/main/server/site_tests/
https://chromium.googlesource.com/chromiumos/third_party/autotest/+/main/server/cros/bluetooth/
https://chromium.googlesource.com/chromiumos/third_party/autotest/+/main/client/cros/bluetooth/
