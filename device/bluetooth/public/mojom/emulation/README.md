# Bluetooth Test Interfaces

This folder contains interfaces that are useful for testing of code that uses
device/bluetooth

## Fake Bluetooth

`FakeBluetooth` is an interface that allows its clients to fake bluetooth events
at the platform abstraction layer. See fake_bluetooth.mojom for more information
about the interface and
[//device/bluetooth/test/README.md](/device/bluetooth/test/README.md) for
information about its implementation.
