# C++ to Rust Cheat Sheet

This document provides a "translation" of
C++ concepts and APIs into their Rust equivalents.

## Other resources

The document focuses on things that seem relevant and helpful to
Chromium engineers.  If you are looking for a more generic C++ to Rust
mapping, then try the following resources:

* [go/rs-for-g3cc](https://goto.google.com/rs-for-g3cc)
* TODO: Add more resources (ideally, public ones...)

## `CHECK`, `DCHECK`, `DCHECK_EQ`, `DCHECK_IS_ON`

`CHECK(condition) << "msg"` from C++ is spelled
`assert!(condition, msg)` in Rust - see also
the official documentation of
[the `assert` macro](https://doc.rust-lang.org/std/macro.assert.html).
There are also
[`assert_eq`](https://doc.rust-lang.org/std/macro.assert_eq.html)
and
[`assert_ne`](https://doc.rust-lang.org/std/macro.assert_ne.html)
macros, but no equivalents of `CHECK_GT`, `CHECK_LE`, etc.

`DCHECK(condition) << "msg"` from C++ is spelled
`debug_assert!(condition, msg)` in Rust - see also
the official documentation of
[the `debug_assert` macro](https://doc.rust-lang.org/std/macro.debug_assert.html).
There are also
[`debug_assert_eq`](https://doc.rust-lang.org/std/macro.debug_assert_eq.html)
and
[`debug_assert_ne`](https://doc.rust-lang.org/std/macro.debug_assert_ne.html)
macros, but no equivalents of `DCHECK_GT`, `DCHECK_LE`, etc.

`#if DCHECK_IS_ON()` is spelled as `#[cfg(debug_assertions)]` - see also
the official documentation of
[the `cfg` attribute](https://doc.rust-lang.org/reference/conditional-compilation.html#the-cfg-attribute).
Note that `build/config/BUILD.gn`
[consistently applies](https://source.chromium.org/chromium/chromium/src/+/main:build/config/BUILD.gn;l=50-53;drc=5f2c0dbe2bf823c4cb9af69f43b38ae68b5e9cd7)
`dcheck_always_on` *both* to C++ and Rust, which means that
`debug_assert!` is active exactly when `DCHECK` is active
and should in general behave in a similar way
(note that https://crbug.com/491515771 tracks some known issues
which should eventually be fixed).

## `#if`, `BUILDFLAG`

`#if BUILDFLAG(IS_MAC)` from C++ is spelled
`#[cfg(target_os = "macos")]` in Rust - see also
the official documentation of
[the `cfg` attribute](https://doc.rust-lang.org/reference/conditional-compilation.html#the-cfg-attribute)
and of
[the conditions natively understood by the Rust compiler](https://doc.rust-lang.org/reference/conditional-compilation.html#set-configuration-options).

Chromium-specific build settings are typically exposed to C++ and Rust
via [`//build/buildflag_header.gni`](../../build/buildflag_header.gni)
which has
[documentation comments](https://source.chromium.org/chromium/chromium/src/+/main:build/buildflag_header.gni;l=59-76;drc=93d7cdf927a4cb81327613abbcfb66ddff0ab330)
that describe how to 1) depend on the
appropriate Rust-specific build `config` and 2) how to check the build
configuration from an `.rs` file.  For example,
`#if BUILDFLAG(CHROMIUM_BRANDING)` from C++ is spelled
`#[cfg(CHROMIUM_BRANDING)]` in Rust.

Note that Rust integration in `//build/buildflag_header.gni` only supports
boolean configuration values.  TODO: Can this be done by
setting (undocumented...) `rustenv` attribute in `rust_static_library` and
reading the value using
[the `env!` macro](https://doc.rust-lang.org/std/macro.env.html)?

## `LOG(ERROR) << ...`

`LOG(ERROR) << "Foo is " << foo` from C++ is spelled as
`log::error!("Foo is {foo}")` in Rust.
This Rust code depends on
[the `log` crate](https://docs.rs/log)
which can be used with the following `.gn` snippet:
`deps = [ "//third_party/rust/log/v0_4:lib" ]`.
