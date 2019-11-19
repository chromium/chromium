# Running OpenVR without a headset

On Windows, you can use a mock OpenVR component to run some basic WebXR
functionality in Chrome without connecting a VR headset. This can be useful for
reproing and investigating WebXR bugs without as much setup required.

Replace `out\debug` with wherever your build output is going.
This assumes Chrome checkout is in `c:\src\chromium\src`

1. Build the mock openvr:
```shell
autoninja -C out\debug openvr_mock
```

2. Set environment variables so we use the mock openvr
```shell
set VR_OVERRIDE=C:\src\chromium\src\out\debug\mock_vr_clients\
set VR_CONFIG_PATH=C:\src\chromium\src\out\debug
set VR_LOG_PATH=C:\src\chromium\src\out\debug
```

3. Run Chrome with WebXR and OpenVR enabled, but WMR disabled.
```shell
out\debug\chrome.exe --enable-features="WebXR,OpenVR" --disable-features="WindowsMixedReality"
```

4. Navigate to a test page, by going to this [index](https://storage.googleapis.com/chromium-webxr-test/index.html)
   clicking the link for the latest revision, and then navigating to the
   appropriate page, such as xr-barebones.html.

5. Click "Enter XR" to start an XR session that uses the mock OpenVR component.
