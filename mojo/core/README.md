# Mojo Core Overview

[TOC]

## Overview

Mojo Core implements an IPC mechanism based on Message routing over a network
of "Nodes". A Node in typical usage corresponds to a system process.
"Messages" are exchanged between pairs of "Ports", where a pair of Ports
represent a "MessagePipe" from the outside. A MessagePipe provides
reliable in-order delivery of Messages.
Messages can be used to transport Ports and platform handles (file descriptors,
Mach ports, Windows handles, etc.) between Nodes.
Communication between Nodes is done through platform IPC channels, such as
AF_UNIX sockets, pipes or Fuchsia channels. A new Node has to be invited into
the network by giving it an IPC channel to an existing Node in the network,
or else provided with a way to establish one, such as e.g. the name/path to a
listening pipe or an AF_UNIX socket.

Mojo Core also provides a shared memory buffer primitive and assists in the
creation and transport of shared memory buffers as required by the underlying
operating system and process permissions.

A single Node in the network is termed the "Broker".
The Broker has some special responsibilities in the network, notably the Broker
is responsible for:
  - Introducing a pair of Nodes in the network to one another to establish a
    direct IPC link between them.
  - Copying handles between Nodes, where they cannot themselves do so. This is
    e.g. the case for sandboxed processes on Windows.
  - Creating shared memory objects for processes that can't themselves do so.

Mojo Core exposes two API sets:
  1. The [C system API](../public/c/system/README.md).
  1. The [embedder API](embedder/README.md).

This document is to describe how these APIs are implemented by Mojo Core.

## Layering

This image provides an overview of how the Mojo Core implementation is layered:

![Mojo Core Layering](doc/layering.png)

The C system API is a stable, versioned API, exposed through a singleton
structure containing C function thunks. This allows multiple clients in the same
application to use a single embedder-provided instance of Mojo, and allows these
clients to be versioned and distributed independently of Mojo.
Using C thunks also makes it easier to provide compatibility with other
programming languages than C and C++.

When clients invoke on the Mojo public API, they go through the dispatch
functions implemented in [dispatcher.cc](dispatcher.cc), which in turn forwards
the call to a global instance of [mojo::core::Core](core.h).
Core further dispatches the calls to the implementation instance, which is
either a [mojo::core::Dispatcher](dispatcher.h), or a
[mojo::core::UserMessageImpl](user_message_impl.h).
In the case of a Dispatcher, the incoming MojoHandle is looked up in the
[mojo::core::HandleTable](handle_table.h), which returns the Dispatcher
corresponding to the MojoHandle.

The public API exposes the following concepts as MojoHandles:
  1. [DataPipeConsumer](data_pipe_consumer_dispatcher.h).
  1. [DataPipeProducer](data_pipe_producer_dispatcher.h).
  1. [Invitation](invitation_dispatcher.h).
  1. [MessagePipe](message_pipe_dispatcher.h).
  1. [PlatformHandle](platform_handle_dispatcher.h).
  1. [SharedBuffer](shared_buffer_dispatcher.h).
  1. [Watcher](watcher_dispatcher.h) - this is know as "Trap" in the public API.

These are implemented as subclasses of [mojo::core::Dispatcher](dispatcher.h).
The Dispatcher class implements the union of all possible operations on a
MojoHandle, and the various subclasses implement the subset appropriate to their
type.

Additionally the public API exposes Messages as MojoMessageHandles, which are
simply an instance of a [mojo::core::UserMessageImpl](user_message_impl.h).

## Threading model

