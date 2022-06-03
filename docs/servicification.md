# Servicifying Chromium Features

[TOC]

## Overview

Much to the dismay of Chromium developers, practicing linguists, and keyboard
operators everywhere, the term **servicificificification** [sic] has been
egregiously smuggled into the Chromium parlance.

Lots of Chromium code is contained in reasonably well-isolated component
libraries with some occasionally fuzzy boundaries and often a surprising number
of gnarly runtime interdependencies among a complex graph of components. Y
implements one of Z's delegate interfaces, while X implements one of Y's
delegate interfaces, and now it's possible for some ridiculous bug to creep in
where W calls into Z at the wrong time and causes a crash in X. Yikes.

Servicification embodies the ongoing process of **servicifying** Chromium
features and subsystems, or refactoring these collections of library code into
services with well-defined public API boundaries and very strong runtime
isolation via Mojo interfaces.

The primary goals are to improve maintainability and extensibility of the system
over time, while also allowing for more flexible runtime configuration. For
example, with the Network Service in place we can now run the entire network
stack either inside or outside of the browser process with the flip of a
command-line switch. Client code using the Network Service stays the same,
independent of that switch.

This document focuses on helpful guidelines and patterns for servicifying parts
of Chromium.

Also see general [Mojo &amp; Services](/docs/README.md#Mojo-Services)
documentation for other introductory guides, API references, *etc.*

## Setting Up The Service

This section briefly covers early decisions and implementation concerns when
introducing a new service.

### Where in the Tree?

Based on the
[service development guidelines](/services/README.md), any service which could
be reasonably justified as a core system service in a hypothetical,
well-designed operating system may belong in the top-level `//services`
directory. If that sounds super hand-wavy and unclear, that's because it is!
There isn't really a great universal policy here, so when in doubt, contact your
favorite local
[services-dev@chromium.org](https://groups.google.com/a/chromium.org/forum#!forum/services-dev)
mailing list and start a friendly discussion.

Other common places where developers place services, and why:

- `//components/services` for services which haven't yet made the cut for
  `//services` but which are either used by Content directly or by multiple
  Content embedders.
- `//chrome/services` for services which are used exclusively within Chrome and
  not shared with other Content embedders.
- `//chromeos/services` for services which are used on Chrome OS by more than
  just Chrome itself (for example, if the `ash` service must also connect to
  them for use in system UI).

### Launching Service Processes

Content provides a simple
[`ServiceProcessHost`](https://cs.chromium.org/chromium/src/content/public/browser/service_process_host.h?rcl=723edf64a56ef6058e886afc67adc786bea39e78&l=47)
API to launch a new Service Process. The Mojo Remote corresponding to each
process launch is effectively a lifetime control for the launched process.

You may choose to maintain only a single concurrent instance of your service
at a time, similar to the Network or Storage services. In this case, typically
you will have some browser code maintain a lazy Mojo Remote to the service
process, and any clients of the service will have their connections brokered
through this interface.

In other cases you may want to manage multiple independent service processes.
The Data Decoder service, for example, allows for arbitrary browser code
to launch a unique isolated instance to process a single decode operation or
a batch of related operations (e.g. to decode a bunch of different objects
from the same untrusted origin).

Insofar as the browser can use ServiceProcessLauncher however it likes, and the
corresponding Mojo Remotes can be owned just like any other object, developers
are free to manage their service instances however they like.

### Hooking Up the Service Implementation

For out-of-process service launching, Content uses its "utility" process type.

For services known to content, this is accomplished by adding an appropriate
factory function to
[`//content/utility/services.cc`](https://cs.chromium.org/chromium/src/content/utility/services.cc)

For other services known only to Chrome, we have a similar file at
[`//chrome/utility/services.cc`](https://cs.chromium.org/chromium/src/chrome/utility/services.cc).

Once an appropriate service factory is registered for your main service
interface in one of these places, `ServiceProcessHost::Launch` can be used to
acquire a new isolated instance from within the browser process.

To run a service in-process, you can simply instantiate your service
implementation (e.g. on a background thread) like you would any other object,
and you can then bind a Mojo Remote which is connected to that instance.

This is useful if you want to avoid the overhead of extra processes in some
scenarios, and it allows the detail of where and how the service runs to be
fully hidden behind management of the main interface's Mojo Remote.

## Incremental Servicification

For large Chromium features it is not feasible to convert an entire subsystem
to a service all at once. As a result, it may be necessary for the subsystem
to spend a considerable amount of time (weeks or months) split between the old
implementation and your beautiful, sparkling new service implementation.

In creating your service, you likely have two goals:

- Making the service available to its consumers
- Making the service self-contained

Those two goals are not the same, and to some extent are at tension:

- To satisfy the first, you need to build out the API surface of the service to
  a sufficient degree for the anticipated use cases.

- To satisfy the second, you need to convert all clients of the code that you
  are servicifying to instead use the service, and then fold that code into the
  internal implementation of the service.

Whatever your goals, you will need to proceed incrementally if your project is
at all non-trivial (as they basically all are given the nature of the effort).
You should explicitly decide what your approach to incremental bringup and
conversion will be. Here are some approaches that have been taken for various
services:

- Build out your service depending directly on existing code,
  convert the clients of that code 1-by-1, and fold the existing code into the
  service implementation when complete ([Identity Service](https://docs.google.com/document/d/1EPLEJTZewjiShBemNP5Zyk3b_9sgdbrZlXn7j1fubW0/edit)).
- Build out the service with new code and make the existing code
  into a client library of the service. In that fashion, all consumers of the
  existing code get converted transparently ([Preferences Service](https://docs.google.com/document/d/1JU8QUWxMEXWMqgkvFUumKSxr7Z-nfq0YvreSJTkMVmU/edit#heading=h.19gc5b5u3e3x)).
- Build out the new service piece-by-piece by picking a given
  bite-size piece of functionality and entirely servicifying that functionality
  ([Device Service](https://docs.google.com/document/d/1_1Vt4ShJCiM3fin-leaZx00-FoIPisOr8kwAKsg-Des/edit#heading=h.c3qzrjr1sqn7)).

These all have tradeoffs:

- The first lets you incrementally validate your API and implementation, but
  leaves the service depending on external code for a long period of time.
- The second can create a self-contained service more quickly, but leaves
  all the existing clients in place as potential cleanup work.
- The third ensures that you're being honest as you go, but delays having
  the breadth of the service API up and going.

Which makes sense depends both on the nature of the existing code and on
the priorities for doing the servicification. The first two enable making the
service available for new use cases sooner at the cost of leaving legacy code in
place longer, while the last is most suitable when you want to be very exacting
about doing the servicification cleanly as you go.

## Platform-Specific Issues: Android

As you servicify code running on Android, you might find that you need to port
interfaces that are served in Java. Here is an
[example CL](https://codereview.chromium.org/2643713002) that gives a basic
pattern to follow in doing this.

You also might need to register JNI in your service. That is simple to set
up, as illustrated in [this CL](https://codereview.chromium.org/2690963002).
(Note that that CL is doing more than *just* enabling the Device Service to
register JNI; you should take the register_jni.cc file added there as your
starting point to examine the pattern to follow).

Finally, it is possible that your feature will have coupling to UI process state
(e.g., the Activity) via Android system APIs. To handle this challenging
issue, see the section on [Coupling to UI](#Coupling-to-UI).

## Platform-Specific Issues: iOS

*** aside
**WARNING:** Some of this content is obsolete and needs to be updated. When in
doubt, look approximately near the recommended bits of code and try to find
relevant prior art.
***

Services are supported on iOS insofar as Mojo is supported. However, Chrome on
iOS is strictly single-process, and all services thus must run in-process on
iOS.

If you have a use case or need for services on iOS, contact
blundell@chromium.org. For general information on the motivations and vision for
supporting services on iOS, see the high-level
[servicification design doc](https://docs.google.com/document/d/15I7sQyQo6zsqXVNAlVd520tdGaS8FCicZHrN0yRu-oU/edit).
In particular, search for the mentions of iOS within the doc.

## Client-Specific Issues

#### Mocking Interface Impls in JS
It is a common pattern in Blink's web tests to mock a remote Mojo interface
in JS so that native Blink code requests interfaces from the test JS rather
than whatever would normally service them in the browser process.

The current way to set up that sort of thing looks like
[this](https://cs.chromium.org/chromium/src/third_party/blink/web_tests/battery-status/resources/mock-battery-monitor.js?rcl=be6e0001855f7f1cfc26205d0ff5a2b5b324fcbd&l=19).

#### Feature Impls That Depend on Blink Headers
In the course of servicifying a feature that has Blink as a client, you might
encounter cases where the feature implementation has dependencies on Blink
public headers (e.g., defining POD structs that are used both by the client and
by the feature implementation). These dependencies pose a challenge:

- Services should not depend on Blink, as this is a dependency inversion (Blink
is a client of services).
- However, Blink is very careful about accepting dependencies from Chromium.

To meet this challenge, you have two options:

1. Move the code in question from C++ to mojom (e.g., if it is simple structs).
2. Move the code into the service's C++ client library, being very explicit
   about its usage by Blink. See
   [this CL](https://codereview.chromium.org/2415083002) for a basic pattern to
   follow.

#### Frame-Scoped Connections
You must think carefully about the scoping of the connection being made
from Blink. In particular, some feature requests are necessarily scoped to a
frame in the context of Blink (e.g., geolocation, where permission to access the
interface is origin-scoped). Servicifying these features is then challenging, as
Blink has no frame-scoped connection to arbitrary services (by design, as
arbitrary services have no knowledge of frames or even a notion of what a frame
is).

After a
[long discussion](https://groups.google.com/a/chromium.org/forum/#!topic/services-dev/CSnDUjthAuw),
the policy that we have adopted for this challenge is the following:

CURRENT

- The renderer makes a request through its frame-scoped connection to the
  browser.
- The browser obtains the necessary permissions before directly servicing the
  request.

AFTER SERVICIFYING THE FEATURE IN QUESTION

- The renderer makes a request through its frame-scoped connection to the
  browser.
- The browser obtains the necessary permissions before forwarding the
  request on to the underlying service that hosts the feature.

Notably, from the renderer's POV essentially nothing changes here.

## Strategies for Challenges to Decoupling from //content

### Coupling to UI

Some feature implementations have hard constraints on coupling to UI on various
platforms. An example is NFC on Android, which requires the Activity of the view
in which the requesting client is hosted in order to access the NFC platform
APIs. This coupling is at odds with the vision of servicification, which is to
make the service physically isolatable. However, when it occurs, we need to
accommodate it.

The high-level decision that we have reached is to scope the coupling to the
feature *and* platform in question (rather than e.g. introducing a
general-purpose FooServiceDelegate), in order to make it completely explicit
what requires the coupling and to avoid the coupling creeping in scope.

The basic strategy to support this coupling while still servicifying the feature
in question is to inject a mechanism of mapping from an opaque "context ID" to
the required context. The embedder (e.g., //content) maintains this map, and the
service makes use of it. The embedder also serves as an intermediary: it
provides a connection that is appropriately context-scoped to clients. When
clients request the feature in question, the embedder forwards the request on
along with the appropriate context ID.  The service impl can then map that
context ID back to the needed context on-demand using the mapping functionality
injected into the service impl.

To make this more concrete, see
[this CL](https://codereview.chromium.org/2734943003).

### Shutdown of Singletons

You might find that your feature includes singletons that are shut down as part
of //content's shutdown process. As part of decoupling the feature
implementation entirely from //content, the shutdown of these singletons must be
either ported into your service or eliminated:

- In general, as Chromium is moving away from graceful shutdown, the first
  question to analyze is: Do the singletons actually need to be shut down at
  all?
- If you need to preserve shutdown of the singleton, the naive approach is to
  move the shutdown of the singleton to the destructor of your service
- However, you should carefully examine when your service is destroyed compared
  to when the previous code was executing, and ensure that any differences
  introduced do not impact correctness.

See
[this thread](https://groups.google.com/a/chromium.org/forum/#!topic/services-dev/Y9FKZf9n1ls)
for more discussion of this issue.

## Additional Support

If this document was not helpful in some way, please post a message to your
friendly local
[chromium-mojo@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/chromium-mojo)
or
[services-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/services-dev)
mailing list.
