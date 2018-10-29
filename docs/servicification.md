# Servicification Strategies

This document captures strategies, hints, and best practices for solving typical
challenges enountered when converting existing Chromium
code to services. It is assumed that you have already read the high-level
documentation on [what a service is](/services).

If you're looking for Mojo documentation, please see the [general
Mojo documentation](/mojo) and/or the [documentation on converting Chrome IPC to
Mojo](/ipc).

Note that throughout the below document we link to CLs to illustrate the
strategies being made. Over the course of time code tends to shift, so it is
likely that the code on trunk does not exactly match what it was at the time of
the CLs. When necessary, use the CLs as a starting point for examining the
current state of the codebase with respect to these issues (e.g., exactly where
a service is embedded within the content layer).

[TOC]

## Questions to Answer When Getting Started

For the basic nuts and bolts of how to create a new service, see [the
documentation on adding a new service](/services#Adding-a-new-service). This 
section gives questions that you should answer in order to shape the design of 
your service, as well as hints as to which answers make sense given your 
situation.

### Is your service global or per-BrowserContext?
The Service Manager can either:

- create one service instance per user ID or
- field all connection requests for a given service via the same instance

Which of these policies the Service Manager employs is determined by the
contents of your service manifest: the former is the default, while the latter
is selected by informing the Service Manager that your service has the
"instance_sharing" option value set to "shared_instance_across_users"
([example](https://cs.chromium.org/chromium/src/services/device/manifest.json)).

Service manifests are described in more detail in this
[document](https://chromium.googlesource.com/chromium/src/+/master/services/service_manager/service_manifests.md).

In practice, there is one user ID per-BrowserContext, so the question becomes:
Is your Service a global or keyed by BrowserContext?  In considering this
question, there is one obvious hint: If you are converting per-Profile classes
(e.g., KeyedServices), then your service is almost certainly going to be
per-user. More generally, if you envision needing to use *any* state related to
the user (e.g., you need to store files in the user's home directory), then your
service should be per-user.

Conversely, your service could be a good fit for being global if it is a utility
that is unconcerned with the identity of the requesting client (e.g., the [data
decoder service](/services/data_decoder), which simply decodes untrusted data in
a separate process.

### Will you embed your service in //content, //chrome, or neither?

At the start (and potentially even long-term), your service will likely not
actually run in its own process but will rather be embedded in the browser
process. This is especially true in the common case where you are converting
existing browser-process code.

You then have a question: Where should it be embedded? The answer to this
question hinges on the nature and location of the code that you are converting:
 
- //content is the obvious choice if you are converting existing //content code
  (e.g., the Device Service). Global services
  are embedded by [content::ServiceManagerContext](https://cs.chromium.org/chromium/src/content/browser/service_manager/service_manager_context.cc?type=cs&q=CreateDeviceService), 
  while per-user services are naturally embedded by [content::BrowserContext](https://cs.chromium.org/chromium/src/content/browser/browser_context.cc?type=cs&q=CreateFileService).

- If your service is converting existing //chrome code, then you will need
  to embed your service in //chrome rather than //content. Global services
  are embedded by [ChromeContentBrowserClient](https://cs.chromium.org/chromium/src/chrome/browser/chrome_content_browser_client.cc?type=cs&q=CreateMediaService), 
  while per-user services are embedded by [ProfileImpl](https://cs.chromium.org/chromium/src/chrome/browser/profiles/profile_impl.cc?type=cs&q=CreateIdentityService).

- If you are looking to convert all or part of a component (i.e., a feature in
  //components) into a service, the question arises of whether your new service
  is worthy of being in //services (i.e., is it a foundational service?). If
  not, then it can be placed in //components/services. See this
  [document](https://docs.google.com/document/d/1Zati5ZohwjUM0vz5qj6sWg5r-_I0iisUoSoAMNdd7C8/edit#) for discussion of this point.

### If your service is embedded in the browser process, what is its threading model?

If your service is embedded in the browser process, it will run on the IO thread
by default. You can change that by specifying a task runner as part of the
information for constructing your service. In particular, if the code that you
are converting is UI-thread code, then you likely want your service running on
the UI thread. Look at the changes to profile_impl.cc in [this
CL](https://codereview.chromium.org/2753753007) to see an example of setting the
task runner that a service should be run on as part of the factory for creating
the service.

### What is your approach for incremental conversion?

In creating your service, you likely have two goals:

- Making the service available to other services
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
conversion will be. Here some approaches that have been taken for various
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

## Platform-Specific Issues

### Android
As you servicify code running on Android, you might find that you need to port 
interfaces that are served in Java. Here is an [example CL](https://codereview.chromium.org/2643713002) that gives a basic
pattern to follow in doing this.

You also might need to register JNI in your service. That is simple to set
up, as illustrated in [this CL](https://codereview.chromium.org/2690963002).
(Note that that CL is doing more than *just* enabling the Device Service to
register JNI; you should take the register_jni.cc file added there as your
starting point to examine the pattern to follow).

Finally, it is possible that your feature will have coupling to UI process state
(e.g., the Activity) via Android system APIs. To handle this challenging
issue, see the section on [Coupling to UI](#Coupling-to-UI).

### iOS

Services are supported on iOS, with the usage model in //ios/web being very
close to the usage model in //content. More specifically:

* To embed a global service in the browser service, override
  [WebClient::RegisterServices](https://cs.chromium.org/chromium/src/ios/web/public/web_client.h?q=WebClient::Register&sq=package:chromium&l=136). For an
  example usage, see
  [ShellWebClient](https://cs.chromium.org/chromium/src/ios/web/shell/shell_web_client.mm?q=ShellWebClient::RegisterS&sq=package:chromium&l=91)
  and the related integration test that [connects to the embedded service](https://cs.chromium.org/chromium/src/ios/web/shell/test/service_manager_egtest.mm?q=service_manager_eg&sq=package:chromium&l=89).
* To embed a per-BrowserState service, override
  [BrowserState::RegisterServices](https://cs.chromium.org/chromium/src/ios/web/public/browser_state.h?q=BrowserState::RegisterServices&sq=package:chromium&l=89). For an
  example usage, see
  [ShellBrowserState](https://cs.chromium.org/chromium/src/ios/web/shell/shell_browser_state.mm?q=ShellBrowserState::RegisterServices&sq=package:chromium&l=48)
  and the related integration test that [connects to the embedded service](https://cs.chromium.org/chromium/src/ios/web/shell/test/service_manager_egtest.mm?q=service_manager_eg&sq=package:chromium&l=110).
* To register a per-frame Mojo interface, override
  [WebClient::BindInterfaceRequestFromMainFrame](https://cs.chromium.org/chromium/src/ios/web/public/web_client.h?q=WebClient::BindInterfaceRequestFromMainFrame&sq=package:chromium&l=148). For an
  example usage, see
  [ShellWebClient](https://cs.chromium.org/chromium/src/ios/web/shell/shell_web_client.mm?type=cs&q=ShellWebClient::BindInterfaceRequestFromMainFrame&sq=package:chromium&l=115)
  and the related integration test that [connects to the interface](https://cs.chromium.org/chromium/src/ios/web/shell/test/service_manager_egtest.mm?q=service_manager_eg&sq=package:chromium&l=130). Note that this is the
  equivalent of [ContentBrowserClient::BindInterfaceRequestFromFrame()](https://cs.chromium.org/chromium/src/content/public/browser/content_browser_client.h?type=cs&q=ContentBrowserClient::BindInterfaceRequestFromFrame&sq=package:chromium&l=667), as on iOS all operation "in the content area" is implicitly
  operating in the context of the page's main frame.

If you have a use case or need for services on iOS, contact
blundell@chromium.org. For general information on the motivations and vision for supporting services on iOS, see the high-level [servicification design doc](https://docs.google.com/document/d/15I7sQyQo6zsqXVNAlVd520tdGaS8FCicZHrN0yRu-oU/edit) (in particular, search for the mentions
of iOS within the doc).

## Client-Specific Issues

### Services and Blink
Connecting to services directly from Blink is fully supported. [This
CL](https://codereview.chromium.org/2698083007) gives a basic example of
connecting to an arbitrary service by name from Blink (look at the change to
SensorProviderProxy.cpp as a starting point).

Below, we go through strategies for some common challenges encountered when
servicifying features that have Blink as a client.

#### Mocking Interface Impls in JS
It is a common pattern in Blink's layout tests to mock a remote Mojo interface
in JS. [This CL](https://codereview.chromium.org/2643713002) illustrates the
basic pattern for porting such mocking of an interface hosted by
//content/browser to an interface hosted by an arbitrary service (see the
changes to mock-battery-monitor.js).

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
   about its usage by Blink. See [this CL](https://codereview.chromium.org/2415083002) for a basic pattern to follow.

#### Frame-Scoped Connections
You must think carefully about the scoping of the connection being made
from Blink. In particular, some feature requests are necessarily scoped to a
frame in the context of Blink (e.g., geolocation, where permission to access the
interface is origin-scoped). Servicifying these features is then challenging, as
Blink has no frame-scoped connection to arbitrary services (by design, as
arbitrary services have no knowledge of frames or even a notion of what a frame
is).

After a [long
discussion](https://groups.google.com/a/chromium.org/forum/#!topic/services-dev/CSnDUjthAuw),
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

In the longer term, this will still be the basic model, only with "the browser"
replaced by "the Navigation Service" or "the web permissions broker".

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
service makes use of it. The embedder also serves as an intermediary: It
provides a connection that is appropriately context-scoped to clients. When
clients request the feature in question, the embedder forwards the request on
along with the appropriate context ID.  The service impl can then map that
context ID back to the needed context on-demand using the mapping functionality
injected into the service impl.

To make this more concrete, see [this CL](https://codereview.chromium.org/2734943003).

### Shutdown of singletons

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

See [this thread](https://groups.google.com/a/chromium.org/forum/#!topic/services-dev/Y9FKZf9n1ls) for more discussion of this issue.

### Tests that muck with service internals
It is often the case that browsertests reach directly into what will become part
of the internal service implementation to either inject mock/fake state or to
monitor private state.

This poses a challenge: As part of servicification, *no* code outside the
service impl should depend on the service impl. Thus, these dependencies need to
be removed. The question is how to do so while preserving testing coverage.

To answer this question, there are several different strategies. These
strategies are not mutually-exclusive; they can and should be combined to
preserve the full breadth of coverage.

- Blink client-side behavior can be tested via [layout tests](https://codereview.chromium.org/2731953003)
- To test service impl behavior, create [service tests](https://codereview.chromium.org/2774783003).
- To preserve tests of end-to-end behavior (e.g., that when Blink makes a
  request via a Web API in JS, the relevant feature impl receives a connection
  request), we are planning on introducing the ability to register mock 
  implementations with the Service Manager.

To emphasize one very important point: it is in general necessary to leave
*some* test of end-to-end functionality, as otherwise it is too easy for bustage
to slip in via e.g. changes to how services are registered. See [this thread](https://groups.google.com/a/chromium.org/forum/#!topic/services-dev/lJCKAElWz-E)
for further discussion of this point.