Mojo Core is multi-threaded, so any API can be used from any thread in a
process. Receiving Messages from other Nodes is done on the "IO Thread", which
is an [embedder-supplied task runner](embedder/README.md#ipc-initialization).
Sending Messages to other Nodes is typically done on the sending thread.
However, if the native IPC channel's buffer is full, the outgoing Message will
be queued and subsequently sent on the "IO Thread".

## Event Dispatching

During the processing of an API call and during activity in the NodeController,
various entities may change states, which can lead to the generation of
[TrapEvents](../public/c/system/README.md#Signals-Traps) to the user.
The [mojo::core::RequestContext](request_context.h) takes care of aggregating
Watchers that need dispatching and issuing outstanding TrapEvents as the last
RequestContext in a thread goes out of scope.

This avoids calling out to user code from deep within Core, and the resulting
reentrancy that might ensue.

## Messaging implementation

Mojo Core is implemented on top of "[Ports](ports)" and is an embedder of Ports.

Ports provides the base abstractions of addressing,
[mojo::core::ports::Node](ports/node.h) and
[mojo::core::ports::Port](ports/port.h) as well as the messaging and event
handling for maintaining and transporting Port pairs across Nodes.

All routing and IPC is however delegated to the embedder, Mojo Core, via
the [mojo::core::NodeController](node_controller.h) which owns an instance of
Node, and implements its delegate interface.

### Naming and Addressing

Nodes are named by a large (128 bit) random number. Ports are likewise named
by a large (128 bit) random number.
Messages are directed at a Port, and so are addressed by a (Node:Port) pair.

Each Port has precisely one "conjugate" Port at any point in time. When a
Message is sent through a Port, its conjugate Port is the implicit destination,
and this is a symmetrical relationship. The address of the conjugate Port can
change as it is moved between nodes.
In practice a Port is renamed each time it's moved between Nodes, and so changes
in both components of the (Node:Port) address pair.

Note that since each Port has a single conjugate Port at any time, en-route
Messages are addressed only with the destination Port address, as the source
is implicit.

### Routing and Topology

A process network initially starts with only the Broker process Node.
New Nodes must be introduced to the broker by some means. This can be done by
inheriting an IPC handle into a newly created process, or by providing them
the name of a listening IPC channel they can connect to.

Each Node thus starts with only a direct IPC connection to a single other Node
in the network. The first time a Node tries to forward a Message to a Node it
doesn't have a direct IPC channel with, it will send a message to the broker to
request an introduction to the new peer Node.

If the broker knows of the peer Node, it will construct a new native IPC
channel, then hand one end of it to the requesting Node and one end to the new
peer Node. This will result in a direct Node-to-Node link between the two, and
so the topology of a process network will trend towards a fully connected graph.

### Messages

A UserMessageImpl is the unit of transport in Mojo Core.
This is a collection of
  1. Handles,
  1. Ports,
  1. User data.

Mojo Core takes care of transferring and serializing and deserializing Handles
and Ports.
The user data on the other hand has to be serialized and deserialized by the
user. Mojo Core optimizes this by allowing the user data to be serialized only
at need, e.g. when a Message is routed to another Node.

### Port

A Node maintains a set of Ports local to that Node.

Logically, each Port has a conjugate Port where all its Messages are destined.
However, when a Port is transferred to a different Node, the newly created Port
on the destination Node will have the transferred Port as its next-hop "Proxy"
Port it sends Messages to. The Proxy Port is responsible for delivering
in-progress Messages across to the new destination Port, and may buffer Messages
to that end. Once a Proxy Port has delivered all in-progress Messages, it
is dissolved and Messages start flowing through the next Peer Port in turn,
which may be the conjugate Port or another Proxy Port.

Each Port maintains state relating to its:
  - Current Peer Port, which may be a Proxy Port, but in the stable state will
    be its conjugate Port.
  - Its current state of messaging, which changes as Ports move across Nodes.
  - Its outgoing Message state, notably Message serial numbers.
  - Its incoming Message queue.

The unit of messaging at this level is the
[mojo::core::ports::Event](ports/event.h), which has several sub-types relating
to the business of maintaining and transporting Ports and keeping their
Peer Port address up to date.

The UserMessageEvent subclass is the event type that carries all user messaging.
A UserMessageEvent owns a
[mojo::core::ports::UserMessage](ports/user_message.h), which is the interface
to the embedder's Message type. Mojo Core implements this in
[mojoe::core::UserMessageImpl](user_message_impl.h). Note that at the Ports
level, this is simply opaque user data. It's the Mojo Core embedder that is
aware of the handles and data attached to a UserMessageImpl.

Note that if a Port is transferred across multiple Nodes, it may end up with a
multi-leg Peer relationship. As result, and because different Messages for the
same Port may take different routes through the process network, out of order
Message delivery is possible. The incoming
[mojo::core::ports::MessageQueue](ports/message_queue.h) takes care of
re-ordering incoming Messages.

The Node does all Message handling and delivery for its local Ports, but
delegates all routing to its delegate through the
[mojo::core::ports::NodeDelegate](ports/node_delegate.h) interface.
Mojo Core implements this in [mojo::core::NodeController](node_controller.h).

### Node to Node IPC

The business of handling Node to Node IPC is primarily handled in
[mojo::core::NodeChannel](node_channel.h) as well as
[mojo::core::Channel](channel.h) and its OS-specific subclasses by exchanging
[mojo::core::Channel::Message](channel.h)s.

Note that a Channel::Message always goes from a source Node to a destination
Node through a Channel (strictly speaking one on each side). As the Channels on
either side (or the OS-provided IPC mechanism) conspire to copy the handles
across, the processes involved in the handle transfer are always implicit.
This means that a low-privilege or sandboxed process can only send its own
handles in a Message, and it's not possible to abuse this to acquire another
process's handles.

A NodeChannel wraps a Channel that communicates to and from another given Node.
It implements all messaging to another specific Node, both relating to general
messaging (see NodeChannel::OnEventMessage), as well as messaging relating to
invitations and Port management.
Since the NodeChannel is associated with a single given Node, the peer Node name
is an implicit given in any incoming Message from the Channel.

The Channel takes care of maintaining and servicing a queue of Messages outgoing
to the peer node, as well as reading and parsing the next incoming Message from
the IPC channel.
Depending on the platform and the Node topology, it may also take care of moving
outgoing and incoming handles between the processes hosting the Nodes on either
side of the Channel.

Note that on Windows, the Broker always takes care of handle copying, as the
Broker will typically be the only process with sufficient privilege to copy
handles. This means that any Message carrying handles is routed through the
Broker on Windows.

### Packet Framing

An in-transit Message on an IPC channel will be serialized as a
[mojo::core::Channel::Message](channel.h).

For user Messages, this will contain:
  1. A [mojo::core::Channel::Message::Header](channel.h) containing:
    - The length of the full Message
    - The length of the header.
    - The Message type.
    - The number of handles.
  2. The Platform handles.
  3. The serialized [mojo::core::ports::Event](ports/event.h). Specifically for
     a UserMessageEvent:
    - [mojo::core::ports::SerializedHeader](ports/event.cc).
    - [mojo::core::ports::UserMessageEventData](ports/event.cc).
    - Ports; num_ports times core::Ports::PortDescriptor.
    - The serialized user data.

## Buffering

Message buffering in Mojo Core primarily occurs at two levels:
  - In the incoming Message queue of a receiving Port.
  - In the outgoing queue for an IPC Channel to a peer Node.

There is also transient buffering of Messages to a new Peer Node while
introductions are in-flight.
Finally, while a Port is in transport to different node, it buffers inbound data
until the destination Node accepts the new Peer, at which time any buffered data
is forwarded to the new Port in the destination Node.

Mojo Core does implement quotas on Port receiving queue Message number and byte
size. If the receive length quota is exceeded on a Port, it signals an
over-quota TrapEvent on the receiving Port. This doesn't however limit buffering
as the caller needs to handle the TrapEvent in some way - most likely by simply
closing the Port.

Since Mojo Core does not limit buffering, the producers and consumers in a
process network must be balanced.
If a producer runs wild, the producer's process may buffer an arbirarly large
number of Messages (and bytes) in the outgoing Message queue on the IPC channel
to the consumer's Node.
Alternatively the consumer's process may buffer an arbitrarily large
number of Messages (and bytes) in the incoming Message queue on the consumer's
Port.

## Security

Mojo can be viewed as a Capability system, where a Port is a Capability.
If a Node can name a Port, that Port can be considered a granted Capability
to that Node. Many Ports will grant other Ports upon request, and so the
transitive closure of those Ports can be considered in the set of granted
Capabilities to a Node.

Native handles can also be viewed as Capabilities, and so any native handle
reachable from the set of Ports granted to a Node can also be considered in
the Node's set of granted Capabilities.

There's however a significant difference between Ports and native handles, in
that native handles are kernel-mediated capabilities. This means there's no way
for a process that doesn't hold a handle to operate on it.

Mojo Ports, in contrast, are a pure user-mode construct and any Node can send
any Message to any Port of any other Node so long as it has knowledge of the
Port and Node names. If it doesn't already have an IPC channel to that node,
it can either send the Message through the Broker, or request an invitation to
the destination Node from the Broker and then send the Message directly.

It is therefore important not to leak Port names into Nodes that shouldn't be
granted the corresponding Capability.
