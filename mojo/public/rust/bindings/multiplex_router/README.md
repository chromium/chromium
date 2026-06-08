# Multiplex Routers

This directory contains the Rust implementation of the `MultiplexRouter` type,
which is used to support [associated interfaces](/mojo/public/tools/bindings/README.md#associated-interfaces).
This type is analogous to the [C++ type of the same name](/mojo/public/cpp/bindings/lib/multiplex_router.h),
but was designed and implemented from scratch. Its goal is to have the same
observable behavior as the C++ version, but its internals may work differently.

This directory exists to explain the _implementation details_ of multiplex
routers and associated interfaces. For usage examples, see the higher-level
Rust Mojo docs.

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
can be bound to a sequence, send and receive message, etc), but don't own their
own message pipe. Instead, they **associate** themselves with an existing
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
router includes the ID s part of the header of each Mojom message, which allows
the router on the other end to determine which endpoint should receive it (by
invoking that endpoint's handler).

The first pair, which actually owns the pipe, get assigned interface ID 0, which
is called the **primary** interface. Every subsequence interface ID is an
**associated** interface.

The router is also responsible for ensuring that incoming messages are processed
in FIFO order. This is typically easy -- just pass along each message as it
comes in -- but with the catch that endpoints which aren't yet bound can't
process messages. If a message arrives for an unbound endpoint, _all_ incoming
message will have to wait until that endpoint is either bound or dropped.

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
interface IDs to information about the endpoint with that ID (`EndpointInfo`).
Before an endpoint is bound, the registry maps its ID to `None`. After its is
bound, the `EndpointInfo` tracks:

1. The handler to run on messages destined for that endpoint.
1. The disconnect handler to run if that endpoint becomes disconnected.
1. The sequence on which to run the two handlers.

The registry is also responsible for generating new interface IDs whenever a new
endpoint pair is registered. In order to prevent collisions, one side of the
pipe always generates IDs with the high bit set to 1, and the other generates
IDs with the high bit set to 0.

### Task queue

Each `MultiplexRouter` maintains a queue of incoming `Task`s (messages or
disconnect notifications) in order to provide FIFO ordering. It is only used
to maintain FIFO ordering if the router receives a task for an unbound endpoint.
In that case, it needs to buffer all incoming tasks until the endpoint is ready
to process messages.

The router checks the task queue whenever a message comes in, a new endpoint is
bound, or it receives a disconnect notification.

### `MessagePipeWatcher`

This wraps the actual message pipe endpoint, and invokes the router's handler
whenever a notification arrives at the pipe. The router's handler simply does a
lookup in the endpoint registry, then either schedules the task or queues it as
appropriate.

Unlike the other two components, the watcher (and its underlying pipe endpoint)
is owned _only_ by the primary endpoint. If an associated endpoint is dropped,
any other endpoints on that end can continue communicating normally. If the
primary endpoint is dropped, then _all_ other endpoints become disconnected.

However, `ArcOrWeak` class exists to abstract over this distinction and allow
all the router functions to work regardless of whether the watcher is owned or
not.

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
