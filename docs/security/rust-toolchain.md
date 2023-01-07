# Experimenting with Rust in Chromium

[TOC]

# Why?

Parsing untrustworthy data is a major source of security bugs, and it's
therefore against Chromium rules [to do it in the browser process](rule-of-2.md)
unless you can use a memory safe language.

For teams building browser process features which need to handle untrustworthy
data, they usually have to do the parsing in a utility process which incurs a
performance overhead and adds engineering complexity.

The Chrome security team is working to make a cross-platform memory safe
language available to Chromium developers. This document describes how to use
that language in Chromium. The language, at least for now, is Rust.

# Guidelines

Support for Rust in Chromium is experimental. We appreciate your help in these
experiments, but please remember that Rust is not supported for production use
cases.

So:

* any experiments must be reversible (you may have to write a C++ equivalent
  in order to ship)
* Rust code must not affect production Chrome binaries nor be shipped to Chrome
  users (we provide `#if defined(...)` and other facilities to make this easy) -
  so if you put Rust code in Chrome, the sole purpose is to help experiment and
  provide data for evaluation of future memory safe language options
* Rust is not yet available on all Chromium platforms (just Linux and Android
  for now)
* Facilities and tooling in Rust are not as rich as other languages yet.

That said, if presence of Rust would make your feature easier, we are keen
for you to join in our experiments. Here's how. Please also let us know
your interest via `rust-dev@chromium.org`.

# Building with Rust support

