# `cpp_api_from_rust`

## Availability

### Experimental support in Chromium

`cpp_api_from_rust` support is currently considered experimental and unstable.

TODO(https://crbug.com/470466915): Edit this section once we officially declare
and announce support for using `cpp_api_from_rust` for some Rust libraries.
("some" because of toolchain availability caveats in the other section below.)

### Toolchain availability outside of Chromium

Chromium's `//third_party/rust-toolchain` includes `cpp_api_from_rust`, but
other projects may not.
This means that Chromium code that is built in such other projects should
not depend on `cpp_api_from_rust`.  Examples of code that should not
depend on `cpp_api_from_rust`:

* `//base`, `//net` and other [Cronet](../../components/cronet/README.md)
  dependencies.
  (This restriction should go away when/if Android support for
  `cpp_api_from_rust` hopefully comes later in 2026.)
* 2nd-party projects like
    - ANGLE (TODO: more details - something about being used by Apple?)
    - Skia (no `cpp_api_from_rust` support in Bazel)
    - V8 (TODO: more details - probably need Crubit support for official
      releases of Rust toolchain)

## Other docs

* Generic, Chromium-agnostic documentation of Crubit can be found at
  https://crubit.rs.
    * Note that some examples are Bazel-specific, but most of the documentation
      should still apply to Chromium.
    * Note that the documentation covers both `cpp_api_from_rust`
      (with some Chromium support - see "availability" above) and
      `rust_api_from_cpp` (with no Chromium support at this point).
* Google-internal Crubit documentation can be found at
  [go/crubit](https://goto2.corp.google.com/crubit)
    * This is mostly the same content as above, but is mentioned here because it
      includes a few extra things like document freshness and owner metadata,
      link to a Google-internal chatroom, etc.)
* TODO: Cover Crubit in
  [Chromium/FFI chapter of Comprehensive Rust course](https://google.github.io/comprehensive-rust/chromium/interoperability-with-cpp.html)

## Using `cpp_api_from_rust` in Chromium

### Enabling `cpp_api_from_rust` for a `rust_static_library` crate

Example:

```rust
// build/rust/tests/test_cpp_api_from_rust/lib.rs:
pub fn mul_two_ints_via_rust(x: i32, y: i32) -> i32 {
    x * y
}
```

```gn
# build/rust/tests/test_cpp_api_from_rust/BUILD.gn

import("//build/rust/rust_static_library.gni")

rust_static_library("rust_lib") {
  crate_root = "lib.rs"
  sources = [ crate_root ]
  cpp_api_from_rust = {
    target_name = "rust_lib_bindings"
    cpp_namespace = "rust_lib"
  }
}

source_set("unittests") {
  sources = [ "unittests.cc" ]
  deps = [
    ":rust_lib_bindings",
  ]
}
```

```
// build/rust/tests/test_cpp_api_from_rust/unittests.cc:

#include "build/rust/tests/test_cpp_api_from_rust/rust_lib.h"

void foo() {
  auto product = rust_lib::mul_two_ints_via_rust(3, 4);
}
```

### Enabling `cpp_api_from_rust` for a `third_party/rust` crate

TODO: This is not implemented yet.

### Inspecting the generated bindings

Let's assume that `cpp_api_from_rust` bindings are generated for
`//some/dir:some_target` - e.g.:

```gn
# some/dir/BUILD.gn

import("//build/rust/rust_static_library.gni")

rust_static_library("some_target") {
  crate_root = "lib.rs"
  sources = [ crate_root ]
  cpp_api_from_rust = {
    target_name = "some_target_bindings"
  }
}
```

The generated bindings can then be found and inspected in
`<out_dir>/gen/some/dir/some_target.h`.  For example:

```sh
$ cat out/rel/gen/build/rust/tests/test_cpp_api_from_rust/rust_lib.h | head -3
// Automatically @generated C++ bindings for the following Rust crate:
// rust_lib_1dc874e1
// Features: <none>
```

### Specifying binding dependencies

If public APIs of a crate depend on types from another crate, then the
dependency on the other crate needs to be explicitly specified in `BUILD.gn`.

#### Bindings dependencies for 1st-party Rust libraries

1st-party Rust libraries can specify dependencies of their bindings
as follows:

```rust
// build/rust/tests/test_cpp_api_from_rust/lib.rs:

chromium::import! {
    "//build/rust/tests/test_cpp_api_from_rust:internal_helper";
    "//build/rust/tests/test_cpp_api_from_rust:other_lib";
}

pub fn create_multiplier(x: i32) -> other_lib::Multiplier {
    internal_helper::do_something();

    other_lib::Multiplier::new(x)
}
```

```gn
# build/rust/tests/test_cpp_api_from_rust/BUILD.gn

import("//build/rust/rust_static_library.gni")

rust_static_library("rust_lib") {
  crate_root = "lib.rs"
  sources = [ crate_root ]
  deps = [
    ":other_lib",
    ":internal_helper",
  ]

  cpp_api_from_rust = {
    target_name = "rust_lib_bindings"
    cpp_namespace = "rust_lib"
    deps = [ "//some/other/lib:other_lib_bindings" ]
  }
}
```

Note how `other_lib_bindings` are listed in `deps` of `cpp_api_from_rust` above.

Note that types from `internal_helper` are _not_ used in public APIs of
`rust_lib` and therefore `internal_helper` is _not_ listed
in `deps` attribute of `cpp_api_from_rust`.

#### Bindings dependencies for `//third_party/rust` libraries

TODO: `gnrt_config.toml` equivalent for `//third_party/rust` libraries.

#### Bindings dependencies for Rust standard library

C++ bindings for Rust standard library
(i.e. the `//build/rust/std:std_bindings` target)
are automatically injected as a dependency of all other bindings
(i.e. there is no need to specify them explicitly in a `deps` entry).

C++ bindings for Rust standard library are placed in a C++ namespace
that corresponds to the original Rust crate as follows:

* `std` crate => `rs_std` namespace
* `core` crate => `rs_core` namespace
* `alloc` crate => `rs_alloc` namespace

## Troubleshooting

### APIs missing from the generated bindings

If `cpp_api_from_rust` is unable to generate bindings for a given Rust API,
then the generated `.h` file will contain a comment explaining why.
The sections below describe a few errors that are somewhat related to
how Chromium integrates Crubit into its build system.

#### No `--crate-header` was specified for this crate

If you see an error like:

```
$ cat out/rel/gen/build/rust/tests/test_cpp_api_from_rust/rust_lib.h
...
// Error generating bindings for `create_multiplier` defined at
// ../../build/rust/tests/test_cpp_api_from_rust/lib.rs;l=22: Error formatting
// function return type `other_lib::Multiplier`: Type `other_lib::Multiplier`
// comes from the `other_lib_1dc874e1` crate, but no `--crate-header` was
// specified for this crate
...
```

Then you want to read the "Specifying binding dependencies" section above.
