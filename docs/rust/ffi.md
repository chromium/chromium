# Rust FFI

This document tries to provide guidance for C++/Rust FFI.
CLs to improve this guidance are welcomed.

## General guidance

### Supported FFI tools

Chromium recommends using [the `cxx` crate](https://cxx.rs/) for C++/Rust FFI.
For introductory guidance, please see
[the `cxx` chapter](https://google.github.io/comprehensive-rust/chromium/interoperability-with-cpp.html)
in the Chromium day of the Comprehensive Rust course.

Chromium also supports the following tools:

* [`bindgen`](https://rust-lang.github.io/rust-bindgen/) - see
  `//build/rust/rust_bindgen.gni` for usage instructions.

At this point Chromium's `//build/rust/*.gni` templates do not support other FFI
tools like:

* [`cbindgen`](https://github.com/mozilla/cbindgen)
* [`crubit`](https://github.com/google/crubit)

### Related Rust idioms

We can't provide comprehensive, generic Rust guidance here, but let's
mention a few items that may be worth using in the FFI layer:

* [`From`](https://doc.rust-lang.org/std/convert/trait.From.html) (or
  [`TryFrom`](https://doc.rust-lang.org/std/convert/trait.TryFrom.html))
  is an idiomatic way of implementing a conversion between two types
  (e.g. between FFI layer types like
  [`ffi::ColorType`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/experimental/rust_png/ffi/FFI.rs;l=20-27;drc=70253db1ecfe261003756f0d81ae30929cc77ee4)
  and third-party crate types like
  [`png::ColorType`](https://docs.rs/png/0.17.6/png/enum.ColorType.html)).
  See an example trait implementation
  [here](https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/experimental/rust_png/ffi/FFI.rs;l=221-231;drc=70253db1ecfe261003756f0d81ae30929cc77ee4)
  and an example of spelling the conversion as `foo.into()`
  [here](https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/experimental/rust_png/ffi/FFI.rs;l=651;drc=70253db1ecfe261003756f0d81ae30929cc77ee4).
  Note that when implementing the conversion for types defined in other crates,
  you may need to work around the
  [orphan rule](https://doc.rust-lang.org/reference/items/implementations.html#r-items.impl.trait.orphan-rule)
  by implementing
  [`Into`](https://doc.rust-lang.org/std/convert/trait.Into.html)
  (or
  [`TryInto`](https://doc.rust-lang.org/std/convert/trait.TryInto.html))
  trait instead.

* [Question mark operator](https://doc.rust-lang.org/reference/expressions/operator-expr.html#r-expr.try)
  is an ergonomic, idiomatic way for checking errors.
  When using it in the FFI layer, this may require splitting some functions
  into 1) one that returns `Result<T, E>` and uses `?` sugar,
  and 2) one that translates `Result<T, E>` into FFI-friendly
  status.  See an example
  [here](https://source.chromium.org/chromium/chromium/src/+/main:components/user_data_importer/utility/zip_ffi_glue.rs;l=297;drc=33f81e080c4c06d18880ec04832511bda3929972)
  and
  [here](https://source.chromium.org/chromium/chromium/src/+/main:components/user_data_importer/utility/zip_ffi_glue.rs;l=421-427;drc=33f81e080c4c06d18880ec04832511bda3929972).
  Additional example
  [here](https://chromium-review.googlesource.com/c/chromium/src/+/6733098/18/components/user_data_importer/utility/zip_ffi_glue.rs#484)
  avoids having to come up with a separate name by using an anonymous function.

* [`let Ok(foo) = ... else { ... }`](https://doc.rust-lang.org/rust-by-example/flow_control/let_else.html)
  is another ergonomic way for checking errors.  See
  [an example here](https://source.chromium.org/chromium/chromium/src/+/main:components/user_data_importer/utility/zip_ffi_glue.rs;l=328-333;drc=33f81e080c4c06d18880ec04832511bda3929972).

## `cxx` guidance

### Best practices

* Generate C++ side of bindings into a project-specific or crate-specific
  `namespace`.  For example: `#[cxx::bridge(namespace = "some_cpp_namespace")]`.
* Maintain binding declarations in a **single** `#[cxx::bridge]` declaration.
  `cxx` supports reusing types across multiple `bridge`s, but there are some
  rough edges.

### Suggestions

TODO: Provide some examples or suggestions on how to structure FFI bindings
(even if these suggestions wouldn't necessarily rise to the level of "best
practices").
