# OpenXR

*The OpenXR specification writes its name as 'OpenXR'; however, the Chrome
style guide requires us to write this as 'OpenXr' in code, when talking about
the system as a whole, we will use 'OpenXR' and will use 'OpenXr' only for
specific classes for accuracy.*

This directory contains Chrome's OpenXR integration. That is to say, this
code translates WebXR requests into OpenXR API Calls, and translates the data
from OpenXR into the more generic device interfaces (which are typically mojom),
that ultimately get translated into WebXR types.

## General Architecture

The main entry point to OpenXR is via an [`OpenXrDevice`](openxr_device.h).
Depending on the platform, this may be directly created by a more general
purpose device provider (e.g. Windows and the [Isolated Xr Device service][xr_device_service])
or by a specific `OpenXrDeviceProvider` (e.g. [Android's](../../../components/webxr/android/openxr_device_provider.h)).
It is a good idea to try to create both an `XrInstance` and an `XrDevice` via
the OpenXR API before creating an `OpenXrDevice`, as that will indicate that a
session can *actually* be created. This `OpenXrDevice`, when requested for a
session will create and maintain an [`OpenXrRenderLoop`](openxr_render_loop.h).
This `OpenXrRenderLoop` will create an [`OpenXrApiWrapper`](openxr_api_wrapper.h),
which is largely responsible for handling the `XrSession` object. The
`OpenXrRenderLoop` and `OpenXrApiWrapper` between themselves will create a
number of helper objects to abstract various aspects of the API (e.g. [OpenXrInputHelper](openxr_inut_helper.h)
and [OpenXrExtensionHelper](openxr_extension_helper.h)). Classes that depend
solely on the core spec can be created directly by the render loop or API
wrapper; but classes that rely on extension methods should be created by the
extension helper.

## Platform Support

Currently, we only support OpenXR on Android and Windows. The vast majority of
the code that we use is cross-platform, but some things (i.e. rendering) are
inherently platform-specific. The `OpenXrPlatformHelper` is used to abstract out
anything that is especially platform-specific, including deciding what kind of
`OpenXrGraphicsBinding` should be used (e.g. to use DirectX on Windows and
OpenGLES on Android). At a minimum to extend platform support you will likely
need to create an implementation of these two interfaces. The specific platform
helper will likely be chosen by either platform-specific ifdefs at construction
or via the device provider mechanism.

## OpenXR Extensions

OpenXR methods provided by an extension and tied to a session should be created
via and stored on the `OpenXrExtensionHelper` and corresponding
`OpenXrExtensionMethods` struct. This helps to avoid multiple instances of a
class that wants to use the method needing to load the method. These methods
should either be checked for their own validity or that the extension which
guards them is enabled before their use. The base class [`OpenXrPlatformHelper`](openxr_platform_helper.h)
is ultimately responsible for building the list of extensions that we wish to
enable on a session based on whether the functionality is required or optional.

Code that leverages extensions wholly or in part to supply the necessary data to
WebXR should typically be wrapped in their own classes, with the ordering in [`GetExtensionHandlerFactories`](openxr_extension_handler_factories.cc)
serving as the factory/arbiter of which class is created based upon the
prioritization of classes due to enabled extensions. This enables us to support
WebXR features with a variety of extensions when multiple ones exist that
surface similar functionality in ways that are (usually) device specific.
Support for these classes and a declaration of their requirements (in the form
of needed extensions), is provided by implementing an [`OpenXrExtensionHandlerFactory`](openxr_extension_handler_factory.h)
and adding it to the [`GetExtensionHandlerFactories`](openxr_extension_handler_factories.cc)
list. Entries in the list should generally be grouped by the type of data that
they can handle and then ordered by priority in that group (the creation code
will create the first handler that provides the data that it wants), with
platform-specific extensions usually coming first.

Extension handlers for XR_EXT_ and XR_KHR_ extensions should be placed in
//device/vr/openxr, whereas code to handle other extensions should be placed in
a subfolder matching the extension vendor (e.g. fb/ for XR_FB_ or msft/ for
XR_MSFT_).

## Input

All input sources for WebXR must support at a bare minimum the ability to be
tracked and to send up a "primary input button" (essentially a click). Adding
a new input source will always require an entry to [openxr_interaction_profile_type.mojom](../public/mojom/openxr_interaction_profile_type.mojom),
and an entry to `GetOpenXrInputProfilesMap` in [openxr_interaction_profiles.cc](openxr_interaction_profiles.cc)
to define the types of profiles that are supported. From there there are two
potential paths to finish adding support via OpenXR.

### Interaction Profiles

If the device has an associated set of interaction profiles that can be bound to
actions, this is the easiest (and preferred) way to add support. A button map
simply needs to be built in `GetOpenXrControllerInteractionProfiles` in [openxr_interaction_profiles.cc](openxr_interaction_profiles.cc).
Any required extension for the profile will automatically be enabled on the
session if the runtime supports it by the setup code. New button types can be
added and extended as needed. The base path of the interaction profile must be
defined as a constant in [openxr_interaction_profile_paths.h](openxr_interaction_profile_paths.h),
for compatibility with tests. If the interaction profile should have hand input
enabled for it, the required extension should also be added to the list in the
[OpenXrHandTrackerHandlerFactory](openxr_hand_tracker.h).

### Hand Gesture Extensions

The secondary means of adding interaction profile support depends upon extension
methods to the XR_EXT_hand_tracking structs. After the initial enum and profile
map has been added, simply extend [OpenXrHandTracker](openxr_hand_tracker.h),
which has some more detailed instructions. [OpenXrHandTrackerAndroid](android/openxr_hand_tracker_android.h)
is an example of a class that extends `OpenXrHandTracker` to provide such
support. Note that you are still responsible for ensuring that your extension is
enabled when available and providing a means to create your new class as
described in [OpenXR Extensions](#openxr-extensions).

[xr_device_service]: https://source.chromium.org/chromium/chromium/src/+/main:content/services/isolated_xr_device/README.md
