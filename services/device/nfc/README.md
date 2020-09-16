# NFC

### High level Overview

The implementation of Web NFC in Chromium consists of two main parts:
The NFC module in Blink located at `third_party/blink/renderer/modules/nfc/` which
contains Blink JavaScript bindings for Web NFC and the browser side platform
level adaptation that is located at `services/device/nfc`. The Blink NFC module
communicates with the browser adaptation through NFC Mojo interface defined in
the `services/device/public/mojom/nfc.mojom` file and implemented by the
`services/device/nfc` module.

NDEFWriter and NDEFReader are the two primary interfaces of the Web NFC APIs.

The NDEFWriter interface has the write method for writing data to NFC tags.
This method will return a promise, which will be resolved when the
message is successfully written to a NFC tag or be rejected when errors
happened or the process is aborted by setting the AbortSignal in the
NDEFWriteOptions.

The NDEFReader interface has the scan method to try to read data from any NFC tag
that comes within proximity. Once there is some data found an
NDEFReadingEvent carrying the data will be dispatched to the NDEFReader.

The most important classes for Android adaptation are

[NfcImpl](../../../services/device/nfc/android/java/src/org/chromium/device/nfc/NfcImpl.java),
[NfcTagHandler](../../../services/device/nfc/android/java/src/org/chromium/device/nfc/NfcTagHandler.java)
and
[NdefMessageUtils](../../../services/device/nfc/android/java/src/org/chromium/device/nfc/NdefMessageUtils.java).

## Web-exposed Interfaces

### [NFC specification ](https://w3c.github.io/web-nfc/)

## Testing:

* Web platform tests are located in
`third_party/blink/web_tests/external/wpt/web-nfc/` and are a mirror of the
[web-platform-tests GitHub repository](https://github.com/web-platform-tests/wpt).
* NFC platform unit tests files for Android are
[NFCTest.java](../../../services/device/nfc/android/junit/src/org/chromium/device/nfc/NFCTest.java) and
[NfcBlocklistTest.java](../../../services/device/nfc/android/junit/src/org/chromium/device/nfc/NfcBlocklistTest.java)

## Security and Privacy

Web NFC API can be only accessed by top-level secure browsing contexts and user
permission is required to access NFC functionality. Web NFC API specification
addresses security and privacy topics in chapter [7. Security and Privacy](https://w3c.github.io/web-nfc/#security).


## Permissions

The device service provides no support for permission checks. When the render
process requests access to a NFC this request is proxied through the browser
process by [NfcPermissionContext](../../../chrome/browser/nfc/nfc_permission_context.h)
which is responsible for checking the permissions granted to the requesting origin.


## Platform Support

At the time of writing, only Android platform is supported.


## Design Documents

Please refer to the [design documentation](https://sites.google.com/a/chromium.org/dev/developers/design-documents/web-nfc)
for more details.
