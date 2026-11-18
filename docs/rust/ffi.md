# Rust FFI

This document provide guidance for C++/Rust interoperation using FFI (foreign
function interfaces).

[TOC]

## Which tool should I use?

For interoperating with **C++ code**, we recommend
[the `cxx` crate](https://cxx.rs/). See [`cxx.md`](cxx.md) for Chromium-specific
guidance, and
[the `cxx` chapter](https://google.github.io/comprehensive-rust/chromium/interoperability-with-cpp.html)
in the Chromium section of the Comprehensive Rust course for an introduction.

If you only need to use a **C APIs** from Rust (and not the other direction),
you can use the [`bindgen`](https://rust-lang.github.io/rust-bindgen/) tool.
Bindgen will automatically create Rust equivalents of the input C code, which
you can then use from your own Rust code. For example, this project uses bindgen to
[import the Mojo C API](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/rust/c_mojo_api/BUILD.gn;l=30;drc=84d3affd8824ebaa8ab04d0f20bcebb64b484704).
For documentation of various options, see `//build/rust/rust_bindgen.gni`.

Bindgen can also be used to import C++ headers, but it only supports a limited
subset of the language.

**In the future**, we intend to recommend [Crubit](https://crubit.rs) for new
code (tracked in <https://crbug.com/351793625>). Chromium now supports
`cc_bindings_from_rs` (aka `cpp_api_from_rust`) for calling Rust from C++.
The reverse direction (Rust calling C++) is enabled for experimentation, but is
not yet officially supported (note that the Android build system does not yet
support it, so it is unavailable in dependencies of Cronet - see
https://crbug.com/576651451). See [`crubit/README.md`](crubit/README.md) for
details.

Chromium **does not support any other FFI tools** (e.g.
[`cbindgen`](https://github.com/mozilla/cbindgen) or
[`zngur`](https://github.com/HKalbasi/zngur)), and we do not plan to support
them in the future.

## Safety considerations

All FFI code is `unsafe` by definition, since the Rust compiler cannot enforce
its rules on non-Rust code. This means that when working with FFI, you must be
careful to understand exactly what behaviors are possible. Make sure you're
familiar with our [safety guidelines](unsafe.md#how-to-use-unsafe).
So long as you're consistent about writing and following safety comments, you
can avoid most problems.

* **Widespread types**: Some types are widely used throughout Chromium's C++
  codebase, and auditing every usage for safety is infeasible. In such cases,
  you may assume that C++ does not already contain undefined behavior.
  * For example, you may assume that mutations to the
  [static per-process `CommandLine` variable](https://source.chromium.org/chromium/chromium/src/+/main:base/command_line.cc;l=38;drc=5496fbe366e6796c5b549c65a2cb267b5c933e35)
  are not already racy, even though there isn't specific code to enforce that.

For `cxx`-specific safety considerations (opaque and shared types), see
[`cxx.md`](cxx.md#safety-considerations).
