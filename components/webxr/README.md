# WebXR Component

## WebXR Overview
The web-exposed interface to WebXR begins in [Blink](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/xr/README.md).
This code (with the help of the `VRService` [mojom interface](https://source.chromium.org/chromium/chromium/src/+/main:device/vr/public/mojom/README.md))
talks with the [browser process](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/xr/README.md)
to broker a connection directly with the corresponding [device code](https://source.chromium.org/chromium/chromium/src/+/main:device/vr/README.md).
Note that this device code is often hosted in a separate XR utility process, and
thus the [isolated_xr_device service](https://source.chromium.org/chromium/chromium/src/+/main:content/services/isolated_xr_device/README.md)
needs to assist the browser in brokering these connections. The code that talks
directly with a device or its corresponding SDK/API (e.g. OpenXR) is often
referred to as a "Runtime" throughout XR code. It is responsible for querying or
formatting the data into/out of the expected WebXR formats.

## Component Code
This component code may depend on code in both //device and //content. It is
intended for code that is necessary for a given runtime to work, but cannot be
added under //device due to layering violations. Often this is because there may
need to be customizable extension points added for different embedders. This
includes code such as rendering utilizing the viz framework, or extension
methods for embedders to customize the install flow for some runtimes.
