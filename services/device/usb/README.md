# USB

`services/device/usb` abstracts [Universal Serial Bus](https://en.wikipedia.org/wiki/USB)
concepts across multiple platforms.

Clients should use the [public Mojo interface](../public/mojom).


## USB ID Repository

`/third_party/usb_ids/usb.ids` is imported regularly to provide human-readable
descriptions of USB devices.


## Ongoing Work transitioning away from `libusb`

`UsbService` is implemented by `UsbServiceImpl` based on `third_party/libusb`.

Work is ongoing to implement each platform directly, without using `libusb`.

*   `UsbServiceAndroid` done.
*   `UsbServiceLinux` done.
*   macOS not started.
*   `UsbServiceWin` in progress. Enable via `chrome://flags/#new-usb-backend`


## Testing

### Unit Tests

Standard use of `*_unittest.cc` files for must code coverage.


### Fuzzers

[libFuzzer] tests are in `*_fuzzer.cc` files. They test for bad input from
devices, e.g. when parsing device descriptors during device enumeration.

[libFuzzer]: /testing/libfuzzer/README.md


### Gadget Tests

[USB/HID API Testing with Gadgets] describes a framework for testing the Chrome
USB, HID and serial device APIs with real devices on generally accessible
hardware.

[USB/HID API Testing with Gadgets]: https://docs.google.com/document/d/1O9jTlOAyeCwZX_XRbmQmNFidcJo8QZQSaodP-wmyess

Unit tests using the gadget can be run manually with a hardware "gadget". These
unit tests all call [UsbTestGadget::Claim].

[UsbTestGadget::Claim]: https://cs.chromium.org/search/?q=UsbTestGadget::Claim&type=cs
