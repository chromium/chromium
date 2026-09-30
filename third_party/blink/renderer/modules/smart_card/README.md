# Web Smart Card Blink Module (`third_party/blink/renderer/modules/smart_card`)

This directory contains the Blink renderer implementation of the
[Web Smart Card API](https://github.com/WICG/web-smart-card), enabling Isolated
Web Apps (IWAs) to communicate with PC/SC smart card readers via
`navigator.smartCard`.

## Companion Documentation

- **Core IWA Component:**
  [`//components/webapps/isolated_web_apps`](/components/webapps/isolated_web_apps/README.md)

## Key Components

- `SmartCardResourceManager` (`navigator.smartCard`): `Supplement<NavigatorBase>`
  exposed on `Navigator` in isolated contexts; establishes a PC/SC context
  (`establishContext()`) via `blink::mojom::blink::SmartCardService`.
- `SmartCardContext`: Lists available readers (`listReaders()`), tracks reader
  status changes (`getStatusChange()`), and connects to a smart card
  (`connect()`).
- `SmartCardConnection`: Executes APDU `transmit()`, `control()`,
  `getAttribute()`, `setAttribute()`, `status()`, `disconnect()`, and atomic
  `startTransaction()` operations via
  `device::mojom::blink::SmartCardConnection`.
- `SmartCardCancelAlgorithm` & `SmartCardError`: Implements `AbortSignal`
  cancellation (`SmartCardCancelAlgorithm`) and maps
  `device::mojom::blink::SmartCardError` codes to Web IDL `SmartCardError` and
  `DOMException` instances.
