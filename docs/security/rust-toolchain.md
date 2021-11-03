# Experimental Rust toolchain

[TOC]

# Why?

Parsing untrustworthy data is a major source of security bugs, and it's therefore
against Chromium rules [to do it in the browser process](rule-of-2.md) unless
you can use a memory safe language.

For teams building browser process features which need to handle untrustworthy
data, they usually have to do the parsing in a utility process which incurs
a performance overhead and adds engineering complexity.

The Chrome security team is working to make a cross-platform memory safe language
available to Chromium developers. This document describes how to use that
language in Chromium. The language, at least for now, is Rust.

# Guidelines

Support for Rust in Chromium is experimental. We appreciate your help in these
experiments, but please remember that Rust is not supported for production use
cases.

So:

* any experiments must be reversible (you may have to write a C++ equivalent
  in order to ship)
* Rust code must not affect production Chrome binaries nor be shipped to Chrome
  users (we provide `#ifdef` and other facilities to make this easy) - so if
  you put Rust code in Chrome, the sole purpose is to help experiment and provide
  data for evaluation of future memory safe language options
* Rust is not yet available on all Chromium platforms (just Linux and Android
  for now)
* Facilities and tooling in Rust are not as rich as other languages yet.

That said, if presence of Rust would make your feature easier, we are keen
for you to join in our experiments. Here's how. Please also let us know
your interest via `rust-dev@chromium.org`.

# GN support

Assume you want to add some Rust code to an existing C++ `source_set`.
Simply:

* `import("//build/rust/mixed_source_set.gni")`
* Replace `source_set` with `mixed_source_set`
* Add `rs_sources = [ "src/lib.rs" ]` (and likely `rs_cxx_bindings`, see below)
* Add your Rust code in `src/lib.rs`
* In your C++ code, make Rust calls based on the `#define` `RUST_ENABLED`.

In toolchains with Rust disabled, your `source_set` will continue to be a plain
C++ source set and absolutely nothing will change.

In toolchains with Rust, `RUST_ENABLED` will be defined and then you can
call into Rust code (again, see the section on C++/Rust interop bindings below).

## A note on source code naming

Within a mixed code source set, it's (currently) normal to have C/C++ code
in its main directory, whilst Rust code goes into a subdirectory called `src`
(and the main file is always called `lib.rs`.) This follows the practice of
other teams, but if you don't like it, that's fine: feel free to store your
`.rs` code alongside your `.cc` code, but specify also `rs_crate_root` in your
`mixed_source_set`.

## I'm not using a `source_set`

If your code isn't already a `source_set`, switching to a `mixed_source_set`
obviously isn't possible. Instead, you'll create a new Rust language target
- see `//build/rust/rust_source_set.gni`. C++ targets can simply depend on
this Rust target but with the suffix `_cpp_bindings` appended to the target
name:

```
deps = [ "//path/to/my_rust_target:my_rust_target_cpp_bindings" ]
```

If your Rust code calls back into C++, this is more complex in order to
avoid layering violations - look into `mutually_dependent_target` in
that `.gni` file.

# Unit tests

Rust supports unit tests within the primary source code files.
With either approach, you'll get a bonus `gn` target created called
`<your target name>_rs_unittests` which is an executable containing any Rust
unit tests in your code.

At present, there is no automatic integration of such unit tests into our
existing test infrastructure, but this is something we're working on.

# Third party dependencies

Adding Rust third party dependencies follows the same protocols
[as for C++ or other languages](../adding_to_third_party.md). But practically,
Rust libraries are almost always distributed as cargo "crates" which have
build scripts and metadata in `Cargo.toml` files.

The crate you need may already be listed in
`//third_party/rust/third_party.toml` - if so, just depend upon it like this:

```
deps = [ "//third_party/rust/cxx/v1:lib" ]
```

(Only those crates explicitly listed in `//third_party/rust/third_party.toml`
are visible to first-party code; other crates in `//third_party/rust` are
transitive dependencies).

If you need to add new Rust third-party dependencies, there are scripts and gn
templates to make it nearly automatic (except of course for review). Please
reach out to `rust-dev@chromium.org` for advice.

# C++/Rust interop

There are multiple different solutions for Rust/C++ interop. In this phase of our
experiments, we're supporting just one: [cxx, described in this excellent online book](https://cxx.rs).

To use this interop facility in Chromium:

* define your `#[cxx::bridge]` module in your `.rs` file
* in your `mixed_source_set`, add `rs_cxx_bindings = [ "src/lib.rs" ]`
* from your C++,

```
#ifdef RUST_ENABLED
#include "path/to/your/target/src/lib.rs.h`
#endif
```

You can now simply call functions and use types declared/defined in your CXX
bridge. A typical usage might be to pass a `const std::string&` or `rust::Slice<const uint8_t>`
from C++ into Rust and then return a struct with the parsed results.

If you need to call back into C++ from Rust, this is also supported -
`include!` directives within an `extern "C++"` section should work:

```
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("path/to/my_cpp_header.h");
        fn some_function_defined_in_cpp();
    } 
}

// Rust code calls ffi::some_function_defined_in_cpp()
```

Future work may expose existing C++ Chromium APIs to Rust with no need
to declare the interface in a `#[cxx::bridge]` module.

## Dependencies between Rust targets

If your `rust_source_set` exposes Rust APIs for other Rust targets in Chromium,
those targets should be able to depend directly on your `rust_source_set` target.

If you have a `mixed_source_set` or any other component which is intended for
both Rust _and_ C++ consumers, please reach out to `rust-dev@chromium.org`
with your use-case. (This _should_ be possible with the current gn rules but
layering here is fragile so we'd rather discuss it.)

# Example

To see an example of all this, look at `//build/rust/tests/test_variable_source_set`.

