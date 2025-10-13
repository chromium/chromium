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
