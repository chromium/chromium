# NFC

### High level Overview

The implementation of Web NFC in Chromium consists of two main parts:

- The NFC module in Blink located at `third_party/blink/renderer/modules/nfc/`
  which contains Blink JavaScript bindings for Web NFC.
- The browser side platform level adaptation located at `services/device/nfc`.

The Blink NFC module communicates with the browser adaptation through NFC Mojo
interface defined in the `services/device/public/mojom/nfc.mojom` file and
implemented by the `services/device/nfc` module.

NDEFReader is the primary interface of Web NFC. The NDEFReader interface has
write, makeReadOnly, and scan methods:

- The write method is for writing data to an NFC tag. This method returns a
  promise, which will be resolved when the message is successfully written to an
  NFC tag, or rejected either when errors happened or process is aborted by
  setting the AbortSignal in the NDEFWriteOptions.
- The makeReadOnly method is for making an NFC tag permanently read-only. This
  method returns a promise, which will be resolved when an NFC tag has been made
  read-only, or rejected either when errors happened or process is aborted by
  setting the AbortSignal in the NDEFMakeReadOnlyOptions.
- The scan method tries to read data from any NFC tag that comes within
  proximity. Once there is some data found, an NDEFReadingEvent carrying the
  data is dispatched to the NDEFReader.

The most important classes for Android adaptation are [NfcImpl], [NfcTagHandler],
and [NdefMessageUtils].

[NfcImpl]: ../../../services/device/nfc/android/java/src/org/chromium/device/nfc/NfcImpl.java
[NfcTagHandler]: ../../../services/device/nfc/android/java/src/org/chromium/device/nfc/NfcTagHandler.java
[NdefMessageUtils]: ../../../services/device/nfc/android/java/src/org/chromium/device/nfc/NdefMessageUtils.java

## Web-exposed Interfaces

### [Web NFC specification](https://w3c.github.io/web-nfc/)

## Testing:

- Web platform tests are located in
  `third_party/blink/web_tests/external/wpt/web-nfc/` and are a mirror of the
  [web-platform-tests GitHub repository].
- NFC platform unit tests files for Android are [NFCTest.java] and
  [NfcBlocklistTest.java]

[web-platform-tests github repository]: https://github.com/web-platform-tests/wpt
[nfctest.java]: ../../../services/device/nfc/android/junit/src/org/chromium/device/nfc/NFCTest.java
[nfcblocklisttest.java]: ../../../services/device/nfc/android/junit/src/org/chromium/device/nfc/NfcBlocklistTest.java

## Security and Privacy

Web NFC API can be only accessed by top-level secure browsing contexts that are
visible. User permission is required to access NFC functionality. Web NFC
specification addresses security and privacy topics in "[Security and Privacy]"
chapter.

[Security and Privacy]: https://w3c.github.io/web-nfc/#security

## Permissions

The device service provides no support for permission checks. When the renderer
process requests access to NFC, this request is proxied through the browser
process by [NfcPermissionContext] which is responsible for checking the
permissions granted to the requesting origin.

[NfcPermissionContext]: ../../../chrome/browser/nfc/nfc_permission_context.h

## Platform Support

At the time of writing, only Android platform is supported.

## Design Documents

Please refer to the [design documentation] for more details.

[design documentation]: https://sites.google.com/a/chromium.org/dev/developers/design-documents/web-nfc
