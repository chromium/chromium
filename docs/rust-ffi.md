# Rust FFI

This document tries to provide guidance for C++/Rust FFI.
CLs to improve this guidance are welcomed.

## General guidance

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

### `cxx` guidance

#### Best practices

* Generate C++ side of bindings into a project-specific or crate-specific
  `namespace`.  For example: `#[cxx::bridge(namespace = "some_cpp_namespace")]`.
* Maintain binding declarations in a **single** `#[cxx::bridge]` declaration.
  `cxx` supports reusing types across multiple `bridge`s, but there are some
  rough edges.

#### Suggestions

TODO: Provide some examples or suggestions on how to structure FFI bindings
(even if these suggestions wouldn't necessarily rise to the level of "best
practices").
