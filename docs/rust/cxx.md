# Using `cxx` in Chromium

The [`cxx` crate](https://cxx.rs/) is the current recommendation for C++/Rust
interop in Chromium.  This document provides advice for working with `cxx` in
Chromium.

For introductory guidance, see
[the `cxx` chapter](https://google.github.io/comprehensive-rust/chromium/interoperability-with-cpp.html)
in the Chromium section of the Comprehensive Rust course. See
[`ffi.md`](ffi.md) for an overview of FFI tools and general FFI guidance.

`cxx` has some rough edges. Our plan for addressing them is to recommend
[Crubit](crubit/README.md) in the future, once its experimental support
graduates out of the teamfood stage (follow <https://crbug.com/351793625> to
keep track of Crubit work).

[TOC]

## Safety considerations

Make sure you're familiar with our
[general FFI safety considerations](ffi.md#safety-considerations) and
[safety guidelines](unsafe.md#how-to-use-unsafe). In addition, keep the
following `cxx`-specific details in mind:

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

## Best practices

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
* List the targets of all C++ headers that a `#[cxx::bridge]` `#include`s in
  the `deps` of the `rust_static_library`.  `cxx` doesn't parse these headers,
  so neither `cxx` nor a plain `gn check` reports missing dependencies, and
  missing dependencies can cause flaky builds (e.g. when a generated header
  isn't generated yet).  To find them, build the target, and then run
  `gn check <out_dir> --check-generated //path/to:<target>_cxx_generated`
  (`gn check` skips generated files that don't exist yet, and it checks the
  generated C++ code only as part of the `..._cxx_generated` target).

## Suggestions

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
