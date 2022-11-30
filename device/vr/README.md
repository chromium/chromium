# VR

_For a more thorough/high level overview of the entire WebXR stack, please refer to
[components/webxr](https://source.chromium.org/chromium/chromium/src/+/main:components/webxr/README.md)_

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

 - \* OpenXR may support multiple OSes and AR use cases as well. Currently we
   only use it for VR on Windows since that's what the majority of existing
   runtimes support.
 - ** OpenXR runtimes are only enabled by default if they implement the
   "[XR_EXT_win32_appcontainer_compatible](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#XR_EXT_win32_appcontainer_compatible)"
   extension.

Integrations with some APIs (such as AR Core) are partially locaed in
chrome/browser/vr due to architectural and historical limitations. In the future
those will ideally be migrated to this directory as well.

## Windows Build setup
On Windows, the device code runs in a sandboxed utility process. The Chrome
installers (e.g. mini_installer) and test setup scripts, will ensure that the
appropriate permissions (ACLs) are set on the install directory to run in the
sandbox. However, if you wish to build and run the code from your local out
directory directly, you will need to either disable the sandbox (not recommended
as it could lead to an accidental compat issue) or manually set these
permissions. The permissions will persist, so the commands need only be run once
per out-directory.

To set the appropriate ACLs run:
```
icacls <chromium out directory> /grant *S-1-15-3-1024-3424233489-972189580-2057154623-747635277-1604371224-316187997-3786583170-1043257646:(OI)(CI)(RX)
icacls <chromium out directory> /grant *S-1-15-3-1024-2302894289-466761758-1166120688-1039016420-2430351297-4240214049-4028510897-3317428798:(OI)(CI)(RX)
```

## Testing
See [XR Browser Tests documentation](../../chrome/vr/test/xr_browser_tests.md).
