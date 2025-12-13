# WebXR Mojom Test Interfaces

## Introduction

This directory contains both the mojom test interfaces and the types to which
those values are ultimately typemapped. The test documentation can be found in
`//chrome/browser/vr/test/xr_browser_tests.md`

## ... But why is it in product code?

While on Android the "WebXR Device" specific code (e.g. the code responsible for
talking to the runtime) runs in the browser process, on Windows, it has to run
as a utility process. In order to setup the test hooks in the OpenXR runtime on
this process, we thus have to plumb them into parts of the product code, and
along the mojom pipes therein to get a connection to the device process.

That being said, we expect that *only* the mojom interface(s) will need to be
included in product code (and even then, a forward declaration *should* be
enough). DO NOT include the typemapped types in product code.

Please refer to `//components/webxr/README.md`, for the WebXR architecture
overview and links to the various components.

## Type Complexity

While WebXR *does* have multiple potential backends, these types are only used
for the `xr_browser_tests` and `android_browsertests`, which support the OpenXR
runtime. In order to test the OpenXR runtime, we build a mock version of that
runtime, which is in fact its own independent shared library. Its test
architecture is thus such that type data is pushed into it across a library
boundary, and then queried via the OpenXR APIs. In order to prevent allocation
errors with tests, all types that are pushed into it need to be simple types
without complex destructors. In order to ease the writing of tests, all values
returned over the mojom interfaces here **SHOULD** have their own typemap, which
**SHOULD NOT** have a complex destructor, thus allowing this type to be passed
to and from the `//device/vr/test/test_hook.h` interface directly.
