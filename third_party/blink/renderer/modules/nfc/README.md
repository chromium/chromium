# Web NFC Blink Module

`renderer/modules/nfc` implements the renderer process details and bindings for
the [Web NFC specification]. It communicates with the browser process through the
[Web NFC Mojo interface]. The platform-specific parts of the
implementation are located in `services/device/nfc`.

[Web NFC specification]: https://w3c.github.io/web-nfc/
[Web NFC Mojo interface]: ../../../../../services/device/public/mojom/nfc.mojom


## Testing

Web NFC is primarily tested in [Web Platform Tests](https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/web_tests/external/wpt/web-nfc/).


## Design Documents

Please refer to the [design documentation](https://sites.google.com/a/chromium.org/dev/developers/design-documents/web-nfc)
for more details.