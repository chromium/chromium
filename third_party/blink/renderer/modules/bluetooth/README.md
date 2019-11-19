# Web Bluetooth Blink Module

`Source/modules/bluetooth` implements the renderer process details and bindings
for the [Web Bluetooth specification]. It uses the Web Bluetooth Service
[mojom] to communicate with the [Web Bluetooth Service].

[Web Bluetooth specification]: https://webbluetoothcg.github.io/web-bluetooth/
[mojom]: ../../../public/mojom/bluetooth/web_bluetooth.mojom
[Web Bluetooth Service]: /content/browser/bluetooth/


## LE only Scanning

There isn't much support for GATT over BR/EDR from neither platforms nor
devices so performing a Dual scan will find devices that the API is not
able to interact with. To avoid wasting power and confusing users with
devices they are not able to interact with, navigator.bluetooth.requestDevice
performs an LE-only Scan.


## Testing

Web Bluetooth implementation details are tested at several layers:

*   `/device/bluetooth/*_unittest.cc`
    *   `device_unittests --gtest_filter="*Bluetooth*"`
    *   General bluetooth platform abstraction level down to the OS.
        See [device/bluetooth/test](/device/bluetooth/test/README.md) for
        details.
*   `/chrome/browser/*bluetooth*_browsertest.cc`
    *   `out/Release/browser_tests --gtest_filter="*Bluetooth*"`
    *   Browser policy level tests (crash recovery, blocklist, killswitch).
*   `/content/*/bluetooth/*_unittest.cc`
    *   `out/Release/content_unittests --gtest_filter="*Bluetooth*"`
    *   Trusted Web Bluetooth code (browser process) tests
        (as opposed to untrusted renderer process).
*   `web_tests/bluetooth/*/*.html`
    *   `blink/tools/run_layout_tests.sh bluetooth`
    *   Web tests in `web_tests/bluetooth/` rely on
        fake Bluetooth implementation classes constructed in
        `content/shell/browser/web_test/web_test_bluetooth_adapter_provider`.
        These tests span JavaScript binding to the `device/bluetooth` platform
        abstraction layer.
*   `testing/clusterfuzz`
    *   [Web Bluetooth Fuzzer] runs on cluster fuzz infrastructure.

[Web Bluetooth Fuzzer]: testing/clusterfuzz/README.md


## Design Documents

See: [Class Diagram of Web Bluetooth through Bluetooth Android][Class]

[Class]: https://sites.google.com/a/chromium.org/dev/developers/design-documents/bluetooth-design-docs/web-bluetooth-through-bluetooth-android-class-diagram