Add `enable_rust = true` in your `gn` arguments. At the moment, this works
only for Linux platforms (but [see below](#Building-on-non-Linux-platforms)
for how to enable on other platforms).

Also add `"use_rust": True` to your `.gclient` file to enable fetching required
tools and libraries:

```
solutions = [
  {
    ...
    "custom_vars": {
      ...
      "use_rust": True,
    },
  },
]
```

See also [Using VSCode](#using-vscode).

# GN support

Assume you want to add some Rust code to an existing C++ `source_set`.
Simply:

* `import("//build/rust/mixed_source_set.gni")`
* Replace `source_set` with `mixed_source_set`
* Add `rs_sources = [ "src/lib.rs" ]` (and likely `rs_cxx_bindings`, see below)
* Add your Rust code in `src/lib.rs`
* In your C++ code, make Rust calls based on the `#if defined(RUST_ENABLED)`.

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

There are equivalent templates for `mixed_component` and
`mixed_executable`. But if you need to do something more sophisticated,
you can create a new pure-Rust language target - see
`//build/rust/rust_source_set.gni`. C++ targets can simply depend on
this Rust target but with the suffix `_cpp_bindings` appended to the target
name:

```
deps = [ "//path/to/my_rust_target:my_rust_target_cpp_bindings" ]
```

If your Rust code calls back into C++, this is more complex in order to
avoid layering violations - look into `mutually_dependent_target` in
that `.gni` file.

# Unit tests

Rust supports unit tests within the primary source code files (e.g. see
[an example here](https://doc.rust-lang.org/rust-by-example/testing/unit_testing.html)).
This section describes how to build and run such unit tests.

## Automatically generated targets

GN templates that work with Rust sources will automatically generate a bonus
`gn` target called
`<your target name>_unittests` (for pure-Rust targets like `cargo_crate`,
`executable`, or `rust_source_set`) or `<your target name>_rs_unittests` (for
mixed C++/Rust targets like (`mixed_component`, `mixed_executable`, or
`mixed_source_set`).  This bonus target builds:
- An `out/Default/<bonus target name>` executable containing Rust unit tests
  from your code
- An `out/Default/bin/run_<bonus target name>` script that enables running
  the tests on Chromium bots.

## Explicitly defined groups of tests

To group multiple Rust unit test executables into a single test step,
please use the `rust_unit_tests_group("my_test_group")` template:

```
rust_unit_tests_group("my_group_of_rust_unit_tests") {
  deps = [
    "my_rust_source_set1",
    "my_rust_source_set2",
    "my_rust_executable",
    # ...
  ]
}
```

The example above will build all the `deps`.  This will also generate a wrapper
script that wraps all the Rust unit test executables from `deps` and their
transitive dependencies.  In the example above, the script will be generated at
`out/Default/bin/run_my_group_of_rust_unit_tests`.

The generated script can be used for integration with Chromium bots, but can
also be used as a convenience to manually/locally run all tests from the group.
Run the script with the `--help` argument to see more details (e.g. how to
filter which tests to run).

## Configuring running Rust unit tests on bots

To manually configure running Rust unit tests on bots, please follow the pattern
from https://crrev.com/c/3322199:
- Define a new isolate in `//testing/buildbot/gn_isolate_map.pyl`:
    - Set `label` to the fully qualified name of either
      the implicit bonus target (e.g. `..._rs_unittests`)
      or the explicit `rust_unit_tests_group` target.
    - Set `type` to `generated_script`
    - There are no requirements on the name of the new isolate,
      but typically it will have the same name as the target mentioned
      in the `label`.
- Define a new test step, or extend an existing test step in
  `//testing/buildbot/test_suites.pyl` (adding an entry referring to
  the new isolate above).  Note that the tests grouped under the test
  step need to have uniform kind (e.g. cannot mix GTest and Rust tests).
- Ensure that `//testing/buildbot/waterfalls.pyl` asks to run the test step
  on specific bots.  The test step needs to be listed under the
  `isolated_scripts` key (rather than under `gtest_tests` key).
- Run `//testing/buildbot/generate_buildbot_json.py`.

Future work:
- At present, there is no automatic integration of such unit tests into our
  existing test infrastructure, but this is something we're working on.
- At present, the bot integration only supports reporting whether the tests
  passed or failed, and doesn't capture results or output of individual tests.
- At present, there is no support for running native Rust unit tests on
  Android.

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
experiments, we're supporting just one:
[cxx, described in this excellent online book](https://cxx.rs).

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
bridge. A typical usage might be to pass a `const std::string&` or
`rust::Slice<const uint8_t>` from C++ into Rust and then return a struct with
the parsed results.

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
those targets should be able to depend directly on your `rust_source_set`
target.

If you have a `mixed_source_set` or any other component which is intended for
both Rust _and_ C++ consumers, please reach out to `rust-dev@chromium.org`
with your use-case. (This _should_ be possible with the current gn rules but
layering here is fragile so we'd rather discuss it.)

# Example

To see an example of all this, look at `//build/rust/tests/test_variable_source_set`.

# Tooling

## Known cases which don't work

* At the moment LTO doesn't work, so you can't use `is_official_build = true`.
  ([Bug.](https://crbug.com/1229423))
* Windows doesn't work just yet. ([Bug.](https://crbug.com/1268157))

## Building on non-Linux platforms

The Rust toolchain is provided only for Linux and Android. To use it on
other platforms, you will need to provide your own nightly Rust toolchain.
You can then tell `gn` about it using these `gn` arguments:

```
enable_rust=true
rust_sysroot_absolute="/Users/you/.rustup/toolchains/<toolchain name>"
rustc_version="<your rustc version>" # add output of rustc -V
# added_rust_stdlib_libs=[]
# removed_rust_stdlib_libs=[]
```

The last two arguments are any Rust standard library .rlibs which have been
added or removed between the version that's distributed for Linux/Android,
and the version you're using. They should rarely be necessary; if you get errors
about missing standard libraries then adjust `removed_rust_stdlib_libs`; if
you get errors about undefined symbols then have a look in your equivalent
of the `.rustup/toolchains/<toolchain name>/lib/rustlib/<target>/lib`
directory and add any new libraries which are not listed in
`//build/rust/std/BUILD.gn` to the `added_rust_stlib_libs` list.

## Using VSCode

1. Ensure you're using the `rust-analyzer` extension for VSCode, rather than
   earlier forms of Rust support.
2. Run `gn` with this extra flag: `gn gen out/Release --export-rust-project`.
3. `ln -s out/Release/rust-project.json rust-project.json`
4. When you run VSCode, or any other IDE that uses
   [rust-analyzer](https://rust-analyzer.github.io/) it should detect the
   `rust-project.json` and use this to give you rich browsing, autocompletion,
   type annotations etc. for all the Rust within the Chromium codebase.

## Source code format

- `git cl format` and `git cl presubmit` have been extended to automatically
  cover `.rs` files on Linux.
  (Currently whole files are re-formatted and/or checked; support for
  only looking at the modified lines is not yet implemented.)
- Rust defaults to 100-columns-wide formatting (similarly to Java).
  This may necessitate tweaking settings in various tools - e.g. in Gerrit you
  might want to increase the default "Diff width" under
  https://chromium-review.googlesource.com/settings/
