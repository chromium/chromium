# XR Browser implementation
_For a more high level overview of the entire WebXR stack, please refer to
[components/webxr](https://source.chromium.org/chromium/chromium/src/+/main:components/webxr/README.md)._

This folder contains trusted XR code. It is largely responsible for either
communicating with other trusted code, or to broker connections and
communication between either other trusted code or the renderer process and the
various XR device runtimes (which are hosted in a separate process).

The primary entrypoint is the [`VRServiceImpl`][vr service impl], which
implements the [`VRService`][vr service] mojom interface. This service is
responsible for brokering connections between the renderer process and the
device process, both for the sake of starting up an XR Session, as well as
simply querying support for a session. Each browsing context ends up creating
its own [`VRService`][vr service]. A [`VRServiceImpl`][vr service impl] instance,
when tracking a session, also leans on code from the ./metrics directory to log
various metrics about that session, both at creation and termination.

The [`XRRuntimeManager`][runtime manager] is a singleton component which
maintains references to [`BrowserXrRuntime`][xr runtime] objects representing
the various devices and/or sensor groups that could back an XR Session. The
[`XRRuntimeManager`][runtime manager] is responsible for tracking the state of
the hardware and aiding the multiple [`VRServices`][vr service impl] with
communicating/tracking this state.

When necessary, [`XRFrameSinkClient`][xr frame sink] instances aid communication
with viz for runtimes which utilize that component to manage their own
compositing.

Some chrome-specific customizations based on this content implementation can be
found in [chrome/browser/vr](https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/vr)

[vr service impl]: https://chromium.googlesource.com/chromium/src/+/HEAD/content/browser/xr/service/vr_service_impl.h
[vr service]: https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/public/mojom/vr_service.mojom
[runtime manager]: https://chromium.googlesource.com/chromium/src/+/HEAD/content/browser/xr/service/xr_runtime_manager_impl.h
[xr runtime]: https://chromium.googlesource.com/chromium/src/+/HEAD/content/browser/xr/service/browser_xr_runtime.h
[xr frame sink]: https://chromium.googlesource.com/chromium/src/+/HEAD/content/browser/xr/service/xr_frame_sink_client_impl.h
