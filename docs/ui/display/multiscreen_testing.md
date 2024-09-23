# Multi-Screen Testing in Chromium

Chromium supports testing multi-screen environments under `interactive_ui_tests`
on most desktop platforms through the `VirtualDisplayUtil` ([`ui/display/test/virtual_display_util.h`](/ui/display/test/virtual_display_util.h)) interface.
This interface instantiates virtual displays at the operating system level to
simulate a multi-display environment being presented to Chromium. For more
information on the background and design, see ["Virtual Displays For Automated Tests"](https://docs.google.com/document/d/1rtxO2FEg0Zl_-oXHzIBsJo6py7wkySUpYruteNMlPys/edit?resourcekey=0-yLkX6DGPwNFn1ARMpM-zLQ#heading=h.in0m2co51p2p).

### How to use
Simply invoke `display::test::VirtualDisplayUtil::TryCreate` to create an
instance of `VirtualDisplayUtil`. The function will return `nullptr` on
platforms that are unsupported, or not implemented. Tests should generally skip
(`GTEST_SKIP()`) when this function returns `nullptr`.

### Supported Platforms
Mac, Linux (X11), and Windows are supported.

ChromeOS mutli-screen testing is supported via
`display::test::DisplayManagerTestApi` and does not yet have a
`VirtualDisplayUtil` implementation.

### Special Considerations

#### MacOS
Requires macOS version 11.0+. The implementation relies
on undocumented CoreGraphics APIs and risks breakage at any time. There is an open [request](https://feedbackassistant.apple.com/feedback/12349099) for official API support.

#### Windows

On Windows, **administrative access** is required for the utility to function.
You must run the chrome/test binary with elevated privileges.

Windows hosts must have a special driver installed for the utility to work.
The driver and controller source code is located at
[`//third_party/win_virtual_display`](/third_party/win_virtual_display).
The pre-built driver is available in CIPD at the path [`chromium/third_party/win_virtual_display/windows-amd64`](https://chrome-infra-packages.appspot.com/p/chromium/third_party/win_virtual_display/windows-amd64). Developers may download the
CIPD package and manually install the driver for local development:

1. On the [`CIPD page`](https://chrome-infra-packages.appspot.com/p/chromium/third_party/win_virtual_display/windows-amd64): Pick the latest instance.
2. Click Download and extract the zip file somewhere (i.e. C:\virtual-display-driver)
3. Install the certificates and driver. Open PowerShell as Administrator and
run the following 3 commands:
```ps
Import-Certificate -FilePath C:\virtual-display-driver\ChromiumVirtualDisplayDriver.cer -CertStoreLocation Cert:\LocalMachine\AuthRoot
Import-Certificate -FilePath C:\virtual-display-driver\ChromiumVirtualDisplayDriver.cer -CertStoreLocation Cert:\LocalMachine\TrustedPublisher
pnputil.exe /add-driver C:\virtual-display-driver\ChromiumVirtualDisplayDriver.inf /install
```

#####

#### Linux (X11)

Linux requires an X11 environment that supports virtual outputs in XrandR.
It is highly recommended to use [`//testing/xvfb.py`](/testing/xvfb.py) to set
up a virtual X server that supports the utility. For example, the following
command will run tests under a virtual X server environment that works with the
utility:

```bash
python3 testing/xvfb.py --use-xorg $(pwd)/out/current_link/interactive_ui_tests --gtest_filter=*VirtualDisplayUtil*
```

Running outside of `xvfb.py` directly on the host X server should work, but is
not explicitly documented here.
The host must have `xserver-xorg-core`, `xserver-xorg-video-dummy` and `x11-xserver-utils` packages installed. See [`//testing/xvfb.py`](/testing/xvfb.py) for more details on the
required Xorg/XRandR setup.
