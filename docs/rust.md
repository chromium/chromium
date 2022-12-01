# Rust in Chromium

[TOC]

# Why?

Parsing untrustworthy data is a major source of security bugs, and it's
therefore against Chromium rules [to do it in the browser process](rule-of-2.md)
unless you can use a memory-safe language.

Rust provides a cross-platform memory-safe language so that all platforms can
handle untrustworthy data directly from a privileged process, without the
performance overheads and complexity of a utility process.

# Guidelines

Rust in Chromium is not production-ready. It is guarded behind a GN flag which
is off by default.

Rust is only used in //third_party/rust, for crates developed outside of the
Chromium tree.

# Status

The Rust toolchain is still experimental and breaks frequently, but it is
behind an off-by-default GN argument, so this does not affect our bots or
developers.

We have a working Rust toolchain for Linux x64 and Android targets. We are
working on support for other platforms.

For questions or help, reach out to `rust-dev@chromium.org` or `#rust` on the
[Chromium Slack](https://www.chromium.org/developers/slack/).

# Building with Rust support

1. Add `enable_rust = true` in your `gn` arguments, via `gn args <outdir>`.
1. Add `"use_rust": True` to your `.gclient` file in the `"custom vars"`
   section.

If you use VSCode, we have [additional advice below](#using-vscode).

# Using a third-party Rust library

## Importing a crate from crates.io

See [//tools/crates/README.md](../tools/crates/README.md) for instructions on
how to import a third-party library and generate GN build rules for it.

## Third-party review

**Since the Rust toolchain is still experimental, there is no Rust in production
and we're not ready to consider approving Rust libraries that aren't part of the
experiment and stabilization of the Rust toolchain.**

All third-party crates need to go through third-party review. See
[//docs/adding_to_third_party.md](adding_to_third_party.md) for instructions on
how to have a library reviewed.

## Writing a wrapper for binding generation

Most Rust libraries will need a more C++-friendly API written on top of them in
order to generate C++ bindings to them. Such wrapper libraries should be written
in `//third_party/rust/<cratename>/<epoch>/wrapper`.

See
[`//third_party/rust/serde_json_lenient/v0_1/wrapper/`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/rust/serde_json_lenient/v0_1/wrapper/)
for an example.

Rust libraries should use our
[`rust_static_library`](https://source.chromium.org/chromium/chromium/src/+/main:build/rust/rust_static_library.gni)
GN template, in place of built-in GN `rust_library`, in order to integrate
properly into the build and get the correct compiler options applied to them.

We provide the [CXX](https://cxx.rs) tool for generating C++ bindings for the
Rust library (or wrapper library). Add any Rust files with a CXX bridge macro to
the `cxx_bindings` variable in the `rust_static_library` GN rule to have CXX
generate C++ a header for that file.

# Building on non-Linux platforms

We only have a working Rust toolchain for Linux and Android at this time. To use
Rust on other platforms, you will need to provide your own nightly Rust
toolchain. You can then tell `gn` about it using these `gn` arguments:

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

# Using VSCode

1. Ensure you're using the `rust-analyzer` extension for VSCode, rather than
   earlier forms of Rust support.
2. Run `gn` with this extra flag: `gn gen out/Release --export-rust-project`.
3. `ln -s out/Release/rust-project.json rust-project.json`
4. When you run VSCode, or any other IDE that uses
   [rust-analyzer](https://rust-analyzer.github.io/) it should detect the
   `rust-project.json` and use this to give you rich browsing, autocompletion,
   type annotations etc. for all the Rust within the Chromium codebase.
