# `cpp_api_from_rust`

`cpp_api_from_rust` (aka `cc_bindings_from_rs`) is a Crubit tool that takes
a Rust crate as input and generates C++ APIs (a `.h` header) as output,
enabling C++ to call Rust.

## Availability

`cpp_api_from_rust` is fully supported by the Rust in Chrome team, with the
following caveats:

*   **Some directories cannot use Crubit:** The Android project's
    Soong/bp build system
    does not support Crubit at this point. Consequently, Crubit cannot be
    used in `//base`, `//net`, or
    [other directories](https://source.chromium.org/chromium/chromium/src/+/main:components/cronet/android/dependencies.txt)
    that [Cronet](../../components/cronet/README.md) depends on.
    This is tracked in https://crbug.com/535682335.
    (Quick clarification:
    Chromium's GN/ninja build system supports Crubit on all Chromium target
    platforms, including Android.  For example, QR code generator in
    Chromium [uses Crubit](https://crrev.com/c/7749970)
    and ships to mobile and desktop targets.)

*   **2nd-party project limitations:** Projects like PDFium or V8 currently
    support non-Chromium clients and alternative toolchains that may lack
    Crubit support. Adopting Crubit in these projects requires either helping
    their clients adopt Crubit, or making a policy decision to only support
    clients that have Crubit available.

Other notes:

*   `cxx` remains fully supported; there are no plans to migrate existing
    `cxx::bridge` code to Crubit.
*   `rust_api_from_cpp` (calling C++ from Rust) is not yet supported, but
    integration work is ongoing.

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
* Crubit's Discord server can be joined using the following invite link:
  https://discord.gg/nHq5fdADKV
* TODO: Cover Crubit in
  [Chromium/FFI chapter of Comprehensive Rust course](https://google.github.io/comprehensive-rust/chromium/interoperability-with-cpp.html)

## Known issues

* https://crbug.com/536539387:
  Crubit support libraries may trigger `-Wnullability-completeness`

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

// `rust_lib` part of the `#include` path comes from the target name
// (i.e. from `rust_static_library("rust_lib")` above).
#include "build/rust/tests/test_cpp_api_from_rust/rust_lib.h"

void foo() {
  auto product = rust_lib::mul_two_ints_via_rust(3, 4);
}
```

### Enabling `cpp_api_from_rust` for a `third_party/rust` crate

Set `cpp_api_from_rust = true` in `gnrt_config.toml` as follows:

```
[crate.qr_code.extra_kv]
allow_unsafe = false
cpp_api_from_rust = true
```

After modifying `gnrt_config.toml` you have to re-run
`tools/crates/run_gnrt.py gen` to regenerate the crate's `BUILD.gn` file.

At this point you should be able to depend on the bindings and use them
as follows:

```
# My BUILD.gn:
source_set("my_cpp_code") {
  # ...
  deps += [ "//third_party/rust/qr_code/v2:cpp_api_from_rust" ]
}
```

```
// my_cpp_code.cc

// The last `qr_code` part of the `#include` path comes from the `crate_name`
// attribute of the `//third_party/rust/qr_code/v2:lib` target.
#include "third_party/rust/qr_code/v2/qr_code.h"

void foo() {
  // ...
  rs_std::SliceRef<const uint8_t> rs_in(in);
  auto result = ::qr_code::QrCode::new_(rs_in);
  // ...
}
```

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

3rd-party Rust crates can specify dependencies of their bindings
with the following `gnrt_config.toml` entry:

```
[crate.my_crate_name.extra_kv]
allow_unsafe = false
cpp_api_from_rust = { deps = ["some_other_crate/v123"] }
```

After modifying `gnrt_config.toml` you have to re-run
`tools/crates/run_gnrt.py gen` to regenerate the crate's `BUILD.gn` file.

#### Bindings dependencies for Rust standard library

C++ bindings for Rust standard library
are automatically injected as a dependency of all other bindings.
Therefore usually there is no need to explicitly depend on these bindings,
but if needed other targets can depend on `//build/rust/crubit`.

C++ bindings for Rust standard library are placed in a C++ namespace
that corresponds to the original Rust crate as follows:

* `std` crate => `rs_std` namespace
* `alloc` crate => `rs_alloc` namespace
* `core` crate => `rs_core` namespace

The bindings can be `#include`d from the following paths:

* `#include "third_party/crubit/support/rs_std/rs_std.h"`
* `#include "third_party/crubit/support/rs_std/rs_alloc.h"`
* `#include "third_party/crubit/support/rs_std/rs_core.h"`

> Side-note: The auto-generated `build/rust/std/rules/BUILD.gn` overrides the
> include paths to make sure that Chromium can use the canonical paths (ones
> that are unified across other major Crubit clients).  There is no actual
> `third_party/crubit/support` directory in the root of the Chromium repo.

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
