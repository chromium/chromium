# The Service Manager &amp; Services

[TOC]

## Overview

The Service Manager is a component which large applications like Chromium can
use support a cross-platform, multi-process, service-oriented,
hyphenated-adjective-laden architecture.

This document covers how to embed
the Service Manager into an application as well as how to define and register
services for it to manage. If you just want to read about defining services and
using common service APIs, skip to the main [Services](#Services) section.

## Embedding the Service Manager

To embed the Service Manager, an application should link against the code in
`//services/service_manager/embedder`. This defines a main entry point for
most platforms, with a relatively small
[`service_manager::MainDelegate`](https://cs.chromium.org/chromium/src/services/service_manager/embedder/main_delegate.h)
interface for the application to implement. In particular, the application
should at least implement
[`GetServiceManifests`](https://cs.chromium.org/chromium/src/services/service_manager/embedder/main_delegate.h?rcl=734122d6a01196706dfc1c252fa09ed933778f8f&l=80) to provide
metadata about the full set of services comprising the application.

*** aside
Note that Chromium does not currently implement `GetServiceManifests` for
production use of the Service Manager. This is because a bunch of process
launching and management logic still lives at the Content layer. As more of this
code moves into Service Manager internals, Chromium will start to look more like
any other Service Manager embedder.
***

*TODO: Improve embedder documentation here, and include support for in-process
service launching once MainDelegate supports it.*

## Services

A **service** in this context can be defined as any self-contained body of
application logic which satisfies *all* of the following constraints:

- It defines a single [implementation](#Implementation) of
  [`Service`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service.h)
  to receive interface requests brokered by the
  Service Manager, and it maintains a connection between this object and the
  Service Manager using a
  [`ServiceBinding`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service_binding.h).
- Its API surface in from or out to other services is restricted exclusively to
  [Mojo](/mojo/README.md) interfaces and self-contained client libraries built
  on those Mojo interfaces. This means no link-time or run-time exposure of
  the service implementation's internal heap or global state.
- It defines a [service manifest](#Manifests) to declare how the Service Manager
  should identify and manage instances of the service, as well as what
  interfaces are exposed to or required from other services in the system.

The Service Manager is responsible for managing the creation and interconnection
of individual service instances, whether they are embedded within an existing
process or each isolated within dedicated processes. Managed service processes
may be sandboxed with any of various supported
[sandbox configurations](#Sandbox-Configurations).

This section walks through important concepts and APIs throughout service
development, and builds up a small working example service in the process.

### A Brief Note About Service Granularity

Many developers fret over what the right "size" or granularity is for a service
or set of services. This makes sense, and there is always going to be some
design tension between choosing a simpler and potentially more efficient,
monolithic implementation, versus choosing a more modular but often more complex
one.

One classic example of this tension is in the origins of Chromium's
`device` service. The service hosts a number of independent device interfacing
subsystems for things like USB, Bluetooth, HID, battery status, etc. You could
easily imagine justifying separate services for each of these features, but it
was ultimately decided keep them merged together as one service thematically
related to hardware device capabilities. Some factors which played into this
decision:

- There was no clear **security** benefit to keeping the features isolated from
  each other.
- There was no clear **code size** benefit to keeping the features isolated from
  each other -- environments supporting any one of the device capabilities are
  fairly likely to support several others and would thus likely include all or
  most of the smaller services anyway.
- There isn't really any coupling between the different features in the service,
  so there would be few **code health** benefits to building separate services.

Given all of the above conditions, opting for a smaller overall number of
services seems likely to have been the right decision.

When making these kinds of decisions yourself, use your best judgment. When in
doubt, start a bike-shedding centithread on
[services-dev@chromium.org](https://groups.google.com/a/chromium.org/forum#!forum/services-dev).

### Implementation

The central fixture in any service implementation is, well, its
[`Service`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service.h)
implementation. This is a small interface with really only three virtual methods
of practical interest, all optional to implement:

``` cpp
class Service {
 public:
  virtual void OnStart();
  virtual void OnBindInterface(const BindSourceInfo& source,
                               const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe);
  virtual void OnDisconnected();
};
```

Services implement a subclass of this to work in conjunction with a
[`ServiceBinding`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service_binding.h)
so the Service Manager can call into the service with lifecycle events and
interface requests from other services.

*** aside
NOTE: As discussed in [Instance Sharing](#Instance-Sharing) below, your service
configuration may allow for the Service Manager to manage many concurrent
instances of your service. Whether these instances run in the same shared
process or in separate processes, each instance is comprised of exactly one
dedicated instance of your actual `Service` subclass.
***

Through the rest of this document we'll build out a basic working service
implementation, complete with a manifest and simple tests. We'll call it the
`storage` service, and it will provide the basis for all persistent storage
capabilities in our crappy operating system hobby project that is doomed to
languish forever in an unfinished state.

*** aside
NOTE: Sheerly for the sake of brevity, example code written here is inlined in
headers where it would typically be moved out-of-line.
***

The first step is usually to imagine and define some mojom API surface to start
with. We'll limit this example to two mojom files. It's conventional to keep
important constants defined in a separate `constants.mojom` file:

``` cpp
// src/services/storage/public/mojom/constants.mojom
module storage.mojom;

// This string will identify our service to the Service Manager. It will be used
// in our manifest when registering the service, and clients can use it when
// sending interface requests to the Service Manager if they want to reach our
// service.
const string kServiceName = "storage";

// We'll use this later, in service manifest definitions.
const string kAllocationCapability = "allocation";
```

And some useful interface definitions:

``` cpp
// src/services/storage/public/mojom/block.mojom
module storage.mojom;

interface BlockAllocator {
  // Allocates a new block of persistent storage for the client. If allocation
  // fails, |receiver| is discarded.
  Allocate(uint64 num_bytes, pending_receiver<Block> receiver);
};

interface Block {
  // Reads and returns a small range of bytes from the block.
  Read(uint64 byte_offset, uint16 num_bytes) => (array<uint8> bytes);

  // Writes a small range of bytes to the block.
  Write(uint64 byte_offset, array<uint8> bytes);
};
```

And finally we'll define our basic `Service` subclass:

``` cpp
// src/services/storage/storage_service.h

#include "base/macros.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/storage/public/mojom/block.mojom.h"

namespace storage {

class StorageService : public service_manager::Service,
                       public mojom::BlockAllocator {
 public:
  explicit StorageService(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {}
  ~StorageService() override = default;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    if (interface_name == mojom::BlockAllocator::Name_) {
      // If the Service Manager sends us a request with BlockAllocator's
      // interface name, we should treat |interface_pipe| as a
      // PendingReceiver<BlockAllocator> that we can bind.
      allocator_receivers_.Add(
          this, mojo::PendingReceiver<mojom::BlockAllocator>(std::move(interface_pipe)));
    }
  }

  // mojom::BlockAllocator:
  void Allocate(uint64_t num_bytes, mojo::PendingReceiver<mojom::Block> receiver) override {
    // This space intentionally left blank.
  }

  service_manager::ServiceBinding service_binding_;
  mojo::ReceiverSet<mojom::BlockAllocator> allocator_receivers_;

  DISALLOW_COPY_AND_ASSIGN(StorageService);
};

}  // namespace storage
```

Great. This is a basic service implementation. It does nothing useful, but we
can come back and fix that some other time.

First, notice that the `StorageService` constructor takes a
`service_manager::mojom::ServiceRequest` and immediately passes it to the
`service_binding_` constructor. This is a nearly universal convention among
service implementations, and your service will probably do it too. The
`ServiceRequest` is an interface pipe that the Service Manager uses to drive
your service, and the `ServiceBinding` is a helper class which translates
messages from the Service Manager into the simpler interface methods of the
`Service` class you've implemented.

`StorageService` also implements `OnBindInterface`, which is what the Service
Manager invokes (via your `ServiceBinding`) when it has decided to route another
service's interface request to your service instance. Note that because this is
a generic API intended to support arbitrary interfaces, the request comes in the
form of an interface name and a raw message pipe handle. It is the service's
responsibility to inspect the name and decide how (or even if) to bind the pipe.
Here we recognize only incoming `BlockAllocator` requests and drop anything
else.

*** aside
NOTE: Because interface receivers are just strongly-type message pipe endpoint
wrappers, you can freely construct any kind of interface receiver over a raw
message pipe handle. If you're planning to pass the endpoint around, it's good
to do this as early as possible (i.e. as soon as you know the intended interface
type) to benefit from your compiler's type-checking and avoid having to pass
around both a name and a pipe.
***

The last piece of our service that we need to lay down is its manifest.

### Manifests

A service's manifest is a simple static data structure provided to the Service
Manager early during its initialization process. The Service Manager combines
all of the manifest data it has in order to form a complete picture of the
system it's coordinating. It uses all of this information to make decisions
like:

- When service X requests interface Q from service Y, should it be allowed?
- Were all of the constraints specified in X's request valid, and is X allowed
  to specify them as such?
- Do I need to spawn a new instance of Y to satisfy this request or can I re-use
  an existing one (assuming there are any)?
- If I have to spawn a new process for a new Y instance, how should I configure
  its sandbox, if at all?

All of this metadata is contained within different instances of the
[`Manifest`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/manifest.h)
class.

#### A Basic Manifest

The most common way to define a service's manifest is to place it in its own
source target within the service's C++ client library. To combine the
convenience of inline one-time initialization with the avoidance of static
initializers, typically this means using a function-local static with
`base::NoDestructor` and `service_manager::ManifestBuilder` as below. First the
header:

``` cpp
// src/services/storage/public/cpp/manifest.h

#include "services/service_manager/public/cpp/manifest.h"

namespace storage {

const service_manager::Manifest& GetManifest();

}  // namespace storage
```

And for the actual implementation:

``` cpp
// src/services/storage/public/cpp/manifest.cc

#include "services/storage/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/storage/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace storage {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .Build()};
  return *manifest;
};

}  // namespace storage
```

Here we've specified only the **service name**, matching the constant defined
in `constants.mojom` so that other services can easily locate us without a
hard-coded string.

With this manifest definition there is no way for our service to reach other
services, and there's no way for other services to reach us; this is because
we neither **expose** nor **require** any capabilities, thus the Service Manager
will always block any interface request from us or targeting us.

#### Exposing Interfaces

Let's expose an "allocator" capability that grants permission to bind a
`BlockAllocator` pipe. We can augment the above manifest definition as follows:

``` cpp
...
#include "services/storage/public/mojom/block.mojom.h"
...

...
          .WithServiceName(mojom::kServiceName)
          .ExposeCapability(
              mojom::kAllocatorCapability,
              service_manager::Manifest::InterfaceList<mojom::BlockAllocator>())
          .Build()
...
```

This declares the existence of an `"allocator"` capability exposed by our
service, and specifies that granting a client this capability means granting it
the privilege to send our service `storage.mojom.BlockAllocator` interface
requests.

You can list as many interfaces as you like for each exposed capability, and
multiple capabilities may list the same interface.

**NOTE**: You only need to expose an interface through a capability if you want
other services to be able to be able to request it *through the Service
Manager* (see [Connectors](#Connectors)) -- that is, if you handle requests for
it in your `Service::OnBindInterface` implementation.

Contrast this with interfaces acquired transitively, like `Block` above. The
Service Manager does not mediate the behavior of existing interface connections,
so once a client has a `BlockAllocator` they can use `BlockAllocator.Allocate`
to send as many `Block` requests as they like. Such requests go directly to
the service-side implementation of `BlockAllocator` to which the pipe is bound,
and so manifest contents are irrelevant to their behavior.

#### Getting Access to Interfaces

We don't need to add anything else to our `storage` manifest, but if another
service wanted to enjoy access to our amazing storage block allocation
facilities, they would need to declare in their manifest that they **require**
our `"allocation"` capability. For ease of maintenance they would utilitize our
publicly defined constants to do this. It's pretty straightforward:

``` cpp
// src/services/some_other_pretty_cool_service/public/cpp/manifest.cc

...       // Somewhere along the chain of ManifestBuilder calls...
          .RequireCapability(storage::mojom::kServiceName,
                             storage::mojom::kAllocationCapability)
...
```

Now `some_other_pretty_cool_service` can use its [Connector](#Connectors) to ask
the Service Manager for a `BlockAllocator` from us, like so:

``` cpp
mojo::Remote<storage::mojom::BlockAllocator> allocator;
connector->Connect(storage::mojom::kServiceName,
                         allocator.BindNewPipeAndPassReceiver());

mojo::Remote<storage::mojom::Block> block;
allocator->Allocate(42, block.BindNewPipeAndPassReceiver());

// etc..
```

#### Other Manifest Elements

There are a handful of other optional elements in a `Manifest` structure which
can affect how your service behaves at runtime. See the current
[`Manifest`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/manifest.h)
definition and comments as well as
[`ManifestBuilder`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/manifest_builder.h)
for the most complete and current information, but some of the more common
properties specified by manifests are:

- **Display Name** - This is the string the Service Manager will use to name
  any new process created to run your service. This string would appear in the
  Windows Task Manager to identify the service process, for example.
- **Options** - A few miscellaneous options are stuffed into a `ManifestOptions`
  field. These include sandbox type (see
  [Sandbox Configurations](#Sandbox-Configurations)),
  [instance sharing policy](#Instance-Sharing), and various behavioral flags to
  control a few [special capabilities](#Additional-Capabilities).
- **Preloaded Files** - On Android and Linux platforms, the Service Manager can
  open specified files on the service's behalf and pass the corresponding open
  file descriptor(s) to each new service process on launch.
- **Packaged Services** - A service may declare that it **packages** another
  service by including a copy of that service's own manifest. See
  [Packaging](#Packaging) for details.

### Running the Service

Hooking the service up so that it can be run in a production environment is
actually outside the scope of this document at the moment, only because it still
depends heavily on the environment in which the Service Manager is embedded. For
now, if you want to get your great little service hooked up in Chromium for
example, you should check out the sections on this in the very Chromium-centric
[Intro to Mojo &amp; Services](/docs/mojo_and_services.md#Hooking-Up-the-Service-Implementation)
and/or
[Servicifying Chromium Features](/docs/servicification.md#Putting-It-All-Together)
documents.

For the sake of this document, we'll focus on running the service in test
environments with the service both in-process and out-of-process.

### Testing

There are three primary approaches used when testing services, applied in
varying combinations:

#### Standard Unit-testing
This is ideal for covering details of your service's internal components and
making sure they operate as expected. There is nothing special here regarding
services. Code is code, you can unit-test it.

#### Out-of-process End-to-end Tests
These are good for emulating a production environment as closely as possible,
with your service implementation isolated in a separate process from the test
(client) code.

The main drawback to this approach is that it limits your test's ability to poke
at or observe internal service state, which can sometimes be useful in test
environments (for *e.g.* faking out some behavior in a predictable manner). In
general, supporting such controls means adding test-only interfaces to your
service.

The
[`TestServiceManager`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/test/test_service_manager.h)
helper and
[`service_executable`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service_executable.gni)
GN target type make this fairly easy to accomplish. You simply define a new
entry point for your service:

``` cpp
// src/services/storage/service_main.cc

#include "base/message_loop.h"
#include "services/service_manager/public/cpp/service_executable/main.h"
#include "services/storage/storage_service.h"

void ServiceMain(service_manager::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_task_executor;
  storage::StorageService(std::move(request)).RunUntilTermination();
}
```

and a GN target for this:

``` python
import "services/service_manager/public/cpp/service_executable.gni"

service_executable("storage") {
  sources = [
    "service_main.cc",
  ]

  deps = [
    # The ":impl" target would be the target that defines our StorageService
    # implementation.
    ":impl",
    "//base",
    "//services/service_manager/public/cpp",
  ]
}

test("whatever_unittests") {
  ...

  # Include the executable target as data_deps for your test target
  data_deps = [ ":storage" ]
}
```

And finally in your test code, use `TestServiceManager` to create a real
Service Manager instance within your test environment, configured to know about
your `storage` service.

`TestServiceManager` allows you to inject an artificial service instance to
treat your test suite as an actual service instance. You can provide a manifest
for your test to simulate requiring (or failing to require) various capabilities
and get a `Connector` with which to reach your service-under-test. This looks
something like:

``` cpp
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/test/test_service.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/storage/public/cpp/manifest.h"
#include "services/storage/public/mojom/constants.mojom.h"
#include "services/storage/public/mojom/block.mojom.h"
...

TEST(StorageServiceTest, AllocateBlock) {
  const char kTestServiceName[] = "my_inconsequentially_named_test_service";
  service_manager::TestServiceManager service_manager(
      // Make sure the Service Manager knows about the storage service.
      {storage::GetManifest,

       // Also make sure it has a manifest for our test service, which this
       // test will effectively act as an instance of.
       service_manager::ManifestBuilder()
           .WithServiceName(kTestServiceName)
           .RequireCapability(storage::mojom::kServiceName,
                              storage::mojom::kAllocationCapability)
           .Build()});
  service_manager::TestService test_service(
      service_manager.RegisterTestInstance(kTestServiceName));

  mojo::Remote<storage::mojom::BlockAllocator> allocator;

  // This Connector belongs to the test service instance and can reach the
  // storage service through the Service Manager by virtue of the required
  // capability above.
  test_service.connector()->Connect(storage::mojom::kServiceName,
                                          allocator.BindNewPipeAndPassReceiver());

  // Verify that we can request a small block of storage.
  mojo::Remote<storage::mojom::Block> block;
  allocator->Allocate(64, block.BindNewPipeAndPassReceiver());

  // Do some stuff with the block, etc...
}
```

#### In-Process Service API Tests

Sometimes you want to poke at your service primarily through its client API,
but you also want to be able to -- either for convenience or out of necessity --
observe or manipulate its internal state within the test code. Running the
service in-process is ideal in this case, and in that case there's not much use
in involving the Service Manager or dealing with manifests.

Instead you can use a
[`TestConnectorFactory`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/test/test_connector_factory.h)
to give yourself a working `Connector` object which routes interface requests
directly to specific service instances which you wire up directly. For a quick
example, suppose we had some client library helper function for allocating a
block of storage when given a `Connector`:

``` cpp
// src/services/storage/public/cpp/allocate_block.h

namespace storage {

// This helper function can be used by any service which is granted the
// |kAllocationCapability| capability.
mojo::Remote<mojom::Block> AllocateBlock(service_manager::Connector* connector,
                              uint64_t size) {
  mojo::Remote<mojom::BlockAllocator> allocator;
  connector->Connect(mojom::kServiceName, allocator.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::Block> block;
  allocator->Allocate(size, block.BindNewPipeAndPassReceiver());
  return block;
}

}  // namespace storage
```

Our test could look something like:

``` cpp
TEST(StorageTest, AllocateBlock) {
  service_manager::TestConnectorFactory test_connector_factory;
  storage::StorageService service(
      test_connector_factory.RegisterInstance(storage::mojom::kServiceName));

  constexpr uint64_t kTestBlockSize = 64;
  mojo::Remote<storage::mojom::Block> block = storage::AllocateBlock(
      test_connector_factory.GetDefaultConnector(), kTestBlockSize);
  block.FlushForTesting();

  // Verify that we have the expected number of bytes allocated within the
  // service implementation.
  EXPECT_EQ(kTestBlockSize, service.GetTotalAllocationSizeForTesting());
}
```

### Connectors

While the
[`Service`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service.h)
interface is what the Service Manager uses to drive a service instance's
behavior, a
[`Connector`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/connector.h)
is what the service instance uses to send requests to the Service Manager. This
interface is connected when your instance is launched, and `ServiceBinding`
maintains and
[exposes](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service_binding.h?rcl=887b934e0d979f3da81c41cadc396b4ef587257a&l=66)
it on your behalf.

#### Sending Interface Receivers

By far the most common and useful method on `Connector` is `Connect`,
which allows your service to send an interface receiver to another service in the
system, configuration permitting.

Supposing the `storage` service actually depended on an even lower-level storage
service to get at its disk, you could imagine its block allocation code doing
something like:

``` cpp
  mojo::Remote<real_storage::mojom::ReallyRealStorage> storage;
  service_binding_.GetConnector()->Connect(
      real_storage::mojom::kServiceName, storage.BindNewPipeAndPassReceiver());
  storage->AllocateBytes(...);
```

Note that the first argument to this particular overload of `Connect` is
a string, but the more generalized form of `Connect` takes a
`ServiceFilter`. See more about these in the section on
[Service Filters](#Service-Filters).

#### Registering Service Instances

One of the superpowers services can be granted is the ability to forcibly inject
new service instances into the Service Manager's universe. This is done via
[`Connector::ServiceInstance`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/connector.h?rcl=ec509adfa3ac85fab3cd51422b8aaf9cbb6b43cb&l=108) and is still
used pretty heavily by Chromium's browser process. Most services don't need to
touch this API.

#### Usage in Multithreaded Environments

Connectors are **not** thread-safe, but they do support **cloning**. There are
two useful ways you can associate a new Connector with an existing one on a
different thread.

You can `Clone` the `Connector` on its own thread and then pass the clone to
another thread:

``` cpp
std::unique_ptr<service_manager::Connector> new_connector = connector->Clone();
base::PostTask(...[elsewhere]...,
               base::BindOnce(..., std::move(new_connector)));
```

Or you can fabricate a brand new `Connector` right from where you're standing,
and asynchronously associate it with one on another thread:

``` cpp
mojo::PendingReceiver<service_manager::mojom::Connector> receiver;
std::unique_ptr<service_manager::Connector> new_connector =
    service_manager::Connector::Create(&receiver);

// |new_connector| can be used to start issuing calls immediately, despite not
// yet being associated with the establshed Connector. The calls will queue as
// long as necessary.

base::PostTask(
    ...[over to the correct thread]...,
    base::BindOnce([](
      mojo::PendingReceiver<service_manager::Connector> receiver) {
      service_manager::Connector* connector = GetMyConnectorForThisThread();
      connector->BindConnectorReceiver(std::move(receiver));
    }));
```

### Identity

Every service instance started by the Service Manager is assigned a globally
unique (across space *and* time) identity, encapsulated by the
[`Identity`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/identity.h)
type. This value is communicated to the service and retained and
[exposed](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service_binding.h?rcl=b8bc0ab281f2cb5cd567dc994692c6022845fb89&l=62)
by `ServiceBinding` immediately before `Service::OnStart` is invoked.

There are *four* components to an `Identity`:

- Service name
- Instance ID
- Instance group ID
- Globally unique ID

You're already quite familiar with the **service name**: this is whatever the
service declared in its manifest, *e.g.*, `"storage"`.

#### Instance ID

**Instance ID** is a `base::Token` qualifier which is simply used to
differentiate multiple instances of the service if multiple instances are
desired for whatever arbitrary reason. By default instances get an instance ID
of zero when started unless a connecting client *explicitly* requests a specific
instance ID. Doing so requires a special manifest-declared capability covered by
[Additional Capabilities](#Additional-Capabilities).

*** aside
A good example of how instance ID can be useful: the `"unzip"` service in
Chrome is used to safely unpack untrusted Chrome extensions (CRX) archives, but
we don't want multiple extensions being unpacked by the same process. To support
this, Chrome generates a random `base::Token` for the instance ID it uses when
connecting to the `"unzip"` service, and this elicits the creation of a new
service instance in a new isolated process for each such connection. See
[Service Filters](#Service-Filters) for how this can be done.
***

#### Instance Group ID

All created service instances implicitly belong to an **instance group**, which
is also identified by a `base::Token`. Unless either specially privileged by
[Additional Capabilities](#Additional-Capabilities), or the target service is
a [singleton or shared across groups](#Instance-Sharing), the service sending out
an interface request can only reach other service instances in the same instance
group. See [Instance Groups](#Instance-Groups) for more information.

#### Globally Unique ID

Finally, the **globally unique ID** is a cryptographically secure, unguessably
random `base::Token` value which can be considered unique across all time and
space. This can never be controlled by an instance or even by a highly
privileged service, and its sole purpose is to ensure that `Identity` itself
can be treated as unique across time and space. See
[Service Filters](#Service-Filters) and
[Observing Service Instances](#Observing-Service-Instances) for why this
property of uniqueness is useful and sometimes necessary.

### Instance Sharing

Assuming the Service Manager has decided to allow an interface request due to
sufficient capability requirements, it must consider a number of factors to
decide where exactly to route the request. The first factor is the **instance
sharing policy** of the target service, declared in its manifest. There are
three supported policies:

- **No sharing** - This means the precise identity of the target instance
  depends on both the instance ID provided by the request's `ServiceFilter`,
  as well as the instance group either provided by the `ServiceFilter` or
  inherited from the source instance's group.
- **Shared across groups** - This means the precise identity of the target
  instance still depends on the instance ID provided by the request's
  `ServiceFilter`, but the instance group of both the `ServiceFilter` and the
  source instance are completely ignored.
- **Singleton** - This means there can be only one instance of the service at
  a time, no matter what. Instance ID and group are always ignored when
  connecting to the service.

Based on one of the policies above, the Service Manager determines whether or
not an existing service instance matches the parameters specified by the given
`ServiceFilter` in conjunction with the source instance's own identity. If so,
that Service Manager will forward the interface request to that instance via
`Service::OnBindInterface`. Otherwise, it will spawn a new instance which
sufficiently matches the constraints, and it will forward the request to that
new instance.

### Instance Groups

Service instances are organized into **instance groups**. These are arbitrary
partitions of instances which can be used by the host application to impose
various kinds of security boundaries.

Most services in the system do not have the privilege of specifying the
instance group they want to connect into when passing a `ServiceFilter` to
`Connector::Connect` (see
[Additional Capabilities](#Additional-Capabilities)). As such, most
`Connect` calls implicitly inherit the group ID of the caller and only
cross outside of the caller's instance group when targeting a service which
adopts either a singleton or shared-across-groups
[sharing policy](#Instance-Sharing) in its manifest.

Singleton and shared-across-groups services are themselves always run in their
own isolated groups.

### Service Filters

The most common form of `Connect` calls passes a simple string as the
first argument. This is essentially telling the Service Manager that the caller
doesn't care about any details regarding the target instance's identity -- it
only cares about talking to *some* instance of the named service.

When a client *does* care about other details, they can explicitly construct and
pass a `ServiceFilter` object, which essentially provides some subset of the
desired target instance's total `Identity`.

Specifying an instance group or instance ID in a `ServiceFilter` requires a
service to declare [additional capabilities](#Additional-Capabilities) in its
manifest options.

A `ServiceFilter` can also wrap a complete `Identity` value, including the
globally unique ID. This filter always *only* matches a specific instance unique
in space and time. So if the identified instance has died and been replaced by
a new instance with the same service name, same instance ID, and same instance
group, the request will still *fail*, because the globally unique ID component
will *never* match this or any future instance.

One useful property of targeting a specific `Identity` is that the client can
connect without any risk of eliciting new target instance creation: either
the target exists and the request can be routed, or the target doesn't exist
and the request will be dropped.

### Additional Capabilities

Service manifests can use `ManifestOptionsBuilder` to set a few additional
boolean options controlling their Service Manager privileges:

- `CanRegisterOtherServiceInstances` - If this is `true` the service can call
  `RegisterServiceInstance` on its `Connector` to forcibly introduce new service
  instances into the environment.
- `CanConnectToInstancesWithAnyId` - If this is `true` the service can specify
  an instance ID in any `ServiceFilter` it passes to `Connect`.
- `CanConnectToInstancesInAnyGroup` - If this is `true` the service can specify
  an instance group ID in any `ServiceFilter` it passes to `Connect`.

### Packaging

A service can declare that it **packages** another service by
[nesting](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/manifest_builder.h?rcl=7839843db1ccdf13c3f1b8cb90a763989dde83a8&l=87) that
service's manifest within its own.

This signals to the Service Manager that it should defer to the packaging
service when it needs a new instance of the packaged service. For example, if
we offered up the manifest:

``` cpp
    service_manager::ManifestBuilder()
        .WithServiceName("fruit_vendor")
        ...
        .PackageService(service_manager::ManifestBuilder()
                            .WithServiceName("banana_stand")
                            .Build())
        .Build()
```

And someone wanted to connect to a new instance of the `"banana_stand"` service
(there's always money in the banana stand), the Service Manager would ask an
appropriate `"fruit_vendor"` instance to do this on its behalf.

*** aside
NOTE: If an appropriate instance of `"fruit_vendor"` wasn't already running --
as determined by the rules described in [Instance Sharing](#Instance-Sharing)
above -- one would first be spawned by the Service Manager.
***

In order to support this operation, the `fruit_vendor` must expose a capability
named exactly `"service_manager:service_factory"` which includes the
`"service_manager.mojom.ServiceFactory"` interface. Then it must handle requests
for the `service_manager.mojom.ServiceFactory` interface in its implementation
of `Service::OnBindInterface`. The implementation of `ServiceFactory` provided
by the service must then handle the `CreateService` that will be sent by
the Service Manager. This call will include the name of the service and the
`ServiceRequest` the new service instance will need to bind.

*** aside
NOTE: It is this complicated for historical reasons. Expect it to be less
complicated soon.
***

Services can use this for example if, in certain runtime environments, they want
to share their process with another service.

*** aside
FUN FACT: This is actually how Chromium manages *all* services today, because
the Content layer still owns much of the production-ready process launching
logic. We have a singleton `content_packaged_services` service which packages
nearly all other registered services in the system, and so the Service Manager
defers (via `ServiceFactory`) nearly all service instance creation operations
to Content.
***

### Sandbox Configurations

Service manifests support specifying a fixed sandbox configuration for the
service to be launched with when run out-of-process. Currently these values
are strings which must match one of the defined constants
[here](https://cs.chromium.org/chromium/src/services/service_manager/sandbox/switches.cc?rcl=2e6a3bddac0aff89c5ff415e9c1cd4da804280ef&l=23).

The most common and default value is `"utility"`, which is a restrictive sandbox
configuration and generally a safe choice. For services which must run
unsandboxed, use the value `"none"`. Use of other sandbox configurations should
be done under the advisory of Chrome's security reviewers.

### Observing Service Instances

Services which require the `"service_manager:service_manager`" capability from
the `"service_manager"` service can connect to the `"service_manager"` service
to request a
[`ServiceManager`](https://cs.chromium.org/chromium/src/services/service_manager/public/mojom/service_manager.mojom?rcl=765c18ee7c317535594ba37520a23c11f0cef008&l=82)
interface. This can in turn be used to register a new
[`ServiceManagerListener`](https://cs.chromium.org/chromium/src/services/service_manager/public/mojom/service_manager.mojom?rcl=765c18ee7c317535594ba37520a23c11f0cef008&l=44) to
observe lifecycle events pertaining to all service instances hosted by the
Service Manager.

There are several
[examples](https://cs.chromium.org/search/?q=mojo::Binding%3Cservice_manager::mojom::ServiceManagerListener%3E&type=cs)
of this throughout the tree.

## Additional Support

If this document was not helpful in some way, please post a message to your
friendly
[services-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/services-dev)
mailing list.

Also don't forget to take a look at other
[Mojo &amp; Services](/docs/README.md#Mojo-Services) documentation in the tree.
