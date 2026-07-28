# WebXR Mojom Test Interfaces

## Introduction

This directory contains both the mojom test interfaces and the types to which
those values are ultimately typemapped. The test documentation can be found in
`//chrome/browser/vr/test/xr_browser_tests.md`

## ... But why is it in product code?

While on Android the "WebXR Device" specific code (e.g. the code responsible for
talking to the runtime) runs in-process within the browser process (to minimize
resource overhead), on Windows, it runs in a sandboxed utility process (the
`isolated_xr_device` service) for security. In order to setup the test hooks in the
OpenXR runtime on Windows, we thus have to plumb them into parts of the product
code, and along the mojom pipes therein to get a connection to the utility process.

That being said, we expect that *only* the mojom interface(s) will need to be
included in product code (and even then, a forward declaration *should* be
enough). DO NOT include the typemapped types in product code.

Please refer to `//components/webxr/README.md`, for the WebXR architecture
overview and links to the various components.

## Type Complexity

While WebXR *does* have multiple potential backends, these test interfaces are only
used for `xr_browser_tests` and `android_browsertests`, which test the OpenXR runtime.
In order to test OpenXR, we build a fake OpenXR implementation (`openxr_test_helper`)
that is embedded directly into the target process (the browser test process on
Android, or the sandboxed `isolated_xr_device` utility process on Windows). Due to
requirements of how the `third_party` OpenXR loader works, a lightweight trampoline
shared library (`openxr_mock`) redirects OpenXR loader calls back to this embedded
implementation via a function dispatch table.
