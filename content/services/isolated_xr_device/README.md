# Isolated XR Device Service
_For a more thorough/high level overview of the entire WebXR stack, please refer
to [components/webxr][components-webxr-readme]._

Chromium's WebXR implementation makes use of the multiprocess architecture for
added security. Thus all code which directly interfaces with and talks to the
XR hardware ("runtimes") ends up loaded/hosted in a separate XR Utility process,
except on Android where it runs in-process within the browser process
(primarily to ensure access to and manipulation of Android SurfaceViews for
rendering, and secondarily to minimize process overhead).

The `XrDeviceService` serves as the entry point for the `VRServiceImpl` in
`content/browser/xr` to talk to this process, while the
`IsolatedXRRuntimeProvider` is the main in-process entry point. The runtime
provider continually polls for supported runtimes and, when a change is
detected, creates and returns the appropriate runtime over Mojo. The Mojo
interfaces used by this process are defined in `device/vr`.

## Testing

To support browser tests without coupling production service definitions to
test-only Mojom, `XRDeviceService::BindHookForTesting` accepts a type-erased
`mojo::ScopedMessagePipeHandle` and forwards it to `device::OpenXrPlatformHelper::BindHookForTesting`.
In test targets, this pipe is unwrapped by the static test hook registrar and
bound to `OpenXrTestHelper`. See
[`//chrome/browser/vr/test/xr_browser_tests.md`](../../../chrome/browser/vr/test/xr_browser_tests.md)
and [`//device/vr/public/mojom/test/README.md`](../../../device/vr/public/mojom/test/README.md)
for complete details.

[components-webxr-readme]: ../../../components/webxr/README.md
