# Development Guide

This doc assumes you're already familiar with ChromeOS on-device development.
If not, please follow
[Simple Chrome workflow](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/simple_chrome_workflow.md)

## Demo test
We've had several tests.

[NewTab demo](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/base/chromeos/crosier/demo_integration_test.cc)
is a test that opens a browser and open a tab which can run DUT or VM.

[Bluetooth test](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/bluetooth/bluetooth_integration_test.cc)
is a test that toggle to turn on and off bluetooth which can only
run on DUT.

## How to run Ash test
Please see: [go/crosier-run](go/crosier-run)

## How to run Lacros test
See the [demo test](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/base/chromeos/crosier/demo_integration_test.cc;l=19)
for instructions

## Continuous builders
Currently the test binary runs against both Ash and Lacros on CI only.

Ash:

https://ci.chromium.org/ui/p/chromium/builders/ci/chromeos-amd64-generic-rel

Lacros:

https://ci.chromium.org/ui/p/chromium/builders/ci/lacros-amd64-generic-rel

Target name is 'chromeos_integration_tests'.

When you make contributions, please add 'Include-Ci-Only-Tests: true' to the
changelist footer so the tests can run on CQ.
