# Mojo Rust Bindings

This document is a subset of the [Mojo documentation](/mojo/README.md).
If you're interested in using the Mojo bindings to write Rust code, you probably
want to read the documentation for the
[Rust Bindings API](/mojo/public/rust/bindings/README.md).

## Overview

This folder contains the source code for using Mojo from Rust code. It is
divided into three layers:

1. C API Wrappers: Safe, idiomatic, Rust wrappers around the [Mojo C API](/mojo/public/c/system/README.md).
2. System API: Ergonomic, mid-level bindings.
3. [Bindings API](/mojo/public/rust/bindings/README.md):
   High-level, often Mojom-specific code.

Note that other languages (particularly C++) have only layers (2) and (3),
building on the C API without wrappers.

The files in this directory are documented using doc comments, which are
([meant to be](https://crbug.com/485632065)) rendered via rustdoc.

## C API Wrappers

The purpose of the C API wrappers is to provide the ability to call the C API
from Rust code without having to deal with the normal problems with FFI calls:
unsafety, raw pointers, needing to precisely match types, etc. This code lives
in the `mojo_c_api` directory.

To that end, this layer provide safe, idiomatic Rust equivalents for the
Mojo C API functions. Not all functions are covered yet (feel free to add more
if you need them!).

These wrappers are suitable for general use; if you find you need to work with
Mojo from Rust at a low level, you should use this crate instead of rolling your
own bindings. However, most uses will benefit from the higher-level abstractions
in the System and Bindings APIs.

Notable improvements over the raw C API include:

1. All functions are safe to call.
1. Functions take and return references instead of pointers, and use native Rust
   types (e.g. `&str`).
1. The raw `MojoHandle` type has been replaced with the `UntypedHandle` type which
   provides lifetime management, analogous to the `ScopedHandle` types in C++.
1. The `MojoResult` type has been replaced with a native Rust `Result` type.
1. The `*Options` structs have been replaced with bitflags.

## System API

The Rust System API is meant to provide an intermediate layer between the
low-level C API and the user-facing Bindings API. Its code lives in the `system`
directory.

This crate consolidates the low-level functions from the C API into a more
idiomatic design. For example, rather than several independent functions that
are meant to operate on message pipe handles, it defines a strongly-typed
`MessagePipeEndpoint` with appropriate traits and inherent impls that provide
all the message-pipe-related functionality.

Like the C Wrappers, this crate is suitable for standalone use on lower-level
projects that want to interact with Mojo via Rust. However, for most use-cases,
the higher-level constructs in the Bindings API will be more convenient.

## Bindings API

The [Rust Bindings API](/mojo/public/rust/bindings/README.md)
provides the user-visible API for sending and
receiving messages via Mojo. In contrast to the lower two layers, which aim to
provide APIs for the basic Mojo primitives, the Bindings API provides high-level
APIs that build atop them. For example, the `MessagePipeWatcher` type uses a
trap to execute a user-provided function whenever a message comes in on the
watched pipe.

Many of the abstractions are specific to [Mojom](/mojo/public/tools/bindings/README.md),
the Interface Definition Language (IDL) used to specify message formats. For
example, the `Remote` and `Receiver` types provide the ability to send and
receive messages that were defined in `.mojom` files.

The types in this crate are designed to be used by chromium developers doing
day-to-day Rust development. If your goal is to send message to/from Rust using
Mojo, you should use the Bindings API.
