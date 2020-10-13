# VR

`device/vr` abstracts [WebXR](https://immersive-web.github.io/webxr/) features
across multiple platforms.

Although the directory is named "vr" both VR and AR features are implemented
here. The name is an artifact from the time when it was associated with the now
deprecated WebVR API.

## Platform support

Windows and Android are the primary supported platforms, largely because they're
the only platforms with significant XR device traction at this point. Several
native APIs are supported for various use cases, though things are trending
towards OpenXR being the only API used on desktops.

| API                   | OS       | Supports | Enabled by Default |
|-----------------------|:--------:|:--------:|:------------------:|
| OpenXR                | Windows* | VR*      | Yes**              |
| AR Core               | Android  | AR       | Yes                |
| Google VR             | Android  | VR       | Yes                |
| Windows Mixed Reality | Windows  | VR       | No                 |

 - \* OpenXR may support multiple OSes and AR use cases as well. Currently we
   only use it for VR on Windows since that's what the majority of existing
   runtimes support.
 - ** OpenXR runtimes are only enabled by default if they implement the
   "[XR_EXT_win32_appcontainer_compatible](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#XR_EXT_win32_appcontainer_compatible)"
   extension.

Integrations with some APIs (such as AR Core) are partially locaed in
chrome/browser/vr due to architectural and historical limitations. In the future
those will ideally be migrated to this directory as well.

## Testing
See [XR Browser Tests documentation](../../chrome/vr/test/xr_browser_tests.md).
