# Intro to Mojo &amp; Services

[TOC]

## Overview

This document contains the minimum amount of information needed for a developer
to start using Mojo effectively in Chromium, with example Mojo interface usage,
service definition and hookup, and a brief overview of the Content layer's core
services.

See other [Mojo &amp; Services](/docs/README.md#Mojo-Services) documentation
for introductory guides, API references, and more.

## Mojo Terminology

A **message pipe** is a pair of **endpoints**. Each endpoint has a queue of
incoming messages, and writing a message at one endpoint effectively enqueues
that message on the other (**peer**) endpoint. Message pipes are thus
bidirectional.

A **mojom** file describes **interfaces**, which are strongly-typed collections
of **messages**. Each interface message is roughly analogous to a single proto
message, for developers who are familiar with Google protobufs.

Given a mojom interface and a message pipe, one of the endpoints
can be designated as a **`Remote`** and is used to *send* messages described by
the interface. The other endpoint can be designated as a **`Receiver`** and is used
to *receive* interface messages.

*** aside
NOTE: The above generalization is a bit oversimplified. Remember that the
message pipe is still bidirectional, and it's possible for a mojom message to
expect a reply. Replies are sent from the `Receiver` endpoint and received by the
`Remote` endpoint.
***

The `Receiver` endpoint must be associated with (*i.e.* **bound** to) an
**implementation** of its mojom interface in order to process received messages.
A received message is dispatched as a scheduled task invoking the corresponding
interface method on the implementation object.

Another way to think about all this is simply that **a `Remote` makes
calls on a remote implementation of its interface associated with a
corresponding remote `Receiver`.**

## Example: Defining a New Frame Interface

Let's apply this to Chrome. Suppose we want to send a "Ping" message from a
render frame to its corresponding `RenderFrameHostImpl` instance in the browser
process. We need to define a nice mojom interface for this purpose, create a
pipe to use that interface, and then plumb one end of the pipe to the right
place so the sent messages can be received and processed there. This section
goes through that process in detail.

### Defining the Interface

The first step involves creating a new `.mojom` file with an interface
definition, like so:

``` cpp
// src/example/public/mojom/ping_responder.mojom
module example.mojom;

interface PingResponder {
  // Receives a "Ping" and responds with a random integer.
  Ping() => (int32 random);
};
```

This should have a corresponding build rule to generate C++ bindings for the
definition here:

``` python
# src/example/public/mojom/BUILD.gn
import("//mojo/public/tools/bindings/mojom.gni")
mojom("mojom") {
  sources = [ "ping_responder.mojom" ]
}
```

### Creating the Pipe

Now let's create a message pipe to use this interface.

*** aside
As a general rule and as a matter of convenience when
using Mojo, the *client* of an interface (*i.e.* the `Remote` side) is
typically the party who creates a new pipe. This is convenient because the
`Remote` may be used to start sending messages immediately without waiting
for the InterfaceRequest endpoint to be transferred or bound anywhere.
***

This code would be placed somewhere in the renderer:

```cpp
// src/third_party/blink/example/public/ping_responder.h
mojo::Remote<example::mojom::PingResponder> ping_responder;
mojo::PendingReceiver<example::mojom::PingResponder> receiver =
    ping_responder.BindNewPipeAndPassReceiver();
```

In this example, ```ping_responder``` is the `Remote`, and ```receiver```
is a `PendingReceiver`, which is a `Receiver` precursor that will eventually
be turned into a `Receiver`. `BindNewPipeAndPassReceiver` is the most common way to create
a message pipe: it yields the `PendingReceiver` as the return
value.

*** aside
NOTE: A `PendingReceiver` doesn't actually **do** anything. It is an
inert holder of a single message pipe endpoint. It exists only to make its
endpoint more strongly-typed at compile-time, indicating that the endpoint
expects to be bound by a `Receiver` of the same interface type.
***

### Sending a Message

Finally, we can call the `Ping()` method on our `Remote` to send a message:

```cpp
// src/third_party/blink/example/public/ping_responder.h
ping_responder->Ping(base::BindOnce(&OnPong));
```

*** aside
**IMPORTANT:** If we want to receive the response, we must keep the
`ping_responder` object alive until `OnPong` is invoked. After all,
`ping_responder` *owns* its message pipe endpoint. If it's destroyed then so is
the endpoint, and there will be nothing to receive the response message.
***

We're almost done! Of course, if everything were this easy, this document
wouldn't need to exist. We've taken the hard problem of sending a message from
a renderer process to the browser process, and transformed it into a problem
where we just need to take the `receiver` object from above and pass it to the
browser process somehow where it can be turned into a `Receiver` that dispatches
its received messages.

### Sending a `PendingReceiver` to the Browser

It's worth noting that `PendingReceiver`s (and message pipe endpoints in general)
are just another type of object that can be freely sent over mojom messages.
The most common way to get a `PendingReceiver` somewhere is to pass it as a
method argument on some other already-connected interface.

One such interface which we always have connected between a renderer's
`RenderFrameImpl` and its corresponding `RenderFrameHostImpl` in the browser
is
[`BrowserInterfaceBroker`](https://cs.chromium.org/chromium/src/third_party/blink/public/mojom/browser_interface_broker.mojom).
This interface is a factory for acquiring other interfaces. Its `GetInterface`
method takes a `GenericPendingReceiver`, which allows passing arbitrary
interface receivers.

``` cpp
interface BrowserInterfaceBroker {
  GetInterface(mojo_base.mojom.GenericPendingReceiver receiver);
}
```
Since `GenericPendingReceiver` can be implicitly constructed from any specific
`PendingReceiver`, it can call this method with the `receiver` object it created
earlier via `BindNewPipeAndPassReceiver`:

``` cpp
RenderFrame* my_frame = GetMyFrame();
my_frame->GetBrowserInterfaceBroker().GetInterface(std::move(receiver));
```

This will transfer the `PendingReceiver` endpoint to the browser process
where it will be received by the corresponding `BrowserInterfaceBroker`
implementation. More on that below.

### Implementing the Interface

Finally, we need a browser-side implementation of our `PingResponder` interface.

```cpp
#include "example/public/mojom/ping_responder.mojom.h"

class PingResponderImpl : example::mojom::PingResponder {
 public:
  explicit PingResponderImpl(mojo::PendingReceiver<example::mojom::PingResponder> receiver)
      : receiver_(this, std::move(receiver)) {}
  PingResponderImpl(const PingResponderImpl&) = delete;
  PingResponderImpl& operator=(const PingResponderImpl&) = delete;

  // example::mojom::PingResponder:
  void Ping(PingCallback callback) override {
    // Respond with a random 4, chosen by fair dice roll.
    std::move(callback).Run(4);
  }

 private:
  mojo::Receiver<example::mojom::PingResponder> receiver_;
};
```

`RenderFrameHostImpl` owns an implementation of `BrowserInterfaceBroker`.
When this implementation receives a `GetInterface` method call, it calls
the handler previously registered for this specific interface.

``` cpp
// render_frame_host_impl.h
class RenderFrameHostImpl
  ...
  void GetPingResponder(mojo::PendingReceiver<example::mojom::PingResponder> receiver);
  ...
 private:
  ...
  std::unique_ptr<PingResponderImpl> ping_responder_;
  ...
};

// render_frame_host_impl.cc
void RenderFrameHostImpl::GetPingResponder(
    mojo::PendingReceiver<example::mojom::PingResponder> receiver) {
  ping_responder_ = std::make_unique<PingResponderImpl>(std::move(receiver));
}

// browser_interface_binders.cc
void PopulateFrameBinders(RenderFrameHostImpl* host,
                          mojo::BinderMap* map) {
...
  // Register the handler for PingResponder.
  map->Add<example::mojom::PingResponder>(base::BindRepeating(
    &RenderFrameHostImpl::GetPingResponder, base::Unretained(host)));
}
```

And we're done. This setup is sufficient to plumb a new interface connection
between a renderer frame and its browser-side host object!

Assuming we kept our `ping_responder` object alive in the renderer long enough,
we would eventually see its `OnPong` callback invoked with the totally random
value of `4`, as defined by the browser-side implementation above.

## Services Overview &amp; Terminology
The previous section only scratches the surface of how Mojo IPC is used in
Chromium. While renderer-to-browser messaging is simple and possibly the most
prevalent usage by sheer code volume, we are incrementally decomposing the
codebase into a set of services with a bit more granularity than the traditional
Content browser/renderer/gpu/utility process split.

A **service** is a self-contained library of code which implements one or more
related features or behaviors and whose interaction with outside code is done
*exclusively* through Mojo interface connections, typically brokered by the
browser process.

Each service defines and implements a main Mojo interface which can be used
by the browser to manage an instance of the service.

## Example: Building a Simple Out-of-Process Service

There are multiple steps typically involved to get a new service up and running
in Chromium:

- Define the main service interface and implementation
- Hook up the implementation in out-of-process code
- Write some browser logic to launch a service process

This section walks through these steps with some brief explanations. For more
thorough documentation of the concepts and APIs used herein, see the
[Mojo](/mojo/README.md) documentation.

### Defining the Service

Typically service definitions are placed in a `services` directory, either at
the top level of the tree or within some subdirectory. In this example, we'll
define a new service for use by Chrome specifically, so we'll define it within
`//chrome/services`.

We can create the following files. First some mojoms:

``` cpp
// src/chrome/services/math/public/mojom/math_service.mojom
module math.mojom;

interface MathService {
  Divide(int32 dividend, int32 divisor) => (int32 quotient);
};
```

``` python
# src/chrome/services/math/public/mojom/BUILD.gn
import("//mojo/public/tools/bindings/mojom.gni")

mojom("mojom") {
  sources = [
    "math_service.mojom",
  ]
}
```

Then the actual `MathService` implementation:

``` cpp
// src/chrome/services/math/math_service.h
#include "base/macros.h"
#include "chrome/services/math/public/mojom/math_service.mojom.h"

namespace math {

class MathService : public mojom::MathService {
 public:
  explicit MathService(mojo::PendingReceiver<mojom::MathService> receiver);
  ~MathService() override;
  MathService(const MathService&) = delete;
  MathService& operator=(const MathService&) = delete;

 private:
  // mojom::MathService:
  void Divide(int32_t dividend,
              int32_t divisor,
              DivideCallback callback) override;

  mojo::Receiver<mojom::MathService> receiver_;
};

}  // namespace math
```

``` cpp
// src/chrome/services/math/math_service.cc
#include "chrome/services/math/math_service.h"

namespace math {

MathService::MathService(mojo::PendingReceiver<mojom::MathService> receiver)
    : receiver_(this, std::move(receiver)) {}

MathService::~MathService() = default;

void MathService::Divide(int32_t dividend,
                         int32_t divisor,
                         DivideCallback callback) {
  // Respond with the quotient!
  std::move(callback).Run(dividend / divisor);
}

}  // namespace math
```

``` python
# src/chrome/services/math/BUILD.gn

source_set("math") {
  sources = [
    "math_service.cc",
    "math_service.h",
  ]

  deps = [
    "//base",
    "//chrome/services/math/public/mojom",
  ]
}
```

Now we have a fully defined `MathService` implementation that we can make
available in- or out-of-process.

### Hooking Up the Service Implementation

For an out-of-process Chrome service, we simply register a factory function
in [`//chrome/utility/services.cc`](https://cs.chromium.org/chromium/src/chrome/utility/services.cc).

``` cpp
auto RunMathService(mojo::PendingReceiver<math::mojom::MathService> receiver) {
  return std::make_unique<math::MathService>(std::move(receiver));
}

mojo::ServiceFactory* GetMainThreadServiceFactory() {
  // Existing factories...
  static base::NoDestructor<mojo::ServiceFactory> factory {
    RunFilePatcher,
    RunUnzipper,

    // We add our own factory to this list
    RunMathService,
    //...
```

With this done, it is now possible for the browser process to launch new
out-of-process instances of MathService.

### Launching the Service

If you're running your service in-process, there's really nothing interesting
left to do. You can instantiate the service implementation just like any other
object, yet you can also talk to it via a Mojo Remote as if it were
out-of-process.

To launch an out-of-process service instance after the hookup performed in the
previous section, use Content's
[`ServiceProcessHost`](https://cs.chromium.org/chromium/src/content/public/browser/service_process_host.h?rcl=e7a1f6c9a24f3151c875598174a05167fb12c5d5&l=47)
API:

``` cpp
mojo::Remote<math::mojom::MathService> math_service =
    content::ServiceProcessHost::Launch<math::mojom::MathService>(
        content::ServiceProcessHost::LaunchOptions()
            .WithDisplayName("Math!")
            .Pass());
```

Except in the case of crashes, the launched process will live as long as
`math_service` lives. As a corollary, you can force the process to be torn
down by destroying (or resetting) `math_service`.

We can now perform an out-of-process division:

``` cpp
// NOTE: As a client, we do not have to wait for any acknowledgement or
// confirmation of a connection. We can start queueing messages immediately and
// they will be delivered as soon as the service is up and running.
math_service->Divide(
    42, 6, base::BindOnce([](int32_t quotient) { LOG(INFO) << quotient; }));
```
*** aside
NOTE: To ensure the execution of the response callback, the
`mojo::Remote<math::mojom::MathService>` object must be kept alive (see
[this section](/mojo/public/cpp/bindings/README.md#A-Note-About-Endpoint-Lifetime-and-Callbacks)
and [this note from an earlier section](#sending-a-message)).
***

### Using a non-standard sandbox

Ideally services will run inside the utility process sandbox, in which
case there is nothing else to do. For services that need a custom
sandbox, a new sandbox type must be defined in consultation with
security-dev@chromium.org.  To launch with a custom sandbox a
specialization of `GetServiceSandboxType()` must be supplied in an
appropriate `service_sandbox_type.h` such as
[`//chrome/browser/service_sandbox_type.h`](https://cs.chromium.org/chromium/src/chrome/browser/service_sandbox_type.h)
or
[`//content/browser/service_sandbox_type.h`](https://cs.chromium.org/chromium/src/content/browser/service_sandbox_type.h)
and included where `ServiceProcessHost::Launch()` is called.

## Content-Layer Services Overview

### Interface Brokers

We define an explicit mojom interface with a persistent connection
between a renderer's frame object and the corresponding
`RenderFrameHostImpl` in the browser process.
This interface is called
[`BrowserInterfaceBroker`](https://cs.chromium.org/chromium/src/third_party/blink/public/mojom/browser_interface_broker.mojom?rcl=09aa5ae71649974cae8ad4f889d7cd093637ccdb&l=11)
and is fairly easy to work with: you add a new method on `RenderFrameHostImpl`:

``` cpp
void RenderFrameHostImpl::GetGoatTeleporter(
    mojo::PendingReceiver<magic::mojom::GoatTeleporter> receiver) {
  goat_teleporter_receiver_.Bind(std::move(receiver));
}
```

and register this method in `PopulateFrameBinders` function in `browser_interface_binders.cc`,
which maps specific interfaces to their handlers in respective hosts:

``` cpp
// //content/browser/browser_interface_binders.cc
void PopulateFrameBinders(RenderFrameHostImpl* host,
                          mojo::BinderMap* map) {
...
  map->Add<magic::mojom::GoatTeleporter>(base::BindRepeating(
      &RenderFrameHostImpl::GetGoatTeleporter, base::Unretained(host)));
}
```

For binding an embedder-specific document-scoped interface, override
[`ContentBrowserClient::RegisterBrowserInterfaceBindersForFrame()`](https://cs.chromium.org/chromium/src/content/public/browser/content_browser_client.h?rcl=3eb14ce219e383daa0cd8d743f475f9d9ce8c81a&l=999)
and add the binders to the provided map.

*** aside
NOTE: if BrowserInterfaceBroker cannot find a binder for the requested
interface, it will call `ReportNoBinderForInterface()` on the relevant
context host, which results in a `ReportBadMessage()` call on the host's
receiver (one of the consequences is a termination of the renderer). To
avoid this crash in tests (when content_shell doesn't bind some
Chrome-specific interfaces, but the renderer requests them anyway),
use the
[`EmptyBinderForFrame`](https://cs.chromium.org/chromium/src/content/browser/browser_interface_binders.cc?rcl=12e73e76a6898cb6df6a361a98320a8936f37949&l=407)
helper in `browser_interface_binders.cc`. However, it is recommended
to have the renderer and browser sides consistent if possible.
***

TODO: add information about workers.

## Additional Support

If this document was not helpful in some way, please post a message to your
friendly
[chromium-mojo@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/chromium-mojo)
or
[services-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/services-dev)
mailing list.
