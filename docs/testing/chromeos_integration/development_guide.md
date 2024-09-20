# Crosier Development Guide

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
Please see: [go/crosier-run](http://go/crosier-run)

## Test metadata

Each Crosier test being added should include test metadata in `yaml` format.

See [Crosier metadata guide](https://source.chromium.org/chromium/chromium/src/+/main:docs/testing/chromeos_integration/crosier_metadata.md)
for more information on how to add it.

## Debugging Crosier tests

Debugging a Crosier test is like debugging any Chrome test on device. Following
developer library resources are available:

https://www.chromium.org/chromium-os/developer-library/guides/#debugging

## Continuous builders

https://ci.chromium.org/ui/p/chromium/builders/ci/chromeos-amd64-generic-rel

Target name is 'chromeos_integration_tests'.

When you make contributions, please add 'Include-Ci-Only-Tests: true' to the
changelist footer so the tests can run on CQ.
