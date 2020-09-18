# Overview
At a high level, AR/VR (collectively known as XR) APIs are wrapped in
XRRuntimes.

Some XRRuntimes must live in the browser process, while others must not live in
the browser process.  The ones that cannot live in the browser, are hosted in a
service.

# Supported Session types

## Magic-window:
Magic window sessions are requested by sites that request poses, but render
through the normal Chrome compositor pipeline.
It serves as a basic mode that requires only some way to get orientation poses.

## Immersive:
Immersive sessions are where the site wishes to request poses, then render
content back to a display other than chrome. The common case for this is Head
Mounted Displays (HMD), like Vive, Oculus, or Daydream.

## Environment Integration
This type of session allows for environment integration by providing functions
that allow the site to query the environment, such as HitTest. A Environment
Integration session may also supply data in addition to the pose, such as a
camera frame.

# Renderer <-> Browser interfaces (defined in vr_service.mojom)
VRService - lives in the browser process, corresponds to a single frame.  Root
object to obtain other XR objects.

XRDevice - lives in the browser process, implemented as XRDeviceImpl. Allows a
client to start a session (either immersive/exclusive/presenting or
non-immersive).

VRServiceClient - lives in the renderer process.  Is notified when VRDisplays
are connected.

VRDisplayClient - lives in the renderer process.  Is notified when display
settings change.

# Renderer <-> Device interfaces (defined in vr_service.mojom)
These interfaces allow communication betwee an XRRuntime and the renderer
process.  They may live in the browser process or may live in the isolated
service.


## Data related:
All sessions need to be able to get data from a XR device.

XRFrameDataProvider - lives in the XRDevice process.  Provides a way to obtain
poses and other forms of data needed to render frames.

## Presentation related:
Presentation is exclusive access to a device, where the experience takes over
the device's display, such as presenting a stereo view in an HMD.

XRPresentationProvider - lives in the XRDevice process.  Implements the details
for a presentation session, such as submitting frames to the underlying VR API.

XRPresentationClient - lives in the renderer process.  Is notified when various
rendering events occur, so it can reclaim/reuse textures.

# Browser <-> Device interfaces (defined in isolated_xr_service.mojom)
The XRDevice process may be the browser process or an isolated service for
different devices implementations.  A device provider in the browser will choose
to start the isolated device service when appropriate.

XRRuntime - an abstraction over a XR API.  Lives in the XRDevice process.
Exposes a way for the browser to register for events, and start sessions (Magic
Window or Presentation).

XRSessionController - Lives in the XRDevice process.  Allows the browser to
pause or stop a session (MagicWindow or Presentation).

XRRuntimeEventListener - Lives in the browser process.  Exposes runtime events
to the browser.

# Browser <-> XRInput interfaces (defined in isolated_xr_service.mojom)
IsolatedXRGamepadProvider and IsolatedXRGamepadProviderFactory - Live in the
XRInput process, and allow GamepadDataFetchers living in the browser process
to expose data from gamepads that cannot be queried from the browser process.

The XRInput process may be the browser process or a separate process depending
on the platform.

# Test interfaces (defined in browser_test_interfaces.mojom)
XRTestHook allows a test to control the behavior of a fake implementation of
OpenXR, and potentially other runtimes.  This allows testing the entire stack
of Chromium WebXR code end-to-end.
