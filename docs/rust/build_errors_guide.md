# Rust Build Errors Guide

This document lists Rust-related build errors that are noteworthy in the context
of Chromium builds.

[TOC]

## Unsafe Rust

Chromium builds disallow `unsafe` Rust by default.
Unexpected `unsafe` Rust can cause build errors below:

```
error: usage of an `unsafe` block
error: declaration of an `unsafe` function
error: implementation of an `unsafe` trait
error: implementation of an `unsafe` method
...
note: requested on the command line with `-F unsafe-code`
```

`unsafe` Rust is disallowed by default to:

* Discourage using `unsafe` Rust code
* Make code reviews easier (e.g. `//third_party/rust` crates with
  `allow_unsafe = false` can get a bit less scrutiny).

To fix the errors above you can either:

* Express the same code in safe Rust if possible
  (e.g. using
  `slice[i]` or
  [`slice.get(i)`](https://doc.rust-lang.org/std/primitive.slice.html#method.get)
  rather than
  [`slice.get_unchecked(i)`](https://doc.rust-lang.org/std/primitive.slice.html#method.get_unchecked),
  unless disassembly results show that the compiler cannot elide the checks
  and/or performance measurements show significant runtime difference)
* Allow `unsafe` code in the given crate (ideally only for a small,
  easy-to-reason-about crate that encapsulates the unsafety behind a safe public
  API).
    * In manually-authored `BUILD.gn` files (e.g. in first-party code) you can
      set `allow_unsafe = true` in `rust_static_library`
    * In `gnrt`-generated `BUILD.gn` files you can
      set `extra_kv.allow_unsafe` property to `true` in
      `third_party/rust/chromium_crates_io/gnrt_config.toml` (and then
      regenerate `BUILD.gn` with `tools/crates/run_gnrt.py gen`).

## Unstable features

Chromium builds require an explicit opt-in to use unstable Rust features
(see the policy in `//tools/rust/unstable_rust_feature_usage.md`).
Unexpected usage of unstable Rust features can cause build errors below:

```
error[E0725]: the feature `feature_name` is not in the list of allowed features
error[E0658]: use of unstable library feature `feature_name`
...
note: see issue #XXXXX <https://github.com/rust-lang/rust/issues/XXXXX> for more information
help: add `#![feature(feature_name)]` to the crate attributes to enable
```

To opt into allowing certain unstable features, you need to:

* Opt into allowing an unstable feature in the `BUILD.gn` of a Rust crate
  (justifying edits to the policy in
  `//tools/rust/unstable_rust_feature_usage.md` as needed)
    * In manually-authored `BUILD.gn` files (e.g. in first-party code) you can
      set the following properties of `rust_static_library`:
        - `rustflags = [ "-Zallow-features=feature_name" ]`
        - `configs -= [ "//build/config/compiler:disallow_unstable_features" ]`
    * In `gnrt`-generated `BUILD.gn` files you can
      set `extra_kv.allow_unstable_features` property in
      `third_party/rust/chromium_crates_io/gnrt_config.toml` to the list of
      allowed feature names (and then
      regenerate `BUILD.gn` with `tools/crates/run_gnrt.py gen`).
* Opt into allowing an unstable feature in the root module of a Rust crate.
  To do this add `#![feature(feature_name)]` to `lib.rs`.  See also:
    * The brief
      [language reference documentation](https://doc.rust-lang.org/reference/attributes.html#:~:text=feature%20%E2%80%94%20Used%20to%20enable%20unstable%20or%20experimental%20compiler%20features.)
      of the `feature` attribute
    * An example usage in
      [the Unstable Book](https://doc.rust-lang.org/unstable-book/index.html)

## Missing sources

`gn` and `ninja` know about build target inputs and sources through `input`
and/or `sources` properties specified in `BUILD.gn` files.
`rustc` independently discovers all `.rs` files by starting from a crate
root, and then following `mod foo;` declarations.
Chromium build will report an error when those 2 sources of information
are out of sync - for example:

```
ERROR: Rust source file or input not in GN sources: ../../foo/bar/baz.rs
```

To fix errors like the one above
you should ensure that the `BUILD.gn` lists the same source files and inputs
as the ones actually used in `.rs` source code:

* In manually-authored `BUILD.gn` files (e.g. in first-party code) you should
  double-check the `sources` property (or the `inputs` property in less common
  cases like when using
  [`include!`](https://doc.rust-lang.org/std/macro.include.html) macro).
    - TODO(lukasza): Figure out if/why it matters whether an `.rs` or `.rs.incl`
      file is listed in `sources` vs `inputs`.
* In `gnrt`-generated `BUILD.gn` files, `gnrt` typically can discover all
  `.rs` files on its own, but sometimes `gnrt` may need extra crate metadata
  that you can provide via `gnrt_config.toml` - for example:
    - `extra_src_roots` (or `extra_input_roots`) can list source files
      to append to `sources` (or `inputs`) for the main Rust target
    - `extra_build_script_src_roots` (or `extra_build_script_input_roots`)
      can list sources files to append to `sources` (or `inputs`)
      for the `build.rs` script
    - See a comment at the top of `gnrt_config.toml` for more information
    - After editing `gnrt_config.toml` run
      `tools/crates/run_gnrt.py gen` to regenerate `BUILD.gn` files.

## Can't find and include `build.rs` output

Third-party crates may depend on `build.rs` output - typically through
`include!` of one or more files from the `env!("OUT_DIR")` directory.
If `gn` and `ninja` are not aware of these `build.rs` outputs, then it may
lead to build errors like the one:

```
   --> ../../third_party/rust/chromium_crates_io/vendor/rustversion-v1/src/lib.rs:217:30
    |
217 | const RUSTVERSION: Version = include!(concat!(env!("OUT_DIR"), "/version.expr"));
    |                              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ in this macro invocation
    |
   --> library/core/src/macros/mod.rs:1487:4
    |
    = note: in this expansion of `include!`
```

To fix the error above:

* check if `build_script_outputs` in `gnrt_config.toml` lists
  all `build.rs` outputs.
    - See a comment at the top of `gnrt_config.toml` for more information
    - After editing `gnrt_config.toml` run
      `tools/crates/run_gnrt.py gen` to regenerate `BUILD.gn` files.

## Dependency not visible to an internal Rust target

Rust target templates like `rust_static_library("some_target")`
internally expand into multiple smaller targets
(e.g. a `rust_library` for invoking `rustc`,
an `action` for invoking `clippy-driver`,
`cxx`-supporting targets, etc.).
This may cause `gn gen` errors when a dependency is exposed to the main
target (e.g. `":some_target"`), but not to the internal targets (e.g.
`":some_target_clippy"` or `":some_target_generator"`).

Example error:

```
$ gn gen out/Default
ERROR at //build/rust/gni_impl/rust_target.gni:587:9: Dependency not allowed.
        action("${_clippy_target_name}") {
        ^---------------------------------
The item //foo/bar:some_target_clippy
can not depend on //foo/bar:internal_impl
because it is not in //foo/bar:internal_impl's visibility list: [
  //foo/bar:internal_target1
  //foo/bar:internal_target2
]
```

To fix the error above, we recommend the following approach to
`visibility` declarations in `BUILD.gn` files:

* Public targets should use unrestricted `visibility`
* Private targets should restrict their `visibility`
  to specific directories - e.g. `visibility = [ ":*" ]`
* Avoid restricting target `visibility` to specific, individual
  targets - e.g. do **not** say `visibility = [ ":some_target" ]`.

Note: Other fix approaches have been discussed with
[`@gn-dev` here](https://groups.google.com/a/chromium.org/g/gn-dev/c/8cUBSIsd8Qw/m/32uA6L1iCAAJ).
