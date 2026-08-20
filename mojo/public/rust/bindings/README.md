# Mojo Rust Bindings API

This document is a subset of the [Mojo documentation](/mojo/README.md).

[TOC]

## Overview

The Mojo Rust Bindings API provides users with high-level components for
sending and receiving Mojom messages from Rust. Most of the constructs are
directly analogous to the [C++ Bindings API](/mojo/public/cpp/bindings/README.md).

## Key Concepts

In this section, we'll walk through the creation of a simple mojom service that
performs arithmetic operations.

For those who prefer to work from existing runnable code, see
[this CL](https://crrev.com/c/7609063) for the setup of a minimal service that
serves strings from the browser process.

### Mojom Files

Using the Rust bindings begins with a  `.mojom` file written in the
[Mojom Interface Definition Language](/mojo/public/tools/bindings/README.md).
Mojom files are processed by a `mojom()` GN target, and produce a Rust crate
containing types and traits corresponding to the definitions in the `.mojom`
file.

To get started, suppose we create a Mojom file at
`//services/math/public/mojom/math.mojom`. We would then add the following GN
target to `//services/math/public/mojom/BUILD.gn`:

```text
import("//mojo/public/tools/bindings/mojom.gni")

mojom("math") {
  sources = [
    "math.mojom",
  ]
}
```

Under the hood, this will generate _several_ GN targets. For C++ code, you
depend on the `//services/math/public/mojom:math` target. For Rust code, you
depend on the `//services/math/public/mojom:math_rust` target:

```text
deps += ["//services/math/public/mojom:math_rust"]
```

In order to use the generated code, simply `chromium::import!` the target like
with any other first-party crate:

```Rust
chromium::import! {
    "//services/math/public/mojom:math_rust";
}

// Refer to it as a normal crate
use math_rust::MathService;
...
```

NOTE: Mojom supports a `module` keyword which can be used to specify the C++
namespace in which its definitions should appear. Unfortunately, C++ namespaces
are a fundamentally different architecture than Rust's crate-based organization.
Therefore, **the `module` keyword is ignored** when generating Rust code;
instead, each `.mojom` file corresponds to a crate with the same name as its
GN target.

#### Inspecting the Generated Code

Building the `math_rust` target (or anything that depends on it) will trigger
the Mojom generator:

```text
autoninja -C out/default services/math/public/mojom:math_rust
```

This will create a generated file
`out/default/gen/services/math/public/mojom/math.mojom.rs` which you can
inspect to see the Rust definitions Mojom has generated for each entry in
the `.mojom` file.

### Interfaces

Interfaces specify the kinds of messages that can be sent between Mojo
endpoints. Say your Mojom file contains the following interface:

```text
interface MathService {
  Add(int32 left, int32 right) => (int32 sum);
  Div(int32 dividend, int32 divisor) => (int32 quotient, int32 remainder);
}
```

This will be represented in Rust as a trait:

```Rust
trait MathService {
  fn Add(&mut self, left: i32, right: i32,
         response_callback: impl FnOnce(i32) + Send);
  fn Div(&mut self, dividend: i32, divisor: i32,
         response_callback: impl FnOnce(i32, i32) + Send);
}
```

This trait is then used to instantiate a `Remote` and `Receiver` pair,
which send messages of the types defined in the `MathService` interface.

### Remotes and Receivers

Remotes and Receivers represent the two ends of a Mojo message pipe. The
**Remote** is a client, sending requests to the **Receiver**, which acts like
a server: handling the request, and sending a response if applicable.
Remotes and Receivers always come in pairs. This abstraction is inherently
unidirectional; if you want the ability to send requests in both directions,
you should create _two_ Remote/Receiver pairs.

Each Remote/Receiver pair sends messages from a single Mojom `interface`. To
create a new pair, call the `PendingRemote::new_pipe` function, and specify the
type of the interface using a trait object:

```Rust
let (p_remote: PendingRemote<dyn MathService>,
     p_receiver: PendingReceiver<dyn MathService>) = PendingRemote::new_pipe();

// Equivalent to the above
let (p_remote, p_receiver) = PendingRemote::<dyn MathService>::new_pipe();
```

### Sending Messages

Sending a Mojo message is a synchronous operation, but receiving one can happen
at any time. Therefore, in order to process the responses to its messages, a
Remote must first be _bound_ to a specific sequence, which will handle the
processing. This is done by calling the `bind` function to transform a
`PendingRemote` into a full-fledged `Remote`.

``` Rust
// Binds to the current default sequence
// `bind_with_options` can be used to specify a different sequence,
// or add a disconnect handler.
let math_remote: Remote<dyn MathService> = p_remote.bind();
```

Once the Remote is bound, you can start sending messages immediately (even
before the Receiver is also bound). To send a message, invoke the corresponding
function on the remote object, and pass a callback specifying how to handle
the response, if there is one.

```Rust
// This sends a message to the receiver asking it to execute its `Add`
// implementation (presumably on a different sequence or in a different
// process). It will send the result back to us, and then we will print
// it on _this_ sequence.
math_remote.Add(1, 2, |sum| println!("1 + 2 = {sum}"));
```

## Receiving Messages

Like Remotes, Receivers must be bound to a particular sequence on which they
will process incoming messages. However, unlike Remotes, which merely forward
the user's arguments across the pipe, Receivers need to specify _how_ to
process the incoming messages. They also frequently need additional state in
order to handle those messages consistently.

Both of these problems are addressed through the use of **state objects**. A
state object is any object which (1) implements a Mojom interface trait -- in
our case, the `MathService` trait -- and (2) has been registered by calling the
`register_mojom_state_object_impls` macro with the `impl` declaration:

```Rust
// A MathService which counts the number of times you've called `Add`.
struct CountingMathService {
  num_times_added: usize;
};

impl MathService for CountingMathService {
  fn Add(&mut self, left: i32, right: i32,
         response_callback: impl FnOnce(i32) + Send) {
    self.num_times_added += 1;
    let ret = left + right;
    response_callback(ret);
  }

  fn Div(&mut self, dividend: i32, divisor: i32,
         response_callback: impl FnOnce(i32, i32) + Send) {
    ...
  }
}

// Don't forget this! It sets up some important definitions that the compiler
// needs.
register_mojom_state_object_impls!(impl MathService for CountingMathService);

// Bind a receiver to the current sequence which counts the number
// of times `Add` is called.
fn setup_counting_receiver(p_receiver: PendingReceiver<dyn MathService>)
  -> Receiver<CountingMathService>
{
  let count_state = CountingMathService { num_times_added: 0 };
  p_receiver.bind(count_state)
}
```

When a receiver receives an `Add` request, it invokes the `Add` method on its
state object. Therefore, you can define multiple implementations of your service
with different behaviors by defining multiple kinds of state object with
different implementations of the trait:

```Rust
// A MathService which uses saturating addition.
struct SaturatingMathService {};
impl MathService for SaturatingMathService {
  fn Add(&mut self, left: i32, right: i32,
         response_callback: impl FnOnce(i32) + Send) {
    response_callback(left.saturating_add(right));
  }
  ...
 }

register_mojom_state_object_impls!(impl MathService for SaturatingMathService);

// Bind a receiver to the current sequence which uses saturating addition
fn setup_counting_receiver(p_receiver: PendingReceiver<dyn MathService>)
  -> Receiver<SaturatingMathService>
{
  p_receiver.bind(SaturatingMathService {})
}
```

If you're curious, you can also view some
[live code examples](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/rust/bindings/test/state_objects.rs).

Notice that because different Receivers may have different behavior for the same
interface, the `Receiver` type is parameterized by the type of the
_state object_, rather than the interface trait.

Note that receivers can receive messages while they are pending, but those
messages simply sit in the pipe until the receiver is bound and begins
scheduling tasks to process them.

### State Objects With Bounds

Due to limitations in macro syntax, if your state object takes generic
parameters with bounds, you must express those bounds in a `where` clause
when invoking the macro:

```Rust
// A MathService which invokes a user-provided notification function.
struct NotifyingMathService<F: FnMut(u32) + Send> { f: F };

impl<F: FnMut(u32) + Send> MathService for NotifyingMathService<F> { ... }

register_mojom_state_object_impls!(\
impl<F> MathService for NotifyingMathService<F> where F: FnMut(u32) + Send);
```

## Representing Mojom Types in Rust

This section describes how Mojom types correspond to Rust types. If you want to
see exactly what the Mojom generator created for your Mojom
types, you can
[inspect the generated files directly](#inspecting-the-generated-code).

### Numbers

Numeric types in Mojo are mapped directly to the corresponding primitive type in
Rust: `int16` to `i16`, `uint64` to `u64`, etc.

Exception: The `float` and `double` types are permitted to appear as map keys in
Mojom, but not in Rust. When they appear in that position, the resulting type
is an `OrderedFloat<f32>` or `OrderedFloat<f64>` instead. See the
[ordered_float](https://docs.rs/ordered-float/latest/ordered_float/)
crate for more information about these types.

### Strings

Mojom strings are mapped to Rust `String`s.

IMPORTANT: The `String` type in Rust requires UTF-8 encoding. If you want to
send a non-UTF-8 string to Rust via Mojo, you should send it as raw bytes
(`array<uint8>`) instead. If you send a `string` value with an improper
encoding, the message will fail validation and the sending process will likely
be terminated as a result.

In the future, Mojom hopes to enforce that _all_ strings
[are UTF-8 endcoded](https://crbug.com/326376204).

### Structs, Enums, and Unions

Mojom structs and enums are represented as Rust structs and enums. Mojom unions
are represented as Rust enums with one parameter for each variant.

```text
// Mojom

struct TwoInts {
    uint8 a;
    uint16 b;
};

enum TestEnum {
    Zero,
    Three = 3,
    Four,
    Seven = 7,
};

union BaseUnion {
  TestEnum e;
  TwoInts n;
  float f;
};
```

```Rust
// Rust

pub struct TwoInts {
  pub a: u8,
  pub b: u16,
}

pub enum TestEnum {
  Zero = 0,
  Three = 3,
  Four = 4,
  Seven = 7,
}

pub enum BaseUnion {
  e(TestEnum),
  n(TwoInts),
  f(f32),
}
```

NOTE: Enums with duplicate discriminants are not supported
(<https://crbug.com/490157675>)

### Arrays and Maps

Mojom arrays correspond to Rust `Vec`s, and Mojom maps correspond to `HashMap`s.

### Nullables

Nullable types in Mojom (e.g. `int8?`) correspond to `std::Option`s in Rust
(`Option<i8>`).

## Other Mojom Features

The Rust Mojo bindings are still in active development; if there's a feature
you'd like to see supported, contact the Rust-in-Chrome team
(<rust-in-chrome@google.com>, or <dloehr@google.com>/<flowerhack@google.com>).

### Type Mapping

By default, each Mojom struct, enum, or union will generate a _new_ Rust type
which exactly matches the Mojom definition. However, many of these types already
have natural representations in Chrome. For example, it can be inconvenient
to receive a
[mojom::Url](https://source.chromium.org/chromium/chromium/src/+/main:url/mojom/url.mojom;l=18;drc=7e4e4023dfeb5be7ae4390d5307e544ad92e726f)
when what you really want is a
[GURL](https://source.chromium.org/chromium/chromium/src/+/main:url/gurl.rs;l=17;drc=56bd8374da2a6d20fa857cd851e6087287ed6190).

To make this easier, you can configure the Rust bindings to use your custom type
in place of the automatically-generated Mojom type. To do so, you'll need to
specify the conversion logic between your types by defining the `From` and
`TryFrom` traits:

```Rust
impl From<GURL> for mojom::Url { ... }
impl TryFrom<mojom::Url> for GURL { ... }
```

It's possible that the value in the Mojom message will be malformed in some way,
violating invariants of your type that can't be expressed in Mojom. This is why
the Mojom to Rust conversion uses `TryFrom`; if the conversion returns an error,
the message will automatically be reported as malformed.

These definitions should be included in a separate file, typically called
something like `traits.rs`. As a technical detail, this file will be compiled
into the auto-generated Mojom crate (to work around the
[orphan rule](https://doc.rust-lang.org/reference/items/implementations.html#r-items.impl.trait.orphan-rule)).

Once that is done, simply add a `rust_typemaps` entry variable in your `mojom()`
GN target:

```text
mojom("my_service") {
  sources = [ "my_service.mojom" ]
  generate_rust = true
  rust_typemaps = [
    {
      types = [
        {
          mojom = "my_service.mojom.MojomStruct"
          rust = "RustStruct"
        },
      ]
      traits_file = "my_traits.rs"
    },
  ]
}
```

Once that's done, any references to `MojomStruct` in generated `interface` code
will be replaced with `RustStruct`. All conversions will happen automatically,
behind the scenes.

For a simple working example, see the test suite:
[test_traits.rs](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/rust/bindings/test/test_traits.rs).

TODO(crbug.com/547988937): link to an in-production use case instead/as well.

### Associated Interfaces

The basic Mojo model is simple: you create two ends of a message pipe (a
`Remote` and a `Receiver`, then send them to different part of the program and
use them to send messages through that pipe). If you want to communicate with
multiple parts of the program, you create multiple pipes.

However, those pipes operate independently and asynchronously. Therefore, two
messages sent on different pipes won't necessarily arrive in the order they were
sent (even if they arrive on the same sequence). If you really need to ensure
ordering between multiple `Remote`/`Receiver` pairs, you can use associated
interfaces.

For an alternate description of the concepts (in terms of C++), you can see
the description in the
[C++ bindings](/mojo/public/cpp/bindings/README.md#Associated-Interfaces).
Both languages behave identically, so this mostly serves as an alternate
explanation of the same thing.

#### Overall Model

The Rust types `AssociatedRemote` and `AssociatedReceiver` are analogoues to the
`Remote/Receiver` types, but with the difference that they do not own their
message pipe. Instead, they communicate using an existing pipe, connected by
some other `Remote/Receiver` pair. Those are the **primary endpoints**; the
other are **associated endpoints**.

Associated endpoints operate just like the primary endpoints; they have an
interface (e.g. `AssociatedRemote<dyn MathService>`) and can be used to send
and receive messages (e.g. `assoc_rem.Add(1, 3)`). The main different is that
_all messages sent on a single message pipe will be processed in the order they
were sent_ (strict FIFO order).

There can be any number of associated endpoints per pipe, but only one primary
pair. The endpoints don't all need to have the same interface; you can have
`AssociatedRemote<dyn MathService>` and `AssociatedRemote<dyn LaundryService>`
operating on the same pipe without problems.

**Caveats**:

* Since the primary pair owns the pipe, closing one of them will permanently
  disconnect _all_ associated interfaces on that pipe.
* Due to the FIFO ordering guarantee, if _any_ remote sends a message before its
  receiver is ready (o.e. it hasn't been [bound](#sending-messages)), _all_
  messages on the pipe will be stalled until the receiver is set up or dropped.
  * This also applies to disconnect notifications when one half of an associated
    pair is dropped.

#### Usage

As with regular remotes and receivers, associated endpoints begin their life in
a "pending" form that must be bound to a particular sequence before it can be
used.

In Rust, these are the `PendingAssociatedRemote` and `PendingAssociatedReceiver`
types; in Mojom, they are written `pending_associated_remote` and
`pending_associated_receiver`.

```text
// This interface does nothing allows users to create an associated
// MathService pair. It can also be used to send other kinds of
// messages, like a normal interface.
interface Foo {
  PassMathService(pending_associated_receiver<MathService> bar_rec);
  DoSomething(int32 arg);
};

// Note that the MathService interface is entirely normal; you can _also_
// create a non-associated Remote/Receiver pair for MathService if you like.
interface MathService {
  Add(int32 x, int32 y);
};
```

To create a new associated pair, call `PendingAssociatedRemote::new_pair` or
`PendingAssociatedReceiver::new_pair`.

```Rust
let (p_remote, p_receiver) = PendingAssociatedRemote::<dyn MathService>::new_pair();
```

Like non-associated `Remotes` and `Receivers`, you must first bind these before
they can be used.

```Rust
let mut bar_remote: AssociatedRemote<dyn MathService> = p_remote.bind();
```

However, associated endpoints have an additional step: you must specify which
message pipe the pair will use. To do so, **you must send one of the endpoints
in a Mojo message**:

```Rust
// Sending `p_remote` over an existing endpoint associates BOTH `p_remote` and `p_receiver`
// with that endpoint's underlying message pipe.
foo_remote.PassBar(p_receiver);
```

Note that `foo_remote` does not have to be the pipe's primary `Remote`; it's
perfectly fine to send `p_receiver` through an already-set-up associated endpoint.

**Caveats**:

* You must send exactly one of each pending associated pair.
* Once sent, you cannot send an associated endpoint again (since it has already
  been associated with the first pipe you sent it through).
* You cannot send messages until you've sent one of the endpoints through a pipe
  (since there's no way to know which pipe to send the message on).
  * You can check whether this has happened yet by calling
    `.ready_for_messages()`.

Once that's done, you can treat the endpoints just like primary endpoints, and
bind/drop them the same way.

#### Associating with a C++ pipe

Sometimes, you may want to create an associated endpoint on a pipe that already
exists in C++ code (that is, attach to a C++ `Remote` or `Receiver`) (for example,
attaching a frame-associated interface to `content::mojom::RenderFrameHost` or
`blink::mojom::LocalFrame`).
In order to do so, you'll need to convert one of your pending endpoints to a form
that C++ can send over a pipe.

To do so, create your pending endpoints using `new_pair_cpp` instead of `new_pair`:

```Rust
let (p_remote, p_receiver_cpp) =
    PendingAssociatedRemote::<dyn MathService>::new_pair_cpp();

let mut math_remote = p_remote.bind();
```

This returns a tuple containing:
1. A normal Rust `PendingAssociatedRemote` (or `PendingAssociatedReceiver`),
   which you can bind and use in Rust. Note that this endpoint is managed
   by C++ under the hood and cannot be sent in a Mojo message.
2. A `cxx::UniquePtr<CxxPendingAssociatedEndpoint>`, which can be converted
   into a C++ `PendingAssociated*` and therefore sent in a Mojo message via the
   C++ endpoint:

```cpp
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"

mojo::rust::bindings::CxxPendingAssociatedEndpoint p_receiver_cpp =
    GetFromRustOverFFI();

mojo::PendingAssociatedReceiver<MathService> pending_receiver =
    mojo::rust::bindings::PassPendingAssociatedReceiver<MathService>(
        std::move(p_receiver_cpp));

// Send `pending_receiver` in a message to associate the pair with that pipe
```

##### Receiving a C++ associated endpoint in Rust

The same process can be applied in reverse when receiving the pending endpoint
via C++. After extracting it from the Mojo message, first convert the
C++ `PendingAssociated*` using `MakeAssociatedEndpointRustAdapter`:

```cpp
// In the response handler for a mojo message

mojo::rust::bindings::CxxPendingAssociatedEndpoint endpoint_adapter =
    mojo::rust::bindings::MakeAssociatedEndpointRustAdapter(
        std::move(pending_receiver));
```

Then you can pass `endpoint_adapter` across FFI to Rust, and convert it into
a Rust pending associated endpoint using `from_cpp()`:

```Rust
let p_receiver =
    PendingAssociatedReceiver::<dyn MathService>::from_cpp(endpoint_adapter);

let mut math_receiver = p_receiver.bind(some_state_object);
```

Once bound, you can use the `Remote` or `Receiver` to communicate over the C++
message pipe just like any other associated endpoint.

### Versioning

<https://crbug.com/496924230>
