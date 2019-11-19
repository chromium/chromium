# WebUSB Blink Module

`renderer/modules/webusb` implements the renderer process details and bindings for the [WebUSB specification]. It communicates with the browser process through the [WebUsbService Mojo interface] which will connect to the DeviceService through [public Mojo interface] for the [UsbService].

[WebUSB specification]: https://wicg.github.io/webusb/
[WebUsbService Mojo interface]: ../../../public/mojom/usb/web_usb_service.mojom
[public Mojo interface]: ../../../../../services/device/public/mojom
[UsbService]: ../../../../../services/device/usb/usb_service.h


## Testing

WebUSB is primarily tested in [Web Platform Tests].
Chromium implementation details are tested in [web tests].

[Web Platform Tests]: ../../../web_tests/external/wpt/webusb/
[Web tests]: ../../../web_tests/usb/
