# Getting Started with Rust in Chromium

Welcome! If you're interested in learning more about how to use Rust as a
Chromium developer, you're in the right place.

Rust support in Chromium is still growing, so your best resource is the
[Rust-in-Chrome team](https://moma.corp.google.com/team/1387596644314)
(internal link). We're
the folks responsible for building and maintaining Chromium's Rust
infrastructure, and we're always happy to help out people who want to use Rust
in their work -- it is quite literally our job to make your lives easier.

[TOC]

## Contacting us

If you have questions or an idea about how to use Rust in your work, we'd be
more than happy to provide advice and support.

## External Resources

The best ways to reach us are at
[`rust-dev@chromium.org`](https://groups.google.com/a/chromium.org/g/rust-dev),
or in the [`#rust` channel](https://chromium.slack.com/archives/C01T3EWCJ9Z)
on the [Chromium Slack](https://www.chromium.org/developers/slack/).

## Team Resources (internal)

For Google employees, there are several internal resources you can use as well.
If you want to contact the team as a whole, feel free to send a message to
<rust-in-chrome@google.com> or join the
[Chrome + Rust chatroom](https://chat.google.com/room/AAAAk1UCFGg?cls=7).
However, we also have spaces dedicated to getting started with
Rust:

* A chatroom
  ([go/rust-in-chrome-questions](https://goto.google.com/rust-in-chrome-questions)).
* Regular office hours
  ([go/rust-in-chrome-office-hours](https://goto.google.com/rust-in-chrome-office-hours)). There are
  no signups, so feel free to drop in to the call at any time!

## FAQ

* Q: Do I need approval to use Rust in Chromium?
  * A: Nope! Rust is considered just another tool; you may choose to use it
    at your own discretion, and only need acceptance from code owners as usual.
* Q: How do I get my Rust code reviewed?
  * A: Code should be reviewed by the local owners. Since Rust is still growing,
    the Rust-in-Chrome team is happy to provide reviews and advice on the
    linguistic portion of the CL, but owner reviews are still needed to ensure
    the code is doing the right things in the first place.
* Q: Are there any restrictions on `unsafe` Rust?
  * A: The short answer is no. We encourage you to avoid it whenever possible,
    but you can opt in to unsafe rust at your own discretion by adding
    `allow_unsafe = true` to your build target. See
    [//docs/rust/unsafe.md](/docs/rust/unsafe.md) for
    more information.
* Q: How should I set myself up for Rust development?
  * A: We recommend using VSCode with the
    [rust-analyzer](https://github.com/rust-lang/rust-analyzer) extension. See
    [//docs/rust/dev_experience_tips_and_tricks.md](/docs/rust/dev_experience_tips_and_tricks.md)
    for more information, and
    see [below](#build-system-setup) for information about getting your files
    hooked into the build system.
* Q: How do I interoperate with C++ code?
  * A: Our standard tool is [cxx](https://cxx.rs/). We also support
    [Crubit](https://crubit.rs/) for calling Rust from C++ (with some
    limitations; see [`crubit.md`](./crubit.md) for details).
    An alternative if you're implementing a service in Rust is to use
    [Mojo](/docs/mojo_and_services.md) as a communication method, which avoids
    the need for direct interop. See [//docs/rust/ffi.md](/docs/rust/ffi.md)
    for more information.
* Q: How should I test my code?
  * A: Chromium uses `gtest` for Rust code, using the
  [`//testing/rust_gtest_interop`](/testing/rust_gtest_interop/README.md)
  library to integrate into existing testing binaries
  ([example](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/rust/mojom_value_parser/test/test_parser.rs;l=184;drc=9aa422077f4e230885819bb84625b69e34457271)). Unlike most Rust project, we
  do not support `#[cfg(test)]`-style testing.
* Q: Can I use unstable compiler features?
  * A: Generally, no. Since the features could change or be removed without
    warning, you need approval from the Rust in Chrome team to use unstable
    features in your code. See
    [//docs/rust/unstable_rust_feature_usage.md](/docs/rust/unstable_rust_feature_usage.md)
    for more information.

## Build system setup

### First-party Rust libraries

If you want to create your own Rust library inside of Chromium, use the
[`rust_static_library`](
https://source.chromium.org/chromium/chromium/src/+/main:build/rust/rust_static_library.gni)
GN template (not the built-in `rust_library`) to integrate properly into the
Chromium build and get the correct compiler options
([example](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/rust/mojom_value_parser/BUILD.gn;l=7;drc=eb1f279735a9a332e78f612be01094998ff7a31d)).
Internally, this will generate a Rust crate with a mangled name based on the
name of the GN target (to ensure crate names are globally unique).

To use that crate from Chromium code, import it using the
[`chromium::import!` macro](/build/rust/chromium_prelude/), which is
automatically available in all Chromium code
([example](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/rust/bindings/message.rs;l=10;drc=d2824a250977087bf00316da7fb6f3e822f6c52e)).
The standard library and crates from `//third_party/rust` do not use mangled
names, so `chromium::import!` is not necessary for them.

Your IDE (or coding agent) may suggest `use mangled_crate_name::foo;`, but
please use `chromium::import!` instead to avoid depending on the mangling scheme.

## Third-party Rust libraries

### crates.io

See
[`//third_party/rust/README-importing-new-crates.md`](/third_party/rust/README-importing-new-crates.md)
for instructions on how to import a crate from <https://crates.io> into Chromium.

The crates will get updated semi-automatically through the process described in
[`//tools/crates/create_update_cl.md`](/tools/crates/create_update_cl.md).

These libraries use the
[`cargo_crate`](
https://source.chromium.org/chromium/chromium/src/+/main:build/rust/cargo_crate.gni)
GN template.

### Other libraries

Third-party Rust libraries that are not distributed through [crates.io](
https://crates.io) should live outside of `//third_party/rust`.
Such libraries will typically depend on `//third_party/rust` crates
and use `//build/rust/*.gni` templates, but there is no other Chromium
tooling to import such libraries or keep them updated.
For examples, see `//third_party/crabbyavif` or
`//third_party/cloud_authenticator`.

## Additional Resources

The other documents in this folder (`//docs/rust`) contain a variety of
information about using Rust in Chromium, including both advice and our
policies. However, they are more useful as a reference once you're familiar
with the language.

If you want to learn more about Rust on your own, Google maintains a free
[Rust course](https://google.github.io/comprehensive-rust/). The course outline
and "slides" are available to everyone, and they alone can be very helpful. If
you're a Google employee, you can also see if there are any instructor-led
courses scheduled
([go/comprehensive-rust](https://goto.google.com/comprehensive-rust)).
