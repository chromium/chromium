# Crubit in Chromium

[Crubit](https://crubit.rs) is a C++/Rust interop tool.  It has two parts:

*   [`cpp_api_from_rust.md`](./cpp_api_from_rust.md) describes how to enable
    C++ to call Rust.  This direction of Crubit interop
    is fully supported in Chromium (but see the "Caveats" section below).
*   `rust_api_from_cpp` enables Rust to call C++.
    It is not yet officially supported in Chromium.
    Information about partial, experimental support can be found in
    [`rust_api_from_cpp.md`](./rust_api_from_cpp.md).

Notes:

*   See [`//docs/rust/ffi.md`](../ffi.md) for an overview of other Rust interop
    tools (e.g. `cxx` or `bindgen`).  These tools remain fully supported
    (i.e. there is no deprecation schedule that would require actively migrating
    existing `cxx::bridge` code to Crubit).
*   Once both interop directions are supported we will consider recommending
    Crubit as the primary C++/Rust interop solution for Chromium.

## Caveats

*   **Some directories cannot use Crubit:** The Android project's
    Soong/bp build system
    does not support Crubit at this point. Consequently, Crubit cannot be
    used in `//base`, `//net`, or
    [other directories](https://source.chromium.org/chromium/chromium/src/+/main:components/cronet/android/dependencies.txt)
    that [Cronet](../../../components/cronet/README.md) depends on.
    This is tracked in https://crbug.com/535682335.
    * Note that it's fine to use Crubit in code that _runs_ on Android
      (e.g. the [QR code generator](https://crrev.com/c/7749970)), so
      long as it doesn't need to be compiled by the Android project.

*   **2nd-party project limitations:** In principle, Crubit will work in
    projects like PDFium or V8 as well. However, such projects usually
    support non-Chromium clients or alternative toolchains that may lack
    Crubit support. In order to use Crubit in a 2nd-party project, that
    project must first:
    * Make a policy decision to only support clients that have Crubit available,
      and/or help their clients set up Crubit support.
    * Enable Crubit in their build system by
      [providing `//build_overrides/crubit.gni`](https://source.chromium.org/chromium/chromium/src/+/main:build/rust/gni_impl/cpp_api_from_rust.gni;l=59-62;drc=51d2448c9b469ac9a7e5fd349a624c71291a7510).

## Other docs

* Generic, Chromium-agnostic documentation of Crubit can be found at
  https://crubit.rs.
  Note that some examples are Bazel-specific, but most of the documentation
  should still apply to Chromium.
* Google-internal Crubit documentation can be found at
  [go/crubit](https://goto2.corp.google.com/crubit)
    * This is mostly the same content as above, but is mentioned here because it
      includes a few extra things like document freshness and owner metadata,
      link to a Google-internal chatroom, etc.)
* Crubit's Discord server can be joined using the following invite link:
  https://discord.gg/nHq5fdADKV
* TODO: Cover Crubit in
  [Chromium/FFI chapter of Comprehensive Rust course](https://google.github.io/comprehensive-rust/chromium/interoperability-with-cpp.html)

## How to report bugs or feature requests

* Googlers can use the [go/crubit-bug](https://goto.google.com/crubit-bug) short
  link to report a Crubit bug or a feature request.
* Alternatively, please open an issue at
  https://github.com/google/crubit/issues/new
