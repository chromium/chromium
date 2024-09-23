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
*   `UsbServiceMac` in progress.
*   `UsbServiceWin` done.


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


### Manual Testing

When making changes to platform-specific code the following manual test steps
should be run to augment automated testing, which is mostly limited to
platform-independent logic. These tests require an Android phone with USB
debugging enabled and support for USB tethering. When USB debugging is enabled
the device creates a vendor-specific interface for the ADB protocol. On
Windows, with the [OEM USB drivers] installed this interface will have the
WinUSB.sys driver loaded. When USB tethering is enabled the device creates
an RNDIS interface for which most operating systems have a built-in driver.
With both of these features enabled the device will have two interfaces and
thus be a "composite" device. This is important for testing on Windows as
composite and non-composite devices must be handled differently.

[OEM USB drivers]: https://developer.android.com/studio/run/oem-usb

#### Steps

1.  [Enable USB debugging] and check that the **USB tethering** option is
    disabled under **Network & Internet > Hotspot & Tethering** in the
    phone's setting app.
2.  Connect phone to the system under test.
3.  Launch Chrome.
4.  Load `chrome://usb-internals`.
5.  Select the **Devices** tab.
6.  Find the phone in the list. Ensure that the **Manufacturer name**,
    **Product name** and **Serial number** columns are all populated for this
    device.
7.  Click the **Inspect** button next to this device.
8.  Click the **Get Device Descriptor** button at the bottom of the page.
9.  Click the **GET** buttons next to **Manufacturer String**,
    **Product String** and **Serial number** fields.
10. Check that the values which appear match the ones seen previously.
11. Load `chrome://inspect` and ensure that the **Discover USB devices**
    option is checked.
12. Check that the phone appears as an available device. It may appear as
    "Offline" until the **Allow** button is tapped on the "Allow USB debugging"
    prompt which appears on the device. It may take some time for the device to
    appear.
13. Launch Chrome on the phone and ensure that the tabs open on the phone are
    available for inspection.
14. Enable USB tethering on the phone and repeat steps 4 through 13. This will
    test hotplugging of a composite device as enabling USB tethering causes
    the device to reconnect with multiple interfaces.
15. Disable USB tethering on the phone and repeat steps 4 through 13. This will
    test hotplugging of a non-composite device as disabling USB tethering
    causes the device to reconnect with a single interface.
16. Close Chrome and re-enable USB tethering on the phone.
17. Repeat steps 3 through 13 for a final time. This will test enumeration of a
    composite device on Chrome launch.

[Enable USB debugging]: https://developer.android.com/studio/debug/dev-options#enable
