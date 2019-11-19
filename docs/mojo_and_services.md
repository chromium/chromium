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

  // example::mojom::PingResponder:
  void Ping(PingCallback callback) override {
    // Respond with a random 4, chosen by fair dice roll.
    std::move(callback).Run(4);
  }

 private:
  mojo::Receiver<example::mojom::PingResponder> receiver_;

  DISALLOW_COPY_AND_ASSIGN(PingResponderImpl);
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
                          service_manager::BinderMap* map) {
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
*exclusively* through Mojo interface connections facilitated by the **Service
Manager.**

The **Service Manager** is a component which can run in a dedicated process
or embedded within another process. Only one Service Manager exists globally
across the system, and in Chromium the browser process runs an embedded Service
Manager instance immediately on startup. The Service Manager spawns
**service instances** on-demand, and it routes each interface request from a
service instance to some destination instance of the Service Manager's choosing.

Each service instance implements the
[**`Service`**](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service.h)
interface to receive incoming interface requests brokered by the Service
Manager, and each service instance has a
[**`Connector`**](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/connector.h)
it can use to issue interface requests to other services via the
Service Manager.

Every service has a **manifest** which declares some static metadata about the
service. This metadata is used by the Service Manager for various purposes,
including as a declaration of what interfaces are exposed to other services in
the system. This eases the security review process.

Inside its manifest every service declares its **service name**, used to
identify instances of the service in the most general sense. Names are free-form
and usually short strings which must be globally unique. Some services defined
in Chromium today include `"device"`, `"identity"`, and `"network"` services.

For more complete and in-depth coverage of the concepts covered here and other
related APIs, see the
[Service Manager documentation](/services/service_manager/README.md).

## Example: Building a Simple Out-of-Process Service

There are multiple steps required to get a new service up and running in
Chromium. You must:

- Define the `Service` implementation
- Define the service's manifest
- Tell Chromium's Service Manager about the manifest
- Tell Chromium how to instantiate the `Service` implementation when it's needed

This section walks through these steps with some brief explanations. For more
thorough documentation of the concepts and APIs used herein, see the
[Service Manager](/services/service_manager/README.md) and
[Mojo](/mojo/README.md) documentation.

### Defining the Service

Typically service definitions are placed in a `services` directory, either at
the top level of the tree or within some subdirectory. In this example, we'll
define a new service for use by Chrome specifically, so we'll define it within
`//chrome/services`.

We can create the following files. First some mojoms:

``` cpp
// src/chrome/services/math/public/mojom/constants.mojom
module math.mojom;

// These are not used by the implementation directly, but will be used in
// following sections.
const string kServiceName = "math";
const string kArithmeticCapability = "arithmetic";
```

``` cpp
// src/chrome/services/math/public/mojom/divider.mojom
module math.mojom;

interface Divider {
  Divide(int32 dividend, int32 divisor) => (int32 quotient);
};
```

``` python
# src/chrome/services/math/public/mojom/BUILD.gn
import("//mojo/public/tools/bindings/mojom.gni")

mojom("mojom") {
  sources = [
    "constants.mojom",
    "divider.mojom",
  ]
}
```

Then the actual `Service` implementation:

``` cpp
// src/chrome/services/math/math_service.h
#include "services/service_manager/public/cpp/service.h"

#include "base/macros.h"
#include "chrome/services/math/public/mojom/divider.mojom.h"

namespace math {

class MathService : public service_manager::Service,
                    public mojom::Divider {
 public:
  explicit MathService(service_manager::mojom::ServiceRequest request);
  ~MathService() override;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::Divider:
  void Divide(int32_t dividend,
              int32_t divisor,
              DivideCallback callback) override;

  service_manager::ServiceBinding service_binding_;

  // You could also use a Receiver. We use ReceiverSet to conveniently allow
  // multiple clients to bind to the same instance of this class. See Mojo
  // C++ Bindings documentation for more information.
  mojo::ReceiverSet<mojom::Divider> divider_receivers_;

  DISALLOW_COPY_AND_ASSIGN(MathService);
};

}  // namespace math
```

``` cpp
// src/chrome/services/math/math_service.cc
#include "chrome/services/math/math_service.h"

namespace math {

MathService::MathService(service_manager::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

MathService::~MathService() = default;

void MathService::OnBindInterface(
    const service_manager::BindSourceInfo& source,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // Note that services typically use a service_manager::BinderRegistry if they
  // plan on handling many different interface request types.
  if (interface_name == mojom::Divider::Name_) {
    divider_receivers_.Add(
        this, mojo::PendingReceiver<mojom::Divider>(std::move(interface_pipe)));
  }
}

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
    "math.cc",
    "math.h",
  ]

  deps = [
    "//base",
    "//chrome/services/math/public/mojom",
    "//services/service_manager/public/cpp",
  ]
}
```

Now we have a fully defined `math` service implementation, including a nice
little `Divider` interface for clients to play with. Next we need to define the
service's manifest to declare how the service can be used.

### Defining the Manifest
Manifests are defined as
[`Manifest`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/manifest.h)
objects, typically built using a
[`ManifestBuilder`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/manifest_builder.h). As a general rule, services should define their manifest
in a dedicated `source_set` or `component` target under their `public/cpp`
subdirectory (typically referred to as the service's **C++ client library**).

We can create the following files for this purpose:

``` cpp
// src/chrome/services/math/public/cpp/manifest.h
#include "services/service_manager/public/cpp/manifest.h"

namespace math {

const service_manager::Manifest& GetManifest();

}  // namespace math
```

``` cpp
// src/chrome/services/math/public/cpp/manifest.cc
#include "chrome/services/math/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chrome/services/math/public/mojom/constants.mojom.h"
#include "chrome/services/math/public/mojom/divider.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace math {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .ExposeCapability(
              mojom::kArithmeticCapability,
              service_manager::Manifest::InterfaceList<mojom::Divider>())
          .Build()};
  return *manifest
}

}  // namespace math
```

We also need to define a build target for our manifest sources:

``` python
# src/chrome/services/math/public/cpp/BUILD.gn

source_set("manifest") {
  sources = [
    "manifest.cc",
    "manifest.h",
  ]

  deps = [
    "//base",
    "//chrome/services/math/public/mojom",
    "//services/service_manager/public/cpp",
  ]
}
```

The above `Manifest` definition declares that the service is named `math` and
that it **exposes** a single **capability** named `arithmetic` which allows
access to the `Divider` interface.

Another service may **require** this capability from its own manifest in order
for the Service Manager to grant it access to a `Divider`. We'll see this a
few sections below. First, let's get the manifest and service implementation
registered with Chromium's Service Manager.

### Registering the Manifest

For the most common out-of-process service cases, we register service manifests
by **packaging** them in Chrome. This can be done by augmenting the value
returned by
[`GetChromePackagedServiceManifests`](https://cs.chromium.org/chromium/src/chrome/app/chrome_packaged_service_manifests.cc?rcl=af43cabf3c01e28be437becb972a7eae44fd54e8&l=133).

We can add our manifest there:

``` cpp
// Deep within src/chrome/app/chrome_packaged_service_manifests.cc...
const std::vector<service_manager::Manifest>
GetChromePackagedServiceManifests() {
      ...
      math::GetManifest(),
      ...
```

And don't forget to add a GN dependency from
[`//chrome/app:chrome_packaged_service_manifests`](https://cs.chromium.org/chromium/src/chrome/app/BUILD.gn?l=564&rcl=a77d5ba9c4621cfe14e7e1cd03bbae16904f269e) onto
`//chrome/services/math/public/cpp:manifest`!

We're almost done with service setup. The last step is to teach Chromium (and
thus the Service Manager) how to launch an instance of our beautiful `math`
service.

### Hooking Up the Service Implementation

There are two parts to this for an out-of-process Chrome service.

First, we need
to inform the embedded Service Manager that this service is an out-of-process
service. The goofiness of this part is a product of some legacy issues and it
should be eliminated soon, but for now it just means teaching the Service
Manager how to *label* the process it creates for this service (e.g. how the process will
appear in the system task manager). We modify
[`ChromeContentBrowserClient::RegisterOutOfProcessServices`](https://cs.chromium.org/chromium/src/chrome/browser/chrome_content_browser_client.cc?rcl=960886a7febcc2acccea7f797d3d5e03a344a12c&l=3766)
for this:

``` cpp
void ChromeContentBrowserClient::RegisterOutOfProcessServices(
    OutOfProcessServicesMap* services) {
  ...

  (*services)[math::mojom::kServiceName] =
      base::BindRepeating([]() -> base::string16 {
        return "Math Service";
      });

  ...
}
```

And finally, since nearly all out-of-process services run in a "utility" process
today, we need to add a dependency on our actual `Service` implementation to
Chrome's service spawning code within the utility process.

For this step we just modify
[`ChromeContentUtilityClient::MaybeCreateMainThreadService`](https://cs.chromium.org/chromium/src/chrome/utility/chrome_content_utility_client.cc?rcl=7226adebd6e8d077d673a82acf1aab0790627178&l=261)
by adding a block of code as follows:

``` cpp
std::unique_ptr<service_manager::Service> ChromeContentUtilityClient::MaybeCreateMainThreadService(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  ...

  if (service_name == math::mojom::kServiceName)
    return std::make_unique<math::MathService>(std::move(request));

  ...
}
```

And we're done!

As one nice follow-up step, let's use our math service from the browser.

### Using the Service

We can grant the browser process access to our `Divider` interface by
**requiring** the `math` service's `arithmetic` capability within the
`content_browser` service manifest.

*** aside
NOTE: See the following section for an elaboration on what `content_browser` is.
For the sake of this example, it's magic.
***

For Chrome-specific features such as our glorious new `math` service, we can
amend the `content_browser` manifest by modifying
[GetChromeContentBrowserOverlayManifest](https://cs.chromium.org/chromium/src/chrome/app/chrome_content_browser_overlay_manifest.cc?rcl=38db90321e8e3627b2f3165cdb051fa8d668af48&l=100)
as follows:

``` cpp
// src/chrome/app/chrome_content_browser_overlay_manifest.cc

...
const service_manager::Manifest& GetChromeContentBrowserOverlayManifest() {
  ...
        .RequireCapability(math::mojom::kServiceName,
                           math::mojom::kArithmeticCapability)
  ...
}
```

Finally, we can use the global `content_browser` instance's `Connector` to send
an interface request to our service. This is accessible from the main thread of
the browser process. Somewhere in `src/chrome/browser`, we can write:

``` cpp
// This gives us the system Connector for the browser process, which has access
// to most service interfaces.
service_manager::Connector* connector = content::GetSystemConnector();

// Recall from the earlier Mojo section that mojo::MakeRequest creates a new
// message pipe for our interface. Connector passes the request endpoint to
// the Service Manager along with the name of our target service, "math".
math::mojom::DividerPtr divider;
connector->BindInterface(math::mojom::kServiceName,
                         mojo::MakeRequest(&divider));

// As a client, we do not have to wait for any acknowledgement or confirmation
// of a connection. We can start queueing messages immediately and they will be
// delivered as soon as the service is up and running.
divider->Divide(
    42, 6, base::BindOnce([](int32_t quotient) { LOG(INFO) << quotient; }));
```
*** aside
NOTE: To ensure the execution of the response callback, the DividerPtr
object must be kept alive (see
[this section](/mojo/public/cpp/bindings/README.md#A-Note-About-Endpoint-Lifetime-and-Callbacks)
and [this note from an earlier section](#sending-a-message)).
***

This should successfully spawn a new process to run the `math` service if it's
not already running, then ask it to do a division, and ultimately log the result
after it's sent back to the browser process.

Finally it's worth reiterating that every service instance in the system has
its own `Connector` and there's no reason we have to limit ourselves to
`content_browser` as the client, as long as the appropriate manifest declares
that it requires our `arithmetic` capability.

If we did not update the `content_browser` manifest overlay as we did in this
example, the `Divide` call would never reach the `math` service (in fact the
service wouldn't even be started) and instead we'd get an error message (or in
developer builds, an assertion failure) informing us that the Service Manager
blocked the `BindInterface` call.

## Content-Layer Services Overview

Apart from very early initialization steps in the browser process, every bit of
logic in Chromium today is effectively running as part of one service instance
or another.

Although we continue to migrate parts of the browser's privileged
functionality to more granular services defined below the Content layer, the
main services defined in Chromium today continue to model the Content layer's
classical multiprocess architecture which defines a handful of
**process types**: browser, renderer, gpu, utility, and plugin processes. For
each of these process types, we now define corresponding services.

Manifest definitions for all of the following services can be found in
`//content/public/app`.

### The Browser Service

`content_browser` is defined to encapsulate general-purpose browser process
code. There are multiple instances of this service, all running within the
singular browser process. There is one shared global instance as well an
additional instance for each `BrowserContext` (*i.e.* per Chrome profile).

The global instance exists primarily so that arbitrary browser process code can
reach various system services conveniently via a global `Connector` instance
on the main thread.

Each instance associated with a `BrowserContext` is placed in an isolated
instance group specific to that `BrowserContext`. This limits the service
instances with which its `Connector` can make contact. These instances are
used primarily to facilitate the spawning of other isolated per-profile service
instances, such as renderers and plugins.

### The Renderer Service

A `content_renderer` instance is spawned in its own sandboxed process for every
site-isolated instance of Blink we require. Instances are placed in the same
instance group as the renderer's corresponding `BrowserContext`, *i.e.* the
profile which navigated to the site being rendered.

Most interfaces used by `content_renderer` are not brokered through the Service
Manager but instead are brokered through dedicated interfaces implemented by
`content_browser`, with which each renderer maintains persistent connections.

### The GPU Service

Only a single instance of `content_gpu` exists at a time and it always runs in
its own isolated, sandboxed process. This service hosts the code in content/gpu
and whatever else Content's embedder adds to that for GPU support.

### The Plugin Service

`content_plugin` hosts a plugin in an isolated process. Similarly to
`content_renderer` instances, each instance of `content_plugin` belongs to
an instance group associated with a specific `BrowserContext`, and in general
plugins get most of their functionality by talking directly to `content_browser`
rather than brokering interface requests through the Service Manager.

### The Utility Service

`content_utility` exists only nominally today, as there is no remaining API
surface within Content which would allow a caller to explicitly create an
instance of it. Instead, this service is used exclusively to bootstrap new
isolated processes in which other services will run.

## Exposing Interfaces Between Content Processes

Apart from the standard Service Manager APIs, the Content layer defines a number
of additional concepts for Content and its embedder to expose interfaces
specifically between Content processes in various contexts.

### Exposing Browser Interfaces to Renderer Documents and Workers

Documents and workers are somewhat of a special case since interface access
decisions often require browser-centric state that the Service Manager cannot
know about, such as details of the current `BrowserContext`, the origin of the
renderered content, installed extensions in the renderer, *etc.* For this
reason, interface brokering decisions are increasingly being made by the
browser.

There are two ways this is done: the Deprecated way and the New way.

#### The Deprecated Way: InterfaceProvider

This is built on the concept of **interface filters** and the
**`InterfaceProvider`** interface. It is **deprecated** and new features should
use [The New Way](#The-New-Way_Interface-Brokers) instead. This section only
briefly covers practical usage in Chromium.

The `content_browser` manifest exposes capabilities on a few named interface
filters, the main one being `"navigation:frame"`. There are others scoped to
different worker contexts, *e.g.* `"navigation:service_worker"`.
`RenderProcessHostImpl` or `RenderFrameHostImpl` sets up an `InterfaceProvider`
for each known execution context in the corresponding renderer, filtered through
the Service Manager according to one of the named filters.

The practical result of all this means the interface must be listed in the
`content_browser` manifest under the
`ExposeInterfaceFilterCapability_Deprecated("navigation:frame", "renderer", ...)`
entry, and a corresponding interface request handler must be registered with the
host's `registry_` in
[`RenderFrameHostImpl::RegisterMojoInterfaces`](https://cs.chromium.org/chromium/src/content/browser/frame_host/render_frame_host_impl.cc?rcl=0a23c78c57ecb2405837155aa0a0def7b5ba9c22&l=3971)

Similarly for worker contexts, an interface must be exposed by the `"renderer"`
capability on the corresponding interface filter
(*e.g.*, `"navigation:shared_worker"`) and a request handler must be registered
within
[`RendererInterfaceBinders::InitializeParameterizedBinderRegistry`](https://cs.chromium.org/chromium/src/content/browser/renderer_interface_binders.cc?rcl=0a23c78c57ecb2405837155aa0a0def7b5ba9c22&l=116).

The best way to understand all of this after reading this section is to look at
the linked code above and examine a few examples. They are fairly repetitive.
For additional convenience, here is also a link to the `content_browser`
[manifest](https://cs.chromium.org/chromium/src/content/public/app/content_browser_manifest.cc).

#### The New Way: Interface Brokers

Rather than the confusing spaghetti of interface filter logic, we now define an
explicit mojom interface with a persistent connection between a renderer's
frame object and the corresponding `RenderFrameHostImpl` in the browser process.
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
                          service_manager::BinderMap* map) {
...
  map->Add<magic::mojom::GoatTeleporter>(base::BindRepeating(
      &RenderFrameHostImpl::GetGoatTeleporter, base::Unretained(host)));
}
```


### Exposing Browser Interfaces to Render Processes

Sometimes (albeit rarely) it's useful to expose a browser interface directly to
a renderer process. This can be done as for any other interface exposed between
two services. In this specific instance, the `content_browser` manifest exposes
a capability named `"renderer"` which `content_renderer` requires. Any interface
listed as part of that capability can be accessed by a `content_renderer`
instance by using its own `Connector`. See below.

### Exposing Browser Interfaces to Content Child Processes

All Content child process types (renderer, GPU, and plugin) share a common API
to interface with the Service Manager. Their Service Manager connection is
initialized and maintained by `ChildThreadImpl` on process startup, and from
the main thread, you can access the process's `Connector` as follows:

``` cpp
auto* connector = content::ChildThread::Get()->GetConnector();

// For example...
connector->Connect(content::mojom::kBrowserServiceName,
                         std::move(some_receiver));
```

### Exposing Content Child Process Interfaces to the Browser

Content child processes may also expose interfaces to the browser, though this
is much less common and requires a fair bit of caution since the browser must be
careful to only call `Connector.BindInterface` in these cases with an exact
`service_manager::Identity` to avoid unexpected behavior.

Every child process provides a subclass of ChildThreadImpl, and this can be used
to install a new `ConnectionFilter` on the process's Service Manager connection
before starting to accept requests.

This behavior should really be considered deprecated, but for posterity, here is
how the GPU process does it:

1. [Disable Service Manager connection auto-start](https://cs.chromium.org/chromium/src/content/gpu/gpu_child_thread.cc?rcl=6b85a56334c0cd64b0e657934060de716714ca64&l=62)
2. [Register a new ConnectionFilter impl to handle certain interface requests](https://cs.chromium.org/chromium/src/content/gpu/gpu_child_thread.cc?rcl=6b85a56334c0cd64b0e657934060de716714ca64&l=255)
3. [Start the Service Manager connection manually](https://cs.chromium.org/chromium/src/content/gpu/gpu_child_thread.cc?rcl=6b85a56334c0cd64b0e657934060de716714ca64&l=257)

It's much more common instead for there to be some primordial interface
connection established by the child process which can then be used to facilitate
push communications from the browser, so please consider not duplicating this
behavior.

## Additional Support

If this document was not helpful in some way, please post a message to your
friendly
[chromium-mojo@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/chromium-mojo)
or
[services-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/services-dev)
mailing list.
