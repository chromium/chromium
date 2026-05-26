# Using `unsafe` code in first-party Chromium code

Rust is well-known as a memory-safe language, but it has an escape hatch in the
form of [unsafe Rust](https://doc.rust-lang.org/book/ch20-01-unsafe-rust.html),
which permits developers to take responsibility from the compiler. Unsafe Rust
can be more powerful or faster than safe Rust, but requires careful and precise
design, as well as additional mental effort spent manually ensuring that no
undefined behavior is possible.

This document discusses when and how to use unsafe code in first-party Chromium.
There is a different policy for
[unsafe code in third-party libraries](//third_party/rust/README-importing-new-crates.md#security).

## When to use `unsafe`

Usage of unsafe Rust is strongly discouraged in Chromium. It opens the door to
undefined behavior, which can lead to bugs, crashes, or security
vulnerabilities. Whenever possible, you should write your code using only safe
Rust.

However, there are times when unsafe code is unavoidable, such as interacting
with C/C++ code, or when the performance overhead of safety (from bounds checks,
ref-counting, etc) is deemed unacceptable. Developers **are permitted** to opt
in to unsafe Rust by adding the `allow_unsafe = true` flag to their GN target.

Ultimately, even Rust with `unsafe` blocks is safer than C++. We trust Chromium
developers to use their judgement when choosing when to use `unsafe` code.
Importantly,
**Using unsafe Rust does not require any additional review or approvals**.
Unsafe code should be reviewed by local owners just like safe Rust code, albeit
with the same extra scrutiny they would apply to other high-risk parts of a CL.

## How to use `unsafe`

### Safety comments

Chromium requires that all `unsafe` code is accompanied by a safety comment.
There are two types of safety documentation:

* Unsafe _declarations_ (`unsafe fn`, `unsafe trait`) declare something which is
unsafe to use, e.g. a function that dereferences a pointer could cause UB if the
pointer is null. The safety comment on a declaration must describe the
_requirements_ for the function to be used safely:

```Rust
/// Frobinate the glorb
///
/// # Safety
/// * `glorb` must be valid to dereference (non-null, valid, aligned, etc.)
unsafe fn frobinate(glorb: *const Glorb) { ... }
```

Of course, taking a `&Glorb` in the first place would be better.

* Unsafe _usages_ (`unsafe {}`, `unsafe impl`, etc) invoke an unsafe item, and
must have a safety comment explaining how the requirements in the declaration
are met:

```Rust
let glorb : Glorb = Glorb::new();
/// SAFETY: the pointer is derived from a reference and is guaranteed to be
/// safe to dereference.
unsafe { frobinate(ptr::from_ref(&glorb)) };
```

Safety comments don't need to be verbose or rehash basic Rust concepts: you
should assume the reader is familiar with the borrow checker, can look up the
[specific requirements to dereference a pointer](https://doc.rust-lang.org/std/ptr/index.html#safety),
and so on. The most important thing is that the comments are complete and
precise: they are the main (and sometimes only) way for readers to reason about
safety, so they must have all the relevant information.

In the future the presence of safety comments will be enforced at compile time
via a [clippy lint](//docs/rust/clippy.md).

### Usage Guidelines

* Write a safe implementation first. You can always retrofit it with `unsafe` if
  you really need the performance later.
* Keep it minimal as much as possible:
  * Encapsulate the unsafety: [avoid exposing unsafe APIs](//docs/rust/api_design.md##unsafety),
    and rely only on local invariants that you can enforce yourself.
  * `unsafe` blocks should wrap only the parts that are actually unsafe:

    ```Rust
    // Bad
    unsafe { do_something_dangerous(do_something_safe()) };

    // Good
    let x = do_something_safe();
    unsafe { do_something_dangerous(x) };
    ```

* Be precise: Document _all_ safety conditions in your comments. If something
  isn't documented, don't rely on it for safety.
* Be pedantic: Safety considerations can be subtle; it's a good idea to
  familiarize yourself with unsafe resources like the
  [Rustonomicon](https://doc.rust-lang.org/nomicon/), and do web searches if
  you're even a little unclear about anything.
