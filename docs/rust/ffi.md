# Rust FFI

This document provide guidance for C++/Rust interoperation using FFI (foreign
function interfaces).

[TOC]

## Which tool should I use?

For interoperating with **C++ code**, we recommend the
[the `cxx` crate](https://cxx.rs/).
For introductory guidance, please see
[the `cxx` chapter](https://google.github.io/comprehensive-rust/chromium/interoperability-with-cpp.html)
in the Chromium section of the Comprehensive Rust course.

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
However, because the Android build system does not yet support Crubit, it cannot be used in
`//base`, `//net`, or other dependencies of Cronet (see https://crbug.com/535682335).
The reverse direction (Rust calling C++) is not yet supported, but integration
is underway. See [`crubit.md`](crubit.md) for details.

Chromium **does not support any other FFI tools** (e.g.
[`cbindgen`](https://github.com/mozilla/cbindgen) or
[`zngur`](https://github.com/HKalbasi/zngur)), and we do not plan to support
them in the future.

## Safety considerations

All FFI code is `unsafe` by definition, since the Rust compiler cannot enforce
its rules on non-Rust code. This means that when working with FFI, you must be
careful to understand exactly what behaviors are possible. Make sure you're
familiar with our [safety guidelines](docs/rust/unsafe.md#how-to-use-unsafe).
So long as you're consistent about writing and following safety comments, you
can avoid most problems.

* **Opaque types**: When sharing a type from C++ to Rust using a `cxx::bridge`,
  it is treated as opaque, zero-sized placeholder type from the perspective of
  the Rust compiler. This means that it must always be behind an indirection (a
  pointer, reference, box, etc.), and the Rust compiler makes no assumptions
  about what it points to.
  * In particular, is it not fundamentally unsound for C++ to mutate an opaque
    value while Rust has a reference to it, although you must still be cautious
    of e.g. race conditions.
* **[Shared types](https://cxx.rs/shared.html)**: Rust has visibility into these
  types, so you must ensure C++ obeys all aliasing rules, e.g. not mutating the
  value while Rust has a reference to it.
* **Widespread types**: Some types are widely used throughout Chromium's C++
  codebase, and auditing every usage for safety is infeasible. In such cases,
  you may assume that C++ does not already contain undefined behavior.
  * For example, you may assume that mutations to the
  [static per-process `CommandLine` variable](https://source.chromium.org/chromium/chromium/src/+/main:base/command_line.cc;l=38;drc=5496fbe366e6796c5b549c65a2cb267b5c933e35)
  are not already racy, even though there isn't specific code to enforce that.

## `cxx` guidance

The `cxx` crate is the current standard for C++/Rust interop in Chromium, but it
has some rough edges that we hope will eventually be smoothed over with the
[adoption of Crubit](https://crbug.com/470466915). This section provides advice
for working with `cxx` in the meantime.

### Best practices

* Maintain binding declarations in a **single** `#[cxx::bridge]` declaration.
  `cxx` supports reusing types across multiple `bridge`s, but there are some
  rough edges.
* Generate C++ side of bindings into a project-specific or crate-specific
  `namespace`.  For example: `#[cxx::bridge(namespace = "some_cpp_namespace")]`.
* Prefer to call existing C++ functions when possible. When you cannot (e.g.
  because it involves a type that `cxx` can't translate), create wrapper
  functions in a C++ shim file.
  * By convention, these files end in `_shim.h` (or `.cc`).
* [Think carefully](#safety-considerations) about if and how data can be
  shared by C++ and Rust at the same time, as this is a major source of
  potential unsafety.

### Suggestions

These are not necessarily best practices, but take them into consideration when
designing your bridges.

* If you're not using a wrapper type, leverage `cxx`'s [attributes] to generate
  more idiomatic code:
  * The `Self` attribute can attach static functions to existing types.
  * The `rust_name` attribute is useful for translating between naming
    conventions.
* If you're exposing a function without safety requirements (e.g. because it
  doesn't manipulate memory), you mark it as safe by annotating the entire
  `extern "C++"` block as `unsafe`.
  * A good rule of thumb is to do this after you try to write a safety comment
    and can't come up with any way for things to go wrong.

## General Tips and Tricks

In general, you should follow
[idomatic Rust style](https://doc.rust-lang.org/style-guide/), but some idioms
are especially useful in an FFI context:

* Use [`From`](https://doc.rust-lang.org/std/convert/trait.From.html) (or
  [`TryFrom`](https://doc.rust-lang.org/std/convert/trait.TryFrom.html)) to
  convert between types; it's especially well-suited to converting between
  types that are logically equivalent, such as FFI types like
  [`ffi::ColorType`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/experimental/rust_png/ffi/FFI.rs;l=20-27;drc=70253db1ecfe261003756f0d81ae30929cc77ee4)
  and third-party crate types like
  [`png::ColorType`](https://docs.rs/png/0.17.6/png/enum.ColorType.html).

  * See an [example trait implementation](https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/experimental/rust_png/ffi/FFI.rs;l=221-231;drc=70253db1ecfe261003756f0d81ae30929cc77ee4)
  and an example of [using the conversion](https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/experimental/rust_png/ffi/FFI.rs;l=651;drc=70253db1ecfe261003756f0d81ae30929cc77ee4)
  as `foo.into()`.

  * Note that when implementing the conversion for types defined in other crates,
  you may need to work around the
  [orphan rule](https://doc.rust-lang.org/reference/items/implementations.html#r-items.impl.trait.orphan-rule)
  by implementing
  [`Into`](https://doc.rust-lang.org/std/convert/trait.Into.html)
  (or
  [`TryInto`](https://doc.rust-lang.org/std/convert/trait.TryInto.html))
  trait instead.

* Use the [? operator](https://doc.rust-lang.org/reference/expressions/operator-expr.html#r-expr.try)
  alongside the [Option](https://doc.rust-lang.org/std/option/) or
  [Result](https://doc.rust-lang.org/std/result/) types to check for errors.

  * When used in the FFI layer, this may require splitting some functions
  into (a) one that returns `Result<T, E>` and uses `?` sugar,
  and (b) one that translates `Result<T, E>` into FFI-friendly
  status.

  * There are some examples
  [here](https://source.chromium.org/chromium/chromium/src/+/main:components/user_data_importer/utility/zip_ffi_glue.rs;l=297;drc=33f81e080c4c06d18880ec04832511bda3929972)
  and
  [here](https://source.chromium.org/chromium/chromium/src/+/main:components/user_data_importer/utility/zip_ffi_glue.rs;l=421-427;drc=33f81e080c4c06d18880ec04832511bda3929972).
  [This example](https://chromium-review.googlesource.com/c/chromium/src/+/6733098/18/components/user_data_importer/utility/zip_ffi_glue.rs#484)
  avoids having to come up with a separate name by using an anonymous function.

* Use the [`let Ok(foo) = ... else { ... }`](https://doc.rust-lang.org/rust-by-example/flow_control/let_else.html) syntax to handle errors when you
  don't always want to return if you see a `None`. See
  [an example here](https://source.chromium.org/chromium/chromium/src/+/main:components/user_data_importer/utility/zip_ffi_glue.rs;l=328-333;drc=33f81e080c4c06d18880ec04832511bda3929972).
