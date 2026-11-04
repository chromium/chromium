# Multiplex Routers

This directory contains the Rust implementation of the `MultiplexRouter` type,
which is used to support [associated interfaces](/mojo/public/tools/bindings/README.md#associated-interfaces).
This type is analogous to the [C++ type of the same name](/mojo/public/cpp/bindings/lib/multiplex_router.h),
but was designed and implemented from scratch. Its goal is to have the same
observable behavior as the C++ version, but its internals may work differently.

This directory exists to explain the _implementation details_ of multiplex
routers and associated interfaces. For usage examples, see the higher-level
[Rust Mojo docs](/mojo/public/rust/bindings/README.md#Associated-Interfaces).

The `MultiplexRouter` code is a submodule of the bindings crate because it is
fairly complex, but provides a relatively small API to the rest of the crate. We
don't split it into a separate crate because it's useful to allow circularity
between it and the rest of the bindings crate (e.g. the `Registrar` trait).

[TOC]

## What is an associated interface?

Note that there are [definitions of common terms](#terminology) later in this
section.

Regular `Remote`s and `Receiver`s each hold one end of a message pipe. When they
are _bound_ to a sequence, they create a `MessagePipeWatcher` which listens for
incoming messages on that pipe, and tell it to invoke a specific member function
("handler") when a message comes in. They can also specify a disconnect handler,
which will get run if the pipe is closed (meaning the other end was dropped).

Associated remotes and receivers provide exactly the same user interface (they
can be bound to a sequence, send and receive messages, etc.), but don't own
their own message pipe. Instead, they **associate** themselves with an existing
`Remote`/`Receiver` pair, and send their messages on that pair's pipe. The most
important property is that they guarantee that all messages on a given pipe will
be processed in the order they were sent. Therefore, association allows multiple
pairs of endpoints to communicate in a consistent manner.

The association step happens when one half of the associated pair is sent via
a Mojo message as a `pending_associated_remote` or `_receiver`. When that
occurs, _both_ halves of the pair magically become associated with the pipe that
sends the message.

### Implications of associated interfaces

The existence of associated endpoints complicates the messaging process.
Normally, each message pipe sends messages for a single Mojom `interface`, and
has only one remote/receiver on each end, so the destination and message
type are unambiguous. Now there are multiple remotes and/or receivers per
end, and they might be sending different kinds of messages from each other, so
we need a way to distinguish between them.

This is the job of the `MultiplexRouter`. The router replaces the
`MessagePipeWatcher` for both primary and associated remotes and receivers. Each
end of each message pipe will have exactly one `MultiplexRouter`, which watches
for incoming messages. Like before, each remote and receiver on that end tells
the router which handlers to run when a message is destined for them, and also
which sequence to run it on (different endpoints may be bound to different
sequences).

It does this by assigning each remote/receiver pair an **interface ID**. Both
halves of the pair will always have the same ID on both sides of the pipe. The
router includes the ID as part of the header of each Mojom message, which allows
the router on the other end to determine which endpoint should receive it (by
invoking that endpoint's handler).

The first pair, which actually owns the pipe, gets assigned interface ID 0,
which is called the **primary** interface. Every subsequent interface ID is an
**associated** interface.

The router is also responsible for ensuring that incoming messages are processed
in FIFO order. This is typically easy -- just pass along each message as it
comes in -- but with the catch that endpoints which aren't yet bound can't
process messages. If a message arrives for an unbound endpoint, _all_ incoming
messages will have to wait until that endpoint is either bound or dropped.

In order to make this easier, the multiplex router gives out a
`MultiplexRouterHandle` to each of its endpoints, which is just a reference to
the underlying router with an interface ID baked in.

### Terminology

This section tries to precisely define some common terms. Unfortunately, due to
history and similar common meanings, some terms are overloaded. For terms where
one meaning is much more common in the context of associated interfaces, that
meaning is bolded.

* Endpoint:
  * One end of a message pipe
  * **A generic term for a remote or a receiver**
* Interface:
  * Mojom interface: A group of related message types defined by the `interface`
    keyword. Each remote/receiver specifies a mojom interface, which determines
    which messages it can send.
  * Interface ID: **A unique identifier for each remote/receiver pair on a pipe**
    * Sometimes used as synecdoche for the endpoint with that ID.
* Primary endpoint/interface:
  * The one that owns the underlying message pipe, with interface ID 0
* Associated endpoint/interface:
  * One that does not own the underlying pipe, with nonzero interface ID
* Sequence:
  * A `SequencedTaskRunner` which executes tasks in the order they were posted.
    Each endpoint must be bound to one sequence; the same sequence can be used
    for multiple endpoints (and arbitrary unrelated tasks) simultaneously.
* Binding:
  * Specifying a sequence on which to process incoming messages for a particular
    endpoint. Endpoints must be bound before they can send or receive messages.
* Associating (or "Registering"):
  * The process of telling associated endpoints which multiplex router to send
    messages to, and telling the router which handlers to run for that endpoint.

## `MultiplexRouter` architecture

The `MultiplexRouter` class has two parts: its internal data (mutable,
thread-safe, shared), plus an underlying message pipe (immutable, owned by
the primary endpoint).

### `EndpointRegistry`

The core of a `MultiplexRouter` is its `EndpointRegistry`. This is a map from
interface IDs to the state of the endpoint with that ID (`EndpointState`):
whether it has been bound yet, and whether either end has been closed. Once the
endpoint is bound, its state also holds an `EndpointInfo`, which tracks:

1. The handler to run on messages destined for that endpoint.
1. The disconnect handler to run if that endpoint becomes disconnected.
1. The sequence on which to run the two handlers.

Entries are never removed, so the router can distinguish IDs it hasn't seen yet
from ones that are already closed. This matters because a disconnect
notification for an endpoint can arrive before the endpoint itself (this
occurs if you close the endpoint's peer before sending the endpoint in
a message).

The registry is also responsible for generating new interface IDs whenever a new
endpoint pair is registered. In order to prevent collisions, one side of the
pipe always generates IDs with the high bit set to 1, and the other generates
IDs with the high bit set to 0.

### Task queue

Each `MultiplexRouter` maintains a queue of incoming `Task`s (messages or
disconnect notifications) in order to provide FIFO ordering. It is only used
to maintain FIFO ordering if the router receives a message for an unbound
endpoint. In that case, it needs to buffer all incoming tasks until the
endpoint is bound or dropped.

Disconnect notifications never block the queue. If the endpoint isn't bound
yet, the router just records that its peer has closed, and runs its disconnect
handler once it's bound.

The router checks the task queue whenever a message comes in, or an endpoint is
registered, bound, dropped, or disconnected.

### `MessagePipeWatcher`

This wraps the actual message pipe endpoint, and invokes the router's handler
whenever a notification arrives at the pipe. The router's handler simply does a
lookup in the endpoint registry, then either schedules the task or queues it as
appropriate.

Unlike the other two components, the watcher (and its underlying pipe endpoint)
is owned _only_ by the primary endpoint. If an associated endpoint is dropped,
any other endpoints on that end can continue communicating normally. If the
primary endpoint is dropped, then _all_ other endpoints become disconnected.

The `ArcOrWeak` class exists to abstract over this distinction and
allow all the router functions to work regardless of whether the watcher is
owned or not.

### `ResponseSender`

The handlers which remotes and receivers provide to the router do not generally
contain a reference to the entire `Remote` or `Receiver` object, to avoid ref
cycles. Therefore, they need a way to send a response, as well as a way to
register any new associated endpoints that they send or receive from within a
handler.

The `ResponseSender` class provides these capabilities; it's essentially a
reference to the underlying router, with a more restricted API. A
`ResponseSender` is passed as an argument to each remote/receiver handler that
the router invokes.

### `MultiplexRouterHandle`

A `MultiplexRouterHandle` provides the public API of `MultiplexRouter`. Remotes
and receivers don't touch the router directly; instead, they interact with it
through a handle, which is just a combination of a reference to a router and
an interface ID. The handle can be used both for sending messages, and for
registering new associated endpoints.

## Interop with C++

Since the vast majority of existing pipes are in C++, it's important that we
can attach Rust associated interfaces to C++ pipes, and vice-versa.
In both directions, the primary consideration is providing intermediate types
that fit the format each language expects. The code that handles this is in
`cpp_interop/`.

### C++ vs. Rust internals

The C++ bindings have a similar, but not identical structure to Rust:

* Each end of a pipe has an `AssociatedGroupController`, which is an abstract
  base class for types that do the job of a `MultiplexRouter`.
* An unbound endpoint is a `ScopedInterfaceEndpointHandle`, analogous to an
  `UntypedHandle` but for associated interface IDs rather than an actual
  handle. These manage lifetimes for the IDs; in Rust, pending endpoints do
  this through their router handle, while the router itself closes any
  unregistered IDs carried by a `MojomMessage`.
* Binding an endpoint creates an `InterfaceEndpointClient` and attaches it to
  the controller. The controller delivers the endpoint's messages to the
  client, and the client sends messages through an `InterfaceEndpointController`
  that the group controller gives it. The client also validates incoming
  messages, matches responses to requests, and passes messages on to a
  `MessageReceiver`.

Thus, to put a Rust endpoint on a C++ pipe, we need something that looks like
a `MultiplexRouterHandle` to the Rust `Remote` or `Receiver`, and which looks
like an `InterfaceEndpointClient` to the C++ controller.

To put a C++ endpoint on a Rust pipe, we need something that looks like an
`AssociatedGroupController` to C++ endpoints, and an `EndpointInfo` to the
Rust `MultiplexRouter`.

### Rust endpoints on C++ pipes

Each Rust endpoint on the pipe has an `AssociatedEndpointRustAdapter`, a C++
object that holds its C++ state. Until the endpoint is bound, it's just a
`ScopedInterfaceEndpointHandle`. Since `cxx` can't pass Mojo's C++ types
directly, this is also the type we use to pass pending endpoints between the
languages. Since that might be done by users, we alias the type to a more
behavior-oriented name, `CxxPendingAssociatedEndpoint`.

When the endpoint is bound, we replace the handle inside the `RustAdapter`
with an `InterfaceEndpointClientAdapter`. This owns a normal
`InterfaceEndpointClient`, so the controller can treat it like any C++
endpoint. The `ClientAdapter`'s main job is to translate between C++ and Rust
concepts:

* It skips C++'s payload validation, since Rust validates messages when it
  deserializes them.
* The client replaces the request ID of outgoing requests with its own, and
  delivers the response to a responder object. The adapter provides one that
  restores Rust's request ID and passes the response to Rust.
* For incoming requests, the client provides a responder object to reply
  through. The adapter passes it to Rust in a `MojoResponderWrapper`, which
  is represented in Rust as a type of `ResponseSender`.

On the Rust side, the adapter is wrapped in a `CppRouterHandle`, which takes
the place of `MultiplexRouterHandle`. The overall `RouterHandle` and
`ResponseSender` types can each hold either the Rust or the C++ version,
so the rest of the bindings don't need to care which language owns the pipe.

### C++ endpoints on Rust pipes

C++ endpoints talk to their pipe through an `AssociatedGroupController`. For
C++ pipes, that's usually a C++ `MultiplexRouter`. For Rust pipes, it's a
`RustAssociatedGroupController`, which forwards to the Rust router.

Otherwise, the C++ stack is basically the same as for a C++ pipe. When a C++
endpoint is bound, its `InterfaceEndpointClient` attaches to the controller,
which creates a `RustAssociatedEndpointClient` to connect the two. To the
endpoint, it appears as a normal `InterfaceEndpointController`. To the router,
it appears as a normal bound endpoint.

### Associated endpoints in messages

One big difference between Rust and C++ expectations is that Rust parses
messages into a structured representation, which validates them in the process.
In contrast, C++ validates messages once at the beginning, then uses raw
pointers to extract data from those messages. As a result, it's important that
Rust does the C++ validation step before passing a message over FFI.

This mismatch can also result in some duplicated work. For example, C++
extracts associated IDs from messages before dispatching them, so they're gone
by the time the message gets passed to Rust. However, Rust expects them to
still be in the message, so we have to write them back before passing the
message over.
