# Rust API Design Principles

This policy lays out guidelines for designing the user-facing parts of core Rust
libraries in Chromium. It is meant to provide guidance on relative priorities
to help decide between different possible designs.

Note: These guidelines are aimed at _core_ Rust libraries (e.g. `//base`) which
expect to see broad, general use across Chromium. They may or may not apply to
code designed for more specialized parts of the codebase. Conversely, since
these guidelines are aimed at libraries _in Chromium_, they have different
considerations than the Rust standard library, which aims to support all
possible use cases.

These design principles are undergoing constant development; expect more to be
added in the future. Existing guidelines should change only rarely.

## Core Design Goals
The first of these goals is the most important; beyond that, these goals are not
ordered.

1. APIs must promote the use of Rust in Chromium. If Chromium developers are
unwilling to use an API (because it’s unergonomic, not performant, etc), then it
is a bad design.
1. Therefore…
    1. APIs should be performant, imposing as little runtime overhead as
       possible.
    1. APIs should be ergonomic, requiring as little boilerplate as possible.
    1. APIs should be idiomatic Rust, to make it easy for Rust developers to
       use it, and for C++ developers to familiarize themselves with Rust.
1. APIs should minimize the potential for misuse, especially misuse with
   security implications (such as undefined behavior).

## (Un)Safety
1. APIs should not require users to use `unsafe` code if at all possible.
    1. This includes `unsafe` functions, as well as those that require `unsafe`
       in order to use the result (e.g. by returning a raw pointer).
    1. Examples of exceptions are code that imposes such a large performance hit
       that it’s deemed unacceptable to use, or that requires patterns that are
       annoying enough that developers will try to avoid using them.
1. If we provide an `unsafe` API, we should try to provide an equivalent safe
   API and encourage its use when possible.
1. _Internal_ use of `unsafe` is acceptable, so long it is not exposed to the
   user, and there isn’t an easy safe alternative.

## Parameter and Return Types
1. Prefer to pass flags via a type that makes their meaning clear at the call
   site. For example:
    ```
    // Bad:
    fn slide(left: bool); // Slide left if true, right if false

    // Good:
    enum Direction { Left, Right }
    fn slide(direction: Direction);
    ```
1. When returning multiple values, prefer to give them names by defining a
   one-off struct if there is any risk of confusion as to what they mean.
   For example:
   ```
   // Bad:
   fn make_connected_ports() -> (Port, Port);

   // Good:
   struct PortPair { input: Port, output: Port}
   fn make_connected_ports() -> PortPair;
   ```